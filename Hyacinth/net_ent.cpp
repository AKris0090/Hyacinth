#include "net_ent.h"

void NetworkEntityManager::updateEntitiesFromPacket(ServerSnapshot& p, uint32_t currentClientID, float deltaTime) {
	for (const auto& e : p.entities) {
		if (e.id == currentClientID) {
			continue;
		}
		auto findit = entities.find(e.id);
		if (findit == entities.end()) { // entity that is sent is not there in current entity list
			ids.push_back(e.id);
			entities[e.id] = new Entity();
			entities[e.id]->id = e.id;
			entityJointBuffers[e.id] = vkdeviceutils::createBuffer(characterObject->skinSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "obj_skin_matrix_buffer");
			entityAnimationControllers[e.id] = ThirdPersonAnimationController();
			characterObject->setTPControllerParameters(entityAnimationControllers[e.id], characterObject->skins[0]);
		}
		entities[e.id]->transform.position = e.transform.position;
		entities[e.id]->transform.pitch = e.transform.pitch;
		entities[e.id]->transform.yaw = e.transform.yaw;
		entities[e.id]->isMoving = e.isMoving;
	}
	for (int i = 0; i < ids.size(); i++) {
		gltfObject::updateThirdPersonAnimation(entities[ids[i]], characterObject, *characterObject->thirdPersonAnimStateMachine, entityAnimationControllers[ids[i]], deltaTime, entityJointBuffers[ids[i]].pMappedData);
	}
}

void NetworkEntityManager::setupFromServerPacket(ServerSnapshot& p, uint32_t currentClientID) {
	if (p.entities.size() > 0) {
		for (const auto& e : p.entities) {
			if (e.id == currentClientID) continue;
			Entity* newEnt = new Entity;
			newEnt->id = e.id;
			newEnt->transform.position = e.transform.position;
			newEnt->transform.pitch = e.transform.pitch;
			newEnt->transform.yaw = e.transform.yaw;
			entities[e.id] = newEnt;
			ids.push_back(e.id);
			entityJointBuffers[e.id] = vkdeviceutils::createBuffer(characterObject->skinSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "obj_skin_matrix_buffer");
			entityAnimationControllers[e.id] = ThirdPersonAnimationController();
			characterObject->setTPControllerParameters(entityAnimationControllers[e.id], characterObject->skins[0]);
		}
	}

	firstPersonObject->setFPControllerParameters(firstPersonAnimationController, firstPersonObject->skins[0]);
	firstPersonJointBuffer = vkdeviceutils::createBuffer(firstPersonObject->skinSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "obj_skin_matrix_buffer_fp");

	pistolObject->setWeaponControllerParams(pistolAnimationController, pistolObject->skins[0]);
	pistolJointBuffer = vkdeviceutils::createBuffer(pistolObject->skinSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "obj_skin_matrix_buffer_pistol");
}

void NetworkEntityManager::drawEntities(VkCommandBuffer& cmd, VulkanPipelineBuilder& pipelineUtil, uint32_t numDrawCommands, VulkanBuffer& dynamicIndirectBuffer, GPUDrawPushConstants& pc) {
	for (int i = 0; i < ids.size(); i++) {
		pc.entityMatrix = entities[ids[i]]->transform.getPositionMatrix();
		pc.jointBufferAddress = entityJointBuffers[ids[i]].gpuAddress;

		vkCmdPushConstants(cmd, pipelineUtil.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GPUDrawPushConstants), &pc);

		vkCmdDrawIndexedIndirect(cmd, dynamicIndirectBuffer.buffer, 0, numDrawCommands, sizeof(VkDrawIndexedIndirectCommand));
	}
}

void NetworkEntityManager::clearPendingPackets(Entity* self) {
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
	for (auto& [id, buff] : entityJointBuffers) {
		vkdeviceutils::destroyBuffer(buff);
	}
	vkdeviceutils::destroyBuffer(firstPersonJointBuffer);
	vkdeviceutils::destroyBuffer(pistolJointBuffer);
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

	if (glm::length(ringBuffer[stateIndex].state.position - serverTransform.position) > DIFF_THRESHOLD) {
		// std::cout << "tick: " << processedTickNum << " | ";
		// for (auto& pack : ringBuffer) {
		// 	std::cout << "|" << pack.tickNum << ":" << (glm::length(pack.state.position - serverTransform.position) <= DIFF_THRESHOLD);
		// }
		// std::cout << "|" << std::endl;
		// std::cout << "diff: " << glm::length(ringBuffer[ind].state.position - serverTransform.position) << std::endl;
		outTransform = rewindState(serverTransform, processedTickNum);
		return true;
	}

	return false;
}

