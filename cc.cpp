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
				cout << name << "recv error:" << WSAGetLastError() << endl;
				return;
			}
			else if (strlen(recvBuf) != 0) {
				//���յ�������
				cout << "��" << recvBuf << "��" << endl;
				flush(recvBuf);
			}
			flush(recvBuf);

		}
	}
	void sendData() {
		int ret = 0;
		do {

			cout << "�����뷢����Ϣ��" << endl;
			cin.getline(sendBuf, 255);
			cin.clear();
			cin.sync();
			char a[255];
			if (strcmp(sendBuf, "q") == 0) {
				return;
			}
			else if (strcmp(sendBuf, "") == 0) {
				cout << "���ܷ��Ϳ��ַ�" << endl;
				continue;
			}
			//strcpy_s(a,(const char*)name);
			sprintf_s(a, "%s˵��%s", name, sendBuf);
			ret = send(c, a, 255, 0);

		}//��Զ��socket��������
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
	cout << "�������ǳ�" << endl;
	cin >> name;
	cin.ignore(1024, '\n');
	cout << "���������ӵķ�����ip��ַ��" << endl;
	cin >> ipaddr;
	
	cin.ignore(1024, '\n');
	cout << "������������˿ں�:" << endl;
	cin >> port;
	cin.ignore(1024, '\n');
	cout << "Ҫ���ӵķ�����ip��" << " " << ipaddr<<"    �˿ںţ�"<<port<<endl;
	client c1(name);

	SOCKADDR_IN addrClient;
	memset(&addrClient, 0, sizeof(addrClient));//��0���
	addrClient.sin_family = AF_INET;//IPv4
	addrClient.sin_addr.S_un.S_addr = inet_addr(ipaddr);//����IP��ַ
	addrClient.sin_port = htons(port);//�˿ں�
	if (connect(c1.c, (SOCKADDR*)&addrClient, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		cout << "client conn error,report error:" << WSAGetLastError() << endl;
		cout << "��Ŀ�����������ʧ�ܣ����˳�";
	}//��һ���ض���Socket�����������󣨣�����IP��Port����
	else {
		SOCKADDR_IN myaddr;
		int len = sizeof(myaddr);
		getsockname(c1.c,(sockaddr*)&myaddr,&len);
		cout<<"����ip��"<<inet_ntoa(myaddr.sin_addr)<<"   ����port:"<<ntohs(myaddr.sin_port)<<endl << "connect sever succeed����input q to exit" << endl;
		thread h1(&client::recvData, c1);
		h1.detach();
		c1.sendData();
		closesocket(c1.c);//�ر�һ�����ڵ�socket
		WSACleanup();
	}
}