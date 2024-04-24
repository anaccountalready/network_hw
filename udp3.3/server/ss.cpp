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
static PDU* recvbuf;
static int* bufflag;
int lastrecvseq = 0;//最近最大接收到的seq
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
void close_conn_first() {//先关闭连接
	sendpdu.flags = ACK_FIN;
	sendpdu.seq = 0;
	//超时重传
	sendret = send();
	cout << "======================send=========================" << endl;
	pdu_to_str(sendpdu);
	cout << "FIRST BYE，go state: FIN_WAIT1" << endl;
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
	Sleep(10000);//等待10秒

	closesocket(Socket);


}
bool close_conn() {//后关闭连接

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
//更新发送区间
void update_winval(uint32_t& start_index, uint32_t& end_index, int winsize, uint32_t nextsendseq) {

	cout << "update_window_from: [" << start_index << "," << end_index << "] to";
	start_index = nextsendseq;
	end_index = start_index + ((sendpdu.win - 1) * (nsize + 1));
	cout << "[" << start_index << "," << end_index << "]" << endl;

}
//检查接收缓冲区的连续文件并写入文件，更新接收缓冲区范围
int checkrecvbuf_and_update_win(int start, ofstream& fout, uint32_t& start_index, uint32_t& end_index) {


	int end = -2;
	int e = (end_index / (nsize + 1)) % sendpdu.win;
	cout << "收到后来的下界，故开始检查连续并写入in[" << start << "," << e << "]" << endl;
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
					cout << "写入seq=>:" << recvbuf[i].seq << endl;
					fout.write(recvbuf[i].buf, recvbuf[i].length);
					bufflag[i] = 0;//清空标志
					if (recvbuf[i].B_E == 1) {
						cout << "RECV finished!!!" << endl; recvpdu.length = 0;//接收完成
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
					cout << "写入seq=>:" << recvbuf[i].seq << endl;
					fout.write(recvbuf[i].buf, recvbuf[i].length);
					bufflag[i] = 0;//清空标志
					if (recvbuf[i].B_E == 1) {
						cout << "RECV finished!!!" << endl; recvpdu.length = 0;//接收完成
						return 1;
					}
				}
			}
			if (f) {//需要继续判断
				for (int i = 0; i <= e; i++) {
					if (bufflag[i] == 0) {
						end = i - 1;
						if (i == 0)end = sendpdu.win - 1;
						break;
					}
					else {
						end = i;
						cout << "写入seq=>:" << recvbuf[i].seq << endl;
						fout.write(recvbuf[i].buf, recvbuf[i].length);
						bufflag[i] = 0;//清空标志
						if (recvbuf[i].B_E == 1) {
							cout << "RECV finished!!!" << endl; recvpdu.length = 0;//接收完成
							return 1;
						}
					}
				}
			}
		}
		cout << "end" << end << " => " << recvbuf[end].seq << "=>" << recvbuf[end].length + 1 << endl;
		update_winval(start_index, end_index, sendpdu.win, recvbuf[end].seq + recvbuf[end].length + 1);
	}//连续范围[start,end]并写入
	return 0;
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
	//接收窗口大小
	cout << "please input the recvwinsize：" << endl;
	cin >> sendpdu.win;
	cin.ignore(1024, '\n');

	if (!socket_init()) { cout << "init failed!!" << endl; return 1; }
	if (!establish_conn()) { cout << "connect failed!!" << endl; return 1; }


	cout << "conneted!!!!!!!!!!!!" << endl;
	cout << "please input the recv file name:" << endl;
	cin >> destfilename;
	cin.ignore(1024, '\n');


	//定向输出到文件
	//ofstream logout("recver_log.txt");
	//streambuf* p0ld = cout.rdbuf(logout.rdbuf());

	ofstream outf(destfilename.c_str(), ios::out | ios::binary);//输出的文件

	//缓存接收到的数据包
	recvbuf = new PDU[sendpdu.win];
	bufflag = new int[sendpdu.win];
	memset(bufflag, 0, sizeof(int) * sendpdu.win);//清空标志
	//接受窗口区间
	static uint32_t start_index = 0;
	static uint32_t end_index = start_index + (sendpdu.win - 1) * (nsize + 1);
	//接收到下界包，如果是之前的丢包就检查并更新窗口范围，否则就更新窗口范围
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
			if (recvpdu.flags != ACK || recvpdu.seq == 1) { continue; }//处理建立连接的多余数据包
			pdu_to_str(recvpdu);
			//校验失败，继续接收
			if (checkchecksum(recvpdu) == 0) { cout << "check faild" << endl; continue; }
			//在接收范围内的数据包:
			//1.接收到最下界，发送ACK，判断连续，写入，更新滑动窗口和bufflag标志（顺序接收到最下界，直接写入；后续接收到最下界，去判断缓存的连续）
			//2.乱序接收，即收到的不是最下界,标记接收，发送ACK
			if (recvpdu.seq >= start_index && recvpdu.seq <= end_index && recvpdu.flags == ACK) {
				//确认后将接收到的数据写入本地文件
				if (recvpdu.seq == start_index)
				{
					if (recvpdu.seq <= lastrecvseq) {//说明收到的是丢包且位于最下界
						cout << recvpdu.seq << "小于 " << lastrecvseq << "保存seq：" << recvpdu.seq << "在[" << (recvpdu.seq / (nsize + 1)) % recvpdu.win << "]中" << endl;
						int index = (recvpdu.seq / (nsize + 1)) % recvpdu.win;
						bufflag[index] = 1;
						recvbuf[(recvpdu.seq / (nsize + 1)) % recvpdu.win] = recvpdu;
						sendpdu.ack = recvpdu.seq + recvpdu.length + 1;//设置发送ack.下一个希望接受的seq
						sendpdu.flags = ACK;//设置ACK
						sendret = send();//发送ACK包
						cout << "======================================================send==================================================" << endl;
						pdu_to_str(sendpdu);
						if (checkrecvbuf_and_update_win(index, outf, start_index, end_index))//检查缓冲区是否有下边界的连续包，有就交给上层并更新接收范围
						{
							break;
						}
					}
					else {
						cout << "写入seq=>:" << start_index << endl;
						outf.write(recvpdu.buf, recvpdu.length);
						sendpdu.ack = recvpdu.seq + recvpdu.length + 1;//设置发送ack.下一个希望接受的seq
						next_want_seq = sendpdu.ack;//设置下一个期望的recv的seq
						sendpdu.flags = ACK;//设置ACK
						sendret = send();//发送ACK包
						cout << "======================================================send==================================================" << endl;
						pdu_to_str(sendpdu);
						if (recvpdu.B_E == 1) {
							cout << "RECV finished!!!" << endl; recvpdu.length = 0;//接收完成
							break;
						}
						update_winval(start_index, end_index, sendpdu.win, sendpdu.ack);
					}

				}
				//乱序接收
				else {
					cout << "=================================================OutOfOrderbuf=====================================================" << endl;
					//存下recvpdu
					int index = (recvpdu.seq / (nsize + 1)) % recvpdu.win;
					recvbuf[(recvpdu.seq / (nsize + 1)) % recvpdu.win] = recvpdu;
					bufflag[index] = 1;//标志
					cout << "保存seq：" << recvpdu.seq << "在[" << index << "]中=>+" << bufflag[index] << endl;
					sendpdu.ack = recvpdu.seq + recvpdu.length + 1;//设置发送ack.下一个希望接受的seq
					sendpdu.flags = ACK;//设置ACK
					sendret = send();//发送ACK包
					cout << "======================================================send==================================================" << endl;
					pdu_to_str(sendpdu);
				}
			}
			//在接受范围窗口之外的ACK包
			//小于窗口范围
			else if (recvpdu.seq < start_index && recvpdu.flags == ACK) {
				//确认包丢失重传
				sendpdu.flags = ACK;
				sendpdu.ack = recvpdu.seq + recvpdu.length + 1;//设置发送ack.下一个希望接受的seq
				sendret = send();//发送ACK包
				cout << "==================================================ACK_LOSS====send==================================================ack=>"<<sendpdu.ack << endl;
				pdu_to_str(sendpdu);

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
		else {
			close_conn_first();
			break;
		}
	}
	system("pause");
	return 0;
}