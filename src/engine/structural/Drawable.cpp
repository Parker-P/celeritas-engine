#include <vector>
#include <string>
#include <optional>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include "structural/IUpdatable.hpp"
#include "structural/Singleton.hpp"
#include "engine/scenes/Vertex.hpp"
#include <engine/vulkan/PhysicalDevice.hpp>
#include "engine/vulkan/Queue.hpp"
#include "engine/vulkan/Image.hpp"
#include "engine/vulkan/Buffer.hpp"
#include "engine/vulkan/ShaderResources.hpp"
#include "engine/structural/IPipelineable.hpp"
#include "engine/structural/Drawable.hpp"
#include <utils/Utils.hpp>

namespace Engine::Structural
{
	void Drawable::CreateVertexBuffer(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& queue, const std::vector<Scenes::Vertex>& vertices)
	{
		_vertices._vertexData = vertices;
		_vertices._vertexBuffer = Engine::Vulkan::Buffer(logicalDevice,
			physicalDevice,
			(VkBufferUsageFlagBits)(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			(void*)vertices.data(),
			Utils::GetVectorSizeInBytes(vertices));
		_vertices._vertexBuffer.SendToGPU(commandPool, queue);
	}

	void Drawable::CreateIndexBuffer(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& queue, const std::vector<unsigned int>& indices)
	{
		_faceIndices._indexData = indices;
		_faceIndices._indexBuffer = Engine::Vulkan::Buffer(logicalDevice,
			physicalDevice,
			(VkBufferUsageFlagBits)(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			(void*)indices.data(),
			Utils::GetVectorSizeInBytes(indices));
		_faceIndices._indexBuffer.SendToGPU(commandPool, queue);
	}
}