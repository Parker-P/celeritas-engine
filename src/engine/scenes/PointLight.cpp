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
	PointLight::PointLight(std::string name)
	{
		_name = name;
		_transform.SetPosition(glm::vec3(3.0f, 10.0f, -10.0f));
	}

	Vulkan::ShaderResources PointLight::CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& graphicsQueue)
	{
		// Create a temporary buffer.
		Buffer buffer{};
		auto bufferSizeBytes = sizeof(_lightData);
		buffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer._createInfo.size = bufferSizeBytes;
		buffer._createInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		vkCreateBuffer(logicalDevice, &buffer._createInfo, nullptr, &buffer._buffer);

		// Allocate memory for the buffer.
		VkMemoryRequirements requirements{};
		vkGetBufferMemoryRequirements(logicalDevice, buffer._buffer, &requirements);
		buffer._gpuMemory = physicalDevice.AllocateMemory(logicalDevice, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		// Map memory to the correct GPU and CPU ranges for the buffer.
		vkBindBufferMemory(logicalDevice, buffer._buffer, buffer._gpuMemory, 0);
		vkMapMemory(logicalDevice, buffer._gpuMemory, 0, bufferSizeBytes, 0, &buffer._cpuMemory);
		memcpy(buffer._cpuMemory, &_lightData, bufferSizeBytes);

		_buffers.push_back(buffer);

		VkDescriptorSetLayoutBinding bindings[1] = { VkDescriptorSetLayoutBinding { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr } };
		VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutCreateInfo.bindingCount = 1;
		layoutCreateInfo.pBindings = bindings;
		VkDescriptorSetLayout layout;
		vkCreateDescriptorSetLayout(logicalDevice, &layoutCreateInfo, nullptr, &layout);

		VkDescriptorPool descriptorPool{};
		VkDescriptorPoolSize poolSizes[1] = { VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 } };
		VkDescriptorPoolCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.maxSets = (uint32_t)1;
		createInfo.poolSizeCount = (uint32_t)1;
		createInfo.pPoolSizes = poolSizes;
		vkCreateDescriptorPool(logicalDevice, &createInfo, nullptr, &descriptorPool);

		// Create the descriptor set.
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = (uint32_t)1;
		allocInfo.pSetLayouts = &layout;
		VkDescriptorSet descriptorSet;
		vkAllocateDescriptorSets(logicalDevice, &allocInfo, &descriptorSet);

		// Update the descriptor set's data.
		VkDescriptorBufferInfo bufferInfo{ buffer._buffer, 0, buffer._createInfo.size };
		VkWriteDescriptorSet writeInfo = {};
		writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeInfo.dstSet = descriptorSet;
		writeInfo.descriptorCount = 1;
		writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeInfo.pBufferInfo = &bufferInfo;
		writeInfo.dstBinding = 0;
		vkUpdateDescriptorSets(logicalDevice, 1, &writeInfo, 0, nullptr);

		_shaderResources._data.emplace(PipelineLayout{ 2, layoutCreateInfo , layout }, descriptorSet);
		return _shaderResources;
	}

	void PointLight::UpdateShaderResources()
	{
		_lightData.position = _transform.Position();
		_lightData.colorIntensity = glm::vec4(1.0f, 1.0f, 1.0f, 15000.0f);
		memcpy(_buffers[0]._cpuMemory, &_lightData, sizeof(_lightData));
	}

	void PointLight::Update() {
		auto& input = Input::KeyboardMouse::Instance();

		if (input.IsKeyHeldDown(GLFW_KEY_UP)) {
			_transform.Translate(_transform.Forward() * 1.5f);
		}

		if (input.IsKeyHeldDown(GLFW_KEY_DOWN)) {
			_transform.Translate(_transform.Forward() * -1.5f);
		}

		if (input.IsKeyHeldDown(GLFW_KEY_LEFT)) {
			_transform.Translate(_transform.Right() * -1.5f);
		}

		if (input.IsKeyHeldDown(GLFW_KEY_RIGHT)) {
			_transform.Translate(_transform.Right() * 1.5f);
		}

		auto pos = _transform.Position();
		UpdateShaderResources();
	}
}
