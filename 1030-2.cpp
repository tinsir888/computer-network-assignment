#include <WinSock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <WINSOCK2.h>
#pragma comment(lib,"ws2_32.lib")
int main()
{


	//1.初始化并创建客户端套接字

	WSADATA wsaData = { 0 };//存放套接字信息的数据结构
	if (WSAStartup(MAKEWORD(2, 2), &wsaData))//套接字初始化
	{
		printf("WSAStartup failed with error code: %d\n", WSAGetLastError());
		return -1;
	}
	//判断套接字版本
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("wVersion was not 2.2\n");
		return -1;
	}

	//创建客户端套接字，指定地址类型、服务类型和协议（TCP）
	SOCKET ClientSocket = INVALID_SOCKET;//声明用于连接的客户端套接字
	ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);//初始化该套接字
	if (ClientSocket == INVALID_SOCKET)//判断初始化是否成功，否则退出程序
	{
		printf("socket failed with error code: %d\n", WSAGetLastError());
		return -1;
	}

	//2.用户在命令行操作，设置要连接到的服务器的IP和端口号

	//输入服务器IP（要连接到的IP地址）
	printf("Please input server IP:");
	char IP[32] = { 0 };//IP地址字符串缓冲区
	gets_s(IP);//读取输入到换行符为止
	//输入聊天的用户名
	printf("Please input your name:");
	char name[32] = { 0 };
	gets_s(name);
	USHORT uPort = 18000;//服务端端口

	//设置要连接到的服务器的地址，包括IP和端口号
	SOCKADDR_IN ServerAddr = { 0 };//存放服务端地址的数据结构
	ServerAddr.sin_family = AF_INET;//指定地址类型，这里是IPv4
	ServerAddr.sin_port = htons(uPort);//指定端口
	ServerAddr.sin_addr.S_un.S_addr = inet_addr(IP);//指定服务器地址，一个UINT（即unsigned long）类型

	printf("connecting......\n");
	//连接服务器，失败则退出程序
	if (SOCKET_ERROR == connect(ClientSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr)))
	{
		printf("connect failed with error code: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return 0;
	}
	//连接成功则打印服务端的地址和端口号
	printf("connecting server successfully IP:%s Port:%d\n\n",
		inet_ntoa(ServerAddr.sin_addr), htons(ServerAddr.sin_port));

	
	//3.收发消息，TCP使用send和recv函数

	char buffer[4096] = { 0 };//存放消息的缓冲区
	int iRecvLen = 0;//收到的字节数
	int iSnedLen = 0;//发送的字节数
	//最开始发送用户名给对方，返回实际发送的字节数
	iSnedLen = send(ClientSocket, name, strlen(name), 0);
	if (SOCKET_ERROR == iSnedLen)
	{
		printf("send failed with error code: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return 0;
	}
	else printf("Successfully sent your name to the server!\n");
	//然后接收对方的用户名，也返回实际接收的字节数
	char nameOther[32] = { 0 };
	iRecvLen = recv(ClientSocket, nameOther, sizeof(nameOther), 0);
	if (SOCKET_ERROR == iRecvLen)
	{
		printf("send failed with error code: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return 0;
	}
	else printf("Successfully sent the server's name to you!\n\n\n\n");
	//接收字节流的末尾要加上"\0"才能构成字符串
	strcat_s(nameOther, "\0");

	time_t Time_now = time(0);//获取时间

	//接下来进入消息循环，发送和接收聊天消息
	while (1)
	{
		memset(buffer, 0, sizeof(buffer));
		//输入消息
		printf("%s: ", name);
		gets_s(buffer);
		char* msg = ctime(&Time_now);
		char* sendingTime = msg;
		strncat(msg, buffer, sizeof(buffer));
		//输入"bye"时退出聊天
		if (strcmp(buffer, "bye") == 0) {
			iSnedLen = send(ClientSocket, buffer, strlen(buffer), 0);
			if (SOCKET_ERROR == iSnedLen)
			{
				printf("send failed with error code: %d\n", WSAGetLastError());
				closesocket(ClientSocket);
				WSACleanup();
				return 0;
			}
			break;
		}
		//发送消息
		iSnedLen = send(ClientSocket, msg, strlen(msg), 0);
		if (SOCKET_ERROR == iSnedLen)
		{
			printf("send failed with error code: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return 0;
		}
		printf("Successfully send message to Server: %s\nSend time: %s\n", nameOther, sendingTime);
		memset(buffer, 0, sizeof(buffer));
		//接收消息并打印到stdout
		iRecvLen = recv(ClientSocket, buffer, sizeof(buffer), 0);
		if (SOCKET_ERROR == iRecvLen)
		{
			printf("send failed with error code: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return 0;
		}
		strcat_s(buffer, "\0");
		printf("%s: %s\n", nameOther, buffer);

	}
	//关闭socket连接
	closesocket(ClientSocket);
	//释放socket DLL资源
	WSACleanup();
	return 0;
}
