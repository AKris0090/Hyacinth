#include "pch.h"
#include "framework.h"
#include "hyacinth_network.h"

void ClientUpdatePacket::print() const {
	std::cout << "id: " << id << ", movement: " << movementX << ", " << movementY << ", " << movementZ << std::endl;
}

std::string ClientUpdatePacket::toString() {
	return "id:" + std::to_string(id) +
		"posX:" + std::to_string(movementX) +
		"posY:" + std::to_string(movementY) +
		"posZ:" + std::to_string(movementZ);
}

ClientUpdatePacket decomposePacket(char buff[DEFAULT_LEN]) {
	ClientUpdatePacket p{};

	std::string s = std::string(buff);
	size_t pos_id = s.find("id:");
	size_t pos_x = s.find("posX:");
	pos_id += 3;
	p.id = static_cast<uint32_t>(std::stoi(s.substr(pos_id, pos_x - pos_id)));

	pos_x += 5;
	size_t pos_y = s.find("posY:");
	p.movementX = std::stof(s.substr(pos_x, pos_y - pos_x));

	pos_y += 5;
	size_t pos_z = s.find("posZ:");
	p.movementY = std::stof(s.substr(pos_y, pos_z - pos_y));

	pos_z += 5;
	size_t length = s.length();
	p.movementZ = std::stof(s.substr(pos_z, length - pos_z));

	return p;
}

std::string ClientRequestConnectionPacket::toString() {
	return std::string("myport:") + std::to_string(port);
}

void ClientRequestConnectionPacket::fromString(std::string s) {
	std::cout << s << std::endl;
	size_t start = s.find("myport:") + 7;
	std::cout << start << ", " << s.length() - start << std::endl;
	port = static_cast<uint32_t>(stoi(s.substr(start, s.length() - start)));
}