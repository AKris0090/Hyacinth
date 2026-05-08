#include "pch.h"
#include "framework.h"
#include "hyacinth_network.h"

std::string ClientRequestConnectionPacket::toString() {
	std::ostringstream oss;
	oss << port << "," << tick << ",";
	return oss.str();
}

void ClientRequestConnectionPacket::fromString(std::string s) {
	std::stringstream es(s);
	std::string field;

	std::getline(es, field, ','); port = std::stoi(field);
	std::getline(es, field, ','); tick = std::stoi(field);
}

std::string ClientUpdatePacket::toString() {
	std::ostringstream oss;
	oss << id << "," 
		<< tick << ","
		<< ack << ","
		<< pitch << "," 
		<< yaw << "," 
		<< static_cast<int>(movementFB) << ","
		<< static_cast<int>(movementLR) << ","
		<< jump << ","
		<< lmb << ",";
	return oss.str();
}

ClientUpdatePacket ClientUpdatePacket::fromString(std::string s) {
	ClientUpdatePacket p{};

	std::stringstream es(s);
	std::string field;
	std::getline(es, field, ','); p.id = std::stoi(field);
	std::getline(es, field, ','); p.tick = std::stoi(field);
	std::getline(es, field, ','); p.ack = std::stoi(field);
	std::getline(es, field, ','); p.pitch = std::stof(field);
	std::getline(es, field, ','); p.yaw = std::stof(field);
	std::getline(es, field, ','); p.movementFB = std::stoi(field);
	std::getline(es, field, ','); p.movementLR = std::stoi(field);
	std::getline(es, field, ','); p.jump = std::stoi(field);
	std::getline(es, field, ','); p.lmb = std::stoi(field);

	return p;
}

std::string ServerSnapshot::toString() {
	std::ostringstream oss;
	oss << processedTickNum << "," << time << "," << serverTickNum << ",";
	for (size_t i = 0; i < entities.size(); i++) {
		const Entity& e = entities[i];
		oss << e.id << "," << e.transform.position.x
			<< "," << e.transform.position.y
			<< "," << e.transform.position.z
			<< "," << e.transform.pitch
			<< "," << e.transform.yaw
			<< "," << e.isMoving
			<< "," << e.shotAck;
		if (i + 1 < entities.size()) oss << "|";
	}
	return oss.str();
}

ServerSnapshot ServerSnapshot::fromString(std::string s) {
	ServerSnapshot packet;

	std::stringstream ss(s);
	std::string entityStr;

	std::string tickField;
	std::getline(ss, tickField, ','); packet.processedTickNum = std::stoi(tickField);
	std::getline(ss, tickField, ','); packet.time = std::stoi(tickField);
	std::getline(ss, tickField, ','); packet.serverTickNum = std::stoi(tickField);

	while (std::getline(ss, entityStr, '|')) {
		std::stringstream es(entityStr);
		std::string field;
		Entity e;

		std::getline(es, field, ','); e.id = std::stoi(field);
		std::getline(es, field, ','); e.transform.position.x = std::stof(field);
		std::getline(es, field, ','); e.transform.position.y = std::stof(field);
		std::getline(es, field, ','); e.transform.position.z = std::stof(field);
		std::getline(es, field, ','); e.transform.pitch = std::stof(field);
		std::getline(es, field, ','); e.transform.yaw = std::stof(field);
		std::getline(es, field, ','); e.isMoving = std::stoi(field);
		std::getline(es, field, ','); e.shotAck = std::stoi(field);

		e.transform.setRotationPitchYaw();

		packet.entities.push_back(e);
	}

	return packet;
}

void SimulateStruct::addPacket(ClientUpdatePacket pack) {
	id = pack.id;
	tick = pack.tick;
	pitch = pack.pitch;
	yaw = pack.yaw;
	movementFB = pack.movementFB;
	movementLR = pack.movementLR;
	jump = pack.jump;
	shooting = pack.lmb;

	ackedTick = pack.ack;
	receivedTimestamp = pack.serverTimestamp;
}

bool ServersideClient::getPacketFor(uint32_t tickNum) {
	if (clientPacketBuffer.empty()) {
		bufferedPacket.addPacket(previousInput);
		return false;
	}

	tickNum -= tickBasis;
	while (!clientPacketBuffer.empty() && (clientPacketBuffer.top().tick < tickNum)) {
		clientPacketBuffer.pop();
	}
	if (!clientPacketBuffer.empty() && clientPacketBuffer.top().tick == tickNum) {
		bufferedPacket.addPacket(clientPacketBuffer.top());
		previousInput = clientPacketBuffer.top();
		clientPacketBuffer.pop();
		return true;
	}
	else {
		bufferedPacket.addPacket(previousInput);
		return false;
	}
}

bool ServersideClient::foundTimestamp(uint32_t serverTickNum) {
	if (sendTimestamps.empty()) return false;

	while (!sendTimestamps.empty() && (sendTimestamps.front().first < serverTickNum)) {
		sendTimestamps.pop();
	}
	if (!sendTimestamps.empty() && sendTimestamps.front().first == serverTickNum) {
		return true;
	}
	else {
		return false;
	}
}