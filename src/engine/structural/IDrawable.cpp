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
#include <vulkan/vulkan.h>
#include "LocalIncludes.hpp"

using namespace Engine::Vulkan;

namespace Engine::Structural
{
	void IDrawable::CreateVertexBuffer(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, const std::vector<Scenes::Vertex>& vertices)
	{
		_vertices._vertexData = vertices;

		// Create a temporary buffer.
		auto& buffer = _vertices._vertexBuffer;
		auto bufferSizeBytes = Utils::GetVectorSizeInBytes(vertices);
		buffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer._createInfo.size = bufferSizeBytes;
		buffer._createInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		vkCreateBuffer(logicalDevice, &buffer._createInfo, nullptr, &buffer._buffer);

		// Allocate memory for the buffer.
		VkMemoryRequirements requirements{};
		vkGetBufferMemoryRequirements(logicalDevice, buffer._buffer, &requirements);
		buffer._gpuMemory = PhysicalDevice::AllocateMemory(physicalDevice, logicalDevice, requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		// Map memory to the correct GPU and CPU ranges for the buffer.
		vkBindBufferMemory(logicalDevice, buffer._buffer, buffer._gpuMemory, 0);

		// Send the buffer to GPU.
		buffer._pData = (void*)vertices.data();
		buffer._sizeBytes = bufferSizeBytes;
		CopyBufferToDeviceMemory(logicalDevice, physicalDevice, commandPool, queue, buffer._buffer, buffer._pData, buffer._sizeBytes);
	}

	void IDrawable::CreateIndexBuffer(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, const std::vector<unsigned int>& indices)
	{
		_faceIndices._indexData = indices;

		// Create a temporary buffer.
		auto& buffer = _faceIndices._indexBuffer;
		auto bufferSizeBytes = Utils::GetVectorSizeInBytes(indices);
		buffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer._createInfo.size = bufferSizeBytes;
		buffer._createInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		vkCreateBuffer(logicalDevice, &buffer._createInfo, nullptr, &buffer._buffer);

		// Allocate memory for the buffer.
		VkMemoryRequirements requirements{};
		vkGetBufferMemoryRequirements(logicalDevice, buffer._buffer, &requirements);
		buffer._gpuMemory = PhysicalDevice::AllocateMemory(physicalDevice, logicalDevice, requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		// Map memory to the correct GPU and CPU ranges for the buffer.
		vkBindBufferMemory(logicalDevice, buffer._buffer, buffer._gpuMemory, 0);

		// TODO: send the buffer to GPU.
		buffer._pData = (void*)indices.data();
		buffer._sizeBytes = bufferSizeBytes;
		CopyBufferToDeviceMemory(logicalDevice, physicalDevice, commandPool, queue, buffer._buffer, buffer._pData, buffer._sizeBytes);
	}
}