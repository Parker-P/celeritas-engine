#include <string>
#include <vector>
#include <iostream>

#include <vulkan/vulkan.h>
#include <glm/gtc/matrix_transform.hpp>

#include "structural/IUpdatable.hpp"
#include "engine/vulkan/PhysicalDevice.hpp"
#include "engine/vulkan/Queue.hpp"
#include "engine/vulkan/Buffer.hpp"
#include "engine/vulkan/Image.hpp"
#include "engine/vulkan/ShaderResources.hpp"
#include "engine/structural/IPipelineable.hpp"
#include "engine/scenes/Vertex.hpp"
#include "engine/structural/Drawable.hpp"
#include "engine/math/Transform.hpp"
#include "engine/scenes/Material.hpp"
#include "engine/scenes/Vertex.hpp"
#include "engine/scenes/Scene.hpp"
#include "engine/scenes/GameObject.hpp"
#include "engine/scenes/Mesh.hpp"

namespace Engine::Scenes
{
	GameObject::GameObject(const std::string& name, Scene& scene)
	{
		_name = name;
		_scene = &scene;
	}

	void GameObject::CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice)
	{
		_buffers = Structural::Array<Vulkan::Buffer>(1);
		_descriptors = Structural::Array<Vulkan::Descriptor>(1);
		_sets = Structural::Array<Vulkan::DescriptorSet>(1);

		_buffers[0] = Vulkan::Buffer(logicalDevice,
			physicalDevice,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			&_transform._matrix,
			sizeof(_transform._matrix));

		_descriptors[0] = Vulkan::Descriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &_buffers[0]);
		_sets[0] = Vulkan::DescriptorSet(logicalDevice, VK_SHADER_STAGE_VERTEX_BIT, { &_descriptors[0] });
		_pool = Vulkan::DescriptorPool(logicalDevice, { &_sets[0] });
		_sets[0].SendDescriptorData();
	}

	void GameObject::UpdateShaderResources()
	{

	}

	void GameObject::Update()
	{
		_mesh->Update();
	}
}