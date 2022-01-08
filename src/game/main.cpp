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
#include "../engine/vulkan_application.h"
#include "../engine/vulkan_factory.h"

int main() {
	VulkanApplication game = VulkanFactory::GetInstance().CreateApplication("Solar System Explorer", 800, 600);
	game.Run();
}