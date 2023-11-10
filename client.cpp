#include"UDP programming.h"

std::fstream Client_log;

static SOCKET Client;
static struct fakeHead sendHead, recvHead;
static sockaddr_in client_addr;
static sockaddr_in server_addr;
static SYSTEMTIME sysTime = { 0 };
static int addrlen;
static u_short NowSeq = 0;
static u_short NowAck = 0;

std::string savePath = "./save/";


_Client::_Client() {

}

int _Client::client_init() {
	Client_log.open("client_log.txt", std::ios::out|std::ios::ate);
	WSADATA wsaData;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("socket Error: %s (errno: %d)\n", strerror(errno), errno);
		return 1;
	}

	if ((Client = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
		printf("socket Error: %s (errno: %d)\n", strerror(errno), errno);
		return 1;
	}
	GetSystemTime(&sysTime);
	Client_log << "Client Socket Start Up!" << "  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;

	//初始化addr与伪首部
	server_addr.sin_family = AF_INET;       //IPV4
	server_addr.sin_port = htons(routerPort);     //PORT:8888,htons将主机小端字节转换为网络的大端字节
	inet_pton(AF_INET, serverIP.c_str(), &server_addr.sin_addr.S_un.S_addr);

	client_addr.sin_family = AF_INET;       //IPV4
	client_addr.sin_port = htons(clientPort);
	inet_pton(AF_INET, clientIP.c_str(), &client_addr.sin_addr.S_un.S_addr);

	sendHead.desIP = server_addr.sin_addr.S_un.S_addr;
	sendHead.srcIP = client_addr.sin_addr.S_un.S_addr;

	recvHead.desIP = client_addr.sin_addr.S_un.S_addr;
	recvHead.srcIP = server_addr.sin_addr.S_un.S_addr;

	addrlen = sizeof(client_addr);

	if (bind(Client, (LPSOCKADDR)&client_addr, addrlen) == SOCKET_ERROR) { //将Client与client_addr绑定
		printf("bind Error: %s (errno: %d)\n", strerror(errno), errno);
		return 1;
	}

	printf("Waiting for Connection....\n");
	Client_log << "Waiting for Connection...." << "  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;

	recvfile();
	WSACleanup();
	system("pause");
	return 0;
}

int _Client::recvfile() {
	msg recvMsg;

	std::fstream file;
	std::string filename;
	struct FileHead descriptor;
	int filelen = 0;
	int lenPointer = 0;

	while (true) {
		if (recvfrom(Client, (char*)&recvMsg, sizeof(msg), 0, (struct sockaddr*)&client_addr, &addrlen) == SOCKET_ERROR) {
			printf("[Log] Recieving Error , try again!\n");
			GetSystemTime(&sysTime);
			Client_log << "[log] Recieving Error , try again!" << "  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;
		}
		else {
			printf("[Log] RECIEVE seq = %d , ack = %d, len = %d ,check = %d ,NowAck = %d\n",recvMsg.seq,recvMsg.ack,recvMsg.len,recvMsg.check,NowAck);
			GetSystemTime(&sysTime);
			Client_log << "[Log] RECIEVE\t seq = " << recvMsg.seq << "\t, ack = " << recvMsg.ack << "\t, len = " << recvMsg.len <<"\t, check = "<<recvMsg.check << "\t, NowAck = " << NowAck << "\t  " << sysTime.wHour + 8 << ":" << sysTime.wMinute << ":" << sysTime.wSecond << ":" << sysTime.wMilliseconds << std::endl;
			if (recvMsg.checkValid(&recvHead) && recvMsg.seq == NowAck) { // 校验和正确 并且Seq与Ack对应

				if (recvMsg.if_SYN()) {  //握手建立连接
					printf("[SYN] First_Hand_Shake\n");
					Client_log << "[SYN] First_Hand_Shake" << "  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;
					cnt_accept(recvMsg.seq+recvMsg.len);
				}
				else if (recvMsg.if_FIN()) { //挥手断开连接
					printf("[FIN] First_Wave\n");
					Client_log << "[FIN] First_Wave" << "  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;
					dic_accept(recvMsg.seq + recvMsg.len);
					return 0;
				}
				else {

				if (recvMsg.if_FDS()) {  //文件头消息
					memset(&descriptor, 0, sizeof(struct FileHead));
					memcpy(&descriptor, recvMsg.message, sizeof(struct FileHead));
					printf("[FDS] Start recieving file %s \n", descriptor.filename.c_str());
					Client_log << "[FDS] Start recieving file " << descriptor.filename << "\t "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;
					filelen = descriptor.filelen;
					filename =savePath + descriptor.filename;
					lenPointer = 0;
					file.open(filename, std::ios::out | std::ios::binary);
					//file.close();

				}
				else {
					printf("[Log] Recieving File content of %d size\n", recvMsg.len);
					file.write(recvMsg.message, recvMsg.len); //文件消息,将数据写入file
					lenPointer += recvMsg.len;
					if (lenPointer >= filelen) {
						printf("[Log] File %s recieved successfully!\n", filename.c_str());
						Client_log << "[Log] File " << filename << " recieved successfully!" << "  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;
						file.close();
						}
					}

				file_accept(recvMsg.seq + recvMsg.len);  //总是以Seq+len来更新ACK,从而达到类似rdt3.0的确认接收功能
				}

				
			}
			else {
				if (recvMsg.if_SYN()) {
					cnt_accept(NowAck);
				}
				else {
					file_accept(NowAck);
				}
				printf("[Error] Disorder OR Data Fault\n");
				Client_log << "[Error] Disorder OR Data Fault" << "  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;
			}

		}
	}

}

