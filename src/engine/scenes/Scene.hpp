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

		// Inherited via IUpdatable
		virtual void Update() override;
	};
}