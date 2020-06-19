#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>
#include <errno.h>

#define CENTRAL_SERVER_PORT 4448
#define MAX_CLIENTS 10
#define BUF_SIZE 1024
#define NICK_SIZE 24


void *listenMode(void *args);
void *recvServer(void *args);

static pthread_t listen_tid;
static pthread_t recv_tid;

typedef struct sockaddr_in sockAddr;

typedef struct client_socket_nick
{
    int socket;
    char nick[24];
} ClientInfo;

int findNick(int socketfd, ClientInfo *tab);
void printMenu();
void connectToCetralServer(int *socketToCentralServer,char *nick);
void connectToFriend(char *nick);



static socklen_t sockaddrSize = sizeof(sockAddr);

int main(){

    
    int socketToCentralServer;

    char buffer[BUF_SIZE];
    memset(buffer, '\0', BUF_SIZE);
    char nick[NICK_SIZE];
    memset(nick, '\0', NICK_SIZE);

    connectToCetralServer(&socketToCentralServer,nick);
    printMenu();

    sockAddr listenAddr;
    //listenAddr is same as addres to CETRAL SERVER but port is incremented 
    getsockname(socketToCentralServer, (struct sockaddr *)&listenAddr, &sockaddrSize); 
    listenAddr.sin_port++;

    if (pthread_create(&listen_tid, NULL, listenMode, (void *)&listenAddr) != 0)
    {
        perror("could not create thread");
        exit(1);
    }
    if (pthread_create(&recv_tid, NULL, recvServer, (void *)&socketToCentralServer) != 0)
    {
        perror("could not create thread");
        exit(1);
    }

    while (1){
        memset(buffer, '\0', BUF_SIZE);
        scanf("%s", &buffer[0]);

        if (strcmp(buffer, "1") == 0)
        {
            strcpy(buffer,":show");
            send(socketToCentralServer, buffer, BUF_SIZE, 0);
        }
        else  if (strcmp(buffer, "2") == 0)
            connectToFriend(nick);
        else if (strcmp(buffer, "3") == 0)
        {
            close(socketToCentralServer);
            printf("[-]Disconnected from server.\n");
            exit(1);
        }
    }

    return 0;
}
void *listenMode(void *args)
{
    int opt = 1;
    int master_socket;
    int new_socket, activity, valread, sd;
    int max_sd;

    sockAddr clientAddr;
    memset(&clientAddr, '\0', sizeof(clientAddr));

        //initialise all clientTab[] to 0 so not checked
    ClientInfo clientTab[MAX_CLIENTS];
    memset(clientTab, '\0', MAX_CLIENTS * sizeof(ClientInfo));


    sockAddr listenAddr = *((sockAddr *)args);

    char buffer[BUF_SIZE]; //data buffer of 1K

    //set of socket descriptors
    fd_set readfds;

    //create a master socket
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    //printf("Socket created...\n");
    //set master socket to allow multiple connections ,
    //this is just a good habit, it will work without this
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    //bind the socket to localhost port listenAddr.sin_port;
    if (bind(master_socket, (struct sockaddr *)&listenAddr, sockaddrSize) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    //printf("Listening on Port:%d\n", ntohs(listenAddr.sin_port));
    //try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while (1)
    {

        // clear the socket set
        FD_ZERO(&readfds);

        //add master socket to set
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        //add child sockets to set
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            //socket descriptor
            sd = clientTab[i].socket;
            //if valid socket descriptor then add to read list
            if (sd > 0)
                FD_SET(sd, &readfds);
            //highest file descriptor number, need it for the select function
            if (sd > max_sd)
                max_sd = sd;
        }
        //wait for an activity on one of the sockets , timeout is NULL ,
        //so wait indefinitely
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR))
        {
            printf("select error");
        }

        //If something happened on the master socket ,
        //then its an incoming connection
        if (FD_ISSET(master_socket, &readfds))
        {
            if (new_socket = accept(master_socket, (struct sockaddr *)&clientAddr, &sockaddrSize) < 0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            //inform user of socket number - used in send and receive commands
            if (recv(new_socket, buffer, BUF_SIZE, 0) < 0)
                perror("recv");
            printf("New connection , socket fd is %d , ip is : %s , port : %d Nick: %s\n ", new_socket, inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), buffer);

            //add new socket to array of sockets
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                //if position is empty
                if (clientTab[i].socket == 0)
                {
                    clientTab[i].socket = new_socket;
                    strncpy(clientTab[i].nick, buffer, NICK_SIZE);
                    printf("Adding to list of sockets as %d:%s\n", i, clientTab[i].nick);

                    break;
                }
            }
        }

        //else its some IO operation on some other socket
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            sd = clientTab[i].socket;
            if (FD_ISSET(sd, &readfds))
            {
                //Check if it was for closing , and also read the
                //incoming message
                if ((valread = read(sd, buffer, BUF_SIZE)) == 0)
                {
                    //Somebody disconnected , get his details and print
                    getpeername(sd, (struct sockaddr *)&listenAddr, &sockaddrSize);
                    //printf("Host disconnected , ip %s , port %d \n",inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                    printf("Host disconnected , ip %s , port %d \n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
                    //Close the socket and mark as 0 in list for reuse
                    close(sd);
                    clientTab[i].socket = 0;
                    memset(clientTab[i].nick, '\0', NICK_SIZE);
                }
                else
                {
                    int idx = findNick(sd, clientTab);
                    if (idx < 0)
                        perror("sd is not in tab\n");
                    buffer[valread] = '\0';
                    printf(">%s:%s", clientTab[idx].nick, buffer);
                }
            }
        }
    }

    return 0;
}


