#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <windows.h>
#include"udp.h"
using namespace std;
using namespace chrono;
#pragma comment(lib,"ws2_32.lib")
#define IP "127.0.0.1"
#define PORT 1000
#define CPORT 1234

char ip[30] = IP;
char cip[20] = IP;
int port = PORT;
int cport = CPORT;
SOCKET Socket;
SOCKADDR_IN RemoteAddr;
SOCKADDR_IN bindAddr;
int RemoteAddr_len = sizeof(RemoteAddr);
WSADATA wsaData;
int sendret = 0;
int recvret = 0;
//���ý��ճ�ʱ5ms
int timeOut = 5;
//�ͻ��˷���
PDU sendpdu, recvpdu;
size_t sendlength;
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
	bindAddr.sin_port = htons(cport);
	inet_pton(AF_INET, cip, &bindAddr.sin_addr.S_un.S_addr);

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
int send() {
	return send(Socket, (char*)&sendpdu, sizeof(PDU), 0);
}
int recv() {
	return recv(Socket, (char*)&recvpdu, sizeof(PDU), 0);
}
int sendbefore() {
	return sendto(Socket, (char*)&sendpdu, sizeof(PDU), 0, (sockaddr*)&RemoteAddr, RemoteAddr_len);
}
int recvbefore() {
	return recvfrom(Socket, (char*)&recvpdu, sizeof(PDU), 0, (sockaddr*)&RemoteAddr, &RemoteAddr_len);
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
			cout << "================recv============================:"  << endl;
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
				//ʱ��̫����,�������ȴ���
				return 0;
			}
		}
		else {
			cout << "other recv error" << GetLastError()  << endl;
			return 0;
		}
	}
	return 1;
}
int timeout_retry_before() {

	while (1) {
		while (sendret < 0) {
			cout << "send error" << GetLastError() << endl;
			sendret = sendbefore();
			cout << "======================resend=======================" << endl;
			pdu_to_str(sendpdu); 
			
		}
		recvret = recvbefore();
		if (recvret > 0) {
			cout << "=========recv=====from====================="  << endl;
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
	//��һ������
	sendpdu.flags = SYN;
	sendpdu.seq = 0;
	sendret = sendbefore();
	if (sendret < 0) {
		cout << "send error" << GetLastError() << endl;
		return false;
	}
	//�ڶ�������
	timeout_retry_before();

	cout << "======================send=======================" << endl;
	pdu_to_str(sendpdu);
	while (!((recvpdu.ack == sendpdu.seq + 1))) {
		sendret = sendbefore();
		timeout_retry_before();
	}
	{
		
		cout << "SECOND SHAKE HAND SUCCEED����" << endl;
		cout << "======================recv=======================" << endl;
		pdu_to_str(recvpdu);
	}

	//����������
	sendpdu.flags = ACK;
	sendpdu.seq++;
	sendpdu.ack = recvpdu.seq + 1;
	sendret = sendbefore();
	cout << "======================send=======================" << endl;
	pdu_to_str(sendpdu);
	if (sendret > 0) { 
		cout << "client_conn_established"; 
		return true; 
	}
	else cout << "conn_send_error" << endl;
	return false;
}
void close_conn() {
	sendpdu.flags = ACK_FIN;
	sendpdu.seq = 0;
	//��ʱ�ش�
	sendret = send();
	cout << "======================send=========================" << endl;
	pdu_to_str(sendpdu);
	cout << "FIRST BYE��go state: FIN_WAIT1" << endl;
	timeout_retry();

	cout << "go state FIN_WAIT2" << endl;

	
		while (!(recvpdu.ack == sendpdu.seq + 1 && recvpdu.flags == ACK_FIN)) {
			Sleep(10000);
			timeout_retry();
	}
		sendpdu.flags = ACK;
		sendpdu.ack = recvpdu.seq + 1;
		sendret = send();
		sendret = send();
		sendret = send();
		cout << "======================send=========================" << endl;
		pdu_to_str(sendpdu);
		cout << "THIRD BYE ,go state:TIME_WAIT" << endl;
		Sleep(10000);//�ȴ�10��
	     
		closesocket(Socket);


}
//�ļ�
string sourcefilename = "C:\\Users\\Ha ha\\vssource\\repos\\udp23.1\\1.jpg";

int main() {
	
	cout << "please input the bind port��" << endl;
	cin >> cport;
	cin.ignore(1024, '\n');


	clock_t start = clock();
	if (!socket_init()) { cout << "init failed!!" << endl; return 1; }
	//establish_conn
	if (!establish_conn()) { cout << "connect failed!!" << endl; return 1; }
	//Sleep(5000);
	if (connect(Socket, (sockaddr*)&RemoteAddr, RemoteAddr_len) == -1)cout << "connect failed!!" << endl;
	else cout << "conneted!!!!!!!!!!!!" << endl;
	
	
	cout << "please input the full file path:" << endl;
	getline(cin, sourcefilename);
	cout << "will open " << sourcefilename << endl;

	ifstream fin(sourcefilename.c_str(), ios::binary);//�����ļ�
	if (!fin) {
		cout << "open" << sourcefilename << "falied!!Please check the path" << endl;
		exit(1);
	}
	fin.seekg(0, ios::end);
	unsigned long long totalsize = fin.tellg();//�ļ�����
	fin.seekg(0, ios::beg);//�ļ���ʼ
	int st = 0;
	memset(recvpdu.buf, 0, nsize);
	
	//ofstream logout("sender_log.txt");
	//streambuf* p0ld = cout.rdbuf(logout.rdbuf());
	while (!fin.eof()) {
		//���Ŷ�ȡ�ļ�
		memset(sendpdu.buf, 0, nsize);
		fin.read(sendpdu.buf, nsize);//���÷����ļ���sendbuf
		//�ж��Ƿ��ǵ�һ�ζ�ȡ���ļ�ͷ��
		sendpdu.length = fin.gcount();
		st++;
		sendpdu.flags = ACK;//���÷���ACK
		//���ÿ�ʼ������־
		if (st == 1) { sendpdu.B_E = 0; sendpdu.seq = 0; }//��һ�ζ�ȡ�����ÿ�ʼ��־��seq
		else { sendpdu.B_E = -1; }//���ǵ�һ�ζ�ȡ
		if (fin.eof()) { sendpdu.B_E = 1; }//���һ�ζ�ȡ�����ý�����־
		sendpdu.checksum = setchecksum(sendpdu);
		sendret = send();//����
		if (sendret < 0) {
			cout << "send error" << GetLastError() << endl; 
			return 1;
		}
		else {
			cout << "======================send=======================" << endl;
			pdu_to_str(sendpdu);
		}
		//�ж��Ƿ��յ�ACK����ʱ�ط�
		timeout_retry();

		//�յ�srv��ACK���м��
		cout << "======================recv============================:" << endl;
		//���һ�����Ѿ�������ɣ��ҵõ�����ACK��
		if (sendpdu.B_E == 1 && ((sendpdu.seq + sendpdu.length + 1) == recvpdu.ack) && recvpdu.flags == ACK) {
			break;
		}
		//���ܶ����ط�
		while (!((sendpdu.seq + sendpdu.length + 1) == recvpdu.ack && recvpdu.flags == ACK)) {
			cout << "ack check failed,may loss or delay ,will resend" << endl;
			if ((sendpdu.seq + sendpdu.length + 1) != recvpdu.ack) {
				cout << "ack check failed" << sendpdu.seq << " " << sendpdu.length + 1 << " " << recvpdu.ack << endl;

			}
			if (recvpdu.flags != ACK) { cout << "not ack packet��" << recvpdu.flags << endl; }
			sendret = send(); //�ط�
			cout << "======================resend==========================:" << endl;
			pdu_to_str(sendpdu);
			timeout_retry();
			
			
		}
		//���յ���ȷ��ȷ�ϰ���ż���

		sendpdu.seq += sendpdu.length + 1;
	}
	cout << "send finished,going to close" << endl;
	cout << "total send file size:" << totalsize<< endl;
	fin.close();//�ر��ļ�
	sendpdu.length = 0;
	close_conn();
	clock_t end = clock();
	double sendtime = (double)(end - start) / CLOCKS_PER_SEC;
	cout << "sendtime:" << sendtime << " sec" << endl;
	cout << "Bytes/sec" << totalsize / sendtime <<" Bytes per sec" << endl;
	return 0;
}