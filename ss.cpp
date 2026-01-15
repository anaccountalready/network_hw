#include<iostream>
#include<winsock.h>
#include<string>
#include<thread>
#include<unordered_map>
#include<set>
#include<list>
#include<mutex>
#include<ctime>

#pragma comment(lib,"ws2_32.lib")

char recvBuf[255] = { "\0" };
char sendBuf[255] = { "\0" };
using namespace std;

// Forward declarations
class node;
class ClientList;

// Define CON struct before node class
typedef struct conn {
	SOCKADDR_IN addrClient;
	SOCKET sockConn;
	int len = sizeof(addrClient);
	string currentRoom; // "public" or room code
}CON;

// Define node class first
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

class ClientList {
public:
	node* first;
	ClientList() {
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

};

// Room management structures
unordered_map<string, ClientList*> privateRooms; // room code -> client list
set<string> usedRoomCodes; // track used room codes
mutex roomsMutex; // protect room operations
static int num = 0;//Current socket count
void flush(char* a) {
	memset(a, 0, sizeof(a));
}

// Function declarations - moved before hthreadfun
string generateRoomCode();
string createPrivateRoom();
bool joinPrivateRoom(string roomCode, node* clientNode);
void leaveRoom(node* clientNode);

// Thread processing function
void hthreadfun(ClientList*cl,node* newnode) {
	//Receive messages and handle room-based broadcasting
	while (1) {
		flush(recvBuf);
		if (recv((newnode->cc).sockConn, recvBuf, 255, 0) == SOCKET_ERROR) {
			if (WSAGetLastError() == 10054) {
				sprintf_s(recvBuf, "Client %d disconnected", (newnode->cc).sockConn);
				num--;
				cout << "Current connections: " << num << endl;
				cl->del(newnode);
				leaveRoom(newnode); // Remove from room
				closesocket((newnode->cc).sockConn);//close socket
				
				// Broadcast disconnect message to appropriate room
				string currentRoom = newnode->cc.currentRoom;
				ClientList* targetList = (currentRoom == "public") ? cl : privateRooms[currentRoom];
				
				if (targetList && !targetList->isempty()) {
					node* p = targetList->first;
					do {
						send((p->cc).sockConn, recvBuf, 255, 0);
						cout << "Broadcasting to client " << (p->cc).sockConn << " message:" << recvBuf << endl;
						p = p->next;
					} while (p != targetList->first);
				}
				return;
			}
			else cout << "recv error:" << WSAGetLastError() << endl;
			return;
		}
		else if (strlen(recvBuf) != 0) {
			cout << "Server received from client " <<(newnode->cc).sockConn << " message:" << recvBuf << endl;
			
			// Handle special commands
			if (strcmp(recvBuf, "MENU") == 0) {
				string menu = "=== MENU ===\nCurrent Room: " + string(newnode->cc.currentRoom) + 
							"\n1. Back to public room\n2. Create private room\n3. Exit\nEnter choice (1-3):";
				send((newnode->cc).sockConn, menu.c_str(), 255, 0);
			}
			else if (strncmp(recvBuf, "CREATE_ROOM:", 12) == 0) {
				string roomCode(recvBuf + 12);
				// Validate room code format (should be 4 characters)
				if (roomCode.length() != 4) {
					string msg = "Invalid room code format. Room codes must be exactly 4 characters.";
					send((newnode->cc).sockConn, msg.c_str(), 255, 0);
				} else {
					// Check if room already exists
					lock_guard<mutex> lock(roomsMutex);
					if (privateRooms.find(roomCode) != privateRooms.end()) {
						// Room exists, join it
						leaveRoom(newnode);
						joinPrivateRoom(roomCode, newnode);
						string msg = "Joined existing private room: " + roomCode;
						send((newnode->cc).sockConn, msg.c_str(), 255, 0);
						// Notify other users in the room
						char clientId[32];
						sprintf_s(clientId, "Client %d", (newnode->cc).sockConn);
						string joinMsg = string(clientId) + " joined the private room.";
						if (privateRooms.find(roomCode) != privateRooms.end() && !privateRooms[roomCode]->isempty()) {
							node* p = privateRooms[roomCode]->first;
							do {
								if (p != newnode) {
									send((p->cc).sockConn, joinMsg.c_str(), 255, 0);
								}
								p = p->next;
							} while (p != privateRooms[roomCode]->first);
						}
					} else {
						// Room doesn't exist, create it
						usedRoomCodes.insert(roomCode);
						privateRooms[roomCode] = new ClientList();
						leaveRoom(newnode);
						joinPrivateRoom(roomCode, newnode);
						string msg = "Private room created! Room code: " + roomCode + "\nShare this code with others to let them join.";
						send((newnode->cc).sockConn, msg.c_str(), 255, 0);
					}
				}
			}
			else if (strncmp(recvBuf, "JOIN_ROOM:", 10) == 0) {
				string roomCode(recvBuf + 10);
				// Validate room code format (should be 4 characters)
				if (roomCode.length() != 4) {
					string msg = "Invalid room code format. Room codes must be exactly 4 characters.";
					send((newnode->cc).sockConn, msg.c_str(), 255, 0);
				} else if (joinPrivateRoom(roomCode, newnode)) {
					string msg = "Joined private room: " + roomCode;
					send((newnode->cc).sockConn, msg.c_str(), 255, 0);
					// Notify other users in the room
					char clientId[32];
					sprintf_s(clientId, "Client %d", (newnode->cc).sockConn);
					string joinMsg = string(clientId) + " joined the private room.";
					if (privateRooms.find(roomCode) != privateRooms.end() && !privateRooms[roomCode]->isempty()) {
						node* p = privateRooms[roomCode]->first;
						do {
							if (p != newnode) {
								send((p->cc).sockConn, joinMsg.c_str(), 255, 0);
							}
							p = p->next;
						} while (p != privateRooms[roomCode]->first);
					}
				} else {
					string msg = "Room not found: " + roomCode;
					send((newnode->cc).sockConn, msg.c_str(), 255, 0);
				}
			}
			else if (strcmp(recvBuf, "BACK_PUBLIC") == 0) {
				leaveRoom(newnode);
				newnode->cc.currentRoom = "public";
				send((newnode->cc).sockConn, "Returned to public room", 255, 0);
			}
			else {
				// Regular message - broadcast to current room
				string currentRoom = newnode->cc.currentRoom;
				cout << "DEBUG: Processing message from client " << (newnode->cc).sockConn << " in room " << currentRoom << endl;
				cout << "DEBUG: Message content: " << recvBuf << endl;
				
				ClientList* targetList = (currentRoom == "public") ? cl : privateRooms[currentRoom];
				
				if (targetList && !targetList->isempty()) {
					cout << "DEBUG: Target list found with members, broadcasting..." << endl;
					int memberCount = 0;
					node* p = targetList->first;
					do {
						memberCount++;
						if (p != newnode) {
							// Broadcast all non-empty messages to room members
							if (strlen(recvBuf) > 0) {
								cout << "DEBUG: Sending to client " << (p->cc).sockConn << " (member " << memberCount << ")" << endl;
								send((p->cc).sockConn, recvBuf, 255, 0);
								cout << "Broadcasting to client " << (p->cc).sockConn << " in room " << currentRoom << " message:" << recvBuf << endl;
							}
						}
						p = p->next;
					} while (p != targetList->first);
					cout << "DEBUG: Total members in room: " << memberCount << endl;
				} else {
					cout << "DEBUG: No target list or empty room" << endl;
				}
			}
		}
		else { }

		flush(recvBuf);

}
}

// Room management functions
string generateRoomCode() {
	string code;
	srand(static_cast<unsigned int>(time(nullptr)));
	for (int i = 0; i < 4; i++) {
		code += 'A' + (rand() % 26);
	}
	return code;
}

string createPrivateRoom() {
	lock_guard<mutex> lock(roomsMutex);
	string roomCode;
	do {
		roomCode = generateRoomCode();
	} while (usedRoomCodes.find(roomCode) != usedRoomCodes.end());
	
	usedRoomCodes.insert(roomCode);
	privateRooms[roomCode] = new ClientList();
	cout << "Created private room: " << roomCode << endl;
	return roomCode;
}

bool joinPrivateRoom(string roomCode, node* clientNode) {
	lock_guard<mutex> lock(roomsMutex);
	if (privateRooms.find(roomCode) == privateRooms.end()) {
		return false;
	}
	privateRooms[roomCode]->add(clientNode);
	clientNode->cc.currentRoom = roomCode;
	return true;
}

void leaveRoom(node* clientNode) {
	lock_guard<mutex> lock(roomsMutex);
	string room = clientNode->cc.currentRoom;
	if (room != "public" && privateRooms.find(room) != privateRooms.end()) {
		privateRooms[room]->del(clientNode);
		if (privateRooms[room]->isempty()) {
			delete privateRooms[room];
			privateRooms.erase(room);
			usedRoomCodes.erase(room);
			cout << "Deleted empty room: " << room << endl;
		}
	}
	clientNode->cc.currentRoom = "public";
}
int main() {
	WSACleanup();
	int port;
	cout << "Please enter server port number: ";
	cin >> port;
	WORD wVersionRequested = MAKEWORD(2, 2);
	WSADATA wsaData;

	if (WSAStartup(wVersionRequested, &wsaData) != 0) {

		cout << "WSAStartup() failed" << endl;
		return 0;
	}//Initialize Socket DLL protocol, using Socket version

	SOCKET sockSrv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);//Create a Socket object and bind it to a specific transport protocol (TCP/IP)
	if (sockSrv == INVALID_SOCKET) {
		cout << " socket error!report error:" << WSAGetLastError() << endl;
	}

