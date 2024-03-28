#pragma once

namespace Engine::Vulkan
{
	/**
	 * @brief .
	 */
	Image SolidColorImage(VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
	
	/**
	 * @brief .
	 */
	void CopyBufferToDeviceMemory(VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice, VkCommandPool commandPool, VkQueue queue, VkBuffer buffer, void* pData, size_t sizeBytes);
	
	/**
	 * @brief .
	 */
	void CopyImageToDeviceMemory(VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice, VkCommandPool& commandPool, VkQueue& queue, VkImage& image, int width, int height, int depth, void* pData, size_t sizeBytes);
	
	/**
	 * @brief Creates a command pool from which resettable command buffers can be allocated.
	 */
	VkCommandPool CreateCommandPool(VkDevice& logicalDevice, int& queueFamilyIndex);

	/**
	 * @brief Returns the queue family index of the queue family that satisfies the given flags, or -1 if not found.
	 */
	int FindQueueFamilyIndex(VkPhysicalDevice& physicalDevice, VkQueueFlagBits queueFlags);

	/**
	 * @brief .
	 */
	void StartRecording(VkCommandBuffer& commandBuffer);

	/**
	 * @brief .
	 */
	void StopRecording(VkCommandBuffer& commandBuffer);

	/**
	 * @brief .
	 */
	void ExecuteCommands(VkCommandBuffer& commandBuffer, VkQueue& queue);

	/**
	 * @brief .
	 */
	VkCommandBuffer CreateCommandBuffer(VkDevice& logicalDevice, VkCommandPool& commandPool);
}