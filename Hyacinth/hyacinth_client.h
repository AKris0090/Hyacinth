#pragma once

#include "transform.h"

#define SERVER_UDP_PORT "6767"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <iostream>
#include <string>
#include <thread>
#include "net_ent.h"
#include "input.h"
#include "time.h"
#include <cmath>

class HyacinthNetworkClient {
private:
	bool connected;
	uint32_t clientID;
	SOCKET twoWayUDPSocket;
	int receiverPort;
	sockaddr serverAddress;
	int serverAddressLen;

	uint32_t serverAck;

	void listenForServer(SOCKET twoWayUDPSocket);

public:
	NetworkEntityManager netEntManager;
	LagSimulator lagSim;

	int setup(std::string serveraddr, SWChainImageFormat swImageFormat, VkDescriptorSetLayout& uniformLayout);
	void updateServerTick(ClientUpdatePacket& p, bool mouseLocked);
	void shutdownNet();
};