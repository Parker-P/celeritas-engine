#pragma once
namespace Engine::Scenes
{
	/**
	 * @brief Forward declaration used to define a pointer used to keep track of the scene the game object belongs to.
	 */
	class Scene;

	/**
	 * @brief Represents a physical object in a celeritas-engine scene.
	 */
	class GameObject : public Structural::IUpdatable, public Structural::Drawable
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

		/**
		 * @brief Updates per-frame game-object-related information.
		 */
		virtual void Update() override;
	};
}