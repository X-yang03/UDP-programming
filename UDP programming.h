#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <iomanip>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include<WinSock2.h>
#include<Windows.h>
#include<WS2tcpip.h>
#include<time.h>
#include <chrono>
#include<list>
#include <conio.h>
#include <thread>
#include <filesystem>
#pragma comment(lib,"ws2_32.lib")

const std::string serverIP = "127.0.0.1";
const std::string clientIP = "127.0.0.1";

#define intro "Type 'send' to send files, 'recv' to recieve files\n"

#define serverPort 8888
#define clientPort 8000
#define routerPort 8088

#define Fin 0b0001   // Finish  ���ڶϿ�����
#define Syn 0b0010
#define Ack 0b0100
#define Fds 0b1000    // File Discriptor �����ļ���С������

#define MSS 4084 //�ײ���12�ֽڵĸ�����Ϣ,��˴����������4084�ֽ�

#define wait_time 100  //��ʱ�ȴ�100ms
#define MSL 2000

struct FileHead {
	std::string filename;
	int filelen;
};

struct fakeHead {   //α�ײ�
	u_long srcIP;
	u_long desIP;
	
	u_short protocol = 17; //Э���,UDPΪ17
	u_short length = 4096; //sizeof(Message)
};

struct Message {
	u_short srcPort;
	u_short desPort;
	u_short len;  //���ݳ���
	u_short flag = 0; //16λ��flag
	u_short seq = 0;
	u_short ack = 0;
	u_short check;   //У���
	char message[MSS] = { 0 };

	void set_srcPort(u_short src) {
		srcPort = src;
	}

	void set_desPort(u_short des) {
		desPort = des;
	}

	void set_len(u_short length) {
		len = length;
	}

	void set_seq(u_short s) {
		seq = s;
	}

	void set_ack(u_short a) {
		ack = a;
	}

	void set_SYN() { flag |= Syn; }

	void set_FIN(){ flag |= Fin; }

	void set_ACK(){ flag |= Ack; }

	void set_FDS() { flag |= Fds; }

	bool if_SYN(){ return flag & Syn; }

	bool if_FIN(){ return flag & Fin; }

	bool if_ACK(){ return flag & Ack; }

	bool if_FDS() { return flag & Fds; }

	void set_check(struct fakeHead *head) {
		check = 0;
		u_int sum = 0;
		for (int i = 0; i < sizeof(struct fakeHead) / 2; i++) { //16λ���ۼ�
			sum += ((u_short*)head)[i];
		}

		for (int i = 0; i < sizeof(struct Message) / 2; i++) { //16λ���ۼ�
			sum += ((u_short*)this)[i];
		}

		while (sum >> 16) {  //���Ͻ�λ
			sum = (sum & 0xffff) + (sum >> 16);
		}

		check = ~sum;
		//printf("seq = %d, ack = %d , set check: %d\n", seq,ack,check);
	}

	bool checkValid(struct fakeHead* head) {
		u_int sum = 0;

		for (int i = 0; i < sizeof(struct fakeHead) / 2; i++) { //16λ���ۼ�
			sum += ((u_short*)head)[i];
		}

		for (int i = 0; i < sizeof(struct Message) / 2; i++) { //16λ���ۼ�
			sum += ((u_short*)this)[i];
		}

		while (sum >> 16) {  //���Ͻ�λ
			sum = (sum & 0xffff) + (sum >> 16);
		}
		//printf("seq = %d, ack = %d , check valid: %d\n", seq, ack, sum);

		return sum == 0x0ffff;
	}

	void set_data(char* src) {
		memset(message, 0, MSS);
		memcpy(message, src, len);
	}

};

typedef struct Message msg;

class _Server {
private:
	
public:
	_Server();
	int start_server();
	int cnt_setup();
	int sendMsg(msg message);
	int cnt_dic();
	int sendFile(std::filesystem::directory_entry);
	int sendFiles();
};

class _Client {

public:
	_Client();
	int client_init();
	int recvfile();
	int cnt_accept(int);
	int dic_accept(int);
	int file_accept(int);
};

