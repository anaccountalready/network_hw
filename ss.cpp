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

};
void flush(char* a) {
	memset(a, 0, sizeof(a));
}
//�̴߳�����
void hthreadfun(list*cl,node* newnode) {
	//���ղ��������Ը��̼߳�������Ϣ
	while (1) {
		flush(recvBuf);
		if (recv((newnode->cc).sockConn, recvBuf, 255, 0) == SOCKET_ERROR) {
			if (WSAGetLastError() == 10054) {
				sprintf_s(recvBuf, "%d���˳�������", (newnode->cc).sockConn);
				num--;
				cout << "��ǰ����" << num << endl;
				cl->del(newnode);
				closesocket((newnode->cc).sockConn);//�ر�socket
				if (num != 0) {
					node* p = cl->first;
					if (num == 1) {
						send((p->cc).sockConn, recvBuf, 255, 0);
						cout << "��������" << (p->cc).sockConn << "ת����Ϣ:" << recvBuf << endl;
					}
					else
					while (p->next!=cl->first) {
							send((p->cc).sockConn, recvBuf, 255, 0);
							cout << "��������" << (p->cc).sockConn << "ת����Ϣ:" << recvBuf << endl;
							p = p->next;
							if (p->next == cl->first) {
								send((p->cc).sockConn, recvBuf, 255, 0);
								cout << "��������" << (p->cc).sockConn << "ת����Ϣ:" << recvBuf << endl;
							}
						}

				}

			
			}
			else cout << "recv error:" << WSAGetLastError() << endl;
			return;
		}
		else if (strlen(recvBuf) != 0) {
			cout << "���������յ�" <<(newnode->cc).sockConn << "����Ϣ:" << recvBuf << endl;
			if (num > 1) {
				node* p = cl->first;
				while (p->next != cl->first) {
					if (p != newnode) {
						send((p->cc).sockConn, recvBuf, 255, 0);
						cout << "��������" << (p->cc).sockConn << "ת����Ϣ:" << recvBuf << endl;
						p = p->next;
					}
					else if (p == newnode) {
						p = p->next;
					}
					if (p->next == cl->first&&p!=newnode)
					{
						send((p->cc).sockConn, recvBuf, 255, 0);
						cout << "��������" << (p->cc).sockConn << "ת����Ϣ:" << recvBuf << endl;
					}
					
				}
			}

		}
		else { }

		flush(recvBuf);

	}
}
int main() {
	WSACleanup();
	int port;
	cout << "������������˿ںţ�";
	cin >> port;
	WORD wVersionRequested = MAKEWORD(2, 2);
	WSADATA wsaData;

	if (WSAStartup(wVersionRequested, &wsaData) != 0) {

		cout << "WSAStartup() failed" << endl;
		return 0;
	}//��ʼ��Socket DLL��Э��ʹ�õ�Socket�汾

	SOCKET sockSrv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);//����һ��Socket�����󶨵�һ���ض��Ĵ�������Э��ΪTCP/IP
	if (sockSrv == INVALID_SOCKET) {
		cout << " socket error!report error:" << WSAGetLastError() << endl;
	}

	//���ü�����ip��ַ�Ͷ˿�
	SOCKADDR_IN addrSrv;
	memset(&addrSrv, 0, sizeof(addrSrv));
	addrSrv.sin_family = AF_INET;//IPv4
	addrSrv.sin_addr.S_un.S_addr = inet_addr("0.0.0.0");//IP��ַ
	addrSrv.sin_port = htons(port);//�˿ں�


	if (bind(sockSrv, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		cout << "bind error,report error:" << WSAGetLastError() << endl;

	}//����ַ�󶨵�ָ��Socket
	else { cout << "bind succeed!!" << endl; }


	if (listen(sockSrv, 5) == SOCKET_ERROR) {
		cout << "listen error!report error:" << WSAGetLastError() << endl;
		return 1;

	}//ʹSocket�������״̬������Զ�������Ƿ���
	else { cout << "listening....." << endl; }

	list *connlist=new list();
	while (1) {
		CON a;
		a.sockConn  = accept(sockSrv, (SOCKADDR*)&a.addrClient, &a.len);//�����ض�socket����ȴ������е���������//ͨ�����к�������ֱ������������
		if (a.sockConn == INVALID_SOCKET) {
			cout << "accept error! report error:" << WSAGetLastError() << endl;
			continue;
		}
		cout << "reveived a conn::"<<"   ip:" << inet_ntoa(a.addrClient.sin_addr) << "   port:"<<a.addrClient.sin_port << endl;
		node* newnode=new node(a);
		connlist->add(newnode);
		 num++;
		//���ճɹ��õ�ͨѶ��sockConn
		sprintf_s(sendBuf, "��ӭ %d ���뵱ǰ%d��������", a.sockConn, num);
		node* p = connlist->first;
		if (num > 0) {
			if (num == 1) {
				if (send((p->cc).sockConn, sendBuf, 255, 0) == SOCKET_ERROR) {
					cout << "send to client error!report error:" << WSAGetLastError() << endl;
				}
				else {
					cout << "���������ͻ�ӭ��Ϣ" << sendBuf << endl;
				}
			}
			else {
				while (p->next != connlist->first) {
					if (send((p->cc).sockConn, sendBuf, 255, 0) == SOCKET_ERROR) {
						cout << "send to client error!report error:" << WSAGetLastError() << endl;
					}
					else {
						cout << "���������ͻ�ӭ��Ϣ" << sendBuf << endl;
					}
					p = p->next;
					if (p->next == connlist->first) {
						if (send((p->cc).sockConn, sendBuf, 255, 0) == SOCKET_ERROR) {
							cout << "send to client error!report error:" << WSAGetLastError() << endl;
						}
						else {
							cout << "���������ͻ�ӭ��Ϣ" << sendBuf << endl;
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
