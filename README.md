# P2P-chat
Peer to peer chat with Central server

This is a simple chat application, where you can figure out who is online from the server and then talk to those other users directly.

Client and a server utilizing TCP socket connections.
The client is able to connect to the server and obtain a list of available clients. 
After obtaining this list, the client is able to connect to one other client on the list.
The clients are then capable of relaying text messages directly to one another without further help from the server.
You can define whatever convention you would like to indicate that a text message is complete.
After disconnecting, the user is able to retrieve a new copy of the server’s client list and connect to a new client.
The connection between the two clients are duplex, so both clients are capable of transmitting and receiving at the same time.
