#define GLFW_INCLUDE_VULKAN

// STL.
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>
#include <functional>
#include <optional>
#include <filesystem>
#include <map>
#include <bitset>

// Math.
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

// Scene loading.
#include <tinygltf/tiny_gltf.h>

#include "LocalIncludes.hpp"

int main()
{
	Settings::GlobalSettings::Instance().Load(Settings::Paths::Settings());
	Engine::Vulkan::VulkanApplication app;
	app.Run();

	return 0;
}