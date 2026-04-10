#pragma once

#include <stdint.h>
#include <iostream>
#include <vector>
#include <queue>
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

//constexpr float SERVER_TIMESTEP = 0.0078125f;
//constexpr std::chrono::duration<double, std::milli> SERVER_TIMESTEP_MS = 7.8125ms;

//constexpr float SERVER_TIMESTEP = 0.01f;
//constexpr std::chrono::duration<double, std::milli> SERVER_TIMESTEP_MS = 10.0ms;

constexpr float SERVER_TIMESTEP = 0.015625f;
constexpr std::chrono::duration<double, std::milli> SERVER_TIMESTEP_MS = 15.625ms;

static auto now() {
	return std::chrono::steady_clock::now();
}

static uint64_t getNowMs() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()
	).count();
}

struct ClientRequestConnectionPacket {
	uint32_t port;
	uint32_t tick = 0;

	std::string toString();
	void fromString(std::string s);
};

struct ClientUpdatePacket {
	uint32_t id;
	uint32_t tick = 0;
	uint64_t time = 0;
	float pitch = 0.f;
	float yaw = 0.f;
	float movementFB = 0;
	float movementLR = 0;
	float movementUD = 0;

	std::string toString();
	static ClientUpdatePacket fromString(std::string s);
};

const auto clientPacketCmp = [](const ClientUpdatePacket& a, const ClientUpdatePacket& b) {
	return a.tick > b.tick;
};

struct SimulateStruct {
	uint32_t id;
	float tick = 0.f;
	float pitch = 0.f;
	float yaw = 0.f;
	float movementFB = 0; // TODO: might need to be cast to larger values
	float movementLR = 0;
	float movementUD = 0;

	void addPacket(ClientUpdatePacket pack);
	void reset() {
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
	std::priority_queue<ClientUpdatePacket, std::vector<ClientUpdatePacket>, decltype(clientPacketCmp)> clientPacketBuffer;
	long long heartBeat;
	uint64_t ping;
	int clientAddrLen;

	void getPacketFor(uint32_t tickNum) {
		 if (clientPacketBuffer.empty()) return;
		 while (!clientPacketBuffer.empty() && (clientPacketBuffer.top().tick < tickNum)) {
		 	clientPacketBuffer.pop();
		 }
		 if (!clientPacketBuffer.empty() && clientPacketBuffer.top().tick == tickNum) {
		 	bufferedPacket.addPacket(clientPacketBuffer.top());
		 	clientPacketBuffer.pop();
		 }
		 else {
		 	bufferedPacket.addPacket(ClientUpdatePacket{});
		 }
	}
};

struct ServerPacket {
	std::vector<Entity> entities;
	uint32_t processedTickNum = 0;
	uint64_t time = 0;

	std::string toString();
	static ServerPacket fromString(std::string s);
};

struct EntityManager {
	std::unordered_map<uint32_t, ServersideClient*> clients;
};