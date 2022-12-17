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

	class KeyboardMouse : public Singleton<KeyboardMouse>, public IUpdatable
	{
		double _lastMouseX;

		double _lastMouseY;

	public:
		
		/**
		 * @brief .
		 */
		std::map<int, Key> _keys;

		/**
		 * @brief .
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

		static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
		static void CursorPositionCallback(GLFWwindow* window, double xPos, double yPos);
		static void ScrollWheelCallback(GLFWwindow* window, double xPos, double yPos);

		void Init(GLFWwindow* window);

		/**
		 * @brief Returns true if the key associated with the given glfw key is being held down.
		 * @param glfwKeyCode
		 * @return 
		 */
		bool IsKeyHeldDown(int glfwKeyCode);

		/**
		 * @brief Returns true if the key associated with the given glfw key code was pressed in the past.
		 * @param glfwKeyCode
		 * @return 
		 */
		bool WasKeyPressed(int glfwKeyCode);

		/**
		 * @brief Hides or shows the cursor.
		 * @param window
		 */
		void ToggleCursor(GLFWwindow* window);

		/**
		 * @brief See IUpdatable.
		 */
		void Update() override;
	};
}