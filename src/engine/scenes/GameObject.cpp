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
	GameObject::GameObject(const std::string& name, Scene* pScene)
	{
		_name = name;
		_pScene = pScene;
	}

	void GameObject::CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& graphicsQueue)
	{
		// We send the transformation matrix to the GPU.
		_buffers.push_back(Vulkan::Buffer(logicalDevice,
			physicalDevice,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			&_transform._matrix,
			sizeof(_transform._matrix)));

		_descriptors.push_back(Vulkan::Descriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, _buffers[0]));
		_sets.push_back(Vulkan::DescriptorSet(logicalDevice, VK_SHADER_STAGE_VERTEX_BIT, _descriptors));
		_pool = Vulkan::DescriptorPool(logicalDevice, _sets);
	}

	void GameObject::UpdateShaderResources()
	{

	}

	void GameObject::Update()
	{
		_pMesh->Update();
	}
}