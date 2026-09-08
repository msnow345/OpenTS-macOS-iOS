#pragma once
#include <windows.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
typedef struct WSAData { WORD wVersion; WORD wHighVersion; char szDescription[257]; char szSystemStatus[129]; unsigned short iMaxSockets; unsigned short iMaxUdpDg; char *lpVendorInfo; } WSADATA, *LPWSADATA;
