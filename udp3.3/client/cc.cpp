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
//设置接收超时5ms
int timeOut = 5;
//客户端发送
PDU sendpdu, recvpdu;
static  PDU* sendbuf;
static int* bufflag;
static int* flag;//标记确认ACK
static PDU resendpdu;
//文件
string sourcefilename = "C:\\Users\\Ha ha\\vssource\\repos\\udp23.1\\3.jpg";
int lastsendbufseq;//记录最近一次的读入
int lastrecvack = 0;//记录最大的正确ack
int sign = 0;//是线程之一结束
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
//传输过程中超时重传会找到重传窗口内最早没有收到ACK的包，只重传该包
int timeout_retry_for(PDU sendpdu, uint32_t& start_index, uint32_t& end_index) {
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

			int start = (start_index / (nsize + 1)) % sendpdu.win;
			int end = (end_index / (nsize + 1)) % sendpdu.win;
			if (start <= end)
				for (int i = start; i <= end; i++) {
					if (flag[i] == 0) {
						cout << "=========Time_Out_Retry_So_Find_The_First_Not_Recv_ACK_Resend_This_PDU============seq[" << i << "]=>" << sendbuf[i].seq << "=+>" << flag[i] << endl;
						send(sendbuf[i]);
						pdu_to_str(sendbuf[i]);
						break;
					}
					else {
						cout << "ACK_seq[" << i << "]=>" << sendbuf[i].seq << endl;
					}
				}
			else {
				int t = 0;
				for (int i = start; i < sendpdu.win; i++) {
					if (flag[i] == 0) {//找到了
						cout << "=========Time_Out_Retry_So_Find_The_First_Not_Recv_ACK_Resend_This_PDU============seq[" << i << "]=>" << sendbuf[i].seq << "=+>" << flag[i] << endl;
						send(sendbuf[i]);
						pdu_to_str(sendbuf[i]);
						t = 1;
						break;
					}
					else {
						cout << "ACK_seq[" << i << "]=>" << sendbuf[i].seq << "=+>" << flag[i] << endl;
					}
				}
				//还没找到，接着找
				if(t==0)
				for (int i = 0; i <= end; i++) {
					if (flag[i] == 0) {
						cout << "=========Time_Out_Retry_So_Find_The_First_Not_Recv_ACK_Resend_This_PDU===========seq[" << i << "]=>" << sendbuf[i].seq << "=+>" << flag[i] << endl;
						send(sendbuf[i]);
						pdu_to_str(sendbuf[i]);
						break;
					}
					else {
						cout << "ACK_seq[" << i << "]=>" << sendbuf[i].seq << endl;
					}
				}
			}
			updateouttime();
		}
		else {
			cout << "other recv error" << GetLastError() << endl;
			return 0;
		}
	}
	return 1;
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
				//时间太长了,不继续等待了
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
	//第一次握手
	sendpdu.flags = SYN;
	sendpdu.seq = 0;
	sendret = send(sendpdu);
	if (sendret < 0) {
		cout << "send error" << GetLastError() << endl;
		return false;
	}
	//第二次握手
	timeout_retry(sendpdu);

	cout << "======================send=======================" << endl;
	pdu_to_str(sendpdu);
	while (!((recvpdu.ack == sendpdu.seq + 1))) {
		sendret = send(sendpdu);
		timeout_retry(sendpdu);
	}
	{

		cout << "SECOND SHAKE HAND SUCCEED！！" << endl;
		cout << "======================recv=======================" << endl;
		pdu_to_str(recvpdu);
	}

	//第三次握手
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
void close_conn() {//先关闭连接
	sendpdu.flags = ACK_FIN;
	sendpdu.seq = 0;
	//超时重传
	sendret = send(sendpdu);
	cout << "======================send=========================" << endl;
	pdu_to_str(sendpdu);
	cout << "FIRST BYE，go state: FIN_WAIT1" << endl;
	while (recvpdu.ack != 1) {
		sendret = send(sendpdu);
	}
	//timeout_retry(sendpdu);

	cout << "go state FIN_WAIT2" << endl;


	while (!(recvpdu.ack == sendpdu.seq + 1 && recvpdu.flags == ACK_FIN)) {
		Sleep(1000);
		timeout_retry(sendpdu);
	}
	sendpdu.flags = ACK;
	sendpdu.ack = recvpdu.seq + 1;
	sendret = send(sendpdu);
	sendret = send(sendpdu);
	cout << "======================send=========================" << endl;
	pdu_to_str(sendpdu);
	cout << "THIRD BYE ,go state:TIME_WAIT" << endl;
	Sleep(10000);//等待10秒

	closesocket(Socket);


}
bool close_conn_after() {//后关闭连接

	//函数前收到了对端的ACK_FIN
	sendpdu.flags = ACK;
	sendpdu.ack = recvpdu.seq + 1;
	sendpdu.seq = 0;
	cout << "SECOND BYE,go to state CLOSE_WAIT" << endl;
	sendret = send(sendpdu);
	//已经接收完毕,发送ACK+FIN
	cout << "======================send=========================" << endl;
	pdu_to_str(sendpdu);
	sendpdu.flags = ACK_FIN;
	sendpdu.seq = 0;
	sendret = send(sendpdu);
	cout << "======================send=========================" << endl;
	pdu_to_str(sendpdu);
	cout << "THIRD BYE,go to state LAST_ACK" << endl;
	//超时重传
	if (timeout_retry(sendpdu) == 0) {
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
void check_ACK_update(int start, uint32_t& start_index, uint32_t& end_index, int winsize) {

	int end;
	int e = (end_index / (nsize + 1)) % sendpdu.win;
	cout << "bufflag" << start << "=>" << flag[start] << "in[" << start_index << "," << end_index << "]" << endl;
	if (flag[start] == 1 && recvpdu.ack == start_index + sendbuf[start].length + 1) {
		if (start <= e) {
			for (int i = start; i <= e; i++) {
				if (flag[i] == 0) {
					cout << "CHECK_seq+=>" << sendbuf[i].seq << "=>flag:=>" << i << "==" << flag[i] << "WRONG" << endl;
					end = i - 1;
					break;
				}
				else {
					cout << "CHECK_seq+=>" << sendbuf[i].seq << "=>flag:=>" << i << "==" << flag[i] << "RIGHT" << endl;
					flag[i] = 0;//清空标志
					end = i;

				}
			}
		}

		else {
			bool f = true;
			for (int i = start; i < sendpdu.win; i++) {
				if (flag[i] == 0) {
					end = i - 1;
					cout << "CHECK_seq+=>" << sendbuf[i].seq << "=>flag:=>" << i << "==" << flag[i] << "---+" << "WRONG" << endl;
					f = false;
					break;
				}
				else {
					cout << "CHECK_seq+=>" << sendbuf[i].seq << "=>flag:=>" << i << "==" << flag[i] << "---+" << "RIGNT" << endl;
					flag[i] = 0;//清空标志
					end = i;
				}
			}
			cout << "这个end=" << end << "这个f" << f << endl;
			if (f) {//要继续判断
				for (int i = 0; i <= e; i++) {
					if (flag[i] == 0) {
						if (i == 0) { end = sendpdu.win - 1; }
						else end = i - 1;
						cout << "CHECK_seq+=>" << sendbuf[i].seq << "=>flag:=>" << i << "==" << flag[i] << "---+" << "WRONG" << endl;
						break;
					}
					else {
						cout << "CHECK_seq+=>" << sendbuf[i].seq << "=>flag:=>" << i << "==" << flag[i] << "RIGHT" << endl;
						flag[i] = 0;//清空标志

						end = i;
					}
				}
			}
		}
	}
	//确认连续小边界，更新滑动窗口
	cout << "end=" << end;
	int next = sendbuf[end].seq + sendbuf[end].length + 1;
	update_winval(start_index, end_index, sendpdu.win, next);

}
int first = 0;
void threadsend(ifstream& fin, uint32_t& start_index, uint32_t& end_index, int& st) {
	while (!fin.eof()) {
		//接着读取文件
		cout << "===sendThrea_go_on===";
		std::unique_lock<std::mutex> lck(mts);
		cout << "滑动区间:[" << start_index << "," << end_index << "]" << endl;
		if (!((sendpdu.seq == start_index || sendpdu.seq > start_index) && (sendpdu.seq == end_index || sendpdu.seq < end_index))) {
			cout << sendpdu.seq << "未在发送区间" << start_index << "," << end_index << "sendThrea_wait" << endl;
			first = 1;
			cv.notify_one();
			cv.wait(lck);
			continue;
		}
		memset(sendpdu.buf, 0, sizeof(char) * nsize);
		fin.read(sendpdu.buf, nsize);//设置发送文件到sendbuf

		st++;//判断是否是第一次读取（文件头)
		cout << "st===" << st << endl;
		//设置开始结束标志
		if (st == 1) {
			cout << "开始标志设置" << endl;
			sendpdu.B_E = 0; sendpdu.seq = 0;
		}//第一次读取，设置开始标志和seq
		else if (!fin.eof()) {
			sendpdu.B_E = -1;

		}//不是第一次读取
		else if (fin.eof()) {
			cout << "设置结束标志" << endl;
			sendpdu.B_E = 1;
			sign = 1;
		}//最后一次读取，设置结束标志
		lastsendbufseq = sendpdu.seq;
		sendpdu.flags = ACK;//设置发送ACK
		sendpdu.length = fin.gcount();
		sendpdu.checksum = setchecksum(sendpdu);
		//始终存win个sendbuf{%win}
		sendbuf[(sendpdu.seq / (nsize + 1)) % sendpdu.win] = sendpdu;
		bufflag[(sendpdu.seq / (nsize + 1)) % sendpdu.win] = 1;
		cout << "保存seq：" << sendpdu.seq << "在[" << (sendpdu.seq / (nsize + 1)) % sendpdu.win << "]中" << endl;
		sendret = send(sendpdu);//发送
		if (sendret < 0) {
			cout << "send error" << GetLastError() << endl;
			return;
		}
		else {
			cout << "=======================================================send=======================================================" << endl;
			pdu_to_str(sendpdu);

		}
		if (!sign) {
			sendpdu.seq = sendpdu.seq + sendpdu.length + 1;//设置下一次将要发送的seq
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
	Sleep(1000);//先进行send
	int u = 0;
	while (1) {
		std::unique_lock<std::mutex> lck(mts);
		cout << "===recvThrea_go_on===" << endl;
		if (first == 0) {//第一次先将发送窗口都发完
			cv.notify_one();
			cv.wait(lck);
		}
		if (u == 0)
		{
			//处理多包
			timeout_retry(sendbuf[(lastsendbufseq / (nsize + 1)) % sendpdu.win]);
		}		//判断是否超时重发
		else 
			timeout_retry_for(sendbuf[(lastsendbufseq / (nsize + 1)) % sendpdu.win],start_index,end_index);

		if ((recvpdu.ack - recvpdu.seq == 1 && recvpdu.flags == ACK_SYN)) {
			cout << "处理多包" << endl;
			continue;
		}//处理建立连接多出来的包
		u = 1;

		//收到srv的ACK进行检查
		if (recvpdu.flags == ACK_FIN) {
			cout << "接收端接收完毕，不必继续等待ACK确认包" << endl;
			return;
		}
		//最后一个包已经发送完成，判断是否得到窗口发送的全部的ACK包
		if (sendbuf[(lastsendbufseq / (nsize + 1)) % sendpdu.win].B_E == 1) {
			end_index = lastsendbufseq;
			//&& 
			//((((lastsendbufseq + sendpdu.length + 1) == recvpdu.ack) && recvpdu.flags == ACK))||flag[(lastsendbufseq / (nsize + 1)) % sendpdu.win]){
			int index = ((recvpdu.ack - sendpdu.length - 1) / (nsize + 1)) % sendpdu.win;
			if (((lastsendbufseq + sendpdu.length + 1) == recvpdu.ack) && recvpdu.flags == ACK)
				flag[index] = 1;//确认ACK
			bool finish = true;
			for (int seq = start_index; seq <= lastsendbufseq; seq += nsize + 1) {
				if (flag[seq / (nsize + 1) % sendpdu.win] == 0) {
					cout << "===========================BEFORE_ACK_NOT_RECV_YET_resend_PDU:=======================" << endl;
					send(sendbuf[seq / (nsize + 1) % sendpdu.win]);
					pdu_to_str(sendbuf[seq / (nsize + 1) % sendpdu.win]);
					finish = false;

				}
			}
			if (finish)
			{
				cout << "最后一个数据包的ACK已经接收，结束传输过程===recv_thread_return" << endl;
				return;
			}
		}
		//如果接收到窗口下界的ack且ACK，判断连续，更新窗口，并转向发送线程
		else if ((recvpdu.ack == start_index + sendpdu.length + 1) && recvpdu.flags == ACK) {

			int index = ((recvpdu.ack - sendpdu.length - 1) / (nsize + 1)) % sendpdu.win;
			flag[index] = 1;//确认ACK
			cout << "=========RECV_LOWER_BOUND============seq=>" << recvpdu.ack - sendpdu.length - 1 << "=>" << index << endl;
			check_ACK_update(index, start_index, end_index, sendpdu.win);
			cv.notify_one();//及时发送滑动窗口内
			cv.wait(lck);
			continue;
		}
		//收到窗口上界ACK但还有部分没有确认
		else if ((recvpdu.ack == end_index + sendpdu.length + 1) && recvpdu.flags == ACK) {
			int index = ((recvpdu.ack - sendpdu.length - 1) / (nsize + 1)) % sendpdu.win;
			flag[index] = 1;
			cout << "=========RECV_HIGHER_BOUND============seq=>" << recvpdu.ack - sendpdu.length - 1 << "=>" << index << endl;
			int start = (start_index / (nsize + 1)) % sendpdu.win;
			int end = (end_index / (nsize + 1)) % sendpdu.win;
			if (start <= end)
				for (int i = start; i <= end; i++) {
					if (flag[i] == 0) {
						cout << "=========before_ACK_not_recv_yet_resend_PDU============seq[" << i << "]=>" << sendbuf[i].seq << "=+>" << flag[i] << endl;
						send(sendbuf[i]);
						pdu_to_str(sendbuf[i]);
					}
					else {
						cout << "ACK_seq[" << i << "]=>" << sendbuf[i].seq << endl;
					}
				}
			else {
				for (int i = start; i < sendpdu.win; i++) {
					if (flag[i] == 0) {
						cout << "=========before_ACK_not_recv_yet_resend_PDU============seq[" << i << "]=>" << sendbuf[i].seq << "=+>" << flag[i] << endl;
						send(sendbuf[i]);
						pdu_to_str(sendbuf[i]);
					}
					else {
						cout << "ACK_seq[" << i << "]=>" << sendbuf[i].seq << "=+>" << flag[i] << endl;
					}
				}
				for (int i = 0; i <= end; i++) {
					if (flag[i] == 0) {
						cout << "=========before_ACK_not_recv_yet_resend_PDU===========seq[" << i << "]=>" << sendbuf[i].seq << "=+>" << flag[i] << endl;
						send(sendbuf[i]);
						pdu_to_str(sendbuf[i]);
					}
					else {
						cout << "ACK_seq[" << i << "]=>" << sendbuf[i].seq << endl;
					}
				}
			}


		}
		//接收到窗口范围内乱序ACK，标记确认接收
		else if (((recvpdu.ack > start_index + sendpdu.length + 1)
			&& (recvpdu.ack < end_index + sendpdu.length + 1 || recvpdu.ack == end_index + sendpdu.length + 1)
			&& recvpdu.flags == ACK)) {

			int index = ((recvpdu.ack - sendpdu.length - 1) / (nsize + 1)) % sendpdu.win;
			if (flag[index] == 1) { continue; }//重复的ACK
			flag[index] = 1;
			cout << "接收到范围内的乱序ACK:对应seq=>" << recvpdu.ack - sendpdu.length - 1 << "=>" << index << endl;
			continue;
		}

		//发送的判断条件只有是否在滑动窗口区间，在此区间就不停发送
		//重发的条件只有在接收到不在滑动窗口区间，接收判断超时
		//接收和send两个线程，但是有共享的数据区间判断和线程阻塞切换
	}
}
int main() {
	//C:\\Users\\Ha ha\\vssource\\repos\\udp23.1\\3.jpg
	//路由端口
	cout << "Please input the router port：" << endl;
	cin >> port;
	cin.ignore(1024, '\n');
	//绑定端口
	cout << "please input the bind port：" << endl;
	cin >> cport;
	cin.ignore(1024, '\n');
	//发送窗口大小
	cout << "please input the sendwinsize：" << endl;
	cin >> sendpdu.win;
	cin.ignore(1024, '\n');
	//缓存发送的数据包
	sendbuf = new PDU[sendpdu.win];
	//判断当前位置是否为空
	bufflag = new int[sendpdu.win];
	memset(bufflag, 0, sizeof(int) * sendpdu.win);
	//判断是否已经收到ACK
	flag = new int[sendpdu.win];
	memset(flag, 0, sizeof(int) * sendpdu.win);
	//窗口发送的数据报区间[s,e]B
	static uint32_t start_index = 0;
	static uint32_t end_index = start_index + (sendpdu.win - 1) * (nsize + 1);

	//计时开始
	clock_t start = clock();
	if (!socket_init()) { cout << "init failed!!" << endl; return 1; }
	//establish_conn
	if (!establish_conn()) { cout << "connect failed!!" << endl; return 1; }

	cout << "conneted!!!!!!!!!!!!" << endl;

	//输入文件
	cout << "please input the full file path:" << endl;
	getline(cin, sourcefilename);
	cout << "will open " << sourcefilename << endl;

	ifstream fin(sourcefilename.c_str(), ios::binary);//输入文件
	if (!fin) {
		cout << "open" << sourcefilename << "falied!!Please check the path" << endl;
		exit(1);
	}
	fin.seekg(0, ios::end);
	unsigned long long totalsize = fin.tellg();//文件长度
	fin.seekg(0, ios::beg);//文件起始
	int st = 0;
	memset(recvpdu.buf, 0, nsize);


	//定向输出到文件
	//ofstream logout("sender_log.txt");
	//streambuf* p0ld = cout.rdbuf(logout.rdbuf());


	thread t(threadsend, ref(fin), ref(start_index), ref(end_index), ref(st));
	thread t1(threadrecv, ref(start_index), ref(end_index));
	t.join();
	t1.join();

	cout << "send finished,going to close" << endl;
	cout << "total send file size:" << totalsize << " Bytes" << endl;
	fin.close();//关闭文件
	sendpdu.length = 0;
	if (recvpdu.flags == ACK_FIN) {
		while (1)
		{
			if (
				close_conn_after()) {
				break;
			}
		};
	}
	else {
		close_conn();
	}
	clock_t end = clock();
	double sendtime = (double)(end - start) / CLOCKS_PER_SEC;
	cout << "sendtime:" << sendtime << " sec" << endl;
	cout << "Bytes/sec:" << totalsize / sendtime << " Bytes per sec" << endl;
	system("pause");
	return 0;
}