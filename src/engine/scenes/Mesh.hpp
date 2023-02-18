#pragma once

namespace Engine::Scenes
{
	/**
	 * @brief Represents a collection of vertices and face indices as triangles.
	 */
	class Mesh : public Engine::Structural::Drawable, public Structural::IUpdatable
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
			Vulkan::DescriptorPool _samplersPool;

			/**
			 * @brief Object-related data directed to the vertex shader. Knowing this, the vertex shader is able to calculate the correct
			 * Vulkan view volume coordinates.
			 */
			struct
			{
				glm::mat4 objectToWorld;
			} _objectData;

			/**
			 * @brief Contains game-object-related data to go to the shaders.
			 */
			Vulkan::Buffer _objectDataBuffer;

			/**
			 * @brief Descriptor that contains the object-to-world transformation matrix, so the vertex shader knows the positional offset
			 * of this gameobject's mesh's vertices, and can therefore calculate the correct viewable-volume coordinates.
			 */
			Vulkan::Descriptor _objectDataDescriptor;

			/**
			 * @brief Descriptor set that contains the _objectDataDescriptor descriptor. This descriptor set will be bound to a graphics
			 * pipeline for each draw call in a render pass that draws this game object's mesh, and will be sent to the vertex shader.
			 */
			Vulkan::DescriptorSet _objectDataSet;

			/**
			 * @brief Descriptor pool used to allocate _uniformSet.
			 */
			Vulkan::DescriptorPool _objectDataPool;

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
		 //void RecordDrawCommands(VkPipeline& pipeline, VkCommandBuffer& commandBuffer) override;

		 /**
		  * @brief Updates per-frame mesh-related information.
		  */
		virtual void Update() override;
	};
}