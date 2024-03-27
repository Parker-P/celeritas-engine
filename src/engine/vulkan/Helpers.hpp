#pragma once

namespace Engine::Vulkan
{
	Image SolidColorImage(VkDevice logicalDevice, PhysicalDevice physicalDevice, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
	void CopyBufferToDeviceMemory(VkDevice logicalDevice, PhysicalDevice& physicalDevice, VkCommandPool commandPool, Queue& queue, Buffer& buffer);
	void CopyImageToDeviceMemory(VkDevice logicalDevice, PhysicalDevice& physicalDevice, VkCommandPool commandPool, Queue& queue, Image& image, int width, int height, int depth);
	void StartRecording(VkCommandBuffer commandBuffer);
	void StopRecording(VkCommandBuffer commandBuffer);
	void ExecuteCommands(VkCommandBuffer commandBuffer, VkQueue queue);
	VkCommandBuffer CreateCommandBuffer(VkDevice logicalDevice, VkCommandPool commandPool);
}