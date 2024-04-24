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
int timeOut = 5;
static PDU sendpdu, recvpdu;
int next_want_seq = 0;//��ʼϣ�����ܵ�seq=0
int sendret = 0;
int recvret = 0;
char ip[30] = IP;
char sip[20] = Srv_ip;
int port = PORT;
int sport = Srv_port;
int wait = 3;
static PDU* recvbuf;
static int* bufflag;
int lastrecvseq = 0;//��������յ���seq
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
	Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);//udpЭ��
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
bool establish_conn() {

	while (1) {
		recvret = recv();
		if (recv() > 0)break;
		else cout << "waiting for client request......." << " " << recvret << endl;
	}
	sendpdu.dest_port = ntohs(RemoteAddr.sin_port);
	cout << "======================recv=======================" << endl;
	pdu_to_str(recvpdu);
	if (recvpdu.win != sendpdu.win) {
		sendpdu.win = recvpdu.win;
		cout << "WARNING::++++++++++++++++++++++sendwin!=recvwin,So update recvwin,let recvwin = sendwin+++++++++++++++++++" << endl;
	}
	if (recvpdu.flags == SYN) {
		cout << "FIRST SHACK HANDS SUCCEED!!" << endl;
		sendpdu.flags = ACK_SYN;
		sendpdu.seq = 0;
		sendpdu.ack = recvpdu.seq + 1;
		sendret = send();
		//�ش�
		timeout_retry();


		//����������
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
void close_conn_first() {//�ȹر�����
	sendpdu.flags = ACK_FIN;
	sendpdu.seq = 0;
	//��ʱ�ش�
	sendret = send();
	cout << "======================send=========================" << endl;
	pdu_to_str(sendpdu);
	cout << "FIRST BYE��go state: FIN_WAIT1" << endl;
	while (recvpdu.ack != 1&&recvpdu.seq!=0) {
		sendret = send();
	}
	cout << "go state FIN_WAIT2" << endl;


	while (!(recvpdu.ack == sendpdu.seq + 1 && recvpdu.flags == ACK_FIN)) {
		Sleep(1000);
		timeout_retry();
	}
	sendpdu.flags = ACK;
	sendpdu.ack = recvpdu.seq + 1;
	sendret = send();
	sendret = send();
	cout << "======================send=========================" << endl;
	pdu_to_str(sendpdu);
	cout << "THIRD BYE ,go state:TIME_WAIT" << endl;
	Sleep(10000);//�ȴ�10��

	closesocket(Socket);


}
bool close_conn() {//��ر�����

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
//���·�������
void update_winval(uint32_t& start_index, uint32_t& end_index, int winsize, uint32_t nextsendseq) {

	cout << "update_window_from: [" << start_index << "," << end_index << "] to";
	start_index = nextsendseq;
	end_index = start_index + ((sendpdu.win - 1) * (nsize + 1));
	cout << "[" << start_index << "," << end_index << "]" << endl;

}
//�����ջ������������ļ���д���ļ������½��ջ�������Χ
int checkrecvbuf_and_update_win(int start, ofstream& fout, uint32_t& start_index, uint32_t& end_index) {


	int end = -2;
	int e = (end_index / (nsize + 1)) % sendpdu.win;
	cout << "�յ��������½磬�ʿ�ʼ���������д��in[" << start << "," << e << "]" << endl;
	cout << "bufflag" << start << "=>" << bufflag[start] << endl;
	if (bufflag[start] == 1 && recvbuf[start].seq == start_index) {
		if (start <= e) {
			for (int i = start; i <= e; i++) {
				if (bufflag[i] == 0) {
					end = i - 1;
					break;
				}
				else {
					end = i;
					cout << "д��seq=>:" << recvbuf[i].seq << endl;
					fout.write(recvbuf[i].buf, recvbuf[i].length);
					bufflag[i] = 0;//��ձ�־
					if (recvbuf[i].B_E == 1) {
						cout << "RECV finished!!!" << endl; recvpdu.length = 0;//�������
						return 1;
					}
				}
			}
		}
		else {
			bool f = true;
			for (int i = start; i < sendpdu.win; i++) {
				if (bufflag[i] == 0) {
					end = i - 1;
					f = false;
					break;
				}
				else {
					end = i;
					cout << "д��seq=>:" << recvbuf[i].seq << endl;
					fout.write(recvbuf[i].buf, recvbuf[i].length);
					bufflag[i] = 0;//��ձ�־
					if (recvbuf[i].B_E == 1) {
						cout << "RECV finished!!!" << endl; recvpdu.length = 0;//�������
						return 1;
					}
				}
			}
			if (f) {//��Ҫ�����ж�
				for (int i = 0; i <= e; i++) {
					if (bufflag[i] == 0) {
						end = i - 1;
						if (i == 0)end = sendpdu.win - 1;
						break;
					}
					else {
						end = i;
						cout << "д��seq=>:" << recvbuf[i].seq << endl;
						fout.write(recvbuf[i].buf, recvbuf[i].length);
						bufflag[i] = 0;//��ձ�־
						if (recvbuf[i].B_E == 1) {
							cout << "RECV finished!!!" << endl; recvpdu.length = 0;//�������
							return 1;
						}
					}
				}
			}
		}
		cout << "end" << end << " => " << recvbuf[end].seq << "=>" << recvbuf[end].length + 1 << endl;
		update_winval(start_index, end_index, sendpdu.win, recvbuf[end].seq + recvbuf[end].length + 1);
	}//������Χ[start,end]��д��
	return 0;
}
//�ļ���
string destfilename = "1.jpg";
int main() {
	cout << "Please input the router port��" << endl;
	cin >> port;
	cin.ignore(1024, '\n');
	cout << "Please input the bind port��" << endl;
	cin >> sport;
	cin.ignore(1024, '\n');
	//���մ��ڴ�С
	cout << "please input the recvwinsize��" << endl;
	cin >> sendpdu.win;
	cin.ignore(1024, '\n');

	if (!socket_init()) { cout << "init failed!!" << endl; return 1; }
	if (!establish_conn()) { cout << "connect failed!!" << endl; return 1; }


	cout << "conneted!!!!!!!!!!!!" << endl;
	cout << "please input the recv file name:" << endl;
	cin >> destfilename;
	cin.ignore(1024, '\n');


	//����������ļ�
	//ofstream logout("recver_log.txt");
	//streambuf* p0ld = cout.rdbuf(logout.rdbuf());

	ofstream outf(destfilename.c_str(), ios::out | ios::binary);//������ļ�

	//������յ������ݰ�
	recvbuf = new PDU[sendpdu.win];
	bufflag = new int[sendpdu.win];
	memset(bufflag, 0, sizeof(int) * sendpdu.win);//��ձ�־
	//���ܴ�������
	static uint32_t start_index = 0;
	static uint32_t end_index = start_index + (sendpdu.win - 1) * (nsize + 1);
	//���յ��½���������֮ǰ�Ķ����ͼ�鲢���´��ڷ�Χ������͸��´��ڷ�Χ
	while (1) {
		recvret = recv();
		if (recvpdu.seq > lastrecvseq) {
			lastrecvseq = recvpdu.seq;
		}
		if (recvret == -1 && GetLastError() == 10060) {
			sendret = send();
			cout << "==================================================TIME_OUTresend===================================================" << endl;
			pdu_to_str(sendpdu);
			updateouttime();
			cout << "timeout happen,so more timeout" << timeOut << endl;
		}
		if (recvret > 0) {
			cout << "=====================================================recv==================================================" << endl;
			lessouttime();
			cout << "success recv,so less timeout" << timeOut << endl;
			if (recvpdu.flags != ACK || recvpdu.seq == 1) { continue; }//���������ӵĶ������ݰ�
			pdu_to_str(recvpdu);
			//У��ʧ�ܣ���������
			if (checkchecksum(recvpdu) == 0) { cout << "check faild" << endl; continue; }
			//�ڽ��շ�Χ�ڵ����ݰ�:
			//1.���յ����½磬����ACK���ж�������д�룬���»������ں�bufflag��־��˳����յ����½磬ֱ��д�룻�������յ����½磬ȥ�жϻ����������
			//2.������գ����յ��Ĳ������½�,��ǽ��գ�����ACK
			if (recvpdu.seq >= start_index && recvpdu.seq <= end_index && recvpdu.flags == ACK) {
				//ȷ�Ϻ󽫽��յ�������д�뱾���ļ�
				if (recvpdu.seq == start_index)
				{
					if (recvpdu.seq <= lastrecvseq) {//˵���յ����Ƕ�����λ�����½�
						cout << recvpdu.seq << "С�� " << lastrecvseq << "����seq��" << recvpdu.seq << "��[" << (recvpdu.seq / (nsize + 1)) % recvpdu.win << "]��" << endl;
						int index = (recvpdu.seq / (nsize + 1)) % recvpdu.win;
						bufflag[index] = 1;
						recvbuf[(recvpdu.seq / (nsize + 1)) % recvpdu.win] = recvpdu;
						sendpdu.ack = recvpdu.seq + recvpdu.length + 1;//���÷���ack.��һ��ϣ�����ܵ�seq
						sendpdu.flags = ACK;//����ACK
						sendret = send();//����ACK��
						cout << "======================================================send==================================================" << endl;
						pdu_to_str(sendpdu);
						if (checkrecvbuf_and_update_win(index, outf, start_index, end_index))//��黺�����Ƿ����±߽�����������оͽ����ϲ㲢���½��շ�Χ
						{
							break;
						}
					}
					else {
						cout << "д��seq=>:" << start_index << endl;
						outf.write(recvpdu.buf, recvpdu.length);
						sendpdu.ack = recvpdu.seq + recvpdu.length + 1;//���÷���ack.��һ��ϣ�����ܵ�seq
						next_want_seq = sendpdu.ack;//������һ��������recv��seq
						sendpdu.flags = ACK;//����ACK
						sendret = send();//����ACK��
						cout << "======================================================send==================================================" << endl;
						pdu_to_str(sendpdu);
						if (recvpdu.B_E == 1) {
							cout << "RECV finished!!!" << endl; recvpdu.length = 0;//�������
							break;
						}
						update_winval(start_index, end_index, sendpdu.win, sendpdu.ack);
					}

				}
				//�������
				else {
					cout << "=================================================OutOfOrderbuf=====================================================" << endl;
					//����recvpdu
					int index = (recvpdu.seq / (nsize + 1)) % recvpdu.win;
					recvbuf[(recvpdu.seq / (nsize + 1)) % recvpdu.win] = recvpdu;
					bufflag[index] = 1;//��־
					cout << "����seq��" << recvpdu.seq << "��[" << index << "]��=>+" << bufflag[index] << endl;
					sendpdu.ack = recvpdu.seq + recvpdu.length + 1;//���÷���ack.��һ��ϣ�����ܵ�seq
					sendpdu.flags = ACK;//����ACK
					sendret = send();//����ACK��
					cout << "======================================================send==================================================" << endl;
					pdu_to_str(sendpdu);
				}
			}
			//�ڽ��ܷ�Χ����֮���ACK��
			//С�ڴ��ڷ�Χ
			else if (recvpdu.seq < start_index && recvpdu.flags == ACK) {
				//ȷ�ϰ���ʧ�ش�
				sendpdu.flags = ACK;
				sendpdu.ack = recvpdu.seq + recvpdu.length + 1;//���÷���ack.��һ��ϣ�����ܵ�seq
				sendret = send();//����ACK��
				cout << "==================================================ACK_LOSS====send==================================================ack=>"<<sendpdu.ack << endl;
				pdu_to_str(sendpdu);

			}

			//�������


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
		else {
			close_conn_first();
			break;
		}
	}
	system("pause");
	return 0;
}