	//Set server IP address and port
	SOCKADDR_IN addrSrv;
	memset(&addrSrv, 0, sizeof(addrSrv));
	addrSrv.sin_family = AF_INET;//IPv4
	addrSrv.sin_addr.S_un.S_addr = inet_addr("0.0.0.0");//IP address
	addrSrv.sin_port = htons(port);//Port number


	if (bind(sockSrv, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		cout << "bind error,report error:" << WSAGetLastError() << endl;

	}//Bind address to specified Socket
	else { cout << "bind succeed!!" << endl; }


	if (listen(sockSrv, 5) == SOCKET_ERROR) {
		cout << "listen error!report error:" << WSAGetLastError() << endl;
		return 1;

	}//Make Socket listen for incoming connections
	else { cout << "listening....." << endl; }

	ClientList *connlist=new ClientList();
	while (1) {
		CON a;
		a.sockConn  = accept(sockSrv, (SOCKADDR*)&a.addrClient, &a.len);//Accept connections on specific socket, wait for incoming connections//Accept until connection is established
		if (a.sockConn == INVALID_SOCKET) {
			cout << "accept error! report error:" << WSAGetLastError() << endl;
			continue;
		}
		cout << "Received connection::"<<"   ip:" << inet_ntoa(a.addrClient.sin_addr) << "   port:"<<a.addrClient.sin_port << endl;
		node* newnode=new node(a);
		newnode->cc.currentRoom = "public"; // Initialize to public room
		connlist->add(newnode);
		 num++;
		//Successfully received communication socket sockConn
		sprintf_s(sendBuf, "Welcome client %d to join current %d clients chatroom", a.sockConn, num);
		node* p = connlist->first;
		if (num > 0) {
			if (num == 1) {
				if (send((p->cc).sockConn, sendBuf, 255, 0) == SOCKET_ERROR) {
					cout << "send to client error!report error:" << WSAGetLastError() << endl;
				}
				else {
					cout << "Broadcasting welcome message to client" << sendBuf << endl;
				}
			}
			else {
				while (p->next != connlist->first) {
					if (send((p->cc).sockConn, sendBuf, 255, 0) == SOCKET_ERROR) {
						cout << "send to client error!report error:" << WSAGetLastError() << endl;
					}
					else {
						cout << "Broadcasting welcome message to client" << sendBuf << endl;
					}
					p = p->next;
					if (p->next == connlist->first) {
						if (send((p->cc).sockConn, sendBuf, 255, 0) == SOCKET_ERROR) {
							cout << "send to client error!report error:" << WSAGetLastError() << endl;
						}
						else {
							cout << "Broadcasting welcome message to client" << sendBuf << endl;
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
