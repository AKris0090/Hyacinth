#pragma once

#include "ecshelpers.h"

#define DEFAULT_PORT "6767"

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <iostream>

class HyacinthNetworkClient {
private:
	bool connected;
	SOCKET connectSocket;
	sockaddr* serverAddress;
	int serverAddressLen;
public:
	int setup(std::string serveraddr);
	void sendMovementString(Transform& t);
};