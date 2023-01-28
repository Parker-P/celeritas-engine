#pragma once

namespace Engine::Scenes
{
	/**
	 * @brief Represents a collection of vertices and face indices as triangles.
	 */
	class Mesh : public IUpdatable, public IDrawable
	{
		

	public:

		/**
		 * @brief Represents vertex attributes, such as positions, normals and UV coordinates.
		 */
		class Vertex
		{
		public:

			/**
			 * @brief Used to identify vertex attribute types.
			 */
			enum class AttributeType
			{
				Position, Normal, UV
			};

			/**
			 * @brief Attribute describing the object-space position of the vertex in the engine's coordinate system (X right, Y up, Z forward).
			 */
			glm::vec3 _position;

			/**
			 * @brief Attribute describing the object-space normal vector of the vertex in the engine's coordinate system (X right, Y up, Z forward).
			 */
			glm::vec3 _normal;

			/**
			 * @brief Attribute describing the UV coordinates of the vertex. A vertex might be part of a UV seam, so it could have multiple
			 * UV coordinates.
			 */
			glm::vec2 _uvCoord;

			/**
			 * @brief Calculates the offset in bytes of a given attribute given its type.
			 * @param attributeType
			 * @return The offset in bytes of the given type within the Vertex class.
			 */
			static size_t OffsetOf(const AttributeType& attributeType);
		};

		/**
		 * @brief All mesh-related resources used by shaders.
		 */
		struct
		{
			/**
			 * @brief Wrapper that contains a GPU-only buffer that contains vertices for drawing operations, and the vertex information.
			 */
			struct
			{
				/**
				 * @brief List of vertices that make up the mesh.
				 */
				std::vector<Vertex> _vertexData;

				/**
				 * @brief Buffer that stores vertex attributes. A vertex attribute is a piece of data
				 * that decorates the vertex with more information, so that the vertex shader can
				 * do more work based on it. For example a vertex attribute could be a position or a normal vector.
				 * Based on the normal vector, the vertex shader can perform lighting calculations by computing
				 * the angle between the source of the light and the normal.
				 * At the hardware level, the contents of the vertex buffer are fed into the array of shader cores,
				 * and each vertex, along with its attributes, are processed by the vertex shader in parallel on each thread of the cores.
				 * This buffer is intended to contain _vertexData to be bound to the graphics pipeline just before
				 * drawing the mesh in a render pass.
				 */
				Vulkan::Buffer _vertexBuffer;

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
			 * @brief Desscriptor that contains a texture.
			 */
			Vulkan::Descriptor _textureDescriptor;

			/**
			 * @brief Descriptor set that contains texture sampler descriptors.
			 */
			Vulkan::DescriptorSet _samplersSet;

			/**
			 * @brief Descriptor pool that contains all descriptor sets.
			 */
			Vulkan::DescriptorPool _descriptorPool;

		} _shaderResources;
		
		/**
		 * @brief Index into the materials list in the Scene class. See the Material and Scene classes.
		 */
		unsigned int _materialIndex;

		/**
		 * @brief Index into the game objects list in the Scene class. See the Material and Scene classes.
		 */
		unsigned int _gameObjectIndex;

		/**
		 * @brief Records the Vulkan draw commands needed to draw the mesh into the given command buffer.
		 * The recorded commands will bind all the mesh's resources to the pipeline.
		 */
		void RecordDrawCommands(VkPipeline& pipeline, VkCommandBuffer& commandBuffer) override;

		/**
		 * @brief Updates per-frame mesh-related information.
		 */
		virtual void Update() override;
};
}