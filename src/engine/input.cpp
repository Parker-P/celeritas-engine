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

	// Q
	if (key == GLFW_KEY_Q) {
		if (action == GLFW_PRESS) {
			Instance()._wasQPressed = true;
			Instance()._isQHeldDown = true;
		}
		else if (action == GLFW_REPEAT) {
			Instance()._isQHeldDown = true;
		}
		else {
			Instance()._isQHeldDown = false;
		}
	}

	// E
	if (key == GLFW_KEY_E) {
		if (action == GLFW_PRESS) {
			Instance()._wasEPressed = true;
			Instance()._isEHeldDown = true;
		}
		else if (action == GLFW_REPEAT) {
			Instance()._isEHeldDown = true;
		}
		else {
			Instance()._isEHeldDown = false;
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
	if (key == "w" || key == "W") {
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
	
	// Q
	if (key == "q" || key == "Q") {
		return Instance()._isQHeldDown;
	}
	
	// E
	if (key == "e" || key == "E") {
		return Instance()._isEHeldDown;
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
