#pragma once

namespace Engine::Scenes
{
	// Forward declarations for the compiler.
	class GameObject;
	
	/**
	 * @brief Represents a celeritas-engine scene.
	 */
	class Scene : public ::Structural::IUpdatable
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
		 * @brief Updates all game objects.
		 */
		virtual void Update() override;
	};
}