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

	// A
	if (key == GLFW_KEY_A) {
		if (action == GLFW_PRESS) {
			Instance().wasAPressed = true;
			Instance().isAHeldDown = true;
		}
		else if (action == GLFW_REPEAT) {
			Instance().isAHeldDown = true;
		}
		else {
			Instance().isAHeldDown = false;
		}
	}

	// S
	if (key == GLFW_KEY_S) {
		if (action == GLFW_PRESS) {
			Instance().wasSPressed = true;
			Instance().isSHeldDown = true;
		}
		else if (action == GLFW_REPEAT) {
			Instance().isSHeldDown = true;
		}
		else {
			Instance().isSHeldDown = false;
		}
	}

	// D
	if (key == GLFW_KEY_D) {
		if (action == GLFW_PRESS) {
			Instance().wasDPressed = true;
			Instance().isDHeldDown = true;
		}
		else if (action == GLFW_REPEAT) {
			Instance().isDHeldDown = true;
		}
		else {
			Instance().isDHeldDown = false;
		}
	}
}

void Input::Init(GLFWwindow* window) {
	glfwSetKeyCallback(window, key_callback);
}

bool Input::IsKeyHeldDown(std::string key) {
	// W
	if (key == "w" || key == "w") {
		return Instance().isWHeldDown;
	}

	// A
	if (key == "a" || key == "A") {
		return Instance().isAHeldDown;
	}

	// S
	if (key == "s" || key == "S") {
		return Instance().isSHeldDown;
	}

	// D
	if (key == "d" || key == "D") {
		return Instance().isDHeldDown;
	}
	return false;
}

bool Input::WasKeyPressed(std::string key) {
	// W
	if (key == "w" || key == "W") {
		if (Instance().wasWPressed) {
			Instance().wasWPressed = false;
			return true;
		}
	}

	// A
	if (key == "a" || key == "A") {
		if (Instance().wasAPressed) {
			Instance().wasAPressed = false;
			return true;
		}
	}

	// S
	if (key == "s" || key == "S") {
		if (Instance().wasSPressed) {
			Instance().wasSPressed = false;
			return true;
		}
	}

	// D
	if (key == "d" || key == "D") {
		if (Instance().wasDPressed) {
			Instance().wasDPressed = false;
			return true;
		}
	}
	return false;
}
