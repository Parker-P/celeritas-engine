#include <iostream>
#include <string>
#include <GLFW/glfw3.h>

#include "Singleton.hpp"
#include "Input.hpp"

void Input::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	// W
	if (key == GLFW_KEY_W) {
		if (action == GLFW_PRESS) {
			Instance()._wasWPressed = true;
			Instance()._isWHeldDown = true;
		}
		else if (action == GLFW_REPEAT) {
			Instance()._isWHeldDown = true;
		}
		else {
			Instance()._isWHeldDown = false;
		}
	}

	// A
	if (key == GLFW_KEY_A) {
		if (action == GLFW_PRESS) {
			Instance()._wasAPressed = true;
			Instance()._isAHeldDown = true;
		}
		else if (action == GLFW_REPEAT) {
			Instance()._isAHeldDown = true;
		}
		else {
			Instance()._isAHeldDown = false;
		}
	}

	// S
	if (key == GLFW_KEY_S) {
		if (action == GLFW_PRESS) {
			Instance()._wasSPressed = true;
			Instance()._isSHeldDown = true;
		}
		else if (action == GLFW_REPEAT) {
			Instance()._isSHeldDown = true;
		}
		else {
			Instance()._isSHeldDown = false;
		}
	}

	// D
	if (key == GLFW_KEY_D) {
		if (action == GLFW_PRESS) {
			Instance()._wasDPressed = true;
			Instance()._isDHeldDown = true;
		}
		else if (action == GLFW_REPEAT) {
			Instance()._isDHeldDown = true;
		}
		else {
			Instance()._isDHeldDown = false;
		}
	}
}

void Input::CursorPositionCallback(GLFWwindow* window, double xPos, double yPos) {
	Input::Instance()._mouseX = xPos;
	Input::Instance()._mouseY = yPos;
	//std::cout << xPos << std::endl;
	//std::cout << yPos << std::endl;
}

void Input::Init(GLFWwindow* window) {
	// Keyboard init
	glfwSetKeyCallback(window, KeyCallback);

	// Mouse init
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	if (glfwRawMouseMotionSupported()) {
		glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
	}
	glfwSetCursorPosCallback(window, CursorPositionCallback);
}

bool Input::IsKeyHeldDown(std::string key) {
	// W
	if (key == "w" || key == "w") {
		return Instance()._isWHeldDown;
	}

	// A
	if (key == "a" || key == "A") {
		return Instance()._isAHeldDown;
	}

	// S
	if (key == "s" || key == "S") {
		return Instance()._isSHeldDown;
	}

	// D
	if (key == "d" || key == "D") {
		return Instance()._isDHeldDown;
	}
	return false;
}

bool Input::WasKeyPressed(std::string key) {
	// W
	if (key == "w" || key == "W") {
		if (Instance()._wasWPressed) {
			Instance()._wasWPressed = false;
			return true;
		}
	}

	// A
	if (key == "a" || key == "A") {
		if (Instance()._wasAPressed) {
			Instance()._wasAPressed = false;
			return true;
		}
	}

	// S
	if (key == "s" || key == "S") {
		if (Instance()._wasSPressed) {
			Instance()._wasSPressed = false;
			return true;
		}
	}

	// D
	if (key == "d" || key == "D") {
		if (Instance()._wasDPressed) {
			Instance()._wasDPressed = false;
			return true;
		}
	}
	return false;
}
