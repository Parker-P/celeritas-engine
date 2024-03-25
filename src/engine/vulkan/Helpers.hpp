#pragma once

namespace Engine::Vulkan
{
	Image SolidColorImage(VkDevice logicalDevice, PhysicalDevice physicalDevice, int r, int g, int b, int a);
	void CopyBufferToDeviceMemory(VkDevice logicalDevice, PhysicalDevice& physicalDevice, VkCommandPool commandPool, Queue& queue, Buffer& buffer, void* data, int size);
}