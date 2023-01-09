#pragma once
namespace Engine::Scenes
{
	class Scene;

	/**
	 * @brief Represents a physical object in a celeritas-engine scene.
	 */
	class GameObject
	{
	public:

		/**
		 * @brief Scene this game object belongs to.
		 */
		Scene* _scene;

		/**
		 * @brief Name of the game object.
		 */
		std::string _name;

		/**
		 * @brief Object-to-world transform.
		 */
		Math::Transform _transform;

		/**
		 * @brief Mesh of this game object.
		 */
		Mesh _mesh;
	};
}