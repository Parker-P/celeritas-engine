#include <GLFW/glfw3.h>
#include <string>

#include "singleton.h"
#include "input.h"

void Input::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	// W
	if (key == GLFW_KEY_W) {
		if (action == GLFW_PRESS) {
			Instance().wasWPressed = true;
			Instance().isWHeldDown = true;
		}
		else if (action == GLFW_REPEAT) {
			Instance().isWHeldDown = true;
		}
		else {
			Instance().isWHeldDown = false;
		}
	}
}

void Input::Init(GLFWwindow* window) {
	glfwSetKeyCallback(window, key_callback);
}

bool Input::IsKeyHeldDown(std::string key) {
	if (key == "w") {
		return Instance().isWHeldDown;
	}
	return false;
}

bool Input::WasKeyPressed(std::string key) {
	if (key == "w") {
		if (Instance().wasWPressed) {
			Instance().wasWPressed = false;
			return true;
		}
	}
	return false;
}
