#pragma once

// Forward declarations for compilation.
//namespace Engine::Scenes { class Mesh { class Vertex; }; }

namespace Engine::Structural
{
	/**
	 * @brief Used by deriving classes to mark themselves as a class that is meant to bind drawing resources (vertex buffers, index buffer)
	 * to a graphics pipeline and send draw commands to the Vulkan API.
	 */
	class Drawable
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

		/**
		 * @brief Creates a GPU-only vertex buffer for the Drawable.
		 * @param physicalDevice Needed to bind the buffer to the GPU.
		 * @param logicalDevice Needed to bind the buffer to the GPU.
		 * @param commandPool Command pool that will be used to allocate a temporary command buffer to be stored in the queue specified below.
		 * @param queue The queue that will contain a command buffer allocated from the command pool above; the queue will contain Vulkan commands to send the vertex buffer to VRAM.
		 * @param vertices The vertex information to send to the GPU.
		 */
		void CreateVertexBuffer(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& queue, const std::vector<Scenes::Vertex>& vertices);

		/**
		 * @brief Creates a GPU-only index buffer for the Drawable.
		 * @param physicalDevice Needed to bind the buffer to the GPU.
		 * @param logicalDevice Needed to bind the buffer to the GPU.
		 * @param commandPool Command pool that will be used to allocate a temporary command buffer to be stored in the queue specified below.
		 * @param queue The queue that will contain a command buffer allocated from the command pool above; the queue will contain Vulkan commands to send the vertex buffer to VRAM.
		 * @param indices The index information to send to the GPU.
		 */
		void CreateIndexBuffer(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& queue, const std::vector<unsigned int>& indices);
	};
}
