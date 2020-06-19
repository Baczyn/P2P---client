#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

#define CENTRAL_SERVER_PORT 4448
#define MAX_CLIENTS 10
#define BUF_SIZE 1024
#define NICK_SIZE 24

typedef struct clientInfo{
	uint16_t port;
	uint32_t ip;
	char nick[NICK_SIZE];
} clientInfo;

void updateClientList(clientInfo *tab, uint32_t ip, uint16_t port,char *nick){
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if(tab[i].ip==0){
			tab[i].ip=ip;
			tab[i].port = port;
			strncpy(tab[i].nick,nick,NICK_SIZE);
			break;
		}
	}
}

void removeClientFromList(clientInfo *tab, uint32_t ip, uint16_t port){
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if(tab[i].ip==ip && tab[i].port==port){
			tab[i].ip = 0;
			tab[i].port = 0;
			memset(tab[i].nick,'\0',NICK_SIZE);
			break;
		}
	}
}

void printClientList(clientInfo *tab,char *buffer){
	int readed=0;
	readed = sprintf(buffer,"\nActive Clients:\n");
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (tab[i].ip != 0 && tab[i].port != 0)
		{
			//typedef uint32_t in_addr_t;
			struct in_addr tmp;
			tmp.s_addr = tab[i].ip;
			readed += sprintf(buffer+readed,"Client[%d]:%s %s:%d \n", i,tab[i].nick, inet_ntoa(tmp), ntohs(tab[i].port+1) );
		}
	}
}

	int main()
{
	//******Config shared memory*******//

	clientInfo *shmp; //pointer to shared mem

	int shmID = shmget(IPC_PRIVATE, MAX_CLIENTS * sizeof(clientInfo), IPC_CREAT | 0666);
	if (shmID >= 0)
		printf("The fragment of name: %d was created with the ID %d\n", IPC_PRIVATE, shmID);
	else
		perror("Can not create SHM with\n");

	// Attach to the segment to get a pointer to it.
	shmp = (clientInfo *)shmat(shmID, NULL, 0);
	if (shmp == (void *)-1){
		perror("Shared memory attach");
		return 1;
	}
	//clear shared mem
	memset(shmp, '\0', MAX_CLIENTS * sizeof(clientInfo));

	//******Config server*******//
	int serverSocket;
	struct sockaddr_in serverAddr; //struct with server info
	socklen_t serverAddrSize = sizeof(serverAddr);

	int newClientSocket;
	struct sockaddr_in clientAddr; //struct with client info

	char buffer[BUF_SIZE];
	char nick[NICK_SIZE];
	memset(buffer, '\0', BUF_SIZE);
	pid_t childpid;

	//clear serverAddr
	memset(&serverAddr, '\0', sizeof(serverAddr));
	serverAddr.sin_family = AF_INET; //famili IPv4
	serverAddr.sin_port = htons(CENTRAL_SERVER_PORT); 
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY); // device IP - INADDR_ANY

	serverSocket = socket(AF_INET, SOCK_STREAM, 0); //create server socket, SOCK_STREAM->TCP
	if (serverSocket < 0){	
		printf("[-]Error in connection.\n");
		exit(1);
	}

	printf("[+]Server Socket is created.\n");

	//register service on port
	if( bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0 ){
		printf("[-]Error in binding.\n");
		exit(1);
	}
	printf("[+]Bind to port %d\n", htons(CENTRAL_SERVER_PORT));
	//listening on server's socket
	if (listen(serverSocket, MAX_CLIENTS) == 0)
		printf("[+]Listening....\n");
	
	else
		printf("[-]Error in binding.\n");

	while (1){
		newClientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &serverAddrSize);
		if (newClientSocket < 0) {
			perror("accept() error: ");
			exit(1);
		}
		recv(newClientSocket, nick, NICK_SIZE, 0);
		printf("Connection accepted from %s %s:%d\n",nick, inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
		//Update ClientList
		updateClientList(shmp, clientAddr.sin_addr.s_addr, clientAddr.sin_port,nick);
		childpid = fork();
		if (childpid == 0)
		{

			close(serverSocket);

			while(1){
				int bytes_read = recv(newClientSocket, buffer, BUF_SIZE, 0);
				if (bytes_read == -1) {
					perror("recv: ");
					exit(1);
				}
				else if(!bytes_read){
					printf("Disconnected from %s:%d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
					removeClientFromList(shmp, clientAddr.sin_addr.s_addr, clientAddr.sin_port);
					exit(0);
				}
				else if(strcmp(buffer, ":show") == 0){
					printClientList(shmp,buffer);
					send(newClientSocket, buffer,BUF_SIZE, 0);
					memset(buffer, '\0', BUF_SIZE);
				}
				else {
					fprintf(stderr, "Unknown command: %s\n", buffer);
				}
			}
		}
		if (childpid < 0)
			perror("fork() error: ");
	}

		return 0;
	}
