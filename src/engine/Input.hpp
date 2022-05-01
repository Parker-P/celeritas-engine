#pragma once

class Input : public Singleton<Input> {

	// W
	bool _isWHeldDown;
	bool _wasWPressed;

	// A
	bool _isAHeldDown;
	bool _wasAPressed;

	// S
	bool _isSHeldDown;
	bool _wasSPressed;

	// D
	bool _isDHeldDown;
	bool _wasDPressed;


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