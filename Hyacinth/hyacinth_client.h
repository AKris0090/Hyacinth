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
#include <thread>
#include "net_ent.h"
#include "input.h"
#include "time.h"

class HyacinthNetworkClient {
private:
	bool connected;
	uint32_t clientID;
	bool tickOffsetSet;
	uint32_t tickOffset;
	SOCKET connectSocket;
	SOCKET udpReceiverSocket;
	int receiverPort;
	SOCKET serverUDPSocket;
	sockaddr serverAddress;
	int serverAddressLen;

	void listenForServer(SOCKET udpReceiverSocket);

public:
	NetworkEntityManager netEntManager;

	int setup(std::string serveraddr, SWChainImageFormat swImageFormat, VkDescriptorSetLayout& uniformLayout);
	void updateServerTick(ClientUpdatePacket& p, bool mouseLocked);
	void shutdownNet();
};