#include<iostream>
using namespace std;
#include<winsock.h>
#include<string>
#include<thread>
#pragma comment(lib,"ws2_32.lib")

class client {
public:
	char name[100];
	SOCKET c;
	char recvBuf[1000] = { '\0' };
	char sendBuf[1000] = { '\0' };
	bool nameSent;
	bool running;
	client(char* name) {
		WORD wVersionRequested = MAKEWORD(2, 2);
		WSADATA wsaData;

		WSAStartup(wVersionRequested, &wsaData);

		c = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		strcpy_s(this->name, name);
		nameSent = false;
		running = true;
	}
	client(SOCKET c) {
		this->c = c;
		nameSent = false;
		running = true;
	}
	void flush(char* a) {
		memset(a, 0, sizeof(a));
	}

	void recvData() {
		while (running) {
			flush(recvBuf);
			int ret = recv(c, recvBuf, 255, 0);
			if (ret == SOCKET_ERROR) {
				cout << name << " recv error:" << WSAGetLastError() << endl;
				running = false;
				return;
			}
			else if (ret == 0) {
				cout << "服务器已断开连接" << endl;
				running = false;
				return;
			}
			else if (strlen(recvBuf) != 0) {
				cout << recvBuf << endl;
				flush(recvBuf);
			}
			flush(recvBuf);
		}
	}
	
	void showMenu() {
		cout << "\n========== 聊天室菜单 ==========" << endl;
		cout << "1. 创建私有聊天室" << endl;
		cout << "2. 加入私有聊天室" << endl;
		cout << "3. 离开私有聊天室" << endl;
		cout << "4. 查看所有私有聊天室" << endl;
		cout << "5. 显示帮助信息" << endl;
		cout << "6. 发送消息" << endl;
		cout << "7. 退出聊天室" << endl;
		cout << "================================\n" << endl;
		cout << "请选择操作 (1-7): ";
	}
	
	int getMenuChoice() {
		int choice;
		while (true) {
			showMenu();
			if (cin >> choice) {
				cin.ignore(1024, '\n');
				if (choice >= 1 && choice <= 7) {
					return choice;
				}
				else {
					cout << "无效的选择，请输入1-7之间的数字" << endl;
				}
			}
			else {
				cout << "输入无效，请输入数字" << endl;
				cin.clear();
				cin.ignore(1024, '\n');
			}
		}
	}
	
	void handleMenuChoice(int choice) {
		int ret = 0;
		switch (choice) {
			case 1: {
				strcpy_s(sendBuf, "/create");
				ret = send(c, sendBuf, 255, 0);
				if (ret == SOCKET_ERROR || ret == 0) {
					cout << "发送命令失败" << endl;
					running = false;
				}
				break;
			}
			case 2: {
				char roomId[10];
				cout << "请输入4位房间号: ";
				cin >> roomId;
				cin.ignore(1024, '\n');
				sprintf_s(sendBuf, "/join %s", roomId);
				ret = send(c, sendBuf, 255, 0);
				if (ret == SOCKET_ERROR || ret == 0) {
					cout << "发送命令失败" << endl;
					running = false;
				}
				break;
			}
			case 3: {
				strcpy_s(sendBuf, "/leave");
				ret = send(c, sendBuf, 255, 0);
				if (ret == SOCKET_ERROR || ret == 0) {
					cout << "发送命令失败" << endl;
					running = false;
				}
				break;
			}
			case 4: {
				strcpy_s(sendBuf, "/list");
				ret = send(c, sendBuf, 255, 0);
				if (ret == SOCKET_ERROR || ret == 0) {
					cout << "发送命令失败" << endl;
					running = false;
				}
				break;
			}
			case 5: {
				strcpy_s(sendBuf, "/help");
				ret = send(c, sendBuf, 255, 0);
				if (ret == SOCKET_ERROR || ret == 0) {
					cout << "发送命令失败" << endl;
					running = false;
				}
				break;
			}
			case 6: {
				cout << "请输入消息内容: ";
				cin.getline(sendBuf, 255);
				cin.clear();
				cin.sync();
				if (strcmp(sendBuf, "") == 0) {
					cout << "不能发送空消息" << endl;
					return;
				}
				ret = send(c, sendBuf, 255, 0);
				if (ret == SOCKET_ERROR || ret == 0) {
					cout << "发送消息失败" << endl;
					running = false;
				}
				break;
			}
			case 7: {
				strcpy_s(sendBuf, "q");
				cout << "正在退出聊天室..." << endl;
				running = false;
				break;
			}
			default: {
				cout << "无效的选择，请重新输入" << endl;
				break;
			}
		}
	}

	void sendData() {
		int ret = 0;
		
		ret = send(c, name, 255, 0);
		if (ret == SOCKET_ERROR || ret == 0) {
			cout << "发送用户名失败" << endl;
			running = false;
			return;
		}
		nameSent = true;
		
		while (running) {
			int choice = getMenuChoice();
			handleMenuChoice(choice);
		}
		return;
	}
	~client() {}

};



int main() {
	WSACleanup();
	int port;
	char name[100];
	char ipaddr[30];
	cout << "请输入昵称" << endl;
	cin >> name;
	cin.ignore(1024, '\n');
	cout << "请输入要连接的服务器ip地址：" << endl;
	cin >> ipaddr;
	
	cin.ignore(1024, '\n');
	cout << "请输入服务器端口号:" << endl;
	cin >> port;
	cin.ignore(1024, '\n');
	cout << "要连接的服务器ip是" << " " << ipaddr<<"    端口号："<<port<<endl;
	client c1(name);

	SOCKADDR_IN addrClient;
	memset(&addrClient, 0, sizeof(addrClient));
	addrClient.sin_family = AF_INET;
	addrClient.sin_addr.S_un.S_addr = inet_addr(ipaddr);
	addrClient.sin_port = htons(port);
	if (connect(c1.c, (SOCKADDR*)&addrClient, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		cout << "client conn error,report error:" << WSAGetLastError() << endl;
		cout << "连接服务器失败，请退出";
	}
	else {
		SOCKADDR_IN myaddr;
		int len = sizeof(myaddr);
		getsockname(c1.c,(sockaddr*)&myaddr,&len);
		cout<<"本地ip："<<inet_ntoa(myaddr.sin_addr)<<"   本地port:"<<ntohs(myaddr.sin_port)<<endl << "connect sever succeed，输入 q to exit" << endl;
		thread h1(&client::recvData, &c1);
		h1.detach();
		c1.sendData();
		closesocket(c1.c);
		WSACleanup();
	}
}
