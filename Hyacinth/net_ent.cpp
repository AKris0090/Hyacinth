#include "net_ent.h"

void NetworkEntityManager::updateEntitiesFromPacket(ServerSnapshot& p, uint32_t currentClientID, float deltaTime) {
	for (const auto& e : p.entities) {
		if (e.id == currentClientID) {
			self->flashPercentage = e.flashPercentage;
			self->flashNDCX = e.flashNDCX;
			self->flashNDCY = e.flashNDCY;
			continue;
		}
		auto findit = entities.find(e.id);
		if (findit == entities.end()) { // entity that is sent is not there in current entity list
			entities[e.id] = new Entity();
			entities[e.id]->type = e.type;
			entities[e.id]->id = e.id;
			if (entities[e.id]->type == E_PLAYER) {
				HAnimatedGameObject* ao = new HTPCharacter(characterMeshRef);
				AnimatedObjectWrap wrap;
				wrap.gameObject = ao;
				wrap.jointMatrixBuffer = vkdeviceutils::createBuffer(ao->mesh->jointMatrixSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "obj_joint_matrix_buffer_char");
				gameObjects[e.id] = wrap;
			}
			else {
				HAnimatedGameObject* ao = new HFlashBang(flashMeshRef);
				AnimatedObjectWrap wrap;
				wrap.gameObject = ao;
				wrap.jointMatrixBuffer = vkdeviceutils::createBuffer(ao->mesh->jointMatrixSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "obj_joint_matrix_buffer_flash");
				gameObjects[e.id] = wrap;
			}
		}
		entities[e.id]->transform.position = e.transform.position;

		if (entities[e.id]->type == E_PLAYER) {
			entities[e.id]->transform.pitch = e.transform.pitch;
			entities[e.id]->transform.yaw = e.transform.yaw;
			entities[e.id]->isMoving = e.isMoving;
		}
		entities[e.id]->updated = true; // saw entity in packet
	}
	std::queue<uint32_t> deletionQueue;
	for (const auto& [id, ent] : entities) {
		if (!ent->updated) {
			deletionQueue.push(id);
			continue;
		}
		ent->updated = false; // reset flag
		if (ent->type == E_PLAYER) {
			dynamic_cast<HTPCharacter*>(gameObjects[id].gameObject)->controller.updateAnimParams(ent);
		}
		gameObjects[id].gameObject->updateAnimation(deltaTime);
		memcpy(gameObjects[id].jointMatrixBuffer.pMappedData, gameObjects[id].gameObject->jointMatrixData, gameObjects[id].gameObject->mesh->jointMatrixSize);
		gameObjects[id].gameObject->transform = entities[id]->transform;
	}
	while (deletionQueue.size() > 0) {
		uint32_t id = deletionQueue.front();
		entities.erase(id);
		gameObjects[id].gameObject->destroy();
		vkdeviceutils::destroyBuffer(gameObjects[id].jointMatrixBuffer);
		gameObjects.erase(id);
		deletionQueue.pop();
	}
}

void NetworkEntityManager::setupFromServerPacket(ServerSnapshot& p, LightMesh* characterMesh, LightMesh* flashMesh, uint32_t currentClientID) {
	characterMeshRef = characterMesh;
	flashMeshRef = flashMesh;
	updateEntitiesFromPacket(p, currentClientID, 0);
}

void NetworkEntityManager::clearPendingPackets(Entity* self, LightMesh* meshRef) {
	while (!rB.pendingPackets.empty()) {
		uint32_t checkTick = rB.ringBuffer.front().tickNum;

		auto& pending = rB.pendingPackets.front();

		if (pending.processedTickNum < checkTick) {
			rB.pendingPackets.pop();
			continue;
		}

		bool found = false;
		StateStorage sS;
		for (auto& s : rB.ringBuffer)
			if (s.tickNum == pending.processedTickNum) { found = true; sS = s; break; }
		if (!found) break;

		Transform serverTransform;
		bool haveShotAck = false;
		for (auto& e : pending.entities) {
			if (e.id == self->id) {
				serverTransform = e.transform;
				haveShotAck = e.shotAck;
			}
			else {
				// if shot ack, add a tracer
				if (e.shotAck) {
					glm::vec3 hitPos = e.hitPos;
					glm::vec3 origin = e.transform.position + glm::vec3(0.f, 1.85f, 0.f);
					origin += e.transform.forward * 0.6f;
					origin -= e.transform.up * 0.5f;
					glm::vec3 dir = hitPos - origin;
					glm::vec3 normDir = glm::normalize(dir);
					Transform t;
					t.scale.x = glm::length(dir);
					t.yaw = glm::degrees(glm::atan2(normDir.z, normDir.x));
					t.pitch = glm::degrees(glm::asin(normDir.y));
					t.setRotationPitchYaw();
					t.position = origin;
					tracerManager->addTracer(t.getMatrix(), meshRef);
				}
			}
		}

		if (haveShotAck) {
			for (auto& e : pending.entities) {
				if (e.id == 1) {
					shotAckPosition = e.transform.position;
				}
			}
		}

		std::pair<Transform, Transform> t;
		if (rB.checkPacketNeedsRewind(self, t, serverTransform, sS.tickNum)) {
			selfSimBuffer.packetBuffer.first.entities[0].transform = t.first;
			selfSimBuffer.packetBuffer.second.entities[0].transform = t.second;
		}
		rB.pendingPackets.pop();
	}
}

void NetworkEntityManager::shutdown() {
	if (gameObjects.size() == 0) return;
	for (auto& [id, ao] : gameObjects) {
		gameObjects[id].gameObject->destroy();
		vkdeviceutils::destroyBuffer(gameObjects[id].jointMatrixBuffer);
	}
}

std::pair<Transform, Transform> RewindBuffer::rewindState(Transform newTransform, uint32_t tickNum) {
	std::shared_lock<std::shared_mutex> lock(rBMutex);
	physicsPosition(newTransform.position);

	Transform prev;

	bool skip = true;
	std::cout << "rewinded ticks: ";
	for (int i = 0; i < ringBuffer.size(); i++) {
		StateStorage& currentState = ringBuffer[i];
		if (currentState.tickNum == tickNum) { skip = false; currentState.state = newTransform; continue; }
		if (skip) continue;
		std::cout << currentState.tickNum << " ";
		newTransform.pitch = currentState.state.pitch;
		newTransform.yaw = currentState.state.yaw;
		newTransform.setRotationPitchYaw();
		physicsStep(newTransform, currentState.fb, currentState.lr);
		currentState.state = newTransform;
		if (i == ringBuffer.size() - 2) {
			prev = newTransform;
		}
	}
	std::cout << std::endl;

	return std::pair<Transform, Transform>(prev, newTransform);
}

// sp.processedTickNum is translated to client local tick
bool RewindBuffer::checkPacketNeedsRewind(Entity* self, std::pair<Transform, Transform>& outTransform, Transform serverTransform, uint32_t processedTickNum) {
	while (!ringBuffer.empty() && ringBuffer.front().tickNum != processedTickNum) {
		ringBuffer.pop_front();
	}

	if (ringBuffer.empty()) return false;

	int stateIndex = 0;
	for (int i = 0; i < ringBuffer.size(); i++) {
		StateStorage& state = ringBuffer[i];
		if (state.tickNum == processedTickNum) {
			stateIndex = i;
			break;
		}
	}

	// if the difference in own position is greater than the threshold, initiate rewind
	if (glm::length(ringBuffer[stateIndex].state.position - serverTransform.position) > DIFF_THRESHOLD) {
		outTransform = rewindState(serverTransform, processedTickNum);
		return true;
	}

	return false;
}

