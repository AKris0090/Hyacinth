#include "pch.h"
#include "framework.h"
#include "hyacinth_network.h"

std::string ClientUpdatePacket::toString() {
	std::ostringstream oss;
	oss << (int) id << "," << (float) tDelta << "," << (float) xRelMouse << "," << (float) yRelMouse << "," << (int) movementFB << "," << (int) movementLR << "," << (int) movementUD;
	return oss.str();
}

ClientUpdatePacket ClientUpdatePacket::fromString(std::string s) {
	ClientUpdatePacket p{};

	std::stringstream es(s);
	std::string field;
	std::getline(es, field, ','); p.id = std::stoi(field);
	std::getline(es, field, ','); p.tDelta = std::stof(field);
	std::getline(es, field, ','); p.xRelMouse = std::stof(field);
	std::getline(es, field, ','); p.yRelMouse = std::stof(field);
	std::getline(es, field, ','); p.movementFB = std::stoi(field);
	std::getline(es, field, ','); p.movementLR = std::stoi(field);
	std::getline(es, field, ','); p.movementUD = std::stoi(field);

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
		oss << e.id << "," << e.transform.position.x
			<< "," << e.transform.position.y
			<< "," << e.transform.position.z
			<< "," << e.transform.rotation.x
			<< "," << e.transform.rotation.y
			<< "," << e.transform.rotation.z
			<< "," << e.transform.rotation.w
			<< "," << e.transform.pitch
			<< "," << e.transform.yaw;
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
		std::getline(es, field, ','); e.transform.position.x = std::stof(field);
		std::getline(es, field, ','); e.transform.position.y = std::stof(field);
		std::getline(es, field, ','); e.transform.position.z = std::stof(field);
		std::getline(es, field, ','); e.transform.rotation.x = std::stof(field);
		std::getline(es, field, ','); e.transform.rotation.y = std::stof(field);
		std::getline(es, field, ','); e.transform.rotation.z = std::stof(field);
		std::getline(es, field, ','); e.transform.rotation.w = std::stof(field);
		std::getline(es, field, ','); e.transform.pitch = std::stof(field);
		std::getline(es, field, ','); e.transform.yaw = std::stof(field);

		packet.entities.push_back(e);
	}

	return packet;
}

void SimulateStruct::addPacket(ClientUpdatePacket pack) {
	id = pack.id;
	xRelMouse += pack.xRelMouse * pack.tDelta;
	yRelMouse += pack.yRelMouse * pack.tDelta;
	movementFB = pack.movementFB;
	movementLR = pack.movementLR;
	movementUD = pack.movementUD;
}
