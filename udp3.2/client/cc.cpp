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
//文件
string sourcefilename = "C:\\Users\\Ha ha\\vssource\\repos\\udp23.1\\3.jpg";
int lastsendbufseq;//记录最近一次的读入
int lastrecvack=0;//记录最大的正确ack
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
void close_conn() {
	sendpdu.flags = ACK_FIN;
	sendpdu.seq = 0;
	//超时重传
	sendret = send(sendpdu);
	cout << "======================send=========================" << endl;
	pdu_to_str(sendpdu);
	cout << "FIRST BYE，go state: FIN_WAIT1" << endl;
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
	Sleep(10000);//等待10秒

	closesocket(Socket);


}
//更新发送区间
void update_winval(uint32_t&start_index, uint32_t&end_index,int winsize, uint32_t nextsendseq) {
	
	cout << "update_window_from: [" << start_index << "," << end_index << "] to";
	start_index = nextsendseq;
	end_index = start_index + ((sendpdu.win - 1) * (nsize + 1));
	cout << "[" << start_index << "," << end_index << "]" << endl;
	
}
void threadsend(ifstream &fin, uint32_t& start_index, uint32_t& end_index,int &st) {
	while (!fin.eof()) {
		//接着读取文件
		cout << "===sendThrea_go_on===";
		std::unique_lock<std::mutex> lck(mts);
		cout << "滑动区间:[" << start_index << "," << end_index <<"]"<<endl;
		if (!((sendpdu.seq == start_index || sendpdu.seq > start_index) &&( sendpdu.seq == end_index||sendpdu.seq<end_index))) { 
			cout <<sendpdu.seq<< "未在发送区间" << start_index<<","<<end_index <<"sendThrea_wait" << endl;
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
		else if(!fin.eof()) { 
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
		cout << "保存seq：" <<sendpdu.seq<<"在[" << (sendpdu.seq / (nsize + 1))%sendpdu.win<<"]中"<< endl;
		sendret = send(sendpdu);//发送
		if (sendret < 0) {
			cout << "send error" << GetLastError() << endl;
			return ;
		}
		else {
			cout << "=======================================================send=======================================================" << endl;
			pdu_to_str(sendpdu);
			
		}
		if(!sign){
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
	Sleep(1);//先进行send

	while (1) {
		std::unique_lock<std::mutex> lck(mts);
		cout << "===recvThrea_go_on===" << endl;
		//判断是否超时重发
		timeout_retry(sendbuf[(lastsendbufseq / (nsize + 1)) % sendpdu.win]);
		
		if ((recvpdu.ack - recvpdu.seq == 1 && recvpdu.flags == ACK_SYN)) { 
			continue;
		}//处理建立连接多出来的包

		
		//收到srv的ACK进行检查
		
		//最后一个包已经发送完成，且得到最后的ACK包
		if (sendbuf[(lastsendbufseq / (nsize + 1)) % sendpdu.win].B_E == 1 && ((lastsendbufseq + sendpdu.length + 1) == recvpdu.ack) && recvpdu.flags == ACK) {
			cout << "最后一个数据包的ACK已经接收，结束传输过程===recv_thread_return" << endl;
			return;
		}
		//如果接收到正确范围内的ack且ACK，且不是重复的ack就更新滑动窗口，转向发送线程
		if ((recvpdu.ack == start_index + sendpdu.length + 1 || recvpdu.ack > start_index + sendpdu.length + 1)
			&& (recvpdu.ack < end_index + sendpdu.length + 1 || recvpdu.ack == end_index + sendpdu.length + 1)
			&& recvpdu.flags == ACK) {
			if (lastrecvack < recvpdu.ack) {
				lastrecvack = recvpdu.ack;
				update_winval(start_index, end_index, sendpdu.win, recvpdu.ack);//更新滑动窗口
				if (!sign) {
					cv.notify_one();//可能此时网络通畅，就及时发送
					cv.wait(lck);
				}
				continue;
			}
			else {
				cout << "丢弃晚到的较小ack:" << recvpdu.ack << "<=" << lastrecvack << endl;//将晚到的较小的ack丢弃
				continue;
			}
		}
		//接收到ack_loss的包或者滑动窗口还有可以发送的数据包
		else {
			while (!
				((recvpdu.ack == start_index + sendpdu.length + 1 || recvpdu.ack > start_index + sendpdu.length + 1)
					&& (recvpdu.ack < end_index + sendpdu.length + 1 || recvpdu.ack == end_index + sendpdu.length + 1)
					&& recvpdu.flags == ACK)) {
				cout << "ack check failed,may loss ,will resend" << endl;
				//在recv前滑动窗口存在未发送的数据包
				if (bufflag[((recvpdu.ack) / (nsize + 1)) % sendpdu.win] == 0 || (sendbuf[((recvpdu.ack) / (nsize + 1)) % sendpdu.win].seq != recvpdu.ack)) {
					break;
				}
				//在此seq后写入的send无效，记录send的最后一次，在此区间的数据包再发送一遍
				int shouldsendindex = ((recvpdu.ack) / (nsize + 1)) % sendpdu.win;
				cout << "ACK_LOSS,so重发错误号==>" << recvpdu.ack << "============" << ((recvpdu.ack) / (nsize + 1)) % sendpdu.win << "======" << lastsendbufseq << "====" << (lastsendbufseq / (nsize + 1)) % sendpdu.win  << endl;
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
				//超时重发最近发送的数据包
				timeout_retry(sendbuf[shouldsendindex]);
			}
			//可能滑动窗口还没全部send完，就转向send线程
			if (sendbuf[((recvpdu.ack) / (nsize + 1)) % (sendpdu.win)].seq != recvpdu.ack || bufflag[((recvpdu.ack) / (nsize + 1)) % sendpdu.win] == 0) {
				cout << "滑动窗口中存在未发送完的数据包:" << recvpdu.ack << "=>" << sendbuf[((recvpdu.ack) / (nsize + 1)) % (sendpdu.win)].seq << " in [" << start_index << "," << end_index << "]" << endl;
				if (!sign) {
					cv.notify_one();
					cv.wait(lck);
				}
				continue;
			}
			//更新滑动窗口，继续接收
			else {
				if (lastrecvack < recvpdu.ack) {
					lastrecvack = recvpdu.ack;
					update_winval(start_index, end_index, sendpdu.win, recvpdu.ack);//更新滑动窗口
					continue;
				}
				else {
					cout << "丢弃较小ACK" << recvpdu.ack << "<=" << lastrecvack << endl;
					continue;
				}
			}
			cout << "recvThread_exit" << endl;
			continue;
		}
		//发送的判断条件只有是否在滑动窗口区间，在此区间就不停发送
		//重发的条件只有在接收到不在滑动窗口区间，接收判断超时
		//接收和send两个线程，但是有共享的数据区间判断和线程阻塞切换
		return;
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
	//窗口发送的数据报区间[s,e]B
	static uint32_t start_index=0;
	static uint32_t end_index=start_index+(sendpdu.win-1)*(nsize+1);
	
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
	

	thread t(threadsend,ref(fin),ref(start_index),ref(end_index),ref(st));
	thread t1(threadrecv, ref(start_index), ref(end_index));
	t.join();
	t1.join();

	cout << "send finished,going to close" << endl;
	cout << "total send file size:" << totalsize<<" Bytes" << endl;
	fin.close();//关闭文件
	sendpdu.length = 0;
	close_conn();
	clock_t end = clock();
	double sendtime = (double)(end - start) / CLOCKS_PER_SEC;
	cout << "sendtime:" << sendtime << " sec" << endl;
	cout << "Bytes/sec:" << totalsize / sendtime << " Bytes per sec" << endl;
	system("pause");
	return 0;
}