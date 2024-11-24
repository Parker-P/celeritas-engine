#pragma once

namespace Engine::Structural
{
	/**
	 * @brief Used by deriving classes to mark themselves as a class that is meant to bind drawing resources (vertex buffers, index buffer)
	 * to a graphics pipeline and send draw commands to the Vulkan API.
	 */
	class IDrawable
	{

	public:

		/**
		 * @brief Wrapper that contains a GPU-only buffer that contains vertices for drawing operations, and the vertex information.
		 */
		struct
		{
			/**
			 * @brief List of vertices that make up the mesh.
			 */
			std::vector<Engine::Scenes::Vertex> _vertexData;

			/**
			 * @brief Buffer that stores vertex attributes. A vertex attribute is a piece of data
			 * that decorates the vertex with more information, so that the vertex shader can
			 * do more work based on it. For example a vertex attribute could be a position or a normal vector.
			 * Based on the normal vector, the vertex shader can perform lighting calculations by computing
			 * the angle between the source of the light and the normal.
			 * At the hardware level, the contents of the vertex buffer are fed into the array of shader cores,
			 * and each vertex, along with its attributes, are processed in parallel by multiple instances of the 
			 * vertex shader on each thread of the cores.
			 * This buffer is intended to contain _vertexData to be bound to the graphics pipeline just before
			 * drawing the mesh in a render pass.
			 */
			Engine::Vulkan::Buffer _vertexBuffer;

		} _vertices;

		/**
		 * @brief Wrapper that contains a GPU-only buffer that contains face indices for drawing operations, and the face indices' information.
		 */
		struct
		{
			/**
			 * @brief List of indices, where each index corresponds to a vertex defined in the _vertices array above.
			 * A face (triangle) is defined by three consecutive indices in this array.
			 */
			std::vector<unsigned int> _indexData;

			/**
			 * @brief This buffer is used by Vulkan when drawing using the vkCmdDrawIndexed command; it gives Vulkan
			 * information about the order in which to draw vertices, and is intended to contain _indexData to be bound
			 * to the graphics pipeline just before drawing the mesh in a render pass.
			 */
			Vulkan::Buffer _indexBuffer;

		} _faceIndices;

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
			buffer._gpuMemory = PhysicalDevice::AllocateMemory(physicalDevice, logicalDevice, requirements, (VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

			vkMapMemory(logicalDevice, buffer._gpuMemory, 0, Utils::GetVectorSizeInBytes(vertices), 0, &buffer._cpuMemory);

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

		/**
		 * @brief Deriving classes should implement this method to bind their vertex and index buffers to a graphics pipeline and draw them via Vulkan draw calls.
		 */
		virtual void Draw(VkPipelineLayout& pipelineLayout, VkCommandBuffer& drawCommandBuffer) = 0;
	};
}
