#pragma once

#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include "engine/vulkan/Queue.hpp"
#include "engine/vulkan/PhysicalDevice.hpp"
#include "engine/vulkan/Image.hpp"
#include "engine/scenes/Material.hpp"

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