#include"UDP programming.h"

std::fstream Client_log;

static SOCKET Client;
static struct fakeHead sendHead, recvHead;
static sockaddr_in client_addr;
static sockaddr_in server_addr;

static int addrlen;
static u_short NowSeq = 0;
static u_short NowAck = 0;
static bool ifAck = 0;

std::string savePath = "./save/";



int client_test() {
	while (true) {
		char buff[20];
		recvfrom(Client, buff, 20, 0, (struct sockaddr*)&client_addr, &addrlen);
		printf("%s\n", buff);
	}
	return 0;
}

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

	Client_log << "Client Socket Start Up!" << std::endl;


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
	Client_log << "Waiting for Connection...." << std::endl;

	recvfile();

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
			Client_log << "[log] Recieving Error , try again!" << std::endl;
		}
		else {
			printf("[Log] RECIEVE seq = %d , ack = %d, len = %d ,NowSeq = %d ,NowAck = %d\n",recvMsg.seq,recvMsg.ack,recvMsg.len,NowSeq,NowAck);
			Client_log << "[Log] RECIEVE seq = " << recvMsg.seq << ", ack = " << recvMsg.ack << ", len = " << recvMsg.len <<", NowSeq = "<<NowSeq<< ", NowAck = " << NowAck << std::endl;
			if (recvMsg.checkValid(&recvHead) && recvMsg.seq == NowAck) {
				if (recvMsg.if_SYN()) {
					printf("[SYN] First_Hand_Shake\n");
					Client_log << "[SYN] First_Hand_Shake" << std::endl;
					cnt_accept(recvMsg.seq+recvMsg.len);
				}
				else if (recvMsg.if_FIN()) {
					printf("[FIN] First_Wave\n");
					Client_log << "[FIN] First_Wave" << std::endl;
					dic_accept(recvMsg.seq + recvMsg.len);
					return 0;
				}
				else {

				if (recvMsg.if_FDS()) {
					memset(&descriptor, 0, sizeof(struct FileHead));
					memcpy(&descriptor, recvMsg.message, sizeof(struct FileHead));
					printf("[FDS] Start recieving file %s \n", descriptor.filename.c_str());
					Client_log << "[FDS] Start recieving file " << descriptor.filename << std::endl;
					filelen = descriptor.filelen;
					filename =savePath + descriptor.filename;
					lenPointer = 0;
					file.open(filename, std::ios::out | std::ios::binary);
					//file.close();

				}
				else {
					Client_log << "[Log] Recieving File content of "<<recvMsg.len<<" size" << std::endl;
					printf("[Log] Recieving File content of %d size\n", recvMsg.len);
					file.write(recvMsg.message, recvMsg.len);
					lenPointer += recvMsg.len;
					if (lenPointer >= filelen) {
						printf("[Log] File %s recieved successfully!\n", filename.c_str());
						Client_log << "[Log] File " << filename << " recieved successfully!" << std::endl;
						file.close();
						}
					}

				file_accept(recvMsg.seq + recvMsg.len);
				}

				
			}
			else {
				printf("[Error] Disorder OR Data Fault\n");
				Client_log << "[Error] Disorder OR Data Fault" << std::endl;
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
	second_shake.set_SYN();
	second_shake.set_ack(ack);
	second_shake.set_check(&sendHead);
	sendto(Client, (char*)&second_shake, sizeof(msg), 0, (struct sockaddr*)&server_addr, addrlen);
	printf("[Log] SEND ack = %d , len = %d\n", second_shake.ack, second_shake.len);
	Client_log << "[Log] SEND ack = " << second_shake.ack << ", len = " << second_shake.len << std::endl;
	printf("[Log] Connection set up!\n");
	Client_log << "[Log] Connection set up!" << std::endl;

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
	printf("[Log] SEND ack = %d , len = %d\n", dic.ack, dic.len);
	Client_log << "[Log] SEND ack = " << dic.ack << ", len = " << dic.len << std::endl;
	printf("[Log] Connection Killed!\n");
	Client_log << "[Log] Connection Killed!" << std::endl;
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
	Client_log << "[Log] SEND ack = " << msg_recv.ack << ", len = " << msg_recv.len << std::endl;
	/*printf("[Log] Message Recieved!\n");
	Client_log << "[Log] Message Recieved!" << std::endl;*/
	return 0;
}