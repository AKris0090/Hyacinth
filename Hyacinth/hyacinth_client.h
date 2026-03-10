#pragma once

#include "transform.h"

#define DEFAULT_PORT "6767"
#define SERVER_UDP_PORT "6969"

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <iostream>
#include <string>

class HyacinthNetworkClient {
private:
	bool connected;
	uint32_t clientID;
	SOCKET connectSocket;
	SOCKET udpReceiverSocket;
	int receiverPort;
	SOCKET serverUDPSocket;
	sockaddr serverAddress;
	int serverAddressLen;
public:
	int setup(std::string serveraddr);
	void sendMovementString(Transform& t);
};