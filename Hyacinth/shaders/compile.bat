C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./gbuffer.vert -o vert.spv
C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./gbuffer.frag -o frag.spv
C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./shadow.vert -o shadow.spv
C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./depth.vert -o depth.spv

C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./fsQuad.vert -o quadVert.spv

C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./probeVis.vert -o probeVert.spv
C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./probeVis.frag -o probeFrag.spv

C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./volumeVis.vert -o volumeVert.spv
C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./volumeVis.frag -o volumeFrag.spv

C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./frustumCull.comp -o frustumCull.spv

C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./irradianceBuild.comp -o irradianceComp.spv
C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./visibilityBuild.comp -o visibilityComp.spv
C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./ddgi.frag -o ddgiFrag.spv
C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./ddgiStencil.frag -o ddgiStencilFrag.spv

C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./raygen.rgen -o raygen.spv --target-env=vulkan1.3
C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./closehit.rchit -o rchit.spv --target-env=vulkan1.3
C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./miss.rmiss -o rmiss.spv --target-env=vulkan1.3
C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./probeMiss.rmiss -o probeMiss.spv --target-env=vulkan1.3

C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./composite.frag -o composite.spv

pause