int _Client::cnt_accept(int ack) { // ACK + SYN
	NowAck = ack;
	msg second_shake;
	second_shake.set_srcPort(clientPort);
	second_shake.set_desPort(serverPort);
	second_shake.set_len(0);
	second_shake.set_ACK();
	second_shake.set_SYN(); // SYN+ACK 表示同意连接
	second_shake.set_ack(ack);
	second_shake.set_check(&sendHead);
	sendto(Client, (char*)&second_shake, sizeof(msg), 0, (struct sockaddr*)&server_addr, addrlen);
	printf("[Log] SEND ack = %d , len = %d\n", second_shake.ack, second_shake.len);
	GetSystemTime(&sysTime);
	Client_log << "[Log] SEND ack = " << second_shake.ack << ", len = " << second_shake.len << "  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;
	printf("[Log] Connection set up!\n");
	GetSystemTime(&sysTime);
	Client_log << "[Log] Connection set up!" << "  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;

	return 0;
}

int _Client::dic_accept(int ack) { // ACK + FIN
	NowAck = ack;
	msg dic;
	dic.set_srcPort(clientPort);
	dic.set_desPort(serverPort);
	dic.set_len(0);
	dic.set_ACK();
	dic.set_FIN();
	dic.set_ack(ack);
	dic.set_check(&sendHead);
	sendto(Client, (char*)&dic, sizeof(msg), 0, (struct sockaddr*)&server_addr, addrlen);
	std::thread wait2MSL([&]() {  //等待2MSL的时间
		msg recvMsg;
		while (true) {
			int valid = recvfrom(Client, (char*)&recvMsg, sizeof(msg), 0, (struct sockaddr*)&client_addr, &addrlen);
			if (valid == SOCKET_ERROR) {
				break;
			}
			else {
				if (recvMsg.if_FIN()) {
					sendto(Client, (char*)&dic, sizeof(msg), 0, (struct sockaddr*)&server_addr, addrlen);
				}
			}
		}

	});
	printf("[Log] Wait for 2 MSL...\n");
	Client_log << "[Log] Wait for 2 MSL..." << std::endl;
	Sleep(2 * MSL);
	closesocket(Client);  //等待2MSL时间后，若无连接则关闭Socket，使thread退出

	wait2MSL.join();
	printf("[Log] SEND ack = %d , len = %d\n", dic.ack, dic.len);
	GetSystemTime(&sysTime);
	Client_log << "[Log] SEND ack = " << dic.ack << ", len = " << dic.len << "  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;
	printf("[Log] Connection Killed!\n");
	GetSystemTime(&sysTime);
	Client_log << "[Log] Connection Killed!" << "  "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;
	return 0;
}

int _Client::file_accept(int ack) { // ACK
	NowAck = ack;
	msg msg_recv;
	msg_recv.set_srcPort(clientPort);
	msg_recv.set_desPort(serverPort);
	msg_recv.set_len(0);
	msg_recv.set_ACK();
	msg_recv.set_ack(ack);
	msg_recv.set_check(&sendHead);
	sendto(Client, (char*)&msg_recv, sizeof(msg), 0, (struct sockaddr*)&server_addr, addrlen);
	printf("[Log] SEND ack = %d , len = %d\n", msg_recv.ack, msg_recv.len);
	GetSystemTime(&sysTime);
	Client_log << "[Log] SEND\t ack = " << msg_recv.ack << "\t, len = " << msg_recv.len << " \t "<<sysTime.wHour+8<<":"<<sysTime.wMinute<<":"<<sysTime.wSecond<<":"<<sysTime.wMilliseconds<<std::endl;
	return 0;
}