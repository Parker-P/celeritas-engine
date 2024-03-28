#define GLFW_INCLUDE_VULKAN
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
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include "LocalIncludes.hpp"

using namespace Engine::Vulkan;

namespace Engine::Scenes 
{
    Material::Material(VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice)
    {
        _name = "DefaultMaterial";

        _albedo = SolidColorImage(logicalDevice, physicalDevice, 255, 0, 255, 255);
        _roughness = SolidColorImage(logicalDevice, physicalDevice, 125, 125, 125, 255);
        _metalness = SolidColorImage(logicalDevice, physicalDevice, 125, 125, 125, 255);
    }
}