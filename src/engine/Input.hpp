#pragma once

class Key {
	bool _isHeldDown;
	bool _wasPressed;
public:
	static std::string _code;
};

class Input : public Singleton<Input> {

	Key W;
	Key A;
	Key S;
	Key D;
	Key Q;
	Key E;

public:

	// Mouse
	double _mouseX;
	double _mouseY;

	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void CursorPositionCallback(GLFWwindow* window, double xPos, double yPos);

	void Init(GLFWwindow* window);
	bool IsKeyHeldDown(std::string);
	bool WasKeyPressed(std::string);
};

class KeyCombo {
	std::vector<Key> _keys;
public:
	bool IsActive();
};