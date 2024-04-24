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
//���ճ�ʱ����
int timeOut=5;
static PDU sendpdu, recvpdu;
int next_want_seq = 0;//��ʼϣ�����ܵ�seq=0
int sendret = 0;
int recvret = 0;
char ip[30] = IP;
char sip[20] = Srv_ip;
int port = PORT;
int sport = Srv_port;
int sendbefore() {
	return sendto(Socket, (char*)&sendpdu, sizeof(PDU), 0, (sockaddr*)&RemoteAddr, RemoteAddr_len);
}
int recvbefore() {
	return recvfrom(Socket, (char*)&recvpdu, sizeof(PDU), 0, (sockaddr*)&RemoteAddr, &RemoteAddr_len);
}
int send() {
	return send(Socket, (char*)&sendpdu, sizeof(PDU), 0);
}
int recv() {
	return recv(Socket, (char*)&recvpdu, sizeof(PDU), 0);
}
bool socket_init() {
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cout << "WSAStartup error:" << GetLastError() << endl;
	}
	Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);//udpЭ��
	if (Socket == INVALID_SOCKET) {
		closesocket(Socket);
		return false;
	}
	bindAddr.sin_family = AF_INET;
	bindAddr.sin_port = htons(sport);
	inet_pton(AF_INET, sip, &bindAddr.sin_addr.S_un.S_addr);

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
		cout << "======================recv=======================" << endl;
		pdu_to_str(recvpdu);

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
				//ʱ��̫�� ��
				return 0;
			}
		}
		else {
			cout << "other recv error" << GetLastError() << endl;
			return 0;//�쳣
		}
	}
	return 1;//����
}
int timeout_retry_before() {

	while (1) {
		while (sendret < 0) {
			cout << "send error" << GetLastError() << endl;
			sendret = sendbefore();
			cout << "======================resend==========================" << endl;
			pdu_to_str(sendpdu);

		}
		recvret = recvbefore();
		if (recvret > 0) {
			cout << "======================recv==========================="  << endl;
			pdu_to_str(recvpdu);
			lessouttime();
			cout << "success recv ,so less timeout" << timeOut << endl; break;

		}
		else if (recvret == -1 && GetLastError() == 10060) {
			sendret = sendbefore();
			cout << "======================resend=======================" << endl;
			pdu_to_str(sendpdu);
			updateouttime();
			cout << "timeout happen,so more timeout" << timeOut << endl;
			if (timeOut > 20000) {
				//ʱ��̫����,�������ȴ���
				return 0;
			}
		}
		else {
			cout << "other error" << GetLastError() << " " << recvret << endl;
			return 0;
		}
	}
	return 1;
}

bool establish_conn() {

	while (1) {
		recvret = recvbefore();
		if (recvbefore() > 0)break;
		else cout << "waiting for client request......." << " "<<recvret<<endl;
	}
	sendpdu.dest_port = ntohs(RemoteAddr.sin_port);
	cout << "======================recv=======================" << endl;
	pdu_to_str(recvpdu);

	if (recvpdu.flags == SYN) {
		cout << "FIRST SHACK HANDS SUCCEED!!" << endl;
		sendpdu.flags = ACK_SYN;
		sendpdu.seq = 0;
		sendpdu.ack = recvpdu.seq + 1;
		sendret = sendbefore();
		//�ش�
		timeout_retry_before();


		//����������
		while (!(recvpdu.ack == sendpdu.seq + 1 && recvpdu.flags == ACK)) {
			sendret = sendbefore();
			if (timeout_retry_before() == 0) {
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
	
	//����ǰ�յ��˶Զ˵�ACK_FIN
	sendpdu.flags = ACK;
	sendpdu.ack = recvpdu.seq + 1;
	cout << "SECOND BYE,go to state CLOSE_WAIT" << endl;
	sendret = send();
	//�Ѿ��������,����ACK+FIN
	cout << "======================send=========================" << endl;
	pdu_to_str(sendpdu);
	sendpdu.flags = ACK_FIN;
	sendpdu.seq = 0;
	sendret = send();
	cout << "======================send=========================" << endl;
	pdu_to_str(sendpdu);
	cout << "THIRD BYE,go to state LAST_ACK" << endl;
	//��ʱ�ش�
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
string destfilename = "2.jpg";
int main() {
	cout << "Please input the bind port��" << endl;
	cin >> sport;
	cin.ignore(1024, '\n');
	

	if (!socket_init()) { cout << "init failed!!" << endl; return 1; }
	if (!establish_conn()) { cout << "connect failed!!" << endl; return 1; }
	if (connect(Socket, (sockaddr*)&RemoteAddr, RemoteAddr_len) == -1) { cout << "connect failed!!" << endl; }
	
	else cout << "conneted!!!!!!!!!!!!" << endl;
	
	cout << "please input the recv file name:" << endl;
	cin >> destfilename;

	//ofstream logout("recver_log.txt");
	//streambuf* p0ld = cout.rdbuf(logout.rdbuf());

	ofstream outf(destfilename.c_str(), ios::out | ios::binary);//������ļ�
	while (1) {
		if (recvret=recv()> 0) {
			cout << "=========recv=============================="  << endl;
			pdu_to_str(recvpdu);
			//ȷ�Ϻ󽫽��յ�������д�뱾���ļ�
			if (checkchecksum(recvpdu) == 0) { cout << "check faild" << endl; continue; }
			if (recvpdu.seq == next_want_seq)
			{
				outf.write(recvpdu.buf, recvpdu.length);
			}
			else if (recvpdu.seq == next_want_seq - recvpdu.length - 1) {
				//��ACKȷ�ϰ���ʧ���µ��ش�
				cout << "ACK packet loss or delay: will resend ACK packet" << endl;
				sendret = send();//����ACK��
				cout << "======================resend======================" << endl;
				pdu_to_str(sendpdu);
				continue;
			}
			else {
				cout << "recv seq:" << recvpdu.seq << "  but EXPECT recvseq��" << next_want_seq << endl;
				continue;
			}
			//�������

			sendpdu.ack = recvpdu.seq + recvpdu.length + 1;//���÷���ack.��һ��ϣ�����ܵ�seq
			next_want_seq = sendpdu.ack;//������һ��������recv��seq
			sendpdu.flags = ACK;//����ACK
			sendret = send();//����ACK��
			cout << "======================send========================="<< endl;
			pdu_to_str(sendpdu);
			if (recvpdu.B_E == 1) { cout << "RECV finished!!!" << endl; recvpdu.length = 0; break; }
			

		}
	}
	
	outf.close();

	while(1){
		
		if (recv()>0&&recvpdu.flags == ACK_FIN) {
			if (close_conn()) { break; }
			else return 1;
		}
	}
	
}