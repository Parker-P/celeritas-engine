#pragma once

class Input : public Singleton<Input> {
	// Keys
	bool isWHeldDown;
	bool wasWPressed;

public:
	static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

	void Init(GLFWwindow* window);
	bool IsKeyHeldDown(std::string);
	bool WasKeyPressed(std::string);
};