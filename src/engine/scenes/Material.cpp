#pragma once

#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include "engine/scenes/Material.hpp"

namespace Engine::Scenes 
{
	Material::Texture::Texture(VkFormat format, const std::vector<unsigned char>& data, const VkExtent2D& sizePixels)
	{
		_format = format;
		_data = data;
		_sizePixels = sizePixels;
	}
}