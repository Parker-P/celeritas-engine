#include "LocalIncludes.hpp"

namespace Engine::Input
{
#pragma region KeyComboFunctions
	bool KeyCombo::IsActive()
	{
		return false;
	}
#pragma endregion

#pragma region InputFunctions
	void KeyboardMouse::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
	{

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

	void KeyboardMouse::CursorPositionCallback(GLFWwindow* window, double xPos, double yPos)
	{
		if (!Instance()._cursorEnabled) {
			Instance()._mouseX = xPos;
			Instance()._mouseY = yPos;
		}
	}

	void KeyboardMouse::ScrollWheelCallback(GLFWwindow* window, double xPos, double yPos)
	{
		if (!Instance()._cursorEnabled) {
			//Instance()._scrollX = xPos;
			Instance()._scrollY += yPos;
		}
	}

	KeyboardMouse::KeyboardMouse(GLFWwindow* window)
	{
		if (nullptr != window) {
			_window = window;

			// Keyboard init
			glfwSetKeyCallback(window, KeyCallback);

			// Mouse init
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			if (glfwRawMouseMotionSupported()) {
				glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
			}
			glfwSetCursorPosCallback(window, CursorPositionCallback);
			glfwSetScrollCallback(window, ScrollWheelCallback);
		}
	}

	bool KeyboardMouse::IsKeyHeldDown(int glfwKeyCode)
	{
		if (!_keys[glfwKeyCode]._isHeldDown) {
			_keys[glfwKeyCode]._wasPressed = false;
		}
		return _keys[glfwKeyCode]._isHeldDown;
	}

	bool KeyboardMouse::WasKeyPressed(int glfwKeyCode)
	{
		bool wasPressed = _keys[glfwKeyCode]._wasPressed;
		_keys[glfwKeyCode]._wasPressed = false;
		return wasPressed;
	}

	void KeyboardMouse::ToggleCursor(GLFWwindow* window)
	{
		if (_cursorEnabled) {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}
		else {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
		_cursorEnabled = !_cursorEnabled;
	}
#pragma endregion

	void KeyboardMouse::Update()
	{
		_deltaMouseX = (_mouseX - _lastMouseX);
		_deltaMouseY = (_mouseY - _lastMouseY);

		_lastMouseX = _mouseX;
		_lastMouseY = _mouseY;

		if (WasKeyPressed(GLFW_KEY_ESCAPE)) {
			ToggleCursor(_window);
		}
	}
}