#include "reliableudp.h"
using namespace std;
SOCKET client;
void Sendpackage(SOCKET client, char* msg, int len, int index, int last);
int Scroll = 0;
bool ACKPkg[100000];
int package_num = 0;
int filelen = 0;
mutex mtx, mtx1, mtx2;				// 线程锁
thread d1, d2;						// 新线程，接收ACK码的时候是子线程
int No1 = 0;
// checksum算法 进行包的检测
unsigned char PkgCheck(char *arr, int len)
{
	if (len == 0)
		return ~(0);
	unsigned char ret = arr[0];
	for (int i = 1; i < len; i++)
	{
		unsigned int tmp = ret + (unsigned char)arr[i];
		tmp = tmp / (1 << 8) + tmp % (1 << 8);
		tmp = tmp / (1 << 8) + tmp % (1 << 8);
		ret = tmp;
	}
	return ~ret;
}
// 关闭客户端
void CloseClient(SOCKET client)
{
	while (1)
	{
		char send[2];
		send[0] = WAVE1;
		send[1] = ACKW1;
		sendto(client, send, 2, 0, (sockaddr *)&serverAddr, sizeof(serverAddr));
		cout << "发送端挥一次手。" << endl;
		break;
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
// 窗口监听
void ScrollListen(SOCKET client)
{
	int serverlen = sizeof(serverAddr);
	char recv[3];
	while (1)
	{
		int begin_time = clock();
		// 随机一个时间，模拟网络延时
		//srand((int)time(NULL));
		//Sleep(rand() % 40 + 10);
		bool flag = SENDSUCCESS;
		while (recvfrom(client, recv, 3, 0, (sockaddr*)&serverAddr, &serverlen) == SOCKET_ERROR)
		{
			int over_time = clock();
			if (over_time - begin_time > TIMEOUT)
			{
				flag = SENDFAIL;
				break;
			}
		}
		// server回复了一个ACK
		if (recv[0] == ACKMsg)
		{
			ACKPkg[recv[1] * 128 + recv[2]] = true;
			// 可以使用的滑动窗口多了一个，所以Scroll减一
			mtx.lock();
			Scroll -= 5;
			printf("第 %d 号数据包：已确认。\n", int(recv[1] * 128 + recv[2]));
			mtx.unlock();
		}
		// server回复了一个NAK
		else
		{
			int indexx = recv[1] * 128 + recv[2];
			printf("第 %d 号数据包：未确认。\n重新发送…\n", indexx);
			ACKPkg[indexx] = false;
			// 把这个错误的包重新发一遍
			mtx1.lock();
			Sendpackage(client, message + indexx * LENGTH
				, indexx == package_num - 1 ? filelen - (package_num - 1)*LENGTH : LENGTH
				, indexx, indexx == package_num - 1);
			mtx1.unlock();
		}
	}
}
// 发包
void Sendpackage(SOCKET client, char* msg, int len, int index, int last)
{
	//srand((int)time(NULL));
	//Sleep(rand() % 40);
	while (Scroll == MaxScroll);
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
	if (!No1)
	{
		buffer[6] = ACKMsg;
	}
	else
	{
		buffer[6] = LAST;
		No1 = 0;
	}
	for (int i = 0; i < len; i++)
	{
		buffer[i + CheckSum] = msg[i];
	}
	buffer[0] = PkgCheck(buffer + 1, len + CheckSum - 1);
	sendto(client, buffer, LENGTH + CheckSum, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));

	// 在Scroll进行改变的时候 上锁防出错。。
	mtx.lock();
	printf("发送第 %d 号数据包。校验和为：%d。\n", index, int((unsigned char)buffer[0]));
	Scroll++;
	mtx.unlock();
	delete buffer;
}
// 发送文件
void Sendmessage(SOCKET client, string filename, int size)
{
	memset(ACKPkg, false, sizeof(ACKPkg) / sizeof(bool));
	if (filename == "quit")
	{
		return;
	}
	else
	{
		ifstream fin(filename.c_str(), ifstream::binary);
		if (!fin)
		{
			cout << "错误：文件不存在。" << endl;
			return;
		}
		unsigned char ch = fin.get();
		while (fin)
		{
			message[filelen] = ch;
			filelen++;
			ch = fin.get();
		}
		fin.close();
	}
	int begin_send_time = clock(), finish_send_time;
	package_num = filelen / LENGTH + (filelen % LENGTH != 0);
	static int index = 0;
	char send[2];
	send[0] = package_num / 128;
	send[1] = package_num % 128;
	sendto(client, send, 2, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
	// 新开线程进行消息的监听
	d2 = thread(ScrollListen, client);
	d2.detach();
	for (int i = 0; i < package_num; i++)
	{
		
		Sendpackage(client, message + i * LENGTH
			, i == package_num - 1 ? filelen - (package_num - 1)*LENGTH : LENGTH
			, index++, i == package_num - 1);
		
	}
	finish_send_time = clock();
	int tot_time = finish_send_time - begin_send_time;
	//printf("完成进度：100.00%%\n");
	printf("发送时间：%dms\n", tot_time);
	printf("吞吐率：%fKpbs\n", (double)filelen * 8 / tot_time / 1024);
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
		cout << "发送端发送第一次握手。" << endl;
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
			cout << "发送端收到第二次握手。" << endl;
			send[0] = SEQ3;
			send[1] = ACK3;
			sendto(client, send, 2, 0, (sockaddr *)&serverAddr, sizeof(serverAddr));
			cout << "发送端发送第三次握手。" << endl;
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
	name[size] = LAST;
	sendto(client, name, size + 1, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
	delete name;
}
// 开始客户端
int StartClient()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		cout << "WSAStartup 错误：" << WSAGetLastError() << endl;
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

	cout << "成功启动发送端。" << endl;
	return 1;
}
int main()
{
	StartClient();
	cout << "正在连接到接收端..." << endl;
	ConnectToServer();
	cout << "成功连接到接收端！" << endl;
	string filename;
	cin >> filename;
	SendName(filename, filename.length());
	Sendmessage(client, filename, filename.length());
	cout << "成功发送文件：" << filename << "！" << endl;
	CloseClient(client);
	WSACleanup();
	return 0;
}
