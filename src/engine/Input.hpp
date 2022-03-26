#pragma once

class Input : public Singleton<Input> {

	// W
	bool isWHeldDown;
	bool wasWPressed;

	// A
	bool isAHeldDown;
	bool wasAPressed;

	// S
	bool isSHeldDown;
	bool wasSPressed;

	// D
	bool isDHeldDown;
	bool wasDPressed;


public:

	// Mouse
	double mouseX;
	double mouseY;

	static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void cursor_position_callback(GLFWwindow* window, double xPos, double yPos);

	void Init(GLFWwindow* window);
	bool IsKeyHeldDown(std::string);
	bool WasKeyPressed(std::string);
};