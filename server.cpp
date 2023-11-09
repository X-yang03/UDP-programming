  
#include "UDP programming.h"
#include <WinSock.h>
#include <IPHlpApi.h>

static std::fstream Server_log;
 
static SOCKET Server;
static struct fakeHead sendHead, recvHead;
static sockaddr_in client_addr;
static sockaddr_in server_addr;
static SYSTEMTIME sysTime = { 0 };
static int addrlen;
static u_short NowSeq = 0;   //消息序列号
static u_short NowAck = 0;
bool ifAck = 0;  //上一条消息是否被接收
double total_time = 0;
double total_size = 0;
 
static std::string currentPath = "./test/";

//DWORD WINAPI timeout_resend(LPVOID lpParam) {  //超时重传线程
//	msg *message = (msg*)lpParam;
//	if (!message->if_SYN()) { 
//		while (!ifAck) {
//			int wait = 0;
//			while (wait < wait_time && !ifAck) {
//				Sleep(1);
//				wait += 1;
//			}
//			if (!ifAck) { //由于超时进入该条件判断
//				printf("[Error] Timeout!   Resend!\n");
//				GetSystemTime(&sysTime);
//				Server_log << "[Error] Timeout!   Resend!" << "  "<<wait<<" " << sysTime.wHour + 8 << ":" << sysTime.wMinute << ":" << sysTime.wSecond << ":" << sysTime.wMilliseconds << std::endl;
//
//				sendto(Server, (char*)message, sizeof(msg), 0, (struct sockaddr*)&client_addr, addrlen);
//				
//			}
//			if (ifAck) {
//				return 0;
//			}
//		}
//	}
//	else {
//		while (!ifAck) {
//			int wait = 0;
//			while (wait < 5000 && !ifAck) {
//				Sleep(1);
//				wait += 1;
//			}
//			if (!ifAck) { //由于超时进入该条件判断
//				
//				printf("[Error] Timeout!   Re-Connect!\n");
//				GetSystemTime(&sysTime);
//				Server_log << "[Error] Timeout!   Re-Connect!" << "  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;
//
//				sendto(Server, (char*)message, sizeof(msg), 0, (struct sockaddr*)&client_addr, addrlen);
//			}
//		}
//	}
//	return 0;
//}


_Server::_Server() {
	
}

