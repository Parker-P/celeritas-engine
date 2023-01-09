#pragma once
namespace Engine::Scenes
{
	/**
	 * @brief Represents a physical object in a celeritas-engine scene.
	 */
	class GameObject
	{
	public:
		std::string _name;
		Math::Transform _transform;
		Mesh _mesh;
	};
}