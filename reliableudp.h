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
SOCKADDR_IN serverAddr, clientAddr;
using namespace std;
const int maxwindowsize = 16;// 滑动窗口大小
const int timeout = 500;
const int maxn = 2e8;//支持传送文件的最大大小
const int datalen = 16000;//数据段最大长度
const int pkglength = 16007;//报文总长度
const int pkgheader = 7;//报文头部大小
