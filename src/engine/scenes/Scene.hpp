#pragma once

namespace Engine::Scenes
{
	/**
	 * @brief Represents a celeritas-engine scene.
	 */
	class Scene : public IUpdatable
	{

	public:
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