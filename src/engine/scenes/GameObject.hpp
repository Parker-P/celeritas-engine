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
		 * @brief Object-to-world transform.
		 */
		Math::Transform _transform;

		/**
		 * @brief Mesh of this game object.
		 */
		Mesh* _pMesh = nullptr;

		/**
		 * @brief See IPipelinable.
		 */
		virtual Vulkan::ShaderResources CreateDescriptorSets(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& graphicsQueue, std::vector<Vulkan::DescriptorSetLayout>& layouts) override;

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