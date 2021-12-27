#pragma once

//Singleton and factory class that acts as a container for 
//multiple Vulkan applications and as a factory to create them
class VulkanFactory {
	//Private member functions
	//Generates a locally unique app id (unique within the apps_ vector) and returns it
	unsigned int GenerateAppId();

public:
	//Public member variables
	//All the apps this container is tracking
	std::vector<VulkanApplication> apps_;

	//Public member functions
	static VulkanFactory& GetInstance();

	//Create an application specifying its name and window width and height
	VulkanApplication CreateApplication(const char* name, unsigned int& width, unsigned int& height);
};