#include "hcinth_animatedmesh.h"

bool isParentOf(HSkinnedMeshNode* search, HSkinnedMeshNode* target) {
	if (search == nullptr) {
		return false;
	}
	if (search == target) {
		return true;
	}
	return isParentOf(search->parent, target);
}

HSkinnedMeshNode* findNodeIndex(uint32_t index, HSkinnedMeshNode* current) {
	if (current->nodeIndex == index) {
		return current;
	}

	HSkinnedMeshNode* found = nullptr;
	if (current->children.size() > 0) {
		for (auto& child : current->children) {
			found = findNodeIndex(index, child);
			if (found != nullptr) {
				return found;
			}
		}
	}
	return found;
}

HSkinnedMeshNode* HSkinnedMesh::getNodeByIndex(uint32_t index) {
	HSkinnedMeshNode* found = nullptr;
	for (auto& n : parentNodes) {
		found = findNodeIndex(index, n);
		if (found != nullptr) {
			break;
		}
	}
	if (found == nullptr) {
		throw std::runtime_error("TS does NOT have the node ur looking for :sob:");
	}

	return found;
}

HSkinnedMeshNode* findNodeName(std::string search, HSkinnedMeshNode* current) {
	if (current->nodeName == search) {
		return current;
	}

	HSkinnedMeshNode* found = nullptr;
	if (current->children.size() > 0) {
		for (auto& child : current->children) {
			found = findNodeName(search, child);
			if (found != nullptr) {
				return found;
			}
		}
	}
	return found;
}

HSkinnedMeshNode* HSkinnedMesh::getNodeByName(std::string nodeName) {
	HSkinnedMeshNode* found = nullptr;
	for (auto& n : parentNodes) {
		found = findNodeName(nodeName, n);
		if (found != nullptr) {
			break;
		}
	}
	if (found == nullptr) {
		throw std::runtime_error("TS does NOT have the node ur looking for :sob:");
	}

	return found;
}

glm::mat4 HSkinnedMeshNode::getMatrix() {
	glm::mat4 nodeMatrix = transform.getMatrix();
	HSkinnedMeshNode* currentParent = parent;
	while (currentParent)
	{
		nodeMatrix = currentParent->transform.getMatrix() * nodeMatrix;
		currentParent = currentParent->parent;
	}
	return nodeMatrix;
}