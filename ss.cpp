#include<iostream>
#include<winsock.h>
#include<string>
#include<thread>
#include<map>
#include<set>
#include<sstream>

#pragma comment(lib,"ws2_32.lib")

char recvBuf[255] = { "\0" };
char sendBuf[255] = { "\0" };
using namespace std;

typedef struct conn {
	SOCKADDR_IN addrClient;
	SOCKET sockConn;
	int len = sizeof(addrClient);
	string name;
	string currentRoom;
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

static int num = 0;

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
};

class PrivateRoom {
public:
	string roomId;
	set<SOCKET> members;
	PrivateRoom() {}
	PrivateRoom(string id) : roomId(id) {}
};

map<string, PrivateRoom> privateRooms;
map<SOCKET, string> socketToName;

void flush(char* a) {
	memset(a, 0, sizeof(a));
}

string generateRoomId() {
	srand(time(0));
	string id;
	do {
		id = "";
		for (int i = 0; i < 4; i++) {
			id += (char)('0' + rand() % 10);
		}
	} while (privateRooms.find(id) != privateRooms.end());
	return id;
}

void sendToClient(SOCKET sock, const string& msg) {
	send(sock, msg.c_str(), 255, 0);
}

void broadcastToRoom(list* cl, const string& roomId, const string& msg, SOCKET excludeSock = INVALID_SOCKET) {
	if (roomId == "public") {
		if (cl->isempty()) return;
		node* p = cl->first;
		do {
			if (p->cc.currentRoom == "public" && p->cc.sockConn != excludeSock) {
				sendToClient(p->cc.sockConn, msg);
			}
			p = p->next;
		} while (p != cl->first);
	}
	else {
		if (privateRooms.find(roomId) == privateRooms.end()) return;
		PrivateRoom& room = privateRooms[roomId];
		for (SOCKET sock : room.members) {
			if (sock != excludeSock) {
				sendToClient(sock, msg);
			}
		}
	}
}

