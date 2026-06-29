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
				entityJointBuffers[e.id] = vkdeviceutils::createBuffer(characterObject->skinSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "obj_skin_matrix_buffer");
				entityAnimationControllers[e.id] = ThirdPersonAnimationController();
				AnimatedGltfObject::setTPControllerParameters(characterObject, entityAnimationControllers[e.id], characterObject->skins[0]);
			}
			else {
				entityJointBuffers[e.id] = vkdeviceutils::createBuffer(grenadeObject->skinSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "obj_skin_matrix_buffer");
				glm::mat4 id(1.f);
				memcpy(entityJointBuffers[e.id].pMappedData, &id, sizeof(glm::mat4));
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
	for (const auto& [id, ent] : entities) {
		if (!ent->updated) {
			if (ent->type = E_PLAYER) {
				entityAnimationControllers.erase(id);
				vkdeviceutils::destroyBuffer(entityJointBuffers[id]);
				entityJointBuffers.erase(id);
			}
			entities.erase(id);
			continue;
		}
		ent->updated = false; // reset flag
		if (ent->type == E_PLAYER) {
			AnimatedGltfObject::updateThirdPersonAnimation(ent, characterObject, *characterObject->thirdPersonAnimStateMachine, entityAnimationControllers[id], deltaTime, entityJointBuffers[id].pMappedData);
		}
	}
}

void NetworkEntityManager::setupFromServerPacket(ServerSnapshot& p, uint32_t currentClientID) {
	if (p.entities.size() > 0) {
		for (const auto& e : p.entities) {
			if (e.id == currentClientID) continue;
			Entity* newEnt = new Entity;
			newEnt->id = e.id;
			newEnt->type = e.type;
			newEnt->transform.position = e.transform.position;
			entities[e.id] = newEnt;
			if (newEnt->type == E_PLAYER) {
				newEnt->transform.pitch = e.transform.pitch;
				newEnt->transform.yaw = e.transform.yaw;
				entityJointBuffers[e.id] = vkdeviceutils::createBuffer(characterObject->skinSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "obj_skin_matrix_buffer");
				entityAnimationControllers[e.id] = ThirdPersonAnimationController();
				AnimatedGltfObject::setTPControllerParameters(characterObject, entityAnimationControllers[e.id], characterObject->skins[0]);
			}
			else if (newEnt->type == E_GRENADE) {
				entityJointBuffers[e.id] = vkdeviceutils::createBuffer(grenadeObject->skinSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "obj_skin_matrix_buffer");
				glm::mat4 id(1.f);
				memcpy(entityJointBuffers[e.id].pMappedData, &id, sizeof(glm::mat4));
			}
		}
	}

	AnimatedGltfObject::setFPControllerParameters(firstPersonObject, firstPersonAnimationController, firstPersonObject->skins[0]);
	firstPersonJointBuffer = vkdeviceutils::createBuffer(firstPersonObject->skinSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "obj_skin_matrix_buffer_fp");

	AnimatedGltfObject::setWeaponControllerParams(pistolObject, pistolAnimationController, pistolObject->skins[0]);
	pistolJointBuffer = vkdeviceutils::createBuffer(pistolObject->skinSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, "obj_skin_matrix_buffer_pistol");
}

void NetworkEntityManager::drawEntities(VkCommandBuffer& cmd, VulkanPipelineBuilder& pipelineUtil, AnimatedGltfObject* characterObject, AnimatedGltfObject* flashObject, VulkanBuffer& dynamicIndirectBuffer, GPUDrawPushConstants& pc) {
	for (const auto& [id, ent] : entities) {
		pc.entityMatrix = ent->transform.getPositionMatrix();
		pc.jointBufferAddress = entityJointBuffers[id].gpuAddress;

		vkCmdPushConstants(cmd, pipelineUtil.m_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GPUDrawPushConstants), &pc);

		if (ent->type == E_PLAYER) {
			vkCmdDrawIndexedIndirect(cmd, dynamicIndirectBuffer.buffer, characterObject->drawCommandOffset, characterObject->numDrawCommands, sizeof(VkDrawIndexedIndirectCommand));
		}
		else if (ent->type == E_GRENADE) {
			vkCmdDrawIndexedIndirect(cmd, dynamicIndirectBuffer.buffer, flashObject->drawCommandOffset, flashObject->numDrawCommands, sizeof(VkDrawIndexedIndirectCommand));
		}
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
					tracerManager->addTracer(t.getMatrix());
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

	// if the difference in own position is greater than the threshold, initiate rewind
	if (glm::length(ringBuffer[stateIndex].state.position - serverTransform.position) > DIFF_THRESHOLD) {
		outTransform = rewindState(serverTransform, processedTickNum);
		return true;
	}

	return false;
}

