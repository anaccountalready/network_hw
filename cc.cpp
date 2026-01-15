#include<iostream>
#include<winsock.h>
#include<string>
#include<thread>
#include<mutex>
#include<cstdlib>
#include<cstring>

using namespace std;

#pragma comment(lib,"ws2_32.lib")

class client {
public:
	char name[100];
	SOCKET c;
	char recvBuf[1000] = { '\0' };
	char sendBuf[1000] = { '\0' };
	bool inMenu;
	string currentRoom;
	
	client(char* name) {
		WORD wVersionRequested = MAKEWORD(2, 2);
		WSADATA wsaData;

		WSAStartup(wVersionRequested, &wsaData);//Initialize Socket DLL protocol

		c = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		strcpy_s(this->name, name);
		inMenu = false;
		currentRoom = "public";

	}
	client(SOCKET c) {
		this->c = c;
		inMenu = false;
		currentRoom = "public";
	}
	void flush(char* a) {
		memset(a, 0, sizeof(a));
	}

	void recvData() {
		while (1) {
			if (strcmp(sendBuf, "q") == 0) {
				return;
			}
			flush(recvBuf);
			if (recv(c, recvBuf, 255, 0) == SOCKET_ERROR) {
				cout << name << " recv error:" << WSAGetLastError() << endl;
				return;
			}
			else if (strlen(recvBuf) != 0) {
				//Received message
				string message(recvBuf);
				
				// Check for special messages
				if (message.find("Private room created! Room code:") != string::npos) {
					cout << "🎉 " << message << endl;
					cout << "📋 Copy the room code above and share it with others!" << endl;
				} else if (message.find("Joined private room:") != string::npos) {
					cout << "✅ " << message << endl;
				} else if (message.find("Room not found:") != string::npos) {
					cout << "❌ " << message << endl;
				} else if (message.find("Enter choice (1-3):") != string::npos) {
					cout << "[" << message << "]" << endl;
					inMenu = true;
				} else {
					// Regular chat message
					cout << message << endl;
					// Show current room status if it's a user message
					if (message.find(" says: ") != string::npos && currentRoom != "public") {
						cout << "[Private Room: " << currentRoom << "]" << endl;
					}
				}
				
				flush(recvBuf);
			}
			flush(recvBuf);

		}
	}
	void sendData() {
		int ret = 0;
		
		// Send initial menu request
		send(c, "MENU", 255, 0);
		
		do {
			if (inMenu) {
				cout << "Enter your choice (1-3): ";
				cin.getline(sendBuf, 255);
				cin.clear();
				cin.sync();
				
				int choice = atoi(sendBuf);
				inMenu = false;
				
				switch (choice) {
					case 1:
						send(c, "BACK_PUBLIC", 255, 0);
						currentRoom = "public";
						cout << "Returned to public room" << endl;
						break;
					case 2:
					cout << "Enter 4-character room code to create/join (e.g., ABCD): ";
					cin.getline(sendBuf, 255);
					cin.clear();
					cin.sync();
					
					if (strlen(sendBuf) == 4) {
						string createCmd = "CREATE_ROOM:" + string(sendBuf);
						send(c, createCmd.c_str(), 255, 0);
						cout << "Creating/joining private room..." << endl;
					} else {
						cout << "Invalid room code format. Must be exactly 4 characters." << endl;
						inMenu = true; // Stay in menu
					}
					break;
					case 3:
						strcpy_s(sendBuf, "q");
						ret = send(c, sendBuf, 255, 0);
						return;
					default:
						cout << "Invalid choice. Please try again." << endl;
						send(c, "MENU", 255, 0);
						inMenu = true;
						break;
				}
			} else {
				cout << "Please enter message to send (or 'h' for menu, '/room' for room status):" << endl;
				cin.getline(sendBuf, 255);
				cin.clear();
				cin.sync();
				
				if (strcmp(sendBuf, "q") == 0) {
					ret = send(c, sendBuf, 255, 0);
					return;
				}
				else if (strcmp(sendBuf, "h") == 0) {
					send(c, "MENU", 255, 0);
					inMenu = true;
					continue;
				}
				else if (strcmp(sendBuf, "") == 0) {
					cout << "Cannot send empty message" << endl;
					continue;
				}
				
				// Check if it's a room join command
				if (strncmp(sendBuf, "/join ", 6) == 0) {
					string roomCode(sendBuf + 6);
					if (roomCode.length() == 4) {
						string joinCmd = "JOIN_ROOM:" + roomCode;
						send(c, joinCmd.c_str(), 255, 0);
						continue;
					} else {
						cout << "Invalid room code format. Room codes must be exactly 4 characters." << endl;
						continue;
					}
				}
				// Check if it's a room status command
				else if (strcmp(sendBuf, "/room") == 0) {
					cout << "Current room: " << (currentRoom == "public" ? "Public Room" : "Private Room " + currentRoom) << endl;
					continue;
				}
				
				char a[255];
				sprintf_s(a, "%s says: %s", name, sendBuf);
				ret = send(c, a, 255, 0);
			}

		}//Send to remote socket
		while (ret != SOCKET_ERROR && ret != 0);
		return;
	}
	~client() {}

};



int main() {
	WSACleanup();
	int port;
	char name[100];
	char ipaddr[30];
	cout << "Please enter your name: " << endl;
	cin >> name;
	cin.ignore(1024, '\n');
	cout << "Please enter server IP address: " << endl;
	cin >> ipaddr;
	
	cin.ignore(1024, '\n');
	cout << "Please enter server port number:" << endl;
	cin >> port;
	cin.ignore(1024, '\n');
	cout << "Connecting to server IP: " << " " << ipaddr<<"    Port:"<<port<<endl;
	client c1(name);

	SOCKADDR_IN addrClient;
	memset(&addrClient, 0, sizeof(addrClient));//��0���
	addrClient.sin_family = AF_INET;//IPv4
	addrClient.sin_addr.S_un.S_addr = inet_addr(ipaddr);//����IP��ַ
	addrClient.sin_port = htons(port);//�˿ں�
	if (connect(c1.c, (SOCKADDR*)&addrClient, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		cout << "client conn error,report error:" << WSAGetLastError() << endl;
		cout << "Connection to target server failed, exiting";
	}//Connect to a specific Socket object (server IP and Port)
	else {
		SOCKADDR_IN myaddr;
		int len = sizeof(myaddr);
		getsockname(c1.c,(sockaddr*)&myaddr,&len);
		cout<<"Local ip:"<<inet_ntoa(myaddr.sin_addr)<<"   Local port:"<<ntohs(myaddr.sin_port)<<endl << "Connected to server successfully, input q to exit" << endl;
		thread h1(&client::recvData, c1);
		h1.detach();
		c1.sendData();
		closesocket(c1.c);//�ر�һ�����ڵ�socket
		WSACleanup();
		cout << "Connection closed" << endl;
	}
}