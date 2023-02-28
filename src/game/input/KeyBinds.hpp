#pragma once

namespace Game::Input
{
	class Action
	{

	};

	class KeyBinds : public ::Structural::Singleton<KeyBinds>
	{
	public:
		KeyBinds();
	};
}
