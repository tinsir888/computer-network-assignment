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
#define TIMEOUT 500
#define SENDSUCCESS true
#define SENDFAIL false
#define CONNECTSUCCESS true
#define CONNECTFAIL false
#define SEQ1 '1'
#define ACK1 '#'
#define SEQ2 '3'
#define ACK2 SEQ1 + 1
#define SEQ3 '5'
#define ACK3 SEQ2 + 1
#define WAVE1 '7'
#define ACKW1 '#'
#define WAVE2 '9'
#define ACKW2 WAVE1 + 1
#define LENGTH 16377// [1,16378]任意值
#define CheckSum 7
#define LAST '$'
#define NOTLAST '@'
#define ACKMsg '%'
#define TEST '%'
#define NAK '^'
#define MaxScroll 16 // 滑动窗口大小
char message[200000000];