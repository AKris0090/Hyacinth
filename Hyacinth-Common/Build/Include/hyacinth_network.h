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

#pragma comment(lib, "Ws2_32.lib")

constexpr int DEFAULT_LEN = 512;

struct Entity {
	uint32_t id;
	Transform transform;
};

struct ClientRequestConnectionPacket {
	uint32_t port;

	std::string toString();
	void fromString(std::string s);
};

struct ClientUpdatePacket {
	uint32_t id;
	float movementX;
	float movementY;
	float movementZ;

	std::string toString();
	void print() const;
};

struct ServersideClient {
	Entity entity;
	uint32_t id;
	sockaddr_in clientAddr;
};

struct ServerPacket {
	Entity* entities;
};

ClientUpdatePacket decomposePacket(char buff[DEFAULT_LEN]);