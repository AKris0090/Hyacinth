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
#include <unordered_map>
#include "transform.h"
#include "entity.h"
#include <chrono>
#include <sstream>

#pragma comment(lib, "Ws2_32.lib")

using namespace std::chrono_literals;

constexpr int DEFAULT_LEN = 512;
constexpr int MAX_CONNECTIONS = 12;

// constexpr float SERVER_TIMESTEP = 0.0078125f;
// constexpr std::chrono::duration<double, std::milli> SERVER_TIMESTEP_MS = 7.8125ms;

// constexpr float SERVER_TIMESTEP = 0.016667f;
// constexpr std::chrono::duration<double, std::milli> SERVER_TIMESTEP_MS = 16.6667ms;

constexpr float SERVER_TIMESTEP = 0.1f;
constexpr std::chrono::duration<double, std::milli> SERVER_TIMESTEP_MS = 100.0ms;

struct ClientRequestConnectionPacket {
	uint32_t port;

	std::string toString();
	void fromString(std::string s);
};

struct ClientUpdatePacket {
	uint32_t id;
	float pitch = 0.f;
	float yaw = 0.f;
	float movementFB = 0;
	float movementLR = 0;
	float movementUD = 0;

	std::string toString();
	static ClientUpdatePacket fromString(std::string s);
};

struct SimulateStruct {
	uint32_t id;
	float pitch = 0.f;
	float yaw = 0.f;
	float movementFB = 0; // TODO: might need to be cast to larger values
	float movementLR = 0;
	float movementUD = 0;

	void addPacket(ClientUpdatePacket pack);
	void reset() {
		pitch = 0.f;
		yaw = 0.f;
		movementFB = 0;
		movementLR = 0;
		movementUD = 0;
	}
};

struct ServersideClient {
	Entity entity;
	SimulateStruct bufferedPacket;
	uint32_t id;
	sockaddr_in clientAddr;
	uint32_t tickOffset;
	long long heartBeat;
	int clientAddrLen;
};

struct ServerPacket {
	std::vector<Entity> entities;
	uint32_t processedTickNum = 0;

	std::string toString();
	static ServerPacket fromString(std::string s);
};

struct EntityManager {
	std::unordered_map<uint32_t, ServersideClient*> clients;
};