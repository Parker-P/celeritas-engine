#include <vector>
#include <GLFW/glfw3.h>
#include "singleton.h"
#include "vulkan_factory.h"
#include "../core/vulkan_application.h"

unsigned int VulkanFactory::GenerateAppId(){
	//Simply use the smallest available number as id
	unsigned int min = 0;
	for (int i = 0; i < apps_.size(); ++i) {
		if (min < apps_[i].id_) {
			min = apps_[i].id_;
		}
	}
	return min;
}

VulkanApplication VulkanFactory::CreateApplication(const char* name, const unsigned int& width, const unsigned int& height){
	VulkanApplication app;
	app.id_ = GenerateAppId();
	app.name_ = name;
	app.width_ = width;
	app.height_ = height;
	apps_.push_back(app);
	return app;
}

VulkanApplication* VulkanFactory::GetApplicationById(unsigned int id){
	for (int i = 0; i < apps_.size(); ++i) {
		if (apps_[i].id_ == id) {
			return &apps_[i];
		}
	}
	return nullptr;
}

VulkanApplication* VulkanFactory::GetApplicationByGlfwWindow(GLFWwindow* window) {
	for (int i = 0; i < apps_.size(); ++i) {
		if (apps_[i].window_ == window) {
			return &apps_[i];
		}
	}
	return nullptr;
}


