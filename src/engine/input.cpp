#include <iostream>
#include <string>
#include <GLFW/glfw3.h>
#include <vector>
#include <map>

#include "Singleton.hpp"
#include "Input.hpp"

#pragma region KeyComboFunctions
bool KeyCombo::IsActive()
{
	return false;
}
#pragma endregion

#pragma region InputFunctions

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

void Input::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {

	Instance()._keys.try_emplace(key, Key(key));

	if (action == GLFW_PRESS) {
		Instance()._keys[key]._wasPressed = true;
		Instance()._keys[key]._isHeldDown = true;
	}
	else if (action == GLFW_REPEAT) {
		Instance()._keys[key]._isHeldDown = true;
	}
	else {
		Instance()._keys[key]._isHeldDown = false;
	}
}

void Input::CursorPositionCallback(GLFWwindow* window, double xPos, double yPos) {
	if (!Input::Instance()._cursorEnabled) {
		Input::Instance()._mouseX = xPos;
		Input::Instance()._mouseY = yPos;
	}
}

bool Input::IsKeyHeldDown(int glfwKeyCode) {
	if (!_keys[glfwKeyCode]._isHeldDown) {
		_keys[glfwKeyCode]._wasPressed = false;
	}
	return _keys[glfwKeyCode]._isHeldDown;
}

bool Input::WasKeyPressed(int glfwKeyCode) {
	bool wasPressed = _keys[glfwKeyCode]._wasPressed;
	_keys[glfwKeyCode]._wasPressed = false;
	return wasPressed;
}

void Input::ToggleCursor(GLFWwindow* window) {
	if (_cursorEnabled) {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
	else {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	_cursorEnabled = !_cursorEnabled;
}
#pragma endregion