void *recvServer(void *args){
    char buffer[BUF_SIZE];
    int socketToCentralServer = *((int *)args);
    int bytes_read ;
    while (1)
    {
        memset(buffer, '\0', BUF_SIZE);
        bytes_read = recv(socketToCentralServer, buffer, BUF_SIZE, 0);
        if (bytes_read < 0)
        {
            printf("[-]Error in receiving data.\n");
        }
        else if (!bytes_read)
        {
            printf("server closed connection");
        }
        else
        {
            printf("%s\n", buffer);
        }
    }
    
   
}

int findNick(int socketfd, ClientInfo *tab)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (tab[i].socket == socketfd)
            return i;
    return -1;
}
void printMenu()
{
    printf("\n*********************MENU*************************\n");
    printf("1 - show all active clients\n");
    printf("2 - connect to firend by insert port\n");
    printf("3 - exit\n");
    printf("**************************************************\n");
}
void connectToCetralServer(int *socketToCentralServer, char *nick)
{
    sockAddr serverAddr; //SERVER CENTRAL ADRESS
    memset(&serverAddr, '\0', sockaddrSize);

    *socketToCentralServer = socket(AF_INET, SOCK_STREAM, 0);
    if (*socketToCentralServer < 0)
    {
        printf("[-]Error in connection.\n");
        exit(1);
    }
    //printf("[+]Server Socket is created.\n");

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(CENTRAL_SERVER_PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(*socketToCentralServer, &serverAddr, sockaddrSize) < 0)
    {
        printf("[-]Error in connection.\n");
        exit(1);
    }
    printf("[+]Connected to Central Server.\n");
    printf("[+]Insert your Nick.\n");

    //gets input from usr
    while (scanf("%s", nick) > NICK_SIZE)
    {
        perror("the name you've entered is too big!\n");
        perror("Please try again!\n");
        scanf("%s", nick);
    }
    printf("[+]Welcome: %s! ", nick);

    //send nick to Central Server
    if (send(*socketToCentralServer, nick, NICK_SIZE, 0) < 0)
        perror("send nick:");
}

void connectToFriend(char *nick)
{
    char buffer[BUF_SIZE];
    memset(buffer, '\0', BUF_SIZE);

    sockAddr friendAddr;
    memset(&friendAddr, '\0', sizeof(friendAddr));

    friendAddr.sin_family = AF_INET;
    friendAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    printf("[+]Insert port of client you want to connect.\n");
    scanf("%s", &buffer[0]);
    u_int16_t to_connect = atoi(buffer);
    friendAddr.sin_port = htons(to_connect);

    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0)
    {
        printf("[-]Error in connection.\n");
        return;
    }
    if (connect(clientSocket, (sockAddr *)&friendAddr, sizeof(friendAddr)) < 0)
    {
        printf("[-]Error in connection.\n");
        return;
    }
    printf("[+]Connected to friend. Type :exit to disconect\n");
    send(clientSocket, nick, NICK_SIZE, 0);
    while (1)
    {
        memset(buffer, '\0', BUF_SIZE);
        if (read(0, buffer, BUF_SIZE) < 0)
            perror("read");

        else if (strcmp(buffer, ":exit\n") == 0)
        {
            if (close(clientSocket) < 0)
                perror("close");
            printf("[-]Disconnected from firend.\n");
            break;
        }
        write(clientSocket, buffer, BUF_SIZE);
    }
}