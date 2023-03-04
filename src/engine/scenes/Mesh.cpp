#include <vector>
#include <string>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/detail/type_vec.hpp>
#include <vulkan/vulkan.h>

#include "structural/IUpdatable.hpp"
#include "engine/vulkan/PhysicalDevice.hpp"
#include "engine/vulkan/Queue.hpp"
#include "engine/vulkan/Image.hpp"
#include "engine/vulkan/Buffer.hpp"
#include "engine/vulkan/ShaderResources.hpp"
#include "engine/scenes/Vertex.hpp"
#include "engine/structural/IPipelineable.hpp"
#include "engine/structural/Drawable.hpp"
#include "engine/scenes/Material.hpp"
#include "engine/scenes/Scene.hpp"
#include "engine/math/Transform.hpp"
#include "engine/scenes/GameObject.hpp"
#include "engine/scenes/Mesh.hpp"
#include <utils/Utils.hpp>

namespace Engine::Scenes
{
	void Mesh::CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice)
	{
		_images = Structural::Array<Vulkan::Image>(1);
		Vulkan::Image* texture = nullptr;

		if (_scene->_materials.size() > 0) {
			texture = &_scene->_materials[_materialIndex]._baseColor;
		}

		_descriptors[1] = Vulkan::Descriptor(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, nullptr, texture);
		_sets[1] = Vulkan::DescriptorSet(logicalDevice, VK_SHADER_STAGE_FRAGMENT_BIT, { &_descriptors[1] });
		_pool = Vulkan::DescriptorPool(logicalDevice, { &_sets[0] });
		_sets[0].SendDescriptorData();
	}

	void Mesh::UpdateShaderResources()
	{

	}

	void Mesh::Update()
	{

	}
}