void handleCommand(list* cl, node* sender, const string& cmd) {
	stringstream ss(cmd);
	string command;
	ss >> command;
	
	if (command == "/create") {
		string roomId = generateRoomId();
		PrivateRoom newRoom(roomId);
		newRoom.members.insert(sender->cc.sockConn);
		privateRooms[roomId] = newRoom;
		
		if (sender->cc.currentRoom != "public") {
			privateRooms[sender->cc.currentRoom].members.erase(sender->cc.sockConn);
			if (privateRooms[sender->cc.currentRoom].members.empty()) {
				privateRooms.erase(sender->cc.currentRoom);
			}
		}
		sender->cc.currentRoom = roomId;
		
		string response = "[系统] 私有聊天室创建成功！房间号：" + roomId;
		sendToClient(sender->cc.sockConn, response);
		cout << "用户 " << sender->cc.name << " 创建了私有聊天室 " << roomId << endl;
	}
	else if (command == "/join") {
		string roomId;
		ss >> roomId;
		
		if (roomId.length() != 4) {
			sendToClient(sender->cc.sockConn, "[系统] 房间号必须是4位数字！");
			return;
		}
		
		if (privateRooms.find(roomId) == privateRooms.end()) {
			sendToClient(sender->cc.sockConn, "[系统] 房间不存在！");
			return;
		}
		
		if (sender->cc.currentRoom == roomId) {
			sendToClient(sender->cc.sockConn, "[系统] 您已经在这个房间了！");
			return;
		}
		
		if (sender->cc.currentRoom != "public") {
			string oldRoom = sender->cc.currentRoom;
			privateRooms[oldRoom].members.erase(sender->cc.sockConn);
			broadcastToRoom(cl, oldRoom, "[系统] " + sender->cc.name + " 离开了房间");
			if (privateRooms[oldRoom].members.empty()) {
				privateRooms.erase(oldRoom);
			}
		}
		else {
			broadcastToRoom(cl, "public", "[系统] " + sender->cc.name + " 离开了公共聊天室");
		}
		
		sender->cc.currentRoom = roomId;
		privateRooms[roomId].members.insert(sender->cc.sockConn);
		
		broadcastToRoom(cl, roomId, "[系统] " + sender->cc.name + " 加入了房间");
		sendToClient(sender->cc.sockConn, "[系统] 成功加入房间 " + roomId);
		cout << "用户 " << sender->cc.name << " 加入了私有聊天室 " << roomId << endl;
	}
	else if (command == "/leave") {
		if (sender->cc.currentRoom == "public") {
			sendToClient(sender->cc.sockConn, "[系统] 您当前在公共聊天室，无法离开！");
			return;
		}
		
		string oldRoom = sender->cc.currentRoom;
		privateRooms[oldRoom].members.erase(sender->cc.sockConn);
		broadcastToRoom(cl, oldRoom, "[系统] " + sender->cc.name + " 离开了房间");
		
		if (privateRooms[oldRoom].members.empty()) {
			privateRooms.erase(oldRoom);
			cout << "私有聊天室 " << oldRoom << " 已解散" << endl;
		}
		
		sender->cc.currentRoom = "public";
		broadcastToRoom(cl, "public", "[系统] " + sender->cc.name + " 回到了公共聊天室");
		sendToClient(sender->cc.sockConn, "[系统] 您已回到公共聊天室");
		cout << "用户 " << sender->cc.name << " 回到了公共聊天室" << endl;
	}
	else if (command == "/list") {
		string response = "[系统] 当前私有聊天室列表：\n";
		if (privateRooms.empty()) {
			response += "  暂无私有聊天室\n";
		}
		else {
			for (auto& pair : privateRooms) {
				response += "  房间号: " + pair.first + " (人数: " + to_string(pair.second.members.size()) + ")\n";
			}
		}
		sendToClient(sender->cc.sockConn, response);
	}
	else if (command == "/help") {
		string help = "[系统] 可用命令：\n";
		help += "  /create - 创建私有聊天室\n";
		help += "  /join <房间号> - 加入私有聊天室\n";
		help += "  /leave - 离开私有聊天室，回到公共聊天室\n";
		help += "  /list - 查看所有私有聊天室\n";
		help += "  /help - 显示帮助信息\n";
		help += "  /menu - 显示菜单\n";
		sendToClient(sender->cc.sockConn, help);
	}
	else if (command == "/menu") {
		string menu = "\n========== 聊天室菜单 ==========\n";
		menu += "当前位置: " + (sender->cc.currentRoom == "public" ? "公共聊天室" : "私有聊天室 " + sender->cc.currentRoom) + "\n";
		menu += "1. 创建私有聊天室 (/create)\n";
		menu += "2. 加入私有聊天室 (/join <房间号>)\n";
		menu += "3. 离开私有聊天室 (/leave)\n";
		menu += "4. 查看所有私有聊天室 (/list)\n";
		menu += "5. 显示帮助信息 (/help)\n";
		menu += "6. 退出聊天室 (q)\n";
		menu += "================================\n";
		sendToClient(sender->cc.sockConn, menu);
	}
	else {
		sendToClient(sender->cc.sockConn, "[系统] 未知命令，输入 /help 查看帮助");
	}
}

