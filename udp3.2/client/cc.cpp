#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <windows.h>
#include <mutex>
#include <thread>
#include<condition_variable>
#include"udp.h"
using namespace std;
using namespace chrono;
#pragma comment(lib,"ws2_32.lib")
#define IP "127.0.0.1"
#define PORT 1000
#define CPORT 1234

std::mutex mts;
//std::recursive_mutex mts;
std::condition_variable cv;
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
static  PDU* sendbuf;
static int* bufflag;
//�ļ�
string sourcefilename = "C:\\Users\\Ha ha\\vssource\\repos\\udp23.1\\3.jpg";
int lastsendbufseq;//��¼���һ�εĶ���
int lastrecvack=0;//��¼������ȷack
int sign = 0;//���߳�֮һ����
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


int send(PDU sendpdu) {
	return sendto(Socket, (char*)&sendpdu, sizeof(PDU), 0, (sockaddr*)&RemoteAddr, RemoteAddr_len);
}
int recv() {
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
int timeout_retry(PDU sendpdu) {

	while (1) {
		while (sendret < 0) {
			cout << "send error" << GetLastError() << endl;
			sendret = send(sendpdu);
			cout << "=================================================resend====================================================" << endl;
			pdu_to_str(sendpdu);
		}
		recvret = recv();
		if (recvret > 0) {
			cout << "===================================================recv===================================================" << endl;
			pdu_to_str(recvpdu);
			lessouttime();
			cout << "success recv ,so less timeout" << timeOut << endl;
			break;
		}
		else if (recvret == -1 && GetLastError() == 10060) {
			sendret = send(sendpdu);
			cout << "===================================================resend=================================================" << endl;
			pdu_to_str(sendpdu);
			updateouttime();
			cout << "timeout happen,so more timeout" << timeOut << endl;
			if (timeOut > 20000) {
				//ʱ��̫����,�������ȴ���
				return 0;
			}
		}
		else {
			cout << "other recv error" << GetLastError() << endl;
			return 0;
		}
	}
	return 1;
}
bool establish_conn() {
	//��һ������
	sendpdu.flags = SYN;
	sendpdu.seq = 0;
	sendret = send(sendpdu);
	if (sendret < 0) {
		cout << "send error" << GetLastError() << endl;
		return false;
	}
	//�ڶ�������
	timeout_retry(sendpdu);

	cout << "======================send=======================" << endl;
	pdu_to_str(sendpdu);
	while (!((recvpdu.ack == sendpdu.seq + 1))) {
		sendret = send(sendpdu);
		timeout_retry(sendpdu);
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
	sendret = send(sendpdu);
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
	sendret = send(sendpdu);
	cout << "======================send=========================" << endl;
	pdu_to_str(sendpdu);
	cout << "FIRST BYE��go state: FIN_WAIT1" << endl;
	timeout_retry(sendpdu);

	cout << "go state FIN_WAIT2" << endl;


	while (!(recvpdu.ack == sendpdu.seq + 1 && recvpdu.flags == ACK_FIN)) {
		Sleep(10000);
		timeout_retry(sendpdu);
	}
	sendpdu.flags = ACK;
	sendpdu.ack = recvpdu.seq + 1;
	sendret = send(sendpdu);
	sendret = send(sendpdu);
	cout << "======================send=========================" << endl;
	pdu_to_str(sendpdu);
	cout << "THIRD BYE ,go state:TIME_WAIT" << endl;
	Sleep(10000);//�ȴ�10��

	closesocket(Socket);


}
//���·�������
void update_winval(uint32_t&start_index, uint32_t&end_index,int winsize, uint32_t nextsendseq) {
	
	cout << "update_window_from: [" << start_index << "," << end_index << "] to";
	start_index = nextsendseq;
	end_index = start_index + ((sendpdu.win - 1) * (nsize + 1));
	cout << "[" << start_index << "," << end_index << "]" << endl;
	
}
void threadsend(ifstream &fin, uint32_t& start_index, uint32_t& end_index,int &st) {
	while (!fin.eof()) {
		//���Ŷ�ȡ�ļ�
		cout << "===sendThrea_go_on===";
		std::unique_lock<std::mutex> lck(mts);
		cout << "��������:[" << start_index << "," << end_index <<"]"<<endl;
		if (!((sendpdu.seq == start_index || sendpdu.seq > start_index) &&( sendpdu.seq == end_index||sendpdu.seq<end_index))) { 
			cout <<sendpdu.seq<< "δ�ڷ�������" << start_index<<","<<end_index <<"sendThrea_wait" << endl;
			cv.notify_one();
			cv.wait(lck);
			continue; 
		}
		memset(sendpdu.buf, 0, sizeof(char) * nsize);
		fin.read(sendpdu.buf, nsize);//���÷����ļ���sendbuf
		
		st++;//�ж��Ƿ��ǵ�һ�ζ�ȡ���ļ�ͷ)
		cout << "st===" << st << endl;
		//���ÿ�ʼ������־
		if (st == 1) { 
			cout << "��ʼ��־����" << endl;
			sendpdu.B_E = 0; sendpdu.seq = 0; 
		}//��һ�ζ�ȡ�����ÿ�ʼ��־��seq
		else if(!fin.eof()) { 
			sendpdu.B_E = -1; 
		
		}//���ǵ�һ�ζ�ȡ
		else if (fin.eof()) { 
			cout << "���ý�����־" << endl;
			sendpdu.B_E = 1; 
			sign = 1;
		}//���һ�ζ�ȡ�����ý�����־
		lastsendbufseq = sendpdu.seq;
		sendpdu.flags = ACK;//���÷���ACK
		sendpdu.length = fin.gcount();
		sendpdu.checksum = setchecksum(sendpdu);
		//ʼ�մ�win��sendbuf{%win}
		sendbuf[(sendpdu.seq / (nsize + 1)) % sendpdu.win] = sendpdu;
		bufflag[(sendpdu.seq / (nsize + 1)) % sendpdu.win] = 1;
		cout << "����seq��" <<sendpdu.seq<<"��[" << (sendpdu.seq / (nsize + 1))%sendpdu.win<<"]��"<< endl;
		sendret = send(sendpdu);//����
		if (sendret < 0) {
			cout << "send error" << GetLastError() << endl;
			return ;
		}
		else {
			cout << "=======================================================send=======================================================" << endl;
			pdu_to_str(sendpdu);
			
		}
		if(!sign){
		sendpdu.seq = sendpdu.seq + sendpdu.length + 1;//������һ�ν�Ҫ���͵�seq
		}
		else {
			cout << "sendthread_finished_will_return" << endl;
			cv.notify_one();
		}
		cout << "sendThread_exit" << endl;
	}
	return;
}
void threadrecv(uint32_t& start_index, uint32_t& end_index) {
	Sleep(1);//�Ƚ���send

	while (1) {
		std::unique_lock<std::mutex> lck(mts);
		cout << "===recvThrea_go_on===" << endl;
		//�ж��Ƿ�ʱ�ط�
		timeout_retry(sendbuf[(lastsendbufseq / (nsize + 1)) % sendpdu.win]);
		
		if ((recvpdu.ack - recvpdu.seq == 1 && recvpdu.flags == ACK_SYN)) { 
			continue;
		}//���������Ӷ�����İ�

		
		//�յ�srv��ACK���м��
		
		//���һ�����Ѿ�������ɣ��ҵõ�����ACK��
		if (sendbuf[(lastsendbufseq / (nsize + 1)) % sendpdu.win].B_E == 1 && ((lastsendbufseq + sendpdu.length + 1) == recvpdu.ack) && recvpdu.flags == ACK) {
			cout << "���һ�����ݰ���ACK�Ѿ����գ������������===recv_thread_return" << endl;
			return;
		}
		//������յ���ȷ��Χ�ڵ�ack��ACK���Ҳ����ظ���ack�͸��»������ڣ�ת�����߳�
		if ((recvpdu.ack == start_index + sendpdu.length + 1 || recvpdu.ack > start_index + sendpdu.length + 1)
			&& (recvpdu.ack < end_index + sendpdu.length + 1 || recvpdu.ack == end_index + sendpdu.length + 1)
			&& recvpdu.flags == ACK) {
			if (lastrecvack < recvpdu.ack) {
				lastrecvack = recvpdu.ack;
				update_winval(start_index, end_index, sendpdu.win, recvpdu.ack);//���»�������
				if (!sign) {
					cv.notify_one();//���ܴ�ʱ����ͨ�����ͼ�ʱ����
					cv.wait(lck);
				}
				continue;
			}
			else {
				cout << "�������Ľ�Сack:" << recvpdu.ack << "<=" << lastrecvack << endl;//�����Ľ�С��ack����
				continue;
			}
		}
		//���յ�ack_loss�İ����߻������ڻ��п��Է��͵����ݰ�
		else {
			while (!
				((recvpdu.ack == start_index + sendpdu.length + 1 || recvpdu.ack > start_index + sendpdu.length + 1)
					&& (recvpdu.ack < end_index + sendpdu.length + 1 || recvpdu.ack == end_index + sendpdu.length + 1)
					&& recvpdu.flags == ACK)) {
				cout << "ack check failed,may loss ,will resend" << endl;
				//��recvǰ�������ڴ���δ���͵����ݰ�
				if (bufflag[((recvpdu.ack) / (nsize + 1)) % sendpdu.win] == 0 || (sendbuf[((recvpdu.ack) / (nsize + 1)) % sendpdu.win].seq != recvpdu.ack)) {
					break;
				}
				//�ڴ�seq��д���send��Ч����¼send�����һ�Σ��ڴ���������ݰ��ٷ���һ��
				int shouldsendindex = ((recvpdu.ack) / (nsize + 1)) % sendpdu.win;
				cout << "ACK_LOSS,so�ط������==>" << recvpdu.ack << "============" << ((recvpdu.ack) / (nsize + 1)) % sendpdu.win << "======" << lastsendbufseq << "====" << (lastsendbufseq / (nsize + 1)) % sendpdu.win  << endl;
				int lastindex = (lastsendbufseq / (nsize + 1)) % sendpdu.win;
				if (!(shouldsendindex > lastindex))
					for (int i = shouldsendindex; i < lastindex + 1; i++) {
						sendret = send(sendbuf[i]);
						pdu_to_str(sendbuf[i]);

					}
				else {
					for (int i = shouldsendindex; i < sendpdu.win; i++) {
						cout << "i=>" << i << endl;
						sendret = send(sendbuf[i]);
						pdu_to_str(sendbuf[i]);

					}
					for (int i = 0; i <= lastindex; i++) {
						cout << "i=>" << i << endl;
						sendret = send(sendbuf[i]);
						pdu_to_str(sendbuf[i]);

					}
				}
				//��ʱ�ط�������͵����ݰ�
				timeout_retry(sendbuf[shouldsendindex]);
			}
			//���ܻ������ڻ�ûȫ��send�꣬��ת��send�߳�
			if (sendbuf[((recvpdu.ack) / (nsize + 1)) % (sendpdu.win)].seq != recvpdu.ack || bufflag[((recvpdu.ack) / (nsize + 1)) % sendpdu.win] == 0) {
				cout << "���������д���δ����������ݰ�:" << recvpdu.ack << "=>" << sendbuf[((recvpdu.ack) / (nsize + 1)) % (sendpdu.win)].seq << " in [" << start_index << "," << end_index << "]" << endl;
				if (!sign) {
					cv.notify_one();
					cv.wait(lck);
				}
				continue;
			}
			//���»������ڣ���������
			else {
				if (lastrecvack < recvpdu.ack) {
					lastrecvack = recvpdu.ack;
					update_winval(start_index, end_index, sendpdu.win, recvpdu.ack);//���»�������
					continue;
				}
				else {
					cout << "������СACK" << recvpdu.ack << "<=" << lastrecvack << endl;
					continue;
				}
			}
			cout << "recvThread_exit" << endl;
			continue;
		}
		//���͵��ж�����ֻ���Ƿ��ڻ����������䣬�ڴ�����Ͳ�ͣ����
		//�ط�������ֻ���ڽ��յ����ڻ����������䣬�����жϳ�ʱ
		//���պ�send�����̣߳������й�������������жϺ��߳������л�
		return;
	}
}
int main() {
	//C:\\Users\\Ha ha\\vssource\\repos\\udp23.1\\3.jpg
	//·�ɶ˿�
	cout << "Please input the router port��" << endl;
	cin >> port;
	cin.ignore(1024, '\n');
	//�󶨶˿�
	cout << "please input the bind port��" << endl;
	cin >> cport;
	cin.ignore(1024, '\n');
	//���ʹ��ڴ�С
	cout << "please input the sendwinsize��" << endl;
	cin >> sendpdu.win;
	cin.ignore(1024, '\n');
	//���淢�͵����ݰ�
	sendbuf = new PDU[sendpdu.win];
	//�жϵ�ǰλ���Ƿ�Ϊ��
	bufflag = new int[sendpdu.win];
	memset(bufflag, 0, sizeof(int) * sendpdu.win);
	//���ڷ��͵����ݱ�����[s,e]B
	static uint32_t start_index=0;
	static uint32_t end_index=start_index+(sendpdu.win-1)*(nsize+1);
	
	//��ʱ��ʼ
	clock_t start = clock();
	if (!socket_init()) { cout << "init failed!!" << endl; return 1; }
	//establish_conn
	if (!establish_conn()) { cout << "connect failed!!" << endl; return 1; }
	
	cout << "conneted!!!!!!!!!!!!" << endl;

	//�����ļ�
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
	
	
	//����������ļ�
	//ofstream logout("sender_log.txt");
	//streambuf* p0ld = cout.rdbuf(logout.rdbuf());
	

	thread t(threadsend,ref(fin),ref(start_index),ref(end_index),ref(st));
	thread t1(threadrecv, ref(start_index), ref(end_index));
	t.join();
	t1.join();

	cout << "send finished,going to close" << endl;
	cout << "total send file size:" << totalsize<<" Bytes" << endl;
	fin.close();//�ر��ļ�
	sendpdu.length = 0;
	close_conn();
	clock_t end = clock();
	double sendtime = (double)(end - start) / CLOCKS_PER_SEC;
	cout << "sendtime:" << sendtime << " sec" << endl;
	cout << "Bytes/sec:" << totalsize / sendtime << " Bytes per sec" << endl;
	system("pause");
	return 0;
}