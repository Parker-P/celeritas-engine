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

namespace Engine::Scenes 
{
    Material::Material(VkDevice& logicalDevice, Vulkan::PhysicalDevice& physicalDevice)
    {
        _name = "DefaultMaterial";
        _albedo = Vulkan::Image::SolidColor(logicalDevice, physicalDevice, 125, 125, 125, 255);
        _roughness = Vulkan::Image::SolidColor(logicalDevice, physicalDevice, 125, 125, 125, 255);
        _metalness = Vulkan::Image::SolidColor(logicalDevice, physicalDevice, 125, 125, 125, 255);
    }
}