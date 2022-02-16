#include <iostream>
#include <string>
#include <GLFW/glfw3.h>

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

void Input::cursor_position_callback(GLFWwindow* window, double xPos, double yPos) {
	Input::Instance().mouseX = xPos;
	Input::Instance().mouseY = yPos;
	//std::cout << xPos << std::endl;
	//std::cout << yPos << std::endl;
}

void Input::Init(GLFWwindow* window) {
	// Keyboard init
	glfwSetKeyCallback(window, key_callback);

	// Mouse init
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	if (glfwRawMouseMotionSupported()) {
		glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
	}
	glfwSetCursorPosCallback(window, cursor_position_callback);
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
