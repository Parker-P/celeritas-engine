#pragma once

namespace Engine::Scenes
{
	// Forward declarations for the compiler.
	class GameObject;
	
	/**
	 * @brief Represents a celeritas-engine scene.
	 */
	class Scene : public ::Structural::IUpdatable, public Structural::IPipelineable
	{

	public:
		/**
		 * @brief Collection of point lights.
		 */
		std::vector<PointLight> _pointLights;

		/**
		 * @brief Game object hierarchy.
		 */
		GameObject _rootGameObject;

		/**
		 * @brief Collection of materials.
		 */
		std::vector<Material> _materials;

		/**
		 * @brief Environment map used for image-based lighting.
		 */
		CubicalEnvironmentMap _environmentMap;

		/**
		 * @brief Default constructor.
		 */
		Scene() = default;

		/**
		 * @brief Creates a scene with a default material.
		 * @param logicalDevice
		 * @param physicalDevice
		 */
		Scene(VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice);

		/**
		 * @brief Returns the default material in the scene, which is always stored as the first material in the _materials vector.
		 */
		Material DefaultMaterial();

		/**
		 * @brief Updates all scene-related data.
		 */
		virtual void Update() override;

		/**
		 * @brief See IPipelinable.
		 */
		Vulkan::ShaderResources CreateDescriptorSets(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, std::vector<Vulkan::DescriptorSetLayout>& layouts);

		/**
		 * @brief See IPipelinable.
		 */
		void UpdateShaderResources();
	};
}