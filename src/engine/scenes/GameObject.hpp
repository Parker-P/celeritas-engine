#pragma once

namespace Engine::Scenes
{
	class Mesh;

	/**
	 * @brief Represents a physical object in a celeritas-engine scene.
	 */
	class GameObject : public ::Structural::IUpdatable, public Structural::IPipelineable
	{
	public:

		/**
		 * @brief Default constructor.
		 */
		GameObject() = default;

		/**
		 * @brief Constructor.
		 * @param name Name of the game object.
		 * @param scene Scene the game object belongs to.
		 */
		GameObject(const std::string& name, Scene* pScene);

		/**
		 * @brief Name of the game object.
		 */
		std::string _name;

		/**
		 * @brief Scene this game object belongs to.
		 */
		Scene* _pScene;

		/**
		 * @brief Object-to-world transform.
		 */
		Math::Transform _transform;

		/**
		 * @brief Mesh of this game object.
		 */
		Mesh* _pMesh;

		/**
		 * @brief See Pipelinable.
		 */
		virtual void CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& graphicsQueue);

		/**
		 * @brief See Pipelinable.
		 */
		virtual void UpdateShaderResources() override;

		/**
		 * @brief Updates per-frame game-object-related information.
		 */
		virtual void Update() override;
	};
}