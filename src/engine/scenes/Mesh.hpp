#pragma once

namespace Engine::Scenes
{
	// Forward declarations for the compiler.
	//class Scene;

	/**
	 * @brief Represents a collection of vertices and face indices as triangles.
	 */
	class Mesh : public ::Structural::IUpdatable, public Engine::Structural::Drawable
	{

	public:
		
		/**
		 * @brief All mesh-related resources used by shaders.
		 */
		//struct
		//{
		//	/**
		//	 * @brief Descriptor that contains a texture.
		//	 */
		//	Vulkan::Descriptor _textureDescriptor;

		//	/**
		//	 * @brief Descriptor set that contains texture sampler descriptors.
		//	 */
		//	Vulkan::DescriptorSet _samplersSet;

		//	/**
		//	 * @brief Descriptor pool that contains all descriptor sets.
		//	 */
		//	Vulkan::DescriptorPool _samplersPool;

		//	/**
		//	 * @brief Object-related data directed to the vertex shader. Knowing this, the vertex shader is able to calculate the correct
		//	 * Vulkan view volume coordinates.
		//	 */
		//	struct
		//	{
		//		glm::mat4 objectToWorld;
		//	} _objectData;

		//	/**
		//	 * @brief Contains game-object-related data to go to the shaders.
		//	 */
		//	Vulkan::Buffer _objectDataBuffer;

		//	/**
		//	 * @brief Descriptor that contains the object-to-world transformation matrix, so the vertex shader knows the positional offset
		//	 * of this gameobject's mesh's vertices, and can therefore calculate the correct viewable-volume coordinates.
		//	 */
		//	Vulkan::Descriptor _objectDataDescriptor;

		//	/**
		//	 * @brief Descriptor set that contains the _objectDataDescriptor descriptor. This descriptor set will be bound to a graphics
		//	 * pipeline for each draw call in a render pass that draws this game object's mesh, and will be sent to the vertex shader.
		//	 */
		//	Vulkan::DescriptorSet _objectDataSet;

		//	/**
		//	 * @brief Descriptor pool used to allocate _uniformSet.
		//	 */
		//	Vulkan::DescriptorPool _objectDataPool;

		//} _shaderResources;

		/**
		 * @brief Pointer to the scene so you can use _materialIndex and _gameObjectIndex.
		 */
		GameObject* _scene;

		/**
		 * @brief Index into the materials list in the Scene this mesh belongs to. See the Material and Scene classes.
		 */
		unsigned int _materialIndex;

		/**
		 * @brief Index into the game objects list in the Scene this mesh belongs to. See the GameObject and Scene classes.
		 */
		unsigned int _gameObjectIndex;

		/**
		 * @brief See Pipelinable.
		 */
		virtual void CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice);

		 /**
		  * @brief Updates per-frame mesh-related information.
		  */
		virtual void Update() override;
	};
}