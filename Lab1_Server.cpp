#include <Ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <WINSOCK2.h>
#pragma comment(lib,"ws2_32.lib")

int main()
{
	//1.初始化服务端主socket

	//存放套接字信息的结构
	WSADATA wsaData = { 0 };
	SOCKET serverSocket = INVALID_SOCKET;//服务端套接字
	SOCKET clientSocket = INVALID_SOCKET;//客户端套接字
	SOCKADDR_IN serverAddr = { 0 };//服务端地址
	SOCKADDR_IN clientAddr = { 0 };//客户端地址
	int iClientAddrLen = sizeof(clientAddr);
	USHORT uPort = 18000;//设置服务器监听端口

	//调用WSAStartup函数初始化套接字，成功则返回0
	//MAKEWORD向系统指定使用的winsock版本，从而使得高版本的Winsock可以使用
	if (WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		printf("WSAStartup failed with error code: %d\n", WSAGetLastError());
		return 0;//失败就退出程序
	}

	//判断套接字版本
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("wVersion was not 2.2!\n");
		return 0;
	}

	//创建主socket，指定地址类型、服务和协议类型
	serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serverSocket == INVALID_SOCKET)
	{
		printf("Server socket creation failed with error code: %d\n", WSAGetLastError());
		return 0;
	}

	//设置服务器地址
	serverAddr.sin_family = AF_INET;//连接方式
	serverAddr.sin_port = htons(uPort);//服务器监听端口
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//任何客户端都能连接这个服务器

	//将服务器地址绑定到主socket，使得服务器具有固定的IP和端口号
	if (SOCKET_ERROR == bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)))
	{
		printf("Binding failed with error code: %d\n", WSAGetLastError());
		closesocket(serverSocket);
		return 0;
	}
	//主socket监听有无客户端连接，设置连接等待队列的最大长度为1
	if (SOCKET_ERROR == listen(serverSocket, 1))
	{
		printf("Listening failed with error code: %d\n", WSAGetLastError());
		closesocket(serverSocket);
		WSACleanup();
		return 0;
	}
	//输入Server端用户名并保存起来
	printf("Please input server's name:");
	char name[32] = { 0 };
	gets_s(name);

	printf("waiting to connect.....\n");
	//如果有客户端申请连接（即服务器运行主socket的端口接收到一个sockaddr地址结构），服务端主socket就接受连接，并为之创建一个单独的socket
	clientSocket = accept(serverSocket, (SOCKADDR*)&clientAddr, &iClientAddrLen);
	if (clientSocket == INVALID_SOCKET)
	{
		printf("Accepting failed with error code: %d\n", WSAGetLastError());
		closesocket(serverSocket);
		WSACleanup();
		return 0;
	}
	//inet_ntoa函数把十进制整数转换成点分十进制
	printf("Successfully got a connection from IP:%s Port:%d\n\n",
		inet_ntoa(clientAddr.sin_addr), htons(clientAddr.sin_port));
		//InetPton(AF_INET, _T("192.168.1.1"), &ClientAddr.sin_addr);

	char buffer[4096] = { 0 };//缓冲区最长4096字节
	int iRecvLen = 0;
	int iSendLen = 0;

	//首先发送用户名给客户端
	iSendLen = send(clientSocket, name, strlen(name), 0);
	if (SOCKET_ERROR == iSendLen)
	{
		printf("Sending failed with error code: %d\n", WSAGetLastError());
		closesocket(serverSocket);
		closesocket(clientSocket);
		WSACleanup();
		return 0;
	}
	else printf("Successfully sent the server's name to the client!\n");
	//接收客户端的用户名
	char nameOther[32] = { 0 };
	iRecvLen = recv(clientSocket, nameOther, sizeof(nameOther), 0);
	if (SOCKET_ERROR == iRecvLen)
	{
		printf("Receiving failed with error code: %d\n", WSAGetLastError());
		closesocket(serverSocket);
		closesocket(clientSocket);
		WSACleanup();
		return 0;
	}
	else printf("Successfully received the client's name from the client!\n\n\n\n");
	strcat_s(nameOther, "\0");


	time_t Time_now;

	//进入消息循环，一直接收客户端发来的消息 并且把消息发送回去
	while (1)
	{
		memset(buffer, 0, sizeof(buffer));
		//接收客户端消息
		iRecvLen = recv(clientSocket, buffer, sizeof(buffer), 0);
		if (SOCKET_ERROR == iRecvLen)
		{
			printf("Receiving failed with error code: %d\n", WSAGetLastError());
			closesocket(serverSocket);
			closesocket(clientSocket);
			WSACleanup();
			return 0;
		}
		//printf("recv %d bytes from %s: ", iRecvLen, nameOther);
		strcat_s(buffer, "\0"); //接收到的数据末尾没有结束符，无法直接作为字符串打印，为此要补上一个结束符
		printf("%s: %s\n", nameOther, buffer);

		//发送消息给客户端
		memset(buffer, 0, sizeof(buffer));
		printf("%s: ", name);
		gets_s(buffer);
		//获取当前系统时间
		Time_now = time(0);
		char* msg = ctime(&Time_now);
		char* sendingTime = msg;
		strncat(msg, buffer, sizeof(buffer));
		//如果对方输入的是bye，就结束整个程序（退出聊天）
		if (strcmp(buffer, "bye") == 0) break;
		iSendLen = send(clientSocket, msg, strlen(msg), 0);
		if (SOCKET_ERROR == iSendLen)
		{
			printf("Sending failed with error code: %d\n", WSAGetLastError());
			closesocket(serverSocket);
			closesocket(clientSocket);
			WSACleanup();
			return 0;
		}
		printf("Successfully send message to client: %s\nSend time: %s\n", nameOther, sendingTime);

	}
	//先关闭给客户端创建的socket，再关闭主socket
	closesocket(clientSocket);
	closesocket(serverSocket);
	WSACleanup();
	return 0;
}

