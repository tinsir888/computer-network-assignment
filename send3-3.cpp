#include "reliableudp.h"
SOCKET client;
char uploadfile[maxn];
int filelen = 0;
int totpkg;
int curwindow = 0;
const int maxwindowsize = 6;//最大窗口长度
int acknum = 0;
void sendpkg(char* pkgdata, int curpkgdatalen, int curpkgno, int last)
{
	char *sendbuffer = new char[curpkgdatalen + pkgheaderlen];
	if (last)
		sendbuffer[1] = '$';//最后一个数据包
	else sendbuffer[1] = '@';//不是最后一个数据包
	sendbuffer[2] = curpkgno / 128;
	sendbuffer[3] = curpkgno % 128;
	sendbuffer[4] = curpkgdatalen / 128;
	sendbuffer[5] = curpkgdatalen % 128;
	for (int i = 0; i < curpkgdatalen; i++)
		sendbuffer[i + pkgheaderlen] = pkgdata[i];
	sendbuffer[0] = checksum(sendbuffer + 1, curpkgdatalen + pkgheaderlen - 1);
	sendto(client, sendbuffer, curpkgdatalen + pkgheaderlen, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
}
int main()
{
	//启动发送端
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		cout << "WSAStartup 错误: " << WSAGetLastError() << endl;
		return 0;
	}
	client = socket(AF_INET, SOCK_DGRAM, 0);
	if (client == SOCKET_ERROR)
	{
		cout << "套接字错误: " << WSAGetLastError() << endl;
		return 0;
	}
	int Port = 1439;
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(Port);

	cout << "成功启动发送端!" << endl;
	cout << "连接到接收端..." << endl;
	//连接到接收端
	while (true)
	{
		char handshake[2];
		handshake[0] = '1';
		handshake[1] = '#';
		sendto(client, handshake, 2, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
		cout << "发送端请求第一次握手。" << endl;
		int begtimer = clock();
		bool flag = true;
		char hsfeedback[2];
		int clientlen = sizeof(clientAddr);
		while (recvfrom(client, hsfeedback, 2, 0, (sockaddr*)&serverAddr, &clientlen) == SOCKET_ERROR)
		{
			int overtimer = clock();
			if (overtimer - begtimer > timeout)
			{
				flag = false;
				break;
			}
		}
		if (flag && hsfeedback[0] == '3' && hsfeedback[1] == '2')
		{
			cout << "发送端收到第二次握手。" << endl;
			handshake[0] = '5';
			handshake[1] = '4';
			sendto(client, handshake, 2, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
			cout << "发送端发送第三次握手。" << endl;
			break;
		}
	}
	cout << "成功连接到接收端！" << endl;
	int time_out = 50;
	setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(time_out));
	string filename;
	cin >> filename;
	//发送文件名
	char* sendfilename = new char[filename.length() + 1];
	for (int i = 0; i < filename.length(); i++)
	{
		sendfilename[i] = filename[i];
	}
	sendfilename[filename.length()] = '$';
	sendto(client, sendfilename, filename.length() + 1, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
	delete sendfilename;
	//将文件保存到发送缓冲区
	ifstream fin(filename.c_str(), ifstream::binary);
	if (!fin)
	{
		cout << "文件不存在! " << endl;
		return 0;
	}
	unsigned char ch = fin.get();
	while (fin)
	{
		uploadfile[filelen] = ch;
		filelen++;
		ch = fin.get();
	}
	fin.close();
	totpkg = filelen / datalen + (filelen % datalen != 0);
	cout << "文件总数据包个数：" << totpkg << endl;
	//开始发送文件
	cout << "开始发送！" << endl;
	int begin_send_time = clock(), finish_send_time;
	bool status = 0;// 0 表示慢启动状态， 1 表示拥塞状态
	int basepkgno = 0;
	while (true)
	{
		if (!curwindow){
			curwindow = 1;
		}
		else if (curwindow < maxwindowsize && status == 0)
		{
			if ((curwindow << 1) > maxwindowsize){
				status = 1;
				curwindow++;
			}
			else curwindow <<= 1;
			
		}
		else if (status == 1)
			curwindow++;
		printf("当前窗口大小: %d\n", curwindow);
		int i;
		for (i = 0; i < curwindow && i + basepkgno < totpkg; i++)
		{
			int curpkgno = i + basepkgno;
			if(curpkgno == totpkg - 1)
				sendpkg(uploadfile + curpkgno * datalen, 
					filelen - (totpkg - 1)* datalen, curpkgno, true);
			else sendpkg(uploadfile + curpkgno * datalen, datalen, curpkgno, false);
		}
		printf("已发送第 %d 号数据包\n", i + basepkgno - 1);
		int begintimer = clock(), endtimer;
		//接收端的反馈
		while (true)
		{
			int clientlen = sizeof(clientAddr);
			bool flag = true;
			char feedback[3];
			while (recvfrom(client, feedback, 3, 0, (sockaddr*)&serverAddr, &clientlen) == SOCKET_ERROR)
			{
				endtimer = clock();
				if (endtimer - begintimer > timeout)
				{
					flag = false;
					//printf("超时！\n");
					break;
				}
			}
			if (flag && feedback[0] == '%')
			{
				printf("收到接收端：第 %d 号数据包 ACK。\n", feedback[1] * 128 + feedback[2]);
				acknum++;
			}
			else if (!flag){
				break;
			}
		}
		if (acknum == curwindow)
		{
			acknum = 0;
			basepkgno += curwindow;
		}
		else
		{
			//basepkgno = feedback;
			acknum = 0;
			curwindow = 0;
			status = 0;
			if (i + basepkgno >= totpkg)
				break;
		}
	}
	//告知客户端发送文件结束
	char finishsend[3] = { 'f', 'i', 'n' };
	sendto(client, finishsend, 3, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
	finish_send_time = clock();
	int tot_time = finish_send_time - begin_send_time;
	printf("发送时间：%dms\n", tot_time);
	printf("吞吐率：%.3fKpbs\n", (double)filelen * 8 / tot_time / 1024);
	cout << "成功发送文件" << filename << "!" << endl;
	//挥手，断开连接
	while (true)
	{
		char wavehand[2];
		wavehand[0] = '7';
		wavehand[1] = '#';
		sendto(client, wavehand, 2, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
		cout << "发送端挥一次手。" << endl;
		int begtimer = clock();
		bool flag = true;
		char whfeedback[2];
		int clientlen = sizeof(clientAddr);
		while (recvfrom(client, whfeedback, 2, 0, (sockaddr*)&serverAddr, &clientlen) == SOCKET_ERROR)
		{
			int overtimer = clock();
			if (overtimer - begtimer > timeout)
			{
				flag = false;
				break;
			}
		}
		if (flag && whfeedback[0] == '9' && whfeedback[1] == '8')
			break;
	}
	cout << "已断开连接" << endl;
	closesocket(client);
	WSACleanup();
	return 0;
}