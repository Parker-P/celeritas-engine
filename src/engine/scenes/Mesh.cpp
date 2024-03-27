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
	Mesh::Mesh(Scene* scene)
	{
		_pScene = scene;
	}

	Vulkan::ShaderResources Mesh::CreateDescriptorSets(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& graphicsQueue, std::vector<Vulkan::DescriptorSetLayout>& layouts)
	{
		auto descriptorSetID = 3;

		// Get the textures to send to the shaders.
		Vulkan::Image albedoMap;
		Vulkan::Image roughnessMap;
		Vulkan::Image metalnessMap;
		if (_materialIndex >= 0) {
			if (VK_NULL_HANDLE != _pScene->_materials[_materialIndex]._albedo._image) {
				albedoMap = _pScene->_materials[_materialIndex]._albedo;
			}
			else {
				albedoMap = SolidColorImage(logicalDevice, physicalDevice, 125, 125, 125, 255);
			}
			if (VK_NULL_HANDLE != _pScene->_materials[_materialIndex]._roughness._image) {
				roughnessMap = _pScene->_materials[_materialIndex]._roughness;
			}
			else {
				roughnessMap = SolidColorImage(logicalDevice, physicalDevice, 255, 255, 255, 255);
			}
			if (VK_NULL_HANDLE != _pScene->_materials[_materialIndex]._metalness._image) {
				metalnessMap = _pScene->_materials[_materialIndex]._metalness;
			}
			else {
				metalnessMap = SolidColorImage(logicalDevice, physicalDevice, 125, 125, 125, 255);
			}
		}
		else {
			auto defaultMaterial = _pScene->DefaultMaterial();
			albedoMap = defaultMaterial._albedo;
			roughnessMap = defaultMaterial._roughness;
			metalnessMap = defaultMaterial._metalness;
		}

		// The mesh MUST have a material by this point, so we assign the images we created for the texture maps it needs.
		_pScene->_materials[_materialIndex]._albedo = albedoMap;
		_pScene->_materials[_materialIndex]._roughness = roughnessMap;
		_pScene->_materials[_materialIndex]._metalness = metalnessMap;

		// Send the textures to the GPU.
		CopyImageToDeviceMemory(logicalDevice, physicalDevice, commandPool, graphicsQueue, albedoMap, albedoMap._createInfo.extent.width, albedoMap._createInfo.extent.height, albedoMap._createInfo.extent.depth);
		CopyImageToDeviceMemory(logicalDevice, physicalDevice, commandPool, graphicsQueue, roughnessMap, roughnessMap._createInfo.extent.width, roughnessMap._createInfo.extent.height, roughnessMap._createInfo.extent.depth);
		CopyImageToDeviceMemory(logicalDevice, physicalDevice, commandPool, graphicsQueue, metalnessMap, metalnessMap._createInfo.extent.width, metalnessMap._createInfo.extent.height, metalnessMap._createInfo.extent.depth);

		auto commandBuffer = CreateCommandBuffer(logicalDevice, commandPool);
		StartRecording(commandBuffer);

		// Transition the images to shader read layout.
		{
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.oldLayout = albedoMap._currentLayout;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.image = albedoMap._image;
			barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
			albedoMap._currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.oldLayout = roughnessMap._currentLayout;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.image = roughnessMap._image;
			barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
			roughnessMap._currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.oldLayout = metalnessMap._currentLayout;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.image = metalnessMap._image;
			barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
			metalnessMap._currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		}

		StopRecording(commandBuffer);
		ExecuteCommands(commandBuffer, graphicsQueue._handle);

		VkDescriptorPool descriptorPool{};
		VkDescriptorPoolSize poolSizes[1] = { VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 } };
		VkDescriptorPoolCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.maxSets = (uint32_t)1;
		createInfo.poolSizeCount = (uint32_t)1;
		createInfo.pPoolSizes = poolSizes;
		vkCreateDescriptorPool(logicalDevice, &createInfo, nullptr, &descriptorPool);

		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = (uint32_t)1;
		allocInfo.pSetLayouts = &layouts[descriptorSetID]._layout;
		VkDescriptorSet descriptorSet;
		vkAllocateDescriptorSets(logicalDevice, &allocInfo, &descriptorSet);

		VkDescriptorImageInfo imageInfo[3];
		imageInfo[0] = { _pScene->_materials[_materialIndex]._albedo._sampler, _pScene->_materials[_materialIndex]._albedo._view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		imageInfo[1] = { _pScene->_materials[_materialIndex]._roughness._sampler, _pScene->_materials[_materialIndex]._roughness._view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		imageInfo[2] = { _pScene->_materials[_materialIndex]._metalness._sampler, _pScene->_materials[_materialIndex]._metalness._view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkWriteDescriptorSet writeInfo = {};
		writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeInfo.dstSet = descriptorSet;
		writeInfo.descriptorCount = 3;
		writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeInfo.pImageInfo = imageInfo;
		writeInfo.dstBinding = 0;
		vkUpdateDescriptorSets(logicalDevice, 1, &writeInfo, 0, nullptr);

		auto descriptorSets = std::vector<VkDescriptorSet>{ descriptorSet };
		_shaderResources._data.try_emplace(layouts[descriptorSetID], descriptorSets);
		return _shaderResources;
	}

	void Mesh::UpdateShaderResources()
	{
		// TODO
	}

	void Mesh::Update()
	{
		// TODO
	}
}