void hthreadfun(list*cl,node* newnode) {
	while (1) {
		flush(recvBuf);
		if (recv((newnode->cc).sockConn, recvBuf, 255, 0) == SOCKET_ERROR) {
			if (WSAGetLastError() == 10054) {
				string name = newnode->cc.name.empty() ? to_string((newnode->cc).sockConn) : newnode->cc.name;
				sprintf_s(recvBuf, "%s 离开了聊天室", name.c_str());
				num--;
				cout << "当前人数 " << num << endl;
				
				if (newnode->cc.currentRoom != "public") {
					string oldRoom = newnode->cc.currentRoom;
					if (privateRooms.find(oldRoom) != privateRooms.end()) {
						privateRooms[oldRoom].members.erase(newnode->cc.sockConn);
						broadcastToRoom(cl, oldRoom, "[系统] " + name + " 离开了房间");
						if (privateRooms[oldRoom].members.empty()) {
							privateRooms.erase(oldRoom);
						}
					}
				}
				else {
					broadcastToRoom(cl, "public", string(recvBuf));
				}
				
				cl->del(newnode);
				closesocket((newnode->cc).sockConn);
			}
			else cout << "recv error:" << WSAGetLastError() << endl;
			return;
		}
		else if (strlen(recvBuf) != 0) {
			string msg = recvBuf;
			cout << "服务器收到" << (newnode->cc).sockConn << "的消息:" << recvBuf << endl;
			
			if (newnode->cc.name.empty()) {
				newnode->cc.name = msg;
				newnode->cc.currentRoom = "public";
				socketToName[newnode->cc.sockConn] = msg;
				
				string welcome = "[系统] 欢迎 " + msg + " 加入公共聊天室！\n";
				welcome += "[系统] 输入 /menu 查看菜单，/help 查看帮助\n";
				sendToClient(newnode->cc.sockConn, welcome);
				
				broadcastToRoom(cl, "public", "[系统] " + msg + " 加入了公共聊天室", newnode->cc.sockConn);
			}
			else if (msg[0] == '/') {
				handleCommand(cl, newnode, msg);
			}
			else {
				string fullMsg = "[" + (newnode->cc.currentRoom == "public" ? "公共" : newnode->cc.currentRoom) + "] " + newnode->cc.name + " 说: " + msg;
				broadcastToRoom(cl, newnode->cc.currentRoom, fullMsg, newnode->cc.sockConn);
			}
		}
		flush(recvBuf);
	}
}

int main() {
	WSACleanup();
	int port;
	cout << "请输入服务器端口号：";
	cin >> port;
	WORD wVersionRequested = MAKEWORD(2, 2);
	WSADATA wsaData;

	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		cout << "WSAStartup() failed" << endl;
		return 0;
	}

	SOCKET sockSrv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockSrv == INVALID_SOCKET) {
		cout << " socket error!report error:" << WSAGetLastError() << endl;
	}

	SOCKADDR_IN addrSrv;
	memset(&addrSrv, 0, sizeof(addrSrv));
	addrSrv.sin_family = AF_INET;
	addrSrv.sin_addr.S_un.S_addr = inet_addr("0.0.0.0");
	addrSrv.sin_port = htons(port);

	if (bind(sockSrv, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		cout << "bind error,report error:" << WSAGetLastError() << endl;
	}
	else { cout << "bind succeed!!" << endl; }

	if (listen(sockSrv, 5) == SOCKET_ERROR) {
		cout << "listen error!report error:" << WSAGetLastError() << endl;
		return 1;
	}
	else { cout << "listening....." << endl; }

	list *connlist=new list();
	while (1) {
		CON a;
		a.sockConn  = accept(sockSrv, (SOCKADDR*)&a.addrClient, &a.len);
		if (a.sockConn == INVALID_SOCKET) {
			cout << "accept error! report error:" << WSAGetLastError() << endl;
			continue;
		}
		cout << "reveived a conn::"<<"   ip:" << inet_ntoa(a.addrClient.sin_addr) << "   port:"<<a.addrClient.sin_port << endl;
		node* newnode=new node(a);
		connlist->add(newnode);
		num++;
		
		sprintf_s(sendBuf, "欢迎 %d 加入，当前%d人在线", a.sockConn, num);
		node* p = connlist->first;
		if (num > 0) {
			if (num == 1) {
				if (send((p->cc).sockConn, sendBuf, 255, 0) == SOCKET_ERROR) {
					cout << "send to client error!report error:" << WSAGetLastError() << endl;
				}
				else {
					cout << "服务器发送欢迎消息" << sendBuf << endl;
				}
			}
			else {
				while (p->next != connlist->first) {
					if (send((p->cc).sockConn, sendBuf, 255, 0) == SOCKET_ERROR) {
						cout << "send to client error!report error:" << WSAGetLastError() << endl;
					}
					else {
						cout << "服务器发送欢迎消息" << sendBuf << endl;
					}
					p = p->next;
					if (p->next == connlist->first) {
						if (send((p->cc).sockConn, sendBuf, 255, 0) == SOCKET_ERROR) {
							cout << "send to client error!report error:" << WSAGetLastError() << endl;
						}
						else {
							cout << "服务器发送欢迎消息" << sendBuf << endl;
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
