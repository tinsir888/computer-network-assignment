#include "reliableudp.h"
SOCKET client;
string filename;
const int maxpkgnum = 2e5;//最大报文个数
int window = 0;//窗口大小
bool pkgcomfirm[maxpkgnum];//表示对应的数据包是否被确认
int totpkg = 0;//数据包的总数
int filelen = 0;//发送文件的总长度
mutex mtx, mtx1;//线程锁
thread newthread;// 新线程，接收ACK码的时候是子线程
bool isfirstpkg = false;
char uploadfile[maxn];
// checksum
unsigned char checksum(char *ch, int len)
{
	if (len == 0) return 0xff;
	unsigned char cksum = ch[0];
	for (int i = 1; i < len; i++)
	{
		unsigned int tmp = cksum + (unsigned char)ch[i];
		tmp = (tmp >> 8) + (tmp & 0xff);
		tmp = (tmp >> 8) + (tmp & 0xff);
		cksum = tmp;
	}
	return ~cksum;
}
void sendpkg(SOCKET client, char* msg, int len, int pkgno, bool islastpkg);
void recvfeedback(SOCKET client)
{
	int serverlen = sizeof(serverAddr);
	char recv[3];
	while (true)
	{
		int begtimer = clock();
		bool flag = true;
		while (recvfrom(client, recv, 3, 0, (sockaddr*)&serverAddr, &serverlen) == SOCKET_ERROR)
		{
			int overtimer = clock();
			if (overtimer - begtimer > timeout)
			{
				flag = false;
				break;
			}
		}
		// server回复了一个ACK
		if (recv[0] == '%')
		{
			pkgcomfirm[((int)recv[1] * 128) + recv[2]] = true;
			mtx.lock();
			window -= 6;
			int curpkgno = ((int)recv[1] * 128) + recv[2];
			printf("第 %d 号数据包：已确认。\n", curpkgno);
			mtx.unlock();
		}
		// server回复了一个NAK
		else
		{
			//mtx1.lock();
			int errorpkgno = ((int)recv[1] * 128) + recv[2];
			printf("第 %d 号数据包：未确认。\n重新发送…\n", errorpkgno);
			pkgcomfirm[errorpkgno] = false;
			// 把这个错误的包重新发一遍
			mtx1.lock();
			if (errorpkgno == totpkg - 1) {
				sendpkg(client, uploadfile + errorpkgno * datalen, filelen - (totpkg - 1) * datalen, errorpkgno, true);
			}
			else sendpkg(client, uploadfile + errorpkgno * datalen, datalen, errorpkgno, false);
			mtx1.unlock();
		}
	}
}
// send packages
void sendpkg(SOCKET client, char* msg, int len, int pkgno, bool islastpkg)
{
	while (window == maxwindowsize)
		continue;
	char *pkg = new char[len + pkgheader];
	pkg[1] = islastpkg ? '$' : '@';
	pkg[2] = pkgno / 128; pkg[3] = pkgno % 128;
	pkg[4] = len / 128; pkg[5] = len % 128;
	//if (!isfirstpkg)
	pkg[6] = '%';
	for (int i = 0; i < len; i++)
	{
		pkg[i + pkgheader] = msg[i];
	}
	pkg[0] = checksum(pkg + 1, len + pkgheader - 1);
	sendto(client, pkg, len + pkgheader, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));

	//在窗口改变时，锁上进程
	mtx.lock();
	printf("发送第 %d 号数据包。校验和为：%d。长度：%dByte\n", pkgno, int((unsigned char)pkg[0]), len + pkgheader);
	window++;
	mtx.unlock();
	delete pkg;
}
int main()
{
	//start sender
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
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(1439);
	cout << "成功启动发送端。" << endl;
	//connect to receiver
	cout << "正在连接到接收端..." << endl;
	while (true)
	{
		char send[2];
		send[0] = '1';//SEQ = 1, SYN
		send[1] = '#';//ACK
		sendto(client, send, 2, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
		cout << "发送端发送第一次握手。" << endl;
		int begtimer = clock();
		bool flag = true;
		char recv[2];
		int clientlen = sizeof(clientAddr);
		while (recvfrom(client, recv, 2, 0, (sockaddr*)&serverAddr, &clientlen) == SOCKET_ERROR)
		{
			int overtimer = clock();
			if (overtimer - begtimer > timeout)
			{
				flag = false;
				break;
			}
		}
		if (flag && recv[0] == '3' && recv[1] == '2')//SEQ2, ACK
		{
			cout << "发送端收到第二次握手。" << endl;
			send[0] = '5';//SEQ3
			send[1] = '4';//ACK
			sendto(client, send, 2, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
			cout << "发送端发送第三次握手。" << endl;
			break;
		}
	}
	cout << "成功连接到接收端！" << endl;
	cin >> filename;
	int filenamelen = filename.length();
	char* file_name = new char[filenamelen + 1];
	for (int i = 0; i < filenamelen; i++)
	{
		file_name[i] = filename[i];
	}
	file_name[filenamelen] = '$';
	sendto(client, file_name, filenamelen + 1, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));

	ifstream fin(filename.c_str(), ifstream::binary);
	unsigned char ch = fin.get();
	while (fin){
		uploadfile[filelen] = ch;
		filelen++;
		ch = fin.get();
	}
	fin.close();
	int begin_send_time = clock(), finish_send_time;
	totpkg = filelen / datalen;
	if (filelen % datalen != 0) totpkg++;
	cout << "数据包总个数：" << totpkg << endl;
	static int pkgno = 0;
	char send[2];
	send[0] = (char)totpkg / 128;
	send[1] = (char)totpkg % 128;
	sendto(client, send, 2, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
	// 新开线程进行消息的监听
	newthread = thread(recvfeedback, client);
	newthread.detach();
	for (int i = 0; i < totpkg; i++)
	{
		if (i == totpkg - 1) {
			sendpkg(client, uploadfile + i * datalen, filelen - (totpkg - 1) * datalen, pkgno, true);
			/*确认最后一个数据包会死锁
			int begtimer = clock(), overtimer, clientlen = sizeof(clientAddr);;
			char *recv = new char[3];
		sendlastpkg:
			while (recvfrom(client, recv, 3, 0, (sockaddr*)&serverAddr, &clientlen) == SOCKET_ERROR)
			{
				overtimer = clock();
				if (overtimer - begtimer > timeout) {
					sendpkg(client, uploadfile + i * datalen, filelen - (totpkg - 1) * datalen, pkgno, true);
					goto sendlastpkg;
				}
			}
			if (recv[0] != '%' || ((int)recv[1] * 128) + recv[2] != pkgno)
				goto sendlastpkg;
			delete recv;
			*/
		}
		else sendpkg(client, uploadfile + i * datalen, datalen , pkgno, false);
		pkgno++;
	}
	finish_send_time = clock();
	if (totpkg == pkgno) {
		printf("发送完毕！\n");
	}
	int tot_time = finish_send_time - begin_send_time;
	printf("发送时间：%dms\n", tot_time);
	printf("吞吐率：%.3fKpbs\n", (double)filelen * 8 / tot_time / 1024);
	cout << "成功发送文件：" << filename << "！" << endl;
	//close sender
	while (true)
	{
		char send[2];
		send[0] = '7';//sender wave hand
		send[1] = '#';
		sendto(client, send, 2, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
		cout << "发送端挥一次手。" << endl;
		break;
		int begtimer = clock();
		bool flag = true;
		char recv[2];
		int clientlen = sizeof(clientAddr);
		while (recvfrom(client, recv, 2, 0, (sockaddr*)&serverAddr, &clientlen) == SOCKET_ERROR)
		{
			int overtimer = clock();
			if (overtimer - begtimer > timeout)
			{
				flag = false;
				break;
			}
		}
		if (flag && recv[0] == '9' && recv[1] == '8')//wave hand and ack
		{
			break;
		}
	}
	WSACleanup();
	return 0;
}
