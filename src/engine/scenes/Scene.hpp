#pragma once

namespace Engine::Scenes
{
	/**
	 * @brief Represents a celeritas-engine scene.
	 */
	class Scene
	{

	public:
		/**
		 * @brief Collection of game objects.
		 */
		std::vector<GameObject> _objects;

		/**
		 * @brief Collection of materials.
		 */
		std::vector<Material> _materials;
	};
}