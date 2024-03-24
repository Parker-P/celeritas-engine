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

	void PointLight::CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& graphicsQueue)
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

		_descriptors.push_back(Vulkan::Descriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, _buffers[0]));
		_sets.push_back(Vulkan::DescriptorSet(logicalDevice, (VkShaderStageFlagBits)(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), _descriptors));
		_pool = Vulkan::DescriptorPool(logicalDevice, _sets);
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
