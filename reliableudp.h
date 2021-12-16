#pragma comment(lib, "Ws2_32.lib")
#include<iostream>
#include<WinSock2.h>
#include<string>
#include<string.h>
#include<time.h>
#include<fstream>
#include<stdio.h>
#include<vector>
#include<thread>
#include<mutex>
using namespace std;
SOCKET server, client;
const int timeout = 200;//超时设定
const int datalen = 14994;//数据包数据长度
const int pkgheaderlen = 6;//数据包头部长度
const int pkglength = 15000;//数据包长度
const int maxn = 2e8;//内存中最大缓存数据包个数
SOCKADDR_IN serverAddr, clientAddr;
unsigned char checksum(char* ch, int len)
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