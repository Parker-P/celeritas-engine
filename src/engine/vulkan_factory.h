#pragma once

//Singleton and factory class that acts as a container for 
//multiple Vulkan applications and as a factory to create them
class VulkanFactory {
	//Private member variables
	//All the apps this container is tracking
	std::vector<VulkanApplication> apps_;

	//Private member functions
	//Generates a locally unique app id (unique within the apps_ vector) and returns it
	unsigned int GenerateAppId();

public:
	//Public member functions
	//Returns the singleton instance
	static VulkanFactory& GetInstance();

	//Create an application specifying its name and window width and height
	VulkanApplication CreateApplication(const char* name, const unsigned int& width, const unsigned int& height);

	//Returns a pointer to the vulkan application by id or nullptr if not found
	VulkanApplication* GetApplicationById(unsigned int id);

	//Returns a pointer to the vulkan application by glfw window or nullptr if not found
	VulkanApplication* GetApplicationByGlfwWindow(GLFWwindow* window);
};