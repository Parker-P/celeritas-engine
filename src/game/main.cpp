#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>
#include <functional>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include "../src/engine/utils/singleton.h"
#include "../engine/core/vulkan_application.h"
#include "../engine/utils/vulkan_factory.h"

int main() {
	VulkanApplication solar_system_explorer = VulkanFactory::GetInstance().CreateApplication("Solar System Explorer", 800, 600);
	solar_system_explorer.Run();
}