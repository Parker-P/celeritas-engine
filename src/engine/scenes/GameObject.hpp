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
		 * @param pScene Scene the game object belongs to.
		 */
		GameObject(const std::string& name, Scene* pScene);

		/**
		 * @brief Name of the game object.
		 */
		std::string _name;

		/**
		 * @brief Scene this game object belongs to.
		 */
		Scene* _pScene = nullptr;

		/**
		 * @brief Transform relative to the parent gameobject.
		 */
		Math::Transform _transform;

		struct
		{
			glm::mat4x4 transform;
		} _gameObjectData;

		/**
		 * @brief Mesh of this game object.
		 */
		Mesh* _pMesh = nullptr;

		/**
		 * @brief Parent game object index.
		 */
		GameObject* _parent;

		/**
		 * @brief Child game object indices.
		 */
		std::vector<GameObject*> _children;

		/**
		 * @brief See IPipelinable.
		 */
		virtual Vulkan::ShaderResources CreateDescriptorSets(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, std::vector<Vulkan::DescriptorSetLayout>& layouts) override;

		/**
		 * @brief Calculates the world space transform based on the hierarchy of parents.
		 * @return 
		 */
		Math::Transform GetWorldSpaceTransform();

		/**
		 * @brief See IPipelinable.
		 */
		virtual void UpdateShaderResources() override;

		/**
		 * @brief Updates per-frame game-object-related information.
		 */
		virtual void Update() override;
	};
}