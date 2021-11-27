//client端发送文件 
# include "reliableudp.h"

SOCKET client;
bool first_pkg = 1;
char message[200000000];
// checksum校验和 
unsigned char check_pkg(char *arr, int len)
{
	if (len == 0)
		return ~(0);
	unsigned char ret = arr[0];
	for (int i = 1; i < len; i++)
	{
		unsigned int tmp = ret + (unsigned char)arr[i];
		tmp = (tmp >> 8) + tmp & 0xff;
		tmp = (tmp >> 8) + tmp & 0xff;
		ret = tmp;
	}
	//if(ret < 16) cout << "checksum: " << '0' << hex << (int)ret << endl;
	//else cout << "checksum: " << hex << (int)ret <<endl;
	return ~ret;
}
// 发送数据包
void Sendpackage(char* msg, int len, int index, int last)
{
	char *buffer = new char[len + CheckSum];
	if (last)
	{
		buffer[1] = LAST;
	}
	else
	{
		buffer[1] = NOTLAST;
	}
	buffer[2] = index / 128;
	buffer[3] = index % 128;
	buffer[4] = len / 128;
	buffer[5] = len % 128;
	if (!first_pkg)
		buffer[6] = ACKMsg;
	else
		buffer[6] = '$';
	
	cout << "序列号: " << index << " " << "长度: " << len << "Byte ";

	for (int i = 0; i < len; i++)
	{
		buffer[i + CheckSum] = msg[i];
	}
	while (1)
	{
		buffer[0] = check_pkg(buffer + 1, len + CheckSum - 1);
		printf("校验和: %d\n", (int)((unsigned char)buffer[0]));
		//cout << "校验和: " << (int)((unsigned char)buffer[0]) << endl;
		sendto(client, buffer, LENGTH + CheckSum, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
		int begin_time = clock();
		char recv[3];
		int clientlen = sizeof(clientAddr);
		while (recvfrom(client, recv, 3, 0, (sockaddr *)&serverAddr, &clientlen) == SOCKET_ERROR)
		{
			int over_time = clock();
			if (over_time - begin_time > TIMEOUT)
			{
				cout << "超时……" << endl;
				break;
			}
		}
		if (recv[0] == ACKMsg)
		{
			if (index != recv[1] * 128 + recv[2])
				continue;
			break;
		}
		else
		{
			cout << "错误！重新发送！" << endl;
			buffer[6] = ACKMsg;
			first_pkg = 0;
		}
	}
	delete buffer;
}
// 发送消息
void Sendmessage(string filename, int size)
{
	int len = 0;
	if (filename == "quit")
	{
		return;
	}
	else
	{
		ifstream fin(filename.c_str(), ifstream::binary);
		if (!fin)
		{
			cout << "文件丢失！" << endl;
			return;
		}
		unsigned char ch = fin.get();
		while (fin)
		{
			message[len] = ch;
			len++;
			ch = fin.get();
		}
		fin.close();
	}
	int begin_send_time = clock(), finish_send_time;
	int package_num = len / LENGTH + (len % LENGTH != 0);
	static int index = 0;
	for (int i = 0; i < package_num; i++)
	{
		Sendpackage(message + i * LENGTH
			, i == package_num - 1 ? len - (package_num - 1)*LENGTH : LENGTH
			, index++, i == package_num - 1);
		//if (i % 10 == 0)
		//{
		//	printf("Finished:%.2f%%\n", (float)i / package_num * 100);
		//}
	}
	finish_send_time = clock();
	int tot_time = finish_send_time - begin_send_time;
	printf("时延: %dms\n", tot_time);
	printf("吞吐率: %fKbps\n", (double)len * 8 / tot_time / 1024);
}
// 连接到服务器
void ConnectToServer()
{
	while (1)
	{
		char send[2];
		send[0] = SEQ1;
		send[1] = ACK1;
		sendto(client, send, 2, 0, (sockaddr *)&serverAddr, sizeof(serverAddr));
		cout << "发送端发送第一次握手" << endl;
		int begin_time = clock();
		bool flag = SENDSUCCESS;
		char recv[2];
		int clientlen = sizeof(clientAddr);
		while (recvfrom(client, recv, 2, 0, (sockaddr *)&serverAddr, &clientlen) == SOCKET_ERROR)
		{
			int over_time = clock();
			if (over_time - begin_time > TIMEOUT)
			{
				flag = SENDFAIL;
				break;
			}
		}
		if (flag && recv[0] == SEQ2 && recv[1] == ACK2)
		{
			cout << "发送端收到第二次握手" << endl;
			send[0] = SEQ3;
			send[1] = ACK3;
			sendto(client, send, 2, 0, (sockaddr *)&serverAddr, sizeof(serverAddr));
			cout << "发送端发送第三次握手" << endl;
			break;
		}
	}
}
// 发送文件名
void SendName(string filename, int size)
{
	char *name = new char[size + 1];
	for (int i = 0; i < size; i++)
	{
		name[i] = filename[i];
	}
	name[size] = '$';
	sendto(client, name, size + 1, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
	delete name;
}
// 开始客户端
int StartClient()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		cout << "WSAStartup错误：" << WSAGetLastError() << endl;
		return -1;
	}
	client = socket(AF_INET, SOCK_DGRAM, 0);

	if (client == SOCKET_ERROR)
	{
		cout << "套接字错误：" << WSAGetLastError() << endl;
		return -1;
	}
	int Port = 1439;
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(Port);
	cout << "成功启动发送端！" << endl;
	return 1;
}
// 关闭客户端
void CloseClient()
{
	while (1)
	{
		char send[2];
		send[0] = WAVE1;
		send[1] = ACKW1;
		sendto(client, send, 2, 0, (sockaddr *)&serverAddr, sizeof(serverAddr));
		cout << "发送端挥手一次。" << endl;
		int begin_time = clock();
		bool flag = SENDSUCCESS;
		char recv[2];
		int clientlen = sizeof(clientAddr);
		while (recvfrom(client, recv, 2, 0, (sockaddr *)&serverAddr, &clientlen) == SOCKET_ERROR)
		{
			int over_time = clock();
			if (over_time - begin_time > TIMEOUT)
			{
				flag = SENDFAIL;
				break;
			}
		}
		if (flag && recv[0] == WAVE2 && recv[1] == ACKW2)
		{
			break;
		}
	}
}
int main()
{
	StartClient();
	cout << "连接到接收端..." << endl;
	ConnectToServer();
	cout << "成功连接到接收端！" << endl;
	int time_out = 1;
	setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(time_out));
	string filename;
	cin >> filename;
	SendName(filename, filename.length());
	Sendmessage(filename, filename.length());
	cout << "发送成功！" << endl;
	CloseClient();
	WSACleanup();
	system("pause");
	return 0;
}
