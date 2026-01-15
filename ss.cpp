#include<iostream>
#include<winsock.h>
#include<string>
#include<thread>

#pragma comment(lib,"ws2_32.lib")

char recvBuf[255] = { "\0" };
char sendBuf[255] = { "\0" };
using namespace std;
typedef struct conn {
	SOCKADDR_IN addrClient;
	SOCKET sockConn;
	int len = sizeof(addrClient);
	char currentRoom[7] = "public"; // "public" for public room, 4-digit string for private room
}CON;
class node{
public:
	node* next;

	CON cc;
	node() {
		next = NULL;
	}
	node(CON cc) {
		this->cc = cc;
		next = NULL;
	}
};
static int num = 0;//��ǰ���ܵ�socket��Ŀ
class list {
public:
	node* first;
	list() {
		first =  NULL;
	}
	int isempty() {
		if (first == NULL) { return 1; }
		return 0;
	}
	void add(node*newnode) {
		node* p = first;
		if (isempty()) {
			first = newnode;
			newnode->next=first;
			return;
		}
		while (p->next != first) {
			p = p->next;
		}
		p->next = newnode;
		newnode->next = first;

	}
	void del(node*q) {
		node* p = first;
		if (p==q) {
			if (p->next != first) {

				node* y = first;
				while (y->next != first) {
					y = y->next;
				}
				y->next = first->next;
				first = y->next;
				delete q;
				q = NULL;


			}
			else {
				first = NULL;
			}
		}
		else {
			while (p->next != q) {
				p = p->next;
			}
			p->next = q->next;
			delete q;
			q = NULL;
		}
	}

	// Find a node by socket
	node* findBySocket(SOCKET sock) {
		if (isempty()) return NULL;
		node* p = first;
		do {
			if (p->cc.sockConn == sock) {
				return p;
			}
			p = p->next;
		} while (p != first);
		return NULL;
	}

	// Count clients in a specific room
	int countClientsInRoom(const char* room) {
		if (isempty()) return 0;
		int count = 0;
		node* p = first;
		do {
			if (strcmp(p->cc.currentRoom, room) == 0) {
				count++;
			}
			p = p->next;
		} while (p != first);
		return count;
	}

};
void flush(char* a) {
	memset(a, 0, sizeof(a));
}

// Function to forward message to clients in the same room
void forwardToRoom(list* cl, node* sender, const char* message) {
	if (cl->isempty()) return;
	
	node* p = cl->first;
	char roomToForward[7];
	strcpy_s(roomToForward, sender->cc.currentRoom);
	
	do {
		if (p != sender && strcmp(p->cc.currentRoom, roomToForward) == 0) {
			send(p->cc.sockConn, message, 255, 0);
			cout << "Forward to client " << p->cc.sockConn << " in room " << roomToForward << " message:" << message << endl;
		}
		p = p->next;
	} while (p != cl->first);
}

