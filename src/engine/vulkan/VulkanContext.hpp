#pragma once

/**
 * @brief Represents all the needed, or neeeded in order to obtain, information to run any call in the Vulkan API.
 */
class VulkanContext
{
public:
	VkInstance _instance = nullptr;
	VkDevice _logicalDevice = nullptr;
	VkPhysicalDevice _physicalDevice = nullptr;
	VkCommandPool _commandPool;
	VkQueue _queue;
	int _queueFamilyIndex;
};
