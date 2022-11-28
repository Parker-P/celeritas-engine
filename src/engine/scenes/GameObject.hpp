#pragma once
namespace Engine::Scenes
{
	/**
	 * @brief Represents a physical object in the world.
	 */
	class GameObject
	{
	public:
		std::string _name;
		Math::Transform _transform;
		Mesh _mesh;
	};
}