//Thread function
void hthreadfun(list*cl,node* newnode) {
	//Processing received data and forward to other clients in the same room
	while (1) {
		flush(recvBuf);
		if (recv((newnode->cc).sockConn, recvBuf, 255, 0) == SOCKET_ERROR) {
			if (WSAGetLastError() == 10054) {
				// Client disconnected
				sprintf_s(recvBuf, "%d has exited the chat", (newnode->cc).sockConn);
				forwardToRoom(cl, newnode, recvBuf);
				
				num--;
				cout << "Current number of clients: " << num << endl;
				cl->del(newnode);
				closesocket((newnode->cc).sockConn);//Close socket
				return;
			}
			else cout << "recv error:" << WSAGetLastError() << endl;
			return;
		}
		else if (strlen(recvBuf) != 0) {
			cout << "Received from client " << (newnode->cc).sockConn << " in room " << (newnode->cc).currentRoom << " message:" << recvBuf << endl;
			
			// Check if it's a command
			if (recvBuf[0] == '/') {
				// Process command
				char command[20];
				char parameter[20];
				sscanf_s(recvBuf, "%s %s", command, sizeof(command), parameter, sizeof(parameter));
				
				if (strcmp(command, "/join") == 0) {
					// Join a room
					strcpy_s((newnode->cc).currentRoom, parameter);
					cout << "Client " << (newnode->cc).sockConn << " joined room " << parameter << endl;
					
					// Send confirmation to client
					char response[255];
					if (strcmp(parameter, "public") == 0) {
						sprintf_s(response, "You are now in the public room");
					} else {
						sprintf_s(response, "You are now in private room %s", parameter);
					}
					send((newnode->cc).sockConn, response, 255, 0);
				}
				else if (strcmp(command, "/create") == 0) {
					// Create private room
					int clientsInRoom = cl->countClientsInRoom(parameter);
					
					if (clientsInRoom == 0) {
						// Room doesn't exist, create it
						strcpy_s((newnode->cc).currentRoom, parameter);
						cout << "Client " << (newnode->cc).sockConn << " created room " << parameter << endl;
						
						// Send confirmation to client
						char response[255];
						sprintf_s(response, "Private room %s created successfully", parameter);
						send((newnode->cc).sockConn, response, 255, 0);
					} else {
						// Room exists, join it
						strcpy_s((newnode->cc).currentRoom, parameter);
						cout << "Client " << (newnode->cc).sockConn << " joined existing room " << parameter << endl;
						
						// Send confirmation to client
						char response[255];
						sprintf_s(response, "Room %s exists, joined successfully", parameter);
						send((newnode->cc).sockConn, response, 255, 0);
					}
				}
				flush(recvBuf);
				continue;
			}
			
			// Forward message to clients in the same room
			forwardToRoom(cl, newnode, recvBuf);
		}
		else { }

		flush(recvBuf);

	}
}
int main() {
	WSACleanup();
	int port;
	cout << "Please enter the listening port:";
	cin >> port;
	WORD wVersionRequested = MAKEWORD(2, 2);
	WSADATA wsaData;

	if (WSAStartup(wVersionRequested, &wsaData) != 0) {

		cout << "WSAStartup() failed" << endl;
		return 0;
	}//Initialize Socket DLL and specify Socket version

	SOCKET sockSrv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);//Create a Socket, using TCP/IP protocol
	if (sockSrv == INVALID_SOCKET) {
		cout << " socket error!report error:" << WSAGetLastError() << endl;
	}

	//Set server IP address and port
	SOCKADDR_IN addrSrv;
	memset(&addrSrv, 0, sizeof(addrSrv));
	addrSrv.sin_family = AF_INET;//IPv4
	addrSrv.sin_addr.S_un.S_addr = inet_addr("0.0.0.0");//IP address
	addrSrv.sin_port = htons(port);//Port


	if (bind(sockSrv, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		cout << "bind error,report error:" << WSAGetLastError() << endl;

	}//Bind address to the specified Socket
	else { cout << "bind succeed!!" << endl; }


	if (listen(sockSrv, 5) == SOCKET_ERROR) {
		cout << "listen error!report error:" << WSAGetLastError() << endl;
		return 1;

	}//Set Socket to listening state, wait for client connections
	else { cout << "listening....." << endl; }

	list *connlist=new list();
	while (1) {
		CON a;
		a.sockConn  = accept(sockSrv, (SOCKADDR*)&a.addrClient, &a.len);//Accept client connection request, waiting for client connections
		if (a.sockConn == INVALID_SOCKET) {
			cout << "accept error! report error:" << WSAGetLastError() << endl;
			continue;
		}
		// Explicitly set current room to "public" for new connections
		strcpy_s(a.currentRoom, "public");
		cout << "received a connection: "<<"   ip:" << inet_ntoa(a.addrClient.sin_addr) << "   port:"<<a.addrClient.sin_port << endl;
		node* newnode=new node(a);
		connlist->add(newnode);
		 num++;
		//Successfully received the communication socket
		sprintf_s(sendBuf, "Welcome %d! Current number of clients: %d", a.sockConn, num);
		node* p = connlist->first;
		if (num > 0) {
			if (num == 1) {
				if (send((p->cc).sockConn, sendBuf, 255, 0) == SOCKET_ERROR) {
					cout << "send to client error!report error:" << WSAGetLastError() << endl;
				}
				else {
					cout << "Sent welcome message to client:" << sendBuf << endl;
				}
			}
			else {
				while (p->next != connlist->first) {
					if (send((p->cc).sockConn, sendBuf, 255, 0) == SOCKET_ERROR) {
						cout << "send to client error!report error:" << WSAGetLastError() << endl;
					}
					else {
						cout << "Sent welcome message to client:" << sendBuf << endl;
					}
					p = p->next;
					if (p->next == connlist->first) {
						if (send((p->cc).sockConn, sendBuf, 255, 0) == SOCKET_ERROR) {
							cout << "send to client error!report error:" << WSAGetLastError() << endl;
						}
						else {
							cout << "Sent welcome message to client:" << sendBuf << endl;
						}
					}

				}
			}
		}
		flush(sendBuf);
		thread h2(hthreadfun,connlist,newnode);
		h2.detach();
	}
	closesocket(sockSrv);
	WSACleanup();
	return 0;
}
