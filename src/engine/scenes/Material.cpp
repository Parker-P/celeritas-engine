#define GLFW_INCLUDE_VULKAN
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