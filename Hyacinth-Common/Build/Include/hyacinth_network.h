#pragma once

#include <stdint.h>
#include <iostream>
#include <vector>
#include <deque>
#include <string>

constexpr int DEFAULT_LEN = 512;

struct Entity {
	uint32_t id;
	float posX;
	float posY;
	float posZ;
};

struct ClientPacket {
	uint32_t id;
	int8_t movementX;
	int8_t movementY;
	int8_t movementZ;

	std::string toString();
	void print() const;
};

struct ServerEntity {
	Entity entity;
	std::deque<ClientPacket> inputPackets;
};

struct ServerPacket {
	Entity* entities;
};

ClientPacket decomposePacket(char buff[DEFAULT_LEN]);