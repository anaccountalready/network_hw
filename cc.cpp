#include<iostream>
using namespace std;
#include<winsock.h>
#include<string>
#include<thread>
#include<cctype>
#pragma comment(lib,"ws2_32.lib")

class client {
public:
	char name[100];
	SOCKET c;
	char recvBuf[1000] = { '\0' };
	char sendBuf[1000] = { '\0' };
	char currentRoom[7] = "public"; // "public" for public room, 4-digit string for private room
	bool inMenu = false;
	
	void showMenu() {
		cout << "\n=== Menu ===" << endl;
		cout << "1. Back into public room" << endl;
		cout << "2. Create private room" << endl;
		cout << "3. Exit" << endl;
		cout << "Please enter your choice: ";
	}
	client(char* name) {
		WORD wVersionRequested = MAKEWORD(2, 2);
		WSADATA wsaData;

		WSAStartup(wVersionRequested, &wsaData);//��ʼ��Socket DLL��Э��ʹ�õ�Socket�汾

		c = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		strcpy_s(this->name, name);

	}
	client(SOCKET c) {
		this->c = c;
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
			cout << recvBuf << endl;
				flush(recvBuf);
			}
			flush(recvBuf);

		}
	}
	void sendData() {
		int ret = 0;
		do {
			if (inMenu) {
				showMenu();
				inMenu = false;
			}
			else {
				cout << "Please enter the message to send (enter 'h' for menu):" << endl;
			}
			cin.getline(sendBuf, 255);
			cin.clear();
			cin.sync();
			char a[255];
			if (strcmp(sendBuf, "q") == 0) {
				return;
			}
			else if (strcmp(sendBuf, "h") == 0) {
				inMenu = true;
				continue;
			}
			else if (strcmp(sendBuf, "1") == 0) {
				// Back to public room
				strcpy_s(currentRoom, "public");
				cout << "You are now in the public room" << endl;
				sprintf_s(a, "/join public");
				ret = send(c, a, 255, 0);
				continue;
			}
			else if (strcmp(sendBuf, "2") == 0) {
				// Create private room
				cout << "Please enter a 4-digit room number: ";
				char roomNumber[5];
				cin.getline(roomNumber, 5);
				cin.clear();
				cin.sync();
				
				if (strlen(roomNumber) != 4) {
					cout << "Room number must be 4 digits" << endl;
					inMenu = true; // Show menu again
					continue;
				}
				
				// Check if all characters are digits
				bool isValid = true;
				for (int i = 0; i < 4; i++) {
					if (!isdigit(roomNumber[i])) {
						isValid = false;
						break;
					}
				}
				
				if (!isValid) {
					cout << "Room number must contain only digits" << endl;
					inMenu = true; // Show menu again
					continue;
				}
				
				// Send create room request to server
				sprintf_s(a, "/create %s", roomNumber);
				ret = send(c, a, 255, 0);
				strcpy_s(currentRoom, roomNumber);
				continue;
			}
			else if (strcmp(sendBuf, "3") == 0) {
				// Exit
				return;
			}
			else if (strcmp(sendBuf, "") == 0) {
				cout << "Cannot send empty message" << endl;
				continue;
			}
			//strcpy_s(a,(const char*)name);
			sprintf_s(a, "%s: %s", name, sendBuf);
			ret = send(c, a, 255, 0);

		}//Loop until socket communication ends
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
	cout << "Please enter your name" << endl;
	cin >> name;
	cin.ignore(1024, '\n');
	cout << "Please enter the server IP address:" << endl;
	cin >> ipaddr;
	
	cin.ignore(1024, '\n');
	cout << "Please enter the server port:" << endl;
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
		cout << "Client connection error, error code:" << WSAGetLastError() << endl;
		cout << "Failed to connect to the target server, exiting...";
	}
	else {
		SOCKADDR_IN myaddr;
		int len = sizeof(myaddr);
		getsockname(c1.c,(sockaddr*)&myaddr,&len);
		cout<<"My IP: "<<inet_ntoa(myaddr.sin_addr)<<"   My Port:"<<ntohs(myaddr.sin_port)<<endl << "Connect server succeed. Input q to exit" << endl;
		thread h1(&client::recvData, c1);
		h1.detach();
		c1.sendData();
		closesocket(c1.c);
		WSACleanup();
	}
}