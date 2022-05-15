#pragma once

class Settings : public Singleton<Settings> {
public:
	float _mouseSensitivity = 0.1f;
};