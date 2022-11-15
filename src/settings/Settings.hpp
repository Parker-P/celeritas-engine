#pragma once

class Settings : public Singleton<Settings>
{
public:
	uint32_t _windowWidth;
	uint32_t _windowHeight;
	float _mouseSensitivity;
};