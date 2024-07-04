#pragma once

namespace Engine
{
	namespace Scenes 
	{
		class GameObject;
	}
}

namespace Game
{
	void Falling(Engine::Scenes::GameObject& gameObject);
	void Ground(Engine::Scenes::GameObject& gameObject);
}
