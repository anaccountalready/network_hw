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
	client(char* name) {
		WORD wVersionRequested = MAKEWORD(2, 2);
		WSADATA wsaData;

		WSAStartup(wVersionRequested, &wsaData);//初始化Socket DLL，协商使用的Socket版本

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
				cout << name << "recv error:" << WSAGetLastError() << endl;
				return;
			}
			else if (strlen(recvBuf) != 0) {
				//接收到的数据
				cout << "《" << recvBuf << "》" << endl;
				flush(recvBuf);
			}
			flush(recvBuf);

		}
	}
	void sendData() {
		int ret = 0;
		do {

			cout << "请输入发送消息：" << endl;
			cin.getline(sendBuf, 255);
			cin.clear();
			cin.sync();
			char a[255];
			if (strcmp(sendBuf, "q") == 0) {
				return;
			}
			else if (strcmp(sendBuf, "") == 0) {
				cout << "不能发送空字符" << endl;
				continue;
			}
			//strcpy_s(a,(const char*)name);
			sprintf_s(a, "%s说：%s", name, sendBuf);
			ret = send(c, a, 255, 0);

		}//向远程socket发送数据
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
	cout << "请输入昵称" << endl;
	cin >> name;
	cin.ignore(1024, '\n');
	cout << "请输入连接的服务器ip地址：" << endl;
	cin >> ipaddr;
	
	cin.ignore(1024, '\n');
	cout << "请输入服务器端口号:" << endl;
	cin >> port;
	cin.ignore(1024, '\n');
	cout << "要连接的服务器ip：" << " " << ipaddr<<"    端口号："<<port<<endl;
	client c1(name);

	SOCKADDR_IN addrClient;
	memset(&addrClient, 0, sizeof(addrClient));//用0填充
	addrClient.sin_family = AF_INET;//IPv4
	addrClient.sin_addr.S_un.S_addr = inet_addr(ipaddr);//具体IP地址
	addrClient.sin_port = htons(port);//端口号
	if (connect(c1.c, (SOCKADDR*)&addrClient, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		cout << "client conn error,report error:" << WSAGetLastError() << endl;
		cout << "与目标服务器连接失败，将退出";
	}//向一个特定的Socket发出建连请求（，包括IP和Port，）
	else {
		SOCKADDR_IN myaddr;
		int len = sizeof(myaddr);
		getsockname(c1.c,(sockaddr*)&myaddr,&len);
		cout<<"本地ip："<<inet_ntoa(myaddr.sin_addr)<<"   本地port:"<<ntohs(myaddr.sin_port)<<endl << "connect sever succeed！！input q to exit" << endl;
		thread h1(&client::recvData, c1);
		h1.detach();
		c1.sendData();
		closesocket(c1.c);//关闭一个存在的socket
		WSACleanup();
	}
}