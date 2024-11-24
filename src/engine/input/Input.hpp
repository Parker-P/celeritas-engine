#pragma once

namespace Engine::Input
{
	class Key
	{
	public:
		bool _isHeldDown;
		bool _wasPressed;
		int _code;
		Key() = default;
		Key(int code) : _code(code), _isHeldDown(false), _wasPressed(false) {}
	};

	class KeyCombo
	{
	public:
		std::vector<Key> _keys;
		bool IsActive();
	};

	class KeyboardMouse : public ::Structural::Singleton<KeyboardMouse>, public ::Structural::IUpdatable
	{
		double _lastMouseX;

		double _lastMouseY;

		GLFWwindow* _window;

	public:
		
		/**
		 * @brief .
		 */
		std::map<int, Key> _keys;

		/**
		 * @brief True if the cursor is visible.
		 */
		bool _cursorEnabled = false;

		/**
		 * @brief Cumulative value of all horizontal mouse movements since a callback function was registered with glfwSetCursorPosCallback.
		 */
		double _mouseX;
		
		/**
		 * @brief Cumulative value of all vertical mouse movements since a callback function was registered with glfwSetCursorPosCallback.
		 */
		double _mouseY;
		
		/**
		 * @brief The difference between the current frame's mouseX and the mouseX registered on the last Update() call.
		 */
		double _deltaMouseX;
		
		/**
		 * @brief The difference between the current frame's mouseY and the mouseY registered on the last Update() call.
		 */
		double _deltaMouseY;
		
		//double _scrollX; // Only used for scroll-pads. For vertical scroll wheels, only the Y value is actually used.
		double _scrollY;

		static void KeyCallback(GLFWwindow* pWindow, int key, int scancode, int action, int mods);
		static void CursorPositionCallback(GLFWwindow* pWindow, double xPos, double yPos);
		static void ScrollWheelCallback(GLFWwindow* pWindow, double xPos, double yPos);

		/**
		 * @brief Constructor.
		 */
		KeyboardMouse() = default;

		bool KeyCombo::IsActive()
		{
			return false;
		}

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
	};
}