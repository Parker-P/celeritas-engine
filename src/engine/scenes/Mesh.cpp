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
#include "engine/structural/Pipelineable.hpp"
#include "engine/structural/Drawable.hpp"
#include "engine/scenes/Material.hpp"
#include "engine/scenes/Mesh.hpp"
#include <utils/Utils.hpp>

namespace Engine::Scenes
{
	void Mesh::CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice)
	{
		_buffers = Structural::Array<Vulkan::Buffer>(1);
		_images = Structural::Array<Vulkan::Image>(1);
		_descriptors = Structural::Array<Vulkan::Descriptor>(2);
		_sets = Structural::Array<Vulkan::DescriptorSet>(2);

		Vulkan::Image* texture = nullptr;

		if (_scene->_materials.size() > 0) {
			texture = &_scene->_materials[_materialIndex]._baseColor;
		}

		_buffers[0] = Vulkan::Buffer(logicalDevice,
			physicalDevice,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			&_scene->_gameObjects[_gameObjectIndex]._transform._matrix,
			sizeof(gameObject._transform._matrix));

		_descriptors[0] = Vulkan::Descriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &_buffers[0]);
		_descriptors[1] = Vulkan::Descriptor(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, nullptr, texture);

		_sets[0] = Vulkan::DescriptorSet(logicalDevice, VK_SHADER_STAGE_VERTEX_BIT, { &_descriptors[0] });
		_sets[1] = Vulkan::DescriptorSet(logicalDevice, VK_SHADER_STAGE_FRAGMENT_BIT, { &_descriptors[1] });

		_pool = Vulkan::DescriptorPool(logicalDevice, { &_sets[0], &_sets[1] });

		_sets[0].SendDescriptorData();
		_sets[1].SendDescriptorData();
	}

	void Mesh::Update()
	{

	}
}