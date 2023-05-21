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
		 * @brief Collection of lights.
		 */
		std::vector<PointLight> _pointLights;

		/**
		 * @brief Collection of game objects.
		 */
		std::vector<GameObject> _gameObjects;

		/**
		 * @brief Collection of materials.
		 */
		std::vector<Material> _materials;

		/**
		 * @brief Environment map used for image-based lighting.
		 */
		SphericalEnvironmentMap _environmentMap;

		

		/**
		 * @brief Updates all scene-related data.
		 */
		virtual void Update() override;
		
		/**
		 * @brief See IPipelinable.
		 */
		void CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& graphicsQueue);

		/**
		 * @brief See IPipelinable.
		 */
		void UpdateShaderResources();
	};
}