#define GLFW_INCLUDE_VULKAN
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