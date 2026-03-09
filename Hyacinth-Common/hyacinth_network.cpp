#include "pch.h"
#include "framework.h"
#include "hyacinth_network.h"

void ClientPacket::print() const {
	std::cout << "id: " << id << ", movement: " << movementX << ", " << movementY << ", " << movementZ << std::endl;
}

std::string ClientPacket::toString() {
	return "id:" + std::to_string(id) +
		"posX:" + std::to_string(movementX) +
		"posY:" + std::to_string(movementY) +
		"posZ:" + std::to_string(movementZ);
}

ClientPacket decomposePacket(char buff[DEFAULT_LEN]) {
	ClientPacket p;

	std::string s = std::string(buff);
	size_t pos_id = s.find("id:");
	size_t pos_x = s.find("posX:");
	pos_id += 3;
	p.id = static_cast<uint32_t>(std::stoi(s.substr(pos_id, pos_x - pos_id)));

	pos_x += 5;
	size_t pos_y = s.find("posY:");
	p.movementX = static_cast<int8_t>(std::stoi(s.substr(pos_x, pos_y - pos_x)));

	pos_y += 5;
	size_t pos_z = s.find("posZ:");
	p.movementY = static_cast<int8_t>(std::stoi(s.substr(pos_y, pos_z - pos_y)));

	pos_z += 5;
	size_t length = s.length();
	p.movementZ = static_cast<int8_t>(std::stoi(s.substr(pos_z, length - pos_z)));

	return p;
}