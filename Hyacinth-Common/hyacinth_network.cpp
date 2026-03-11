#include "pch.h"
#include "framework.h"
#include "hyacinth_network.h"

void ClientUpdatePacket::print() const {
	std::cout << "id: " << id << ", movement: " << movementX << ", " << movementY << ", " << movementZ << std::endl;
}

std::string ClientUpdatePacket::toString() {
	std::ostringstream oss;
	oss << id << "," << movementX << "," << movementY << "," << movementZ;
	return oss.str();
}

ClientUpdatePacket decomposePacket(char buff[DEFAULT_LEN]) {
	ClientUpdatePacket p{};

	std::string s = std::string(buff);
	std::stringstream es(s);
	std::string field;
	std::getline(es, field, ','); p.id = std::stoi(field);
	std::getline(es, field, ','); p.movementX = std::stof(field);
	std::getline(es, field, ','); p.movementY = std::stof(field);
	std::getline(es, field, ','); p.movementZ = std::stof(field);

	return p;
}

std::string ClientRequestConnectionPacket::toString() {
	return std::string("myport:") + std::to_string(port);
}

void ClientRequestConnectionPacket::fromString(std::string s) {
	size_t start = s.find("myport:") + 7;
	port = static_cast<uint32_t>(stoi(s.substr(start, s.length() - start)));
}

std::string ServerPacket::toString() {
	std::ostringstream oss;
	for (size_t i = 0; i < entities.size(); i++) {
		const Entity& e = entities[i];
		oss << e.id << "," << e.pos.x
			<< "," << e.pos.y 
			<< "," << e.pos.z 
			<< "," << e.rot.x 
			<< "," << e.rot.y 
			<< "," << e.rot.z;
		if (i + 1 < entities.size()) oss << "|";
	}
	return oss.str();
}

ServerPacket ServerPacket::fromString(std::string s) {
	ServerPacket packet;

	std::stringstream ss(s);
	std::string entityStr;

	while (std::getline(ss, entityStr, '|')) {
		std::stringstream es(entityStr);
		std::string field;
		Entity e;

		std::getline(es, field, ','); e.id = std::stoi(field);
		std::getline(es, field, ','); e.pos.x = std::stof(field);
		std::getline(es, field, ','); e.pos.y = std::stof(field);
		std::getline(es, field, ','); e.pos.z = std::stof(field);
		std::getline(es, field, ','); e.rot.x = std::stof(field);
		std::getline(es, field, ','); e.rot.y = std::stof(field);
		std::getline(es, field, ','); e.rot.z = std::stof(field);

		packet.entities.push_back(e);
	}

	return packet;
}