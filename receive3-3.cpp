#include "reliableudp.h"
SOCKET server;
string filename;
int pkgnolen[maxn];//每个数据包的长度
char downloadfile[maxn];
bool issendfinish(char* s) {
	return *s == 'f' && *(s + 1) == 'i' && *(s + 2) == 'n';
}
int main()
{
	//启动接收端
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		cout << "WSAStartup 错误：" << WSAGetLastError() << endl;
		return 0;
	}
	server = socket(AF_INET, SOCK_DGRAM, 0);

	if (server == SOCKET_ERROR)
	{
		cout << "套接字错误：" << WSAGetLastError() << endl;
		return 0;
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

	//连接上发送端
	cout << "等待发送端连接..." << endl;
	while (true)
	{
		char handshake[2];
		int clientlen = sizeof(clientAddr);
		while (recvfrom(server, handshake, 2, 0, (sockaddr*)&clientAddr, &clientlen) == SOCKET_ERROR);
		if (handshake[0] != '1')
			continue;
		cout << "接收端收到第一次握手。" << endl;
		bool flag = true;
		while (true)
		{
			memset(handshake, 0, 2);
			char hsfeedback[2];
			hsfeedback[0] = '3';
			hsfeedback[1] = '2';
			sendto(server, hsfeedback, 2, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
			cout << "接收端发送第二次握手。" << endl;
			while (recvfrom(server, handshake, 2, 0, (sockaddr*)&clientAddr, &clientlen) == SOCKET_ERROR);
			if (handshake[0] == '1')
				continue;
			if (handshake[0] != '5' || handshake[1] != '4')
			{
				cout << "连接错误。\n请重启发送端。" << endl;
				flag = false;
			}
			break;
		}
		if (!flag)
			continue;
		break;
	}
	cout << "成功连接到发送端！" << endl;
	//接收文件名
	char name[100];
	int clientlen = sizeof(clientAddr);
	bool flag = true;
	int begin_time = clock();
	while (recvfrom(server, name, 100, 0, (sockaddr*)&clientAddr, &clientlen) == SOCKET_ERROR)
	{
		int over_time = clock();
		if (over_time - begin_time > timeout)
		{
			flag = false;
			cout << "未收到文件名。" << endl;
			break;
		}
	}
	if (flag)
	{
		for (int i = 0; name[i] != '$'; i++)
		{
			filename += name[i];
		}
	}
	//接收文件
	printf("开始接收文件内容!\n");
	int curpkglen, curpkgno = 0, ackbefore = -1;
	while (true)
	{
		char receivepkg[pkglength];
		memset(receivepkg, '\0', sizeof receivepkg);
		while (true)
		{
			clientlen = sizeof(clientAddr);
			int begin_time = clock();
			bool flag = true;
			while (recvfrom(server, receivepkg, pkglength, 0, (sockaddr*)&clientAddr, &clientlen) == SOCKET_ERROR)
			{
				int over_time = clock();
				if (over_time - begin_time > timeout)
				{
					flag = false;
					break;
				}
			}
			curpkgno = receivepkg[2] * 128 + receivepkg[3];
			curpkglen = receivepkg[4] * 128 + receivepkg[5];
			char feedback[3];
			memset(feedback, '\0', 3);
			// 没有超时，ACK码正确，差错检查正确
			unsigned int cks = checksum(receivepkg, curpkglen + pkgheaderlen);
			if (flag && cks == 0)
			{
				feedback[0] = '%';
				feedback[1] = receivepkg[2];
				feedback[2] = receivepkg[3];
				/*
				if (curpkgno == ackbefore + 1) {
					ackbefore++;
					feedback[1] = receivepkg[2];
					feedback[2] = receivepkg[3];
				}
				else {//如果乱序，仅发送累积确认的包
					cout << "乱序：" << curpkgno << "号当前，而" << ackbefore << "号之前全部确认" << endl;
					feedback[1] = ackbefore / 128;
					feedback[2] = ackbefore % 128;
					curpkgno = ackbefore;
				}
				*/
				sendto(server, feedback, 3, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));

				printf("成功接收第 %d 号数据包, 数据段长度为 %d 字节\n", curpkgno, curpkglen);
				break;
			}
			else
			{
				if (issendfinish(receivepkg))
				{
					cout << "接收文件内容完毕。" << endl;
					goto receivedone;
					break;
				}
				cout << "数据包出错，返回 NAK 给发送端。校验码: " << cks << endl;
				feedback[0] = '^';
				feedback[1] = receivepkg[2];
				feedback[2] = receivepkg[3];
				sendto(server, feedback, 3, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
				continue;
			}
		}
		// 收到的包可能顺序不对（因为存在差错检测之后的重传）
		// 根据它的index码确定它的位置
		int curpkgno = datalen * (receivepkg[2] * 128 + receivepkg[3]);
		pkgnolen[receivepkg[2] * 128 + receivepkg[3]] = curpkglen;
		for (int i = pkgheaderlen; i < curpkglen + pkgheaderlen; i++)
		{
			downloadfile[curpkgno + i - pkgheaderlen] = receivepkg[i];
		}
	}

receivedone:
	cout << "成功接收文件: " << filename << "!" << endl;
	int filelength = 0;
	int pkgnum;
	for (pkgnum = 0; pkgnolen[pkgnum] != '\0'; pkgnum++)
		filelength += pkgnolen[pkgnum];
	cout << "文件长度:" << filelength << "字节" << endl;
	cout << "数据包数：" << pkgnum << endl;
	ofstream fout(filename.c_str(), ofstream::binary);
	for (int i = 0; i < filelength; i++)
		fout << downloadfile[i];
	fout.close();
	//等待发送端断开连接
	while (true)
	{
		char wavehand[2];
		int clientlen = sizeof(clientAddr);
		while (recvfrom(server, wavehand, 2, 0, (sockaddr*)&clientAddr, &clientlen) == SOCKET_ERROR);
		if (wavehand[0] != '7')
			continue;
		cout << "接收端收到第一次挥手。" << endl;
		char whfeedback[2];
		whfeedback[0] = '9';
		whfeedback[1] = '8';
		sendto(server, whfeedback, 2, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
		break;
	}
	cout << "发送端断开连接。" << endl;
	closesocket(server);
	WSACleanup();
	return 0;
}