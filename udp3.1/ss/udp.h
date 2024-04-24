#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
using namespace std;
#pragma pack(1)//按一字节对齐，不对齐
//每次传送的大小
#define nsize 6000
struct PDU {
	
	uint16_t source_port;//源端口
	uint16_t dest_port;//目的端口
	uint32_t ack = -1;//确认号
	uint32_t seq = 0;//序列号
	uint8_t flags = 0;//标志位 ACK SYN RST FIN
	int B_E = -1;//0开始，1结束
	size_t length = 0;//数据大小，保持文件大小不变
	uint16_t checksum = 0;//16位校验和
	char buf[nsize] = { 0 };
};

#define ACK 8
#define SYN 4
#define RST 2
#define FIN 1
#define ACK_SYN 12
#define ACK_FIN 9
void pdu_to_str(PDU pdu) {
	cout << "source port: " << pdu.source_port << "	dest_port: " << pdu.dest_port << endl << "ack: " << pdu.ack << "	seq: " << pdu.seq << "	length: "<<pdu.length << "	flags: ";
	if (pdu.flags == ACK) { cout << "ACK"; }
	else if (pdu.flags == SYN) { cout << "SYN"; }
	else if (pdu.flags == ACK_SYN)cout << "ACK SYN";
	else if (pdu.flags == ACK_FIN) cout << "ACK FIN";
	cout << endl << "checksum: " << pdu.checksum << endl;
}


int check_ack_seq(PDU sendpdu, PDU recvpdu) {
	if ((recvpdu.ack == (sendpdu.seq + strlen(sendpdu.buf) + 1)) && (recvpdu.flags == ACK)) { return 1; }
	else return 0;
}

uint32_t overadd(uint32_t tem) {
	while (tem & 0x10000) {
		tem++;
		tem = tem & 0xffff;
	}
	tem = tem & 0xffff;
	return tem;
}
uint32_t add(uint32_t a, uint32_t b) {
	return overadd(overadd(a & 0xffff + (a & 0xffff0000 >> 16)) + overadd(b & 0xffff + (b & 0xffff0000 >> 16)));
}
int checkchecksum(PDU &recvpdu) {
	uint32_t tem = add(add(add(add(add(recvpdu.source_port, recvpdu.dest_port), recvpdu.ack), recvpdu.seq), recvpdu.flags),recvpdu.length);
	int i = 0;
	for (int i = 0; i < 6000; i += 2) {
		uint16_t t = 0 | recvpdu.buf[i] | recvpdu.buf[i + 1] << 8;
		tem = add(tem, t);
	}
	while (i < 6000) {
		tem = add(tem, recvpdu.buf[i]);
		i++;
	}
	tem =tem+ recvpdu.checksum;
	if (tem == 0xffff) {return 1; }
	else { return 0; }
}