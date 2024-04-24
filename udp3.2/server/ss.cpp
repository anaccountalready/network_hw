#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <windows.h>
#include "udp.h"
using namespace std;
#pragma comment(lib,"ws2_32.lib")
#define Srv_ip "127.0.0.1"
#define Srv_port 8888
#define PORT 1000
#define IP "127.0.0.1"

SOCKET Socket;
SOCKADDR_IN bindAddr;
SOCKADDR_IN RemoteAddr;
int RemoteAddr_len = sizeof(RemoteAddr);
WSADATA wsaData;
//接收超时设置
int timeOut = 5;
static PDU sendpdu, recvpdu;
int next_want_seq = 0;//开始希望接受的seq=0
int sendret = 0;
int recvret = 0;
char ip[30] = IP;
char sip[20] = Srv_ip;
int port = PORT;
int sport = Srv_port;
int wait = 3;
int send() {
	return sendto(Socket, (char*)&sendpdu, sizeof(PDU), 0, (sockaddr*)&RemoteAddr, RemoteAddr_len);
}
int recv() {
	return recvfrom(Socket, (char*)&recvpdu, sizeof(PDU), 0, (sockaddr*)&RemoteAddr, &RemoteAddr_len);
}
bool socket_init() {
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cout << "WSAStartup error:" << GetLastError() << endl;
	}
	Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);//udp协议
	if (Socket == INVALID_SOCKET) {
		closesocket(Socket);
		return false;
	}
	bindAddr.sin_family = AF_INET;
	bindAddr.sin_port = htons(sport);
	inet_pton(AF_INET, sip, &bindAddr.sin_addr.S_un.S_addr);
	sendpdu.sourceip = bindAddr.sin_addr.S_un.S_addr;

	auto ret = bind(Socket, (sockaddr*)&bindAddr, sizeof(SOCKADDR));
	if (ret == SOCKET_ERROR) {
		closesocket(Socket);
		Socket = INVALID_SOCKET;
		cout << "bind failed!!";
		return false;
	}
	cout << "bind successed!!" << endl;

	RemoteAddr.sin_family = AF_INET;
	RemoteAddr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &RemoteAddr.sin_addr);
	sendpdu.destip = RemoteAddr.sin_addr.S_un.S_addr;
	SOCKADDR_IN local_addr;
	getsockname(Socket, (sockaddr*)&local_addr, &RemoteAddr_len);
	sendpdu.source_port = ntohs(local_addr.sin_port);
	sendpdu.dest_port = port;

	if (setsockopt(Socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeOut, sizeof(timeOut)) < 0) {
		cout << "timeout setting failed!!" << endl;
	}
	else cout << "set_time_succeed!!" << endl;
	return true;
}
void updateouttime() {
	timeOut *= 2;
	if (setsockopt(Socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeOut, sizeof(timeOut)) < 0) {
		cout << "timeout setting failed!!" << endl;
	}
	else cout << "set_time_succeed!!" << endl;
}
void lessouttime() {
	if (timeOut < 1024) { return; }
	timeOut /= 2;
	if (setsockopt(Socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeOut, sizeof(timeOut)) < 0) {
		cout << "timeout setting failed!!" << endl;
	}
	else cout << "set_time_succeed!!" << endl;
}
int timeout_retry() {

	while (1) {
		while (sendret < 0) {
			cout << "send error" << GetLastError() << endl;
			sendret = send();
			cout << "======================resend=======================" << endl;
			pdu_to_str(sendpdu);
		}
		recvret = recv();
		if (recvret > 0) {
			cout << "=========recv================================" << endl;
			pdu_to_str(recvpdu);
			lessouttime();
			cout << "success recv ,so less timeout" << timeOut << endl;
			break;
		}
		else if (recvret == -1 && GetLastError() == 10060) {
			sendret = send();
			cout << "======================resend=======================" << endl;
			pdu_to_str(sendpdu);
			updateouttime();
			cout << "timeout happen,so more timeout" << timeOut << endl;
			if (timeOut > 20000) {
				//时间太长 了
				return 0;
			}
		}
		else {
			cout << "other recv error" << GetLastError() << endl;
			return 0;//异常
		}
	}
	return 1;//正常
}
bool establish_conn() {

	while (1) {
		recvret = recv();
		if (recv() > 0)break;
		else cout << "waiting for client request......." << " " << recvret << endl;
	}
	sendpdu.dest_port = ntohs(RemoteAddr.sin_port);
	cout << "======================recv=======================" << endl;
	pdu_to_str(recvpdu);

	if (recvpdu.flags == SYN) {
		cout << "FIRST SHACK HANDS SUCCEED!!" << endl;
		sendpdu.flags = ACK_SYN;
		sendpdu.seq = 0;
		sendpdu.ack = recvpdu.seq + 1;
		sendret = send();
		//重传
		timeout_retry();


		//第三次握手
		while (!(recvpdu.ack == sendpdu.seq + 1 && recvpdu.flags == ACK)) {
			sendret = send();
			if (timeout_retry() == 0) {
				cout << "THRID SHACK HAND NOT ARRVE!!!no try anymore,establish conn" << endl;
				return true;
			}
		}
		{
			cout << "THRID SHACK HAND SUCCEED!!!" << endl;
			cout << "======================recv=======================" << endl;
			pdu_to_str(recvpdu);
			cout << "server_conn_established"; return true;
		}

		return false;
	}
	return false;

}
bool close_conn() {

	//函数前收到了对端的ACK_FIN
	sendpdu.flags = ACK;
	sendpdu.ack = recvpdu.seq + 1;
	cout << "SECOND BYE,go to state CLOSE_WAIT" << endl;
	sendret = send();
	//已经接收完毕,发送ACK+FIN
	cout << "======================send=========================" << endl;
	pdu_to_str(sendpdu);
	sendpdu.flags = ACK_FIN;
	sendpdu.seq = 0;
	sendret = send();
	cout << "======================send=========================" << endl;
	pdu_to_str(sendpdu);
	cout << "THIRD BYE,go to state LAST_ACK" << endl;
	//超时重传
	if (timeout_retry() == 0) {
		cout << "FOUR BYES NOT ARRIVED but not wait anymore,close connect!!" << endl;
		closesocket(Socket);
		return true;
	}

	if (check_ack_seq(sendpdu, recvpdu)) {
		cout << "FOUR BYES SUCCEED!!" << endl;
		closesocket(Socket);
		return true;
	}
	return false;

}
//文件名
string destfilename = "1.jpg";
int main() {
	cout << "Please input the router port：" << endl;
	cin >> port;
	cin.ignore(1024, '\n');
	cout << "Please input the bind port：" << endl;
	cin >> sport;
	cin.ignore(1024, '\n');

	if (!socket_init()) { cout << "init failed!!" << endl; return 1; }
	if (!establish_conn()) { cout << "connect failed!!" << endl; return 1; }
	

	cout << "conneted!!!!!!!!!!!!" << endl;
	cout << "please input the recv file name:" << endl;
	cin >> destfilename;

	//定向输出到文件
	//ofstream logout("recver_log.txt");
	//streambuf* p0ld = cout.rdbuf(logout.rdbuf());

	ofstream outf(destfilename.c_str(), ios::out | ios::binary);//输出的文件
	
	while (1) {
		recvret = recv();
		if (recvret == -1 && GetLastError() == 10060) {
			sendret = send();
			cout << "==================================================TIME_OUTresend===================================================" << endl;
			pdu_to_str(sendpdu);
			updateouttime();
			cout << "timeout happen,so more timeout" << timeOut << endl;
		}
		 if (recvret  > 0) {
			 cout << "=====================================================recv==================================================" << endl;
			 lessouttime();
			 cout << "success recv,so less timeout" << timeOut << endl;
			if (recvpdu.flags != ACK || recvpdu.seq == 1) {  continue; }//处理建立连接的多余数据包
			pdu_to_str(recvpdu);
			//校验失败，继续接收
			if (checkchecksum(recvpdu) == 0) {  cout << "check faild" << endl; continue; }
			//确认后将接收到的数据写入本地文件
			if (recvpdu.seq == next_want_seq)
			{
				wait--;//累计确认
				cout << "写入seq=>:" << next_want_seq<<endl;
				outf.write(recvpdu.buf, recvpdu.length);
					sendpdu.ack = recvpdu.seq + recvpdu.length + 1;//设置发送ack.下一个希望接受的seq
					next_want_seq = sendpdu.ack;//设置下一个期望的recv的seq
					sendpdu.flags = ACK;//设置ACK
					
					if (wait == 0) {//累计确认win个包后发送ACK包
						sendret = send();//发送ACK包
						cout << "======================================================send==================================================" << endl;
						pdu_to_str(sendpdu);
						wait = recvpdu.win;
					}
				
					//最后一个确认包发送
				else
				if (recvpdu.B_E == 1) { 
					cout << "RECV finished!!!" << endl; recvpdu.length = 0;
					sendret = send();//发送ACK包
					cout << "==================================================send_LAST_ACK=======================================================" << endl;
					pdu_to_str(sendpdu);
					break;
				}
				
				continue;
			}
			else if(recvpdu.seq > next_want_seq){
				//发生丢包
				cout << "recv seq=>" << recvpdu.seq << ";but EXPECT recvseq=>" << next_want_seq;
				//接收的序列号不正确就一直重发ACK_LOSS<期望得到的seq号>
					sendpdu.ack =next_want_seq;//设置发送ack.下一个希望接受的seq
					sendpdu.flags = ACK_LOSS;//设置ACK
					sendret = send();//发送ACK包
					cout << "====================================================send_ACK_LOSS=================================================" << endl;
					pdu_to_str(sendpdu);
				continue;
			}
			else {//这个是recvpdu.seq < next_want_seq的情况
				//是重复的数据包,可能是由于超时重传
				if (sendpdu.flags == ACK_LOSS) {
					cout << "duplicated PDU,discard" << endl;
					wait--;//缩小累计确认的个数
					continue;
				}
				//可能ACK确认包丢失
				sendret = send();//重发ACK包
				cout << "================================================resend_ACK=>May be ACK loss====================================" << endl;
				pdu_to_str(sendpdu);
				continue;
			}
			//接收完成


		}
	}

	outf.close();

	while (1) {

		if (recv() > 0 && recvpdu.flags == ACK_FIN) {
			cout << "======================recv_ACK_FIN=========================" << endl;
			pdu_to_str(recvpdu);
			if (close_conn()) { break; }
			else return 1;
		}
		else if (recv() > 0 && recvpdu.B_E == 1) {
			cout << "最后一个确认包丢失,将重新发送" << endl;
			sendret = send();//发送ACK包
			cout << "======================resend_ACK=========================" << endl;
			pdu_to_str(sendpdu);

		}
	}
	system("pause");
	return 0;
}