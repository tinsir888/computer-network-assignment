//server端接收文件 
#include "reliableudp.h"
SOCKET server;
string filename;
char message[200000000];
static char pindex[2];		// 用于标志已经收到了哪个包
// checksum检查包
unsigned char check_pkg(char *arr, int len)
{
	if (len == 0)
		return ~(0);
	unsigned char ret = arr[0];
	for (int i = 1; i < len; i++)
	{
		unsigned int tmp = ret + (unsigned char)arr[i];
		tmp = (tmp >> 8) + tmp % (1 << 8);
		tmp = (tmp >> 8) + tmp % (1 << 8);
		ret = tmp;
	}
	return ~ret;
}
// 等待连接
void WaitConnection()
{
	while (1)
	{
		char recv[2];
		int clientlen = sizeof(clientAddr);
		while (recvfrom(server, recv, 2, 0, (sockaddr *)&clientAddr, &clientlen) == SOCKET_ERROR);
		if (recv[0] != SEQ1)
			continue;
		cout << "接收端收到第一次握手。" << endl;
		bool flag = CONNECTSUCCESS;
		while (1)
		{
			memset(recv, 0, 2);
			char send[2];
			send[0] = SEQ2;
			send[1] = ACK2;
			sendto(server, send, 2, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
			cout << "接收端发送第二次握手" << endl;
			while (recvfrom(server, recv, 2, 0, (sockaddr *)&clientAddr, &clientlen) == SOCKET_ERROR);
			if (recv[0] == SEQ1)
				continue;
			if (recv[0] != SEQ3 || recv[1] != ACK3)
			{
				cout << "连接失败……\n请重启发送端。" << endl;
				flag = CONNECTFAIL;
			}
			break;
		}
		if (!flag)
			continue;
		break;
	}
}
// 开启接收端
int StartServer()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		cout << "WSAStartup错误：" << WSAGetLastError() << endl;
		return -1;
	}
	server = socket(AF_INET, SOCK_DGRAM, 0);

	if (server == SOCKET_ERROR)
	{
		cout << "套接字错误：" << WSAGetLastError() << endl;
		return -1;
	}
	int Port = 1439;
	serverAddr.sin_addr.s_addr = htons(INADDR_ANY);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(Port);

	if (bind(server, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		cout << "绑定端口错误：" << WSAGetLastError() << endl;
		return -1;
	}
	cout << "成功启动接收端！" << endl;
	return 1;
}
// 等待断开连接
void WaitDisconnection()
{
	while (1)
	{
		char recv[2];
		int clientlen = sizeof(clientAddr);
		while (recvfrom(server, recv, 2, 0, (sockaddr *)&clientAddr, &clientlen) == SOCKET_ERROR);
		if (recv[0] != WAVE1)
			continue;
		cout << "接收端接收到一次挥手。" << endl;
		char send[2];
		send[0] = WAVE2;
		send[1] = ACKW2;
		sendto(server, send, 2, 0, (sockaddr *)&clientAddr, sizeof(clientAddr));
		break;
	}
	cout << "发送端连接断开。" << endl;
}
// 接收
void Recvmessage()
{
	pindex[0] = 0;
	pindex[1] = -1;
	int len = 0;
	while (1)
	{
		char recv[LENGTH + CheckSum];
		memset(recv, '\0', LENGTH + CheckSum);
		int length;
		while (1)
		{
			int clientlen = sizeof(clientAddr);
			while (recvfrom(server, recv, LENGTH + CheckSum, 0, (sockaddr *)&clientAddr, &clientlen) == SOCKET_ERROR);
			length = recv[4] * 128 + recv[5];
			char send[3];
			memset(send, '\0', 3);
			printf("序列号: %d 长度: %dByte ", recv[2]*128 + recv[3] ,length);
			// 检查ACK
			if (recv[6] == ACKMsg)
			{
				send[0] = ACKMsg;
				// 这一个包是不是顺序的下一个包
				if (((pindex[0] == recv[2] && pindex[1] + 1 == recv[3]) || (pindex[0] + 1 == recv[2] && recv[3] == 0 && pindex[1] == 127)) && check_pkg(recv, length + CheckSum) == 0)
				{
					pindex[0] = recv[2];
					pindex[1] = recv[3];
					send[1] = recv[2];
					send[2] = recv[3];
					printf("校验正确\n");
				}
				else
				{
					printf("出错了。\n");
					send[1] = -1;
					send[2] = -1;
					sendto(server, send, 3, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
					continue;
				}
				sendto(server, send, 3, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
				break;
			}
			// 出错了 发送一个NAK
			else
			{
				printf("出错了。\n");
				send[0] = NAK;
				send[1] = recv[2];
				send[2] = recv[3];
				sendto(server, send, 3, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
				continue;
			}
		}
		for (int i = CheckSum; i < length + CheckSum; i++)
		{
			message[len] = recv[i];
			len++;
		}
		if (recv[1] == LAST)
			break;
	}
	ofstream fout(filename.c_str(), ofstream::binary);
	for (int i = 0; i < len; i++)
		fout << message[i];
	fout.close();
}
// 接收文件名
void RecvName()
{
	char name[100];
	int clientlen = sizeof(clientAddr);
	while (recvfrom(server, name, 100, 0, (sockaddr*)&clientAddr, &clientlen) == SOCKET_ERROR);
	cout << "文件名为: ";
	for (int i = 0; name[i] != '$'; i++)
	{
		filename += name[i];
		putchar(name[i]);
	}
	cout << endl;
}
int main()
{
	StartServer();
	cout << "等待发送端连接..." << endl;
	WaitConnection();
	cout << "成功连接到发送端！" << endl;
	RecvName();
	Recvmessage();
	cout << "接收文件结束！" << endl;
	WaitDisconnection();
	closesocket(server);
	WSACleanup();
	system("pause");
	return 0;
}
