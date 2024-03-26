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

	Vulkan::ShaderResources Mesh::CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& graphicsQueue)
	{
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
		//_pScene->_materials[_materialIndex]._albedo.SendToGPU(commandPool, graphicsQueue);
		//_pScene->_materials[_materialIndex]._roughness.SendToGPU(commandPool, graphicsQueue);
		//_pScene->_materials[_materialIndex]._metalness.SendToGPU(commandPool, graphicsQueue);

		VkDescriptorSetLayoutBinding bindings[3];
		bindings[0] = { VkDescriptorSetLayoutBinding {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &_pScene->_materials[_materialIndex]._albedo._sampler} };
		bindings[1] = { VkDescriptorSetLayoutBinding {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &_pScene->_materials[_materialIndex]._roughness._sampler} };
		bindings[2] = { VkDescriptorSetLayoutBinding {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &_pScene->_materials[_materialIndex]._metalness._sampler} };
		VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutCreateInfo.bindingCount = 3;
		layoutCreateInfo.pBindings = bindings;
		VkDescriptorSetLayout layout;
		vkCreateDescriptorSetLayout(logicalDevice, &layoutCreateInfo, nullptr, &layout);

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
		allocInfo.pSetLayouts = &layout;
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

		_shaderResources._data.emplace(PipelineLayout{ 3, layoutCreateInfo, layout}, descriptorSet);
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