#pragma once
namespace Engine::Scenes
{
	class GameObject
	{
	public:
		std::string _name;
		Math::Transform _transform;
		Mesh _mesh;
	};
}