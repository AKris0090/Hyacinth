#pragma once

#include "hcinth_assetdrawer.h"

namespace gltfutils {
    void loadAnimatedMesh(HAssetDrawer& scene, std::string path, std::string meshName);
    void loadStaticMesh(HAssetDrawer& scene, std::string path, std::string meshName);
}