C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./shader.vert -o vert.spv
C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./shader.frag -o frag.spv
C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./shadow.vert -o shadow.spv

C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./raygen.rgen -o raygen.spv --target-env=vulkan1.3
C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./closehit.rchit -o rchit.spv --target-env=vulkan1.3
C:/VulkanSDK/1.4.335.0/Bin/glslc.exe ./miss.rmiss -o rmiss.spv --target-env=vulkan1.3

pause