int _Server::start_server() {
	Server_log.open("server_log.txt", std::ios::out);
	WSADATA wsaData;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("[Error] Socket Error: %s (errno: %d)\n", strerror(errno), errno);
		return 1;
	}

	if ((Server = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
		printf("[Error] Socket Error: %s (errno: %d)\n", strerror(errno), errno);
		return 1;
	}
	GetSystemTime(&sysTime);
	Server_log << "[Log] Server Socket Start Up!" << "  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;

	// 初始化地址和伪首部
	server_addr.sin_family = AF_INET;       //IPV4
	server_addr.sin_port = htons(serverPort);     //PORT:8888,htons将主机小端字节转换为网络的大端字节
	inet_pton(AF_INET, serverIP.c_str(), &server_addr.sin_addr.S_un.S_addr);

	client_addr.sin_family = AF_INET;       //IPV4
	client_addr.sin_port = htons(routerPort);     
	inet_pton(AF_INET, clientIP.c_str(), &client_addr.sin_addr.S_un.S_addr);
	
	sendHead.srcIP = server_addr.sin_addr.S_un.S_addr;
	sendHead.desIP = client_addr.sin_addr.S_un.S_addr; //伪首部初始化

	recvHead.srcIP = client_addr.sin_addr.S_un.S_addr;
	recvHead.desIP = server_addr.sin_addr.S_un.S_addr;

	addrlen = sizeof(client_addr);
	
	if (bind(Server, (LPSOCKADDR)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) { //将Server与server_addr绑定
		printf("bind Error: %s (errno: %d)\n", strerror(errno), errno);
		return 1;
	}

	printf("[Log] Connecting to client..........\n");
	Server_log << "[Log] Connecting to client.........." << "  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;

	cnt_setup();  //建立握手

	sendFiles();
	printf("Successfully sent all the files, Time used: %f s, Throughput : %f Bytes/s \n", total_time,total_size/total_time);
	Server_log << "Successfully sent all the files, time used: " << total_time << " s, Throughput : "<< total_size / total_time<<" Bytes/s" << std::endl;

	cnt_dic();

	closesocket(Server);
	WSACleanup();

}

int _Server::cnt_setup() {
	msg first_shake;
	first_shake.set_srcPort(serverPort);
	first_shake.set_desPort(clientPort);
	first_shake.set_len(0);
	first_shake.set_seq(NowSeq);
	first_shake.set_SYN();   //第一次握手,仅发出一条SYN消息
	first_shake.set_check(&sendHead);

	printf("[SYN] Seq=%d  len=%d\n", first_shake.seq, first_shake.len);
	GetSystemTime(&sysTime);
	Server_log << "[SYN] Seq= " << first_shake.seq << "\tlen = " << first_shake.len << "\t  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;
	sendMsg(first_shake);

	printf("[Log] Connection Set Up！\n");
	GetSystemTime(&sysTime);
	Server_log << "[Log] Connection Set Up！" << "  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;
	return 0;
}

int _Server::sendMsg(msg message) {  //发送单条数据
	sendto(Server, (char*)&message, sizeof(msg), 0, (struct sockaddr*)&client_addr, addrlen);
	//printf("check : %d\n", message.check);
	NowSeq += (message.len);  //更新Seq
	printf("[Log] SEND seq = %d,len = %d, NowSeq = %d\n", message.seq, message.len, NowSeq);
	GetSystemTime(&sysTime);
	Server_log << "[Log] SEND \tseq =" << message.seq << "\t, len =" << message.len << "\t , NowSeq =" << NowSeq << "\t  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;
	
	ifAck = 0;

	std::thread timeout_resend([&]() {  //超时重传
		if (!message.if_SYN()) {  //如果是握手超时,则每3s重新连接
			while (!ifAck) {
				int wait = 0;
				while (wait < wait_time && !ifAck) {
					Sleep(1);
					wait += 1;
				}
				if (!ifAck) { //由于超时进入该条件判断
					printf("[Error] Timeout!   Resend!\n");
					GetSystemTime(&sysTime);
					Server_log << "[Error] Timeout!   Resend!" << "  " << wait << " " << sysTime.wHour + 8 << ":" << sysTime.wMinute << ":" << sysTime.wSecond << ":" << sysTime.wMilliseconds << std::endl;

					sendto(Server, (char*)&message, sizeof(msg), 0, (struct sockaddr*)&client_addr, addrlen);

				}
			}
		}
		else {
			while (!ifAck) {
				int wait = 0;
				while (wait < 3000 && !ifAck) {
					Sleep(1);
					wait += 1;
				}
				if (!ifAck) { //由于超时进入该条件判断
					printf("[Error] Timeout!   Re-Connect!\n");
					GetSystemTime(&sysTime);
					Server_log << "[Error] Timeout!   Re-Connect!" << "  " << wait << " " << sysTime.wHour + 8 << ":" << sysTime.wMinute << ":" << sysTime.wSecond << ":" << sysTime.wMilliseconds << std::endl;

					sendto(Server, (char*)&message, sizeof(msg), 0, (struct sockaddr*)&client_addr, addrlen);

				}
			}
		}
		});
	//HANDLE handle = CreateThread(NULL, 0, timeout_resend, &message, 0, NULL);  //创建检测超时线程,用于超时重传
	//CloseHandle(handle);

	while (true) {
		msg recv_msg;

		if (recvfrom(Server, (char*)&recv_msg, sizeof(msg), 0, (struct sockaddr*)&client_addr, &addrlen) == SOCKET_ERROR) {
			printf("[Error] Recieving Error , try to resend!\n");
			Server_log << "[Error] Recieving Error , try to resend!" << "  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;
		}
		else { //成功接收到回复
			printf("[Log] RECIEVE ack = %d , len = %d\n", recv_msg.ack, recv_msg.len);
			GetSystemTime(&sysTime);
			Server_log << "[Log] RECIEVE\t ack = " << recv_msg.ack << "\t, len = " << recv_msg.len << "\t  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;
			if (recv_msg.checkValid(&recvHead) && recv_msg.ack == NowSeq) {  //校验和正确,且Seq与ack对应
				ifAck = 1;
				timeout_resend.join();
				NowAck = recv_msg.seq + recv_msg.len; 
				printf("[Log] Seq %d send successfully!\n", message.seq);
				GetSystemTime(&sysTime);
				Server_log << "[Log] Seq" << message.seq << " send successfully!" << " \t "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;
				break;

			}
			else { 
				int errorType = 0;
				if (recv_msg.checkValid(&recvHead)) {
					errorType = 1; // ack != NowSeq
				}
				else {
					errorType = 2; // 校验和出错
				}
				printf("[Error] Valid Error %d , try to resend!\n",errorType);
				GetSystemTime(&sysTime);
				Server_log << "[Error] Valid Error "<<errorType<<" , try to resend!" << "  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;
			}
		}

	}

	return 0;
}

int _Server::sendFiles() {

	for (const auto& entry : std::filesystem::directory_iterator(currentPath)) {  //获取所有文件entry
		
			std::string filename = entry.path().filename().string();
			std::cout << "[Log] Send File: " << filename << "  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;
			GetSystemTime(&sysTime);
			Server_log << "[Log] Send File: " << filename << "  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;
			auto begin = std::chrono::steady_clock::now();
			sendFile(entry);
			auto after = std::chrono::steady_clock::now();
			total_time += std::chrono::duration<double>(after - begin).count();
		
	}

	return 0;
}

int _Server::sendFile(std::filesystem::directory_entry entry) {
	std::string filename = entry.path().filename().string();

	int filelen = entry.file_size(); //文件大小
	total_size += filelen;
	struct FileHead descriptor{
		filename,
		filelen
	};

	msg fileDescriptor;

	fileDescriptor.set_srcPort(serverPort);
	fileDescriptor.set_desPort(clientPort);
	fileDescriptor.set_len(sizeof(struct FileHead));
	fileDescriptor.set_ACK();
	fileDescriptor.set_FDS(); //FileDescriptor,表示这是一条描述性消息
	fileDescriptor.set_seq(NowSeq);
	fileDescriptor.set_ack(NowAck);
	fileDescriptor.set_data((char*)&descriptor);
	fileDescriptor.set_check(&sendHead);
	printf("send head\n");
	GetSystemTime(&sysTime);
	Server_log << "[Log] Send file head" << "  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;
	sendMsg(fileDescriptor);

	std::string path = currentPath + filename;

	std::ifstream input(path, std::ios::binary);

	int segments = ceil((float)filelen / MSS);  //分为多个节发送

	for (int i = 0; i < segments; i++) {
		char buffer[MSS];
		int send_len = (i == segments - 1) ? filelen % MSS : MSS;
		input.read(buffer,send_len);
		msg file_msg;

		file_msg.set_srcPort(serverPort) ;
		file_msg.set_desPort(clientPort);
		file_msg.set_len(send_len);
		file_msg.set_ACK();
		file_msg.set_seq(NowSeq);
		file_msg.set_ack(NowAck);
		file_msg.set_data(buffer);
		file_msg.set_check(&sendHead);

		sendMsg(file_msg);
		printf("[Log] Segment %d of %d Sent Successfully\n",i,segments);
		GetSystemTime(&sysTime);
		Server_log << "[Log] Segment\t "<<i<<" of "<<segments<<"\t Sent Successfully" << "\t  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;
	}

	printf("File %s Sent Successfully!\n", filename.c_str());
	Server_log << "[Log] File " << filename << "Sent Successfully!" << std::endl;
	return 0;
}

int _Server::cnt_dic() {

	msg wave;
	wave.set_srcPort(serverPort);
	wave.set_desPort(clientPort);
	wave.set_len(0);
	wave.set_seq(NowSeq);
	wave.set_FIN();  //设置Fin位
	wave.set_check(&sendHead);

	sendMsg(wave);

	printf("[Log] Connection destroyed!\n");
	GetSystemTime(&sysTime);
	Server_log << "[Log] Connection destroyed!" << "  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;

	return 0;
}