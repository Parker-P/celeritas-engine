#define GLFW_INCLUDE_VULKAN
#include "LocalIncludes.hpp"

using namespace Engine::Vulkan;

namespace Engine::Scenes
{
	GameObject::GameObject(const std::string& name, Scene* pScene)
	{
		_name = name;
		_pScene = pScene;
	}

	Vulkan::ShaderResources GameObject::CreateDescriptorSets(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, std::vector<Vulkan::DescriptorSetLayout>& layouts)
	{
		auto descriptorSetID = 1;
		auto globalTransform = GetWorldSpaceTransform();

		// Create a temporary buffer.
		Buffer buffer{};
		auto bufferSizeBytes = sizeof(_localTransform._matrix);
		buffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer._createInfo.size = bufferSizeBytes;
		buffer._createInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		vkCreateBuffer(logicalDevice, &buffer._createInfo, nullptr, &buffer._buffer);

		// Allocate memory for the buffer.
		VkMemoryRequirements requirements{};
		vkGetBufferMemoryRequirements(logicalDevice, buffer._buffer, &requirements);
		buffer._gpuMemory = PhysicalDevice::AllocateMemory(physicalDevice, logicalDevice, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		// Map memory to the correct GPU and CPU ranges for the buffer.
		vkBindBufferMemory(logicalDevice, buffer._buffer, buffer._gpuMemory, 0);
		vkMapMemory(logicalDevice, buffer._gpuMemory, 0, bufferSizeBytes, 0, &buffer._cpuMemory);
		memcpy(buffer._cpuMemory, &globalTransform._matrix, bufferSizeBytes);

		_buffers.push_back(buffer);

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
		allocInfo.pSetLayouts = &layouts[descriptorSetID]._layout;
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

		auto descriptorSets = std::vector<VkDescriptorSet>{ descriptorSet };
		_shaderResources._data.try_emplace(layouts[descriptorSetID], descriptorSets);

		auto meshResources = _pMesh->CreateDescriptorSets(physicalDevice, logicalDevice, commandPool, queue, layouts);
		_shaderResources.MergeResources(meshResources);

		for (auto& child : _pChildren) {
			auto childResources = child->CreateDescriptorSets(physicalDevice, logicalDevice, commandPool, queue, layouts);
			_shaderResources.MergeResources(childResources);
		}

		return _shaderResources;
	}


	Math::Transform GameObject::GetWorldSpaceTransform()
	{
		Math::Transform outTransform;
		GameObject current = *this;
		outTransform._matrix *= current._localTransform._matrix;
		while (current._pParent != nullptr) {
			current = *current._pParent;
			outTransform._matrix *= current._localTransform._matrix;
		}
		return outTransform;
	}

	void GameObject::UpdateShaderResources()
	{
		_gameObjectData.transform = GetWorldSpaceTransform()._matrix;
		memcpy(_buffers[0]._cpuMemory, &_gameObjectData, sizeof(_gameObjectData));
	}

	void GameObject::Update()
	{
		if (_pMesh != nullptr) {
			_pMesh->Update();
		}

		UpdateShaderResources();

		for (auto& child : _pChildren) {
			child->Update();
		}
	}

	void GameObject::Draw(VkPipelineLayout& pipelineLayout, VkCommandBuffer& drawCommandBuffer)
	{
		if (_pMesh != nullptr) {
			_pMesh->Draw(pipelineLayout, drawCommandBuffer);
		}

		for (int i = 0; i < _pChildren.size(); ++i) {
			_pChildren[i]->Draw(pipelineLayout, drawCommandBuffer);
		}
	}
}