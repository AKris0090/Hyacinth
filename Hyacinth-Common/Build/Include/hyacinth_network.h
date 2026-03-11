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
#include <sstream>

#pragma comment(lib, "Ws2_32.lib")

constexpr int DEFAULT_LEN = 512;

struct Entity {
	uint32_t id;
	glm::vec3 pos;
	glm::vec3 rot;

	std::string toString();
	Entity fromString(std::string s);
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
	int clientAddrLen;
};

struct ServerPacket {
	std::vector<Entity> entities;

	std::string toString();
	ServerPacket fromString(std::string);
};

ClientUpdatePacket decomposePacket(char buff[DEFAULT_LEN]);