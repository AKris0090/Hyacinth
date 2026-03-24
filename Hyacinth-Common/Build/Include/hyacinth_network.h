#pragma once

#include <stdint.h>
#include <iostream>
#include <vector>
#include <deque>
#include <string>
#include <utility>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include "transform.h"
#include "entity.h"
#include <sstream>

#pragma comment(lib, "Ws2_32.lib")

constexpr int DEFAULT_LEN = 512;

struct ClientRequestConnectionPacket {
	uint32_t port;

	std::string toString();
	void fromString(std::string s);
};

struct ClientUpdatePacket {
	uint32_t id;
	float tDelta = 0.f;
	float xRelMouse = 0.f;
	float yRelMouse = 0.f;
	int8_t movementFB = 0;
	int8_t movementLR = 0;
	int8_t movementUD = 0;

	std::string toString();
	static ClientUpdatePacket fromString(std::string s);
};

struct SimulateStruct {
	uint32_t id;
	float xRelMouse = 0.f;
	float yRelMouse = 0.f;
	int8_t movementFB = 0;
	int8_t movementLR = 0;
	int8_t movementUD = 0;

	void addPacket(ClientUpdatePacket pack);
	void reset() {
		xRelMouse = 0.f;
		yRelMouse = 0.f;
		movementFB = 0;
		movementLR = 0;
		movementUD = 0;
	}
};

struct ServersideClient {
	Entity entity;
	SimulateStruct bufferedPackets;
	uint32_t id;
	sockaddr_in clientAddr;
	int clientAddrLen;
};

struct ServerPacket {
	std::vector<Entity> entities;

	std::string toString();
	static ServerPacket fromString(std::string s);
};