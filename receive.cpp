#include "reliableudp.h"
SOCKET server;
int cntpkg = 0;
char downloadfile[maxn];
string filename;
int confirm = 0;
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
int main()
{
	//先启动接收端
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		cout << "WSAStartup 错误：" << WSAGetLastError() << endl;
		return -1;
	}
	server = socket(AF_INET, SOCK_DGRAM, 0);
	if (server == SOCKET_ERROR)
	{
		cout << "套接字错误：" << WSAGetLastError() << endl;
		return -1;
	}
	serverAddr.sin_addr.s_addr = htons(INADDR_ANY);
	serverAddr.sin_family = AF_INET;//ipv4
	serverAddr.sin_port = htons(1439);
	if (bind(server, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		cout << "绑定端口错误：" << WSAGetLastError() << endl;
		return -1;
	}
	cout << "成功启动接收端！" << endl;
	//等待发送端连接接收端
	cout << "等待发送端连接..." << endl;
	while (true)
	{
		char receivepkg[2];
		int clientlen = sizeof(clientAddr);
		while (recvfrom(server, receivepkg, 2, 0, (sockaddr*)&clientAddr, &clientlen) == SOCKET_ERROR)
			continue;
		if (receivepkg[0] != '1')//first hand shake
			continue;
		cout << "接收端接收到第一次握手。" << endl;
		bool flag = true;//first hand shake confirmed.
		while (true)
		{
			memset(receivepkg, 0, 2);
			char send[2];
			send[0] = '3';//SEQ2
			send[1] = '2';//ACK=SEQ1+1
			sendto(server, send, 2, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
			cout << "接收端发送第二次握手。" << endl;
			while (recvfrom(server, receivepkg, 2, 0, (sockaddr*)&clientAddr, &clientlen) == SOCKET_ERROR);
			if (receivepkg[0] == '1')//SEQ1, SYN
				continue;
			if (receivepkg[0] != '5' || receivepkg[1] != '4')//SEQ3,ACK3
			{
				cout << "连接错误。\n请重启发送端！" << endl;
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
	char name[50];
	int clientlen = sizeof(clientAddr);
	while (recvfrom(server, name, 50, 0, (sockaddr*)&clientAddr, &clientlen) == SOCKET_ERROR)
		continue;
	for (int i = 0; name[i] != '$'; i++){
		filename += name[i];
	}
	//接收文件内容
	char tot_pkg[2];
	while (recvfrom(server, tot_pkg, 2, 0, (sockaddr*)&clientAddr, &clientlen) == SOCKET_ERROR);
	int totpkg = (int)tot_pkg[0] * 128 + tot_pkg[1];
	cout << "数据包总个数：" << totpkg << endl;
	int len = 0;
	int curpkglen;
	int curpkgno;
	while (true)
	{
		char receivepkg[pkglength];
		memset(receivepkg, '\0', pkglength);
		while (true)
		{
			int clientlen = sizeof(clientAddr);
			int begtimer = clock(), overtimer;
			bool flag = true;
			while (recvfrom(server, receivepkg, pkglength, 0, (sockaddr*)&clientAddr, &clientlen) == SOCKET_ERROR)
			{
				overtimer = clock();
				if (overtimer - begtimer > timeout)
				{
					flag = false;
					break;
				}
				cout << WSAGetLastError() << endl;
			}
			curpkglen = ((int)receivepkg[4] * 128) + receivepkg[5];
			curpkgno = (int)receivepkg[2] * 128 + receivepkg[3];
			char send[3];
			memset(send, '\0', 3);
			// 没有超时，ACK码正确，差错检查正确
			unsigned char cksum = checksum(receivepkg, curpkglen + pkgheader);
			if (flag && receivepkg[6] == '%' && cksum == 0)
			{
				confirm++;
				if (confirm == 6 || receivepkg[1] == '$')
				{
					cout << "确认第 " << curpkgno << " 号数据包。" << endl;
					confirm = 0;
					send[0] = '%';//ACK
					send[1] = receivepkg[2];
					send[2] = receivepkg[3];
					sendto(server, send, 3, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
				}
				cntpkg++;
				break;
			}
			//发送NAK
			else
			{
				cout << "接收 "<< curpkgno <<" 号之前的数据包出现错误。" << endl;
				printf("校验和为: %d\n", cksum);
				send[0] = '*';//NAK
				send[1] = receivepkg[2];
				send[2] = receivepkg[3];
				sendto(server, send, 3, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
				continue;
			}
		}
		int loc = datalen * (receivepkg[2] * 128 + receivepkg[3]);
		for (int i = pkgheader; i < curpkglen + pkgheader; i++)
		{
			downloadfile[loc + i - pkgheader] = receivepkg[i];
			len++;
		}
		// last package
		if (receivepkg[1] == '$') {
			//为什么确认最后一个数据包会死锁？
			/*char send[3];
			memset(send, '\0', 3);
			cout << "确认第 " << receivepkg[2] * 128 + receivepkg[3] << " 号数据包。" << endl;
			send[0] = '%';
			send[1] = pos[0] = receivepkg[2];
			send[2] = pos[1] = receivepkg[3];
			sendto(server, send, 3, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
			*/
			break;
		}
	}
	// 累计确认总的包数
	if (totpkg == cntpkg)
	{
		cout << "没有数据包丢失！" << endl;
	}
	ofstream fout(filename.c_str(), ofstream::binary);
	for (int i = 0; i < len; i++)
		fout << downloadfile[i];
	fout.close();
	cout << "成功接收文件！" << endl;
	while (1)
	{
		char receivepkg[2];
		clientlen = sizeof(clientAddr);
		while (recvfrom(server, receivepkg, 2, 0, (sockaddr*)&clientAddr, &clientlen) == SOCKET_ERROR);
		if (receivepkg[0] != '7')
			continue;
		cout << "接收端收到一次挥手。" << endl;
		char send[2];
		send[1] = '8';//receiver has received sender's waving hand.
		send[0] = '9';//reveiver wave hand
		sendto(server, send, 2, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
		break;
	}
	cout << "发送端断开连接。" << endl;
	closesocket(server);
	WSACleanup();
	return 0;
}
