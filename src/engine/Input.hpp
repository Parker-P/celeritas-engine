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

	/*Key W		{ GLFW_KEY_W };
	Key A		{ GLFW_KEY_A };
	Key S		{ GLFW_KEY_S };
	Key D		{ GLFW_KEY_D };
	Key Q		{ GLFW_KEY_Q };
	Key E		{ GLFW_KEY_E };
	Key SPACE	{ GLFW_KEY_SPACE };
	Key ESC		{ GLFW_KEY_ESCAPE };
	Key CTRL	{ GLFW_KEY_LEFT_CONTROL };*/

	// Mouse
	double _mouseX;
	double _mouseY;

	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void CursorPositionCallback(GLFWwindow* window, double xPos, double yPos);

	void Init(GLFWwindow* window);
	bool IsKeyHeldDown(int glfwKeyCode);
	bool WasKeyPressed(int glfwKeyCode);
};