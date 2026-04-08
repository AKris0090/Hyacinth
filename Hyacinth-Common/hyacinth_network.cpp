#include "pch.h"
#include "framework.h"
#include "hyacinth_network.h"

std::string ClientUpdatePacket::toString() {
	std::ostringstream oss;
	oss << (int) id << "," 
		<< pitch << "," 
		<< yaw << "," 
		<< movementFB << ","
		<< movementLR << "," 
		<< movementUD;
	return oss.str();
}

ClientUpdatePacket ClientUpdatePacket::fromString(std::string s) {
	ClientUpdatePacket p{};

	std::stringstream es(s);
	std::string field;
	std::getline(es, field, ','); p.id = std::stoi(field);
	std::getline(es, field, ','); p.pitch = std::stof(field);
	std::getline(es, field, ','); p.yaw = std::stof(field);
	std::getline(es, field, ','); p.movementFB = std::stof(field);
	std::getline(es, field, ','); p.movementLR = std::stof(field);
	std::getline(es, field, ','); p.movementUD = std::stof(field);

	return p;
}

std::string ClientRequestConnectionPacket::toString() {
	std::ostringstream oss;
	oss << port << ",";
	return oss.str();
}

void ClientRequestConnectionPacket::fromString(std::string s) {
	std::stringstream es(s);
	std::string field;

	std::getline(es, field, ','); port = std::stoi(field);
}

std::string ServerPacket::toString() {
	std::ostringstream oss;
	oss << processedTickNum << ",";
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

	std::string tickField;
	std::getline(ss, tickField, ','); packet.processedTickNum = std::stoi(tickField);

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
	pitch = pack.pitch;
	yaw = pack.yaw;
	movementFB = pack.movementFB;
	movementLR = pack.movementLR;
	movementUD = pack.movementUD;
}
