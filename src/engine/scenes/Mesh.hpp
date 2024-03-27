#pragma once

namespace Engine::Scenes
{
	// Forward declarations for the compiler.
	//class Scene;

	/**
	 * @brief Represents a collection of vertices and face indices as triangles.
	 */
	class Mesh : public ::Structural::IUpdatable, public Engine::Structural::Drawable, public Structural::IPipelineable
	{

	public:

		/**
		 * @brief Constructor.
		 * @param scene Pointer to the scene the mesh belongs to.
		 */
		Mesh(Scene* pScene);

		/**
		 * @brief Pointer to the scene so you can use _materialIndex and _gameObjectIndex from this class.
		 */
		Scenes::Scene* _pScene;

		/**
		 * @brief Index into the materials list in the Scene this mesh belongs to. A scene shoud always have a default material defined at index 0. See the Material and Scene classes.
		 */
		int _materialIndex = 0;

		/**
		 * @brief Index into the game objects list in the Scene this mesh belongs to. See the GameObject and Scene classes.
		 */
		int _gameObjectIndex = -1;

		/**
		 * @brief See Pipelinable.
		 */
		virtual Vulkan::ShaderResources CreateDescriptorSets(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& graphicsQueue, std::vector<Vulkan::DescriptorSetLayout>& layouts);

		/**
		 * @brief See Pipelinable.
		 */
		virtual void UpdateShaderResources() override;

		 /**
		  * @brief See IUpdatable.
		  */
		virtual void Update() override;

		/**
		 * @brief Returns the game object this mesh belongs to.
		 */
		GameObject GameObject() {
			return _pScene->_gameObjects[_gameObjectIndex];
		}
	};
}