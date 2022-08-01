#pragma once

class Key {
public:
	bool _isHeldDown;
	bool _wasPressed;
	int _code;
	Key() = default;
	Key(int code) : _code(code), _isHeldDown(false), _wasPressed(false) {}
};

class KeyCombo {
public:
	std::vector<Key> _keys;
	bool IsActive();
};

class Input : public Singleton<Input> {
public:
	std::map<int, Key> _keys;
	bool _cursorEnabled = false;

	// Mouse
	double _mouseX;
	double _mouseY;

	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void CursorPositionCallback(GLFWwindow* window, double xPos, double yPos);

	void Init(GLFWwindow* window);
	
	bool IsKeyHeldDown(int glfwKeyCode);

	bool WasKeyPressed(int glfwKeyCode);

	void ToggleCursor(GLFWwindow* window);
};