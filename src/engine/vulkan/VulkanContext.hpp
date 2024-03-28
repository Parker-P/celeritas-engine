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
};

/**
 * @brief Represents all the information needed to execute Vulkan commands on a device. 
 * Vulkan commands to be used with command buffers typically start with vkCmd.
 */
class CommandContext
{
public:
	VkCommandPool _commandPool;
	VkQueue _queue;
	int _queueFamilyIndex;
};
