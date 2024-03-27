#pragma once

/**
 * @brief Represents a fully initialized Vulkan context.
 */
class VulkanContext
{
public:
	VkInstance _instance;
	VkDevice _logicalDevice;
	VkPhysicalDevice _physicalDevice;
	VkQueue _queue;
	VkCommandPool _commandPool;
};