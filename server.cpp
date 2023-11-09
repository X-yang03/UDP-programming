  
#include "UDP programming.h"
#include <WinSock.h>
#include <IPHlpApi.h>

static std::fstream Server_log;
 
static SOCKET Server;
static struct fakeHead sendHead, recvHead;
static sockaddr_in client_addr;
static sockaddr_in server_addr;

static int addrlen;
static u_short NowSeq = 0;   //消息序列号
static u_short NowAck = 0;
static bool ifAck = 0;  //上一条消息是否被接收
 
static std::string currentPath = "./test/";

int test() {
	char t1[10] = { 0 };
	char t2[10] = { 0 };
	char t3[10] = { 0 };
	strcpy(t1, "test1");
	strcpy(t2, "test2");
	strcpy(t3, "test3");


	sendto(Server, t1, sizeof(t1), 0, (struct sockaddr*)&client_addr, addrlen);
	sendto(Server, t2, sizeof(t1), 0, (struct sockaddr*)&client_addr, addrlen);
	sendto(Server, t3, sizeof(t1), 0, (struct sockaddr*)&client_addr, addrlen);

	return 0;

}

DWORD WINAPI timeout_resend(LPVOID lpParam) {  //超时重传线程
	msg *message = (msg*)lpParam;
	int wait = 0;
	if (!message->if_SYN()) { 
		while (!ifAck) {
			while (wait < wait_time && !ifAck) {
				Sleep(1);
				wait += 1;
			}
			if (!ifAck) { //由于超时进入该条件判断
				printf("[Error] Timeout!   Resend!\n");
				Server_log << "[Error] Timeout!   Resend!" << std::endl;

				sendto(Server, (char*)message, sizeof(msg), 0, (struct sockaddr*)&client_addr, addrlen);
			}
		}
	}
	else {
		while (!ifAck) {
			while (wait < 5000 && !ifAck) {
				Sleep(1);
				wait += 1;
			}
			if (!ifAck) { //由于超时进入该条件判断
				printf("[Error] Timeout!   Re-Connect!\n");
				Server_log << "[Error] Timeout!   Re-Connect!" << std::endl;

				sendto(Server, (char*)message, sizeof(msg), 0, (struct sockaddr*)&client_addr, addrlen);
			}
		}
	}
	return 0;
}


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

	Server_log << "[Log] Server Socket Start Up!" << std::endl;


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
	Server_log << "[Log] Connecting to client.........." << std::endl;

	//test();
	cnt_setup();

	sendFiles();

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
	Server_log << "[SYN] Seq= " << first_shake.seq << "len = " << first_shake.len << std::endl;
	sendMsg(first_shake);

	printf("[Log] Connection Set Up！\n");
	Server_log << "[Log] Connection Set Up！" << std::endl;
	return 0;
}

int _Server::sendMsg(msg message) {
	sendto(Server, (char*)&message, sizeof(msg), 0, (struct sockaddr*)&client_addr, addrlen);
	//printf("check : %d\n", message.check);
	printf("[Log] SEND seq = %d,len = %d\n", message.seq, message.len);
	Server_log << "[Log] SEND seq = " << message.seq << ", len = " << message.len << std::endl;
	NowSeq += (message.len);
	int wait = 0;
	ifAck = 0;

	HANDLE handle = CreateThread(NULL, 0, timeout_resend, &message, 0, NULL);  //创建检测超时线程,用于超时重传
	CloseHandle(handle);

	while (true) {
		msg recv_msg;

		if (recvfrom(Server, (char*)&recv_msg, sizeof(msg), 0, (struct sockaddr*)&client_addr, &addrlen) == SOCKET_ERROR) {
			printf("[Error] Recieving Error , try to resend!\n");
			Server_log << "[Error] Recieving Error , try to resend!" << std::endl;
		}
		else {
			if (recv_msg.checkValid(&recvHead) && recv_msg.seq == NowAck) {
				printf("[Log] RECIEVE ack = %d , len = %d\n", recv_msg.ack, recv_msg.len);
				Server_log << "[Log] RECIEVE ack = " << recv_msg.ack << ", len = " << recv_msg.len << std::endl;
				ifAck = 1;
				NowAck = recv_msg.seq + recv_msg.len;
				printf("[Log] Seq %d send successfully!\n ", message.seq);
				Server_log << "[Log] Seq " << message.seq << " send successfully!" << std::endl;
				break;

			}
			else { //校验和出错
				std::cout << recv_msg.check;
				printf("[Error] Valid Error , try to resend111!\n");
				Server_log << "[Error] Valid Error , try to resend!" << std::endl;
			}
		}

	}

	return 0;
}

int _Server::sendFiles() {

	for (const auto& entry : std::filesystem::directory_iterator(currentPath)) {
		if (entry.is_regular_file()) {
			std::string filename = entry.path().filename().string();
			std::cout << "[Log] Send File: " << filename << std::endl;
			Server_log << "[Log] Send File: " << filename << std::endl;
			sendFile(entry);
		}
	}

	return 0;
}

int _Server::sendFile(std::filesystem::directory_entry entry) {
	std::string filename = entry.path().filename().string();

	int filelen = entry.file_size(); //文件大小
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
	Server_log << "[Log] Send file head" << std::endl;
	sendMsg(fileDescriptor);

	std::string path = currentPath + filename;

	std::ifstream input(path, std::ios::binary);

	int segments = ceil((float)filelen / MSS);

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
		Server_log << "[Log] Segment "<<i<<" of "<<segments<<" Sent Successfully" << std::endl;
	}

	printf("File %s Sent Successfully!\n", filename);
	return 0;
}

int _Server::cnt_dic() {

	msg wave;
	wave.set_srcPort(serverPort);
	wave.set_desPort(clientPort);
	wave.set_len(0);
	wave.set_seq(NowSeq);
	wave.set_FIN();
	wave.set_check(&sendHead);

	sendMsg(wave);

	printf("[Log] Connection destroyed!\n");
	Server_log << "[Log] Connection destroyed!" << std::endl;

	return 0;
}