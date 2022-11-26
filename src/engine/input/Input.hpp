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

	class KeyboardMouse : public Singleton<KeyboardMouse>
	{
	public:
		std::map<int, Key> _keys;
		bool _cursorEnabled = false;

		// Mouse
		double _mouseX;
		double _mouseY;
		//double _scrollX; // Only used for scroll-pads. For vertical scroll wheels, only the Y value is actually used.
		double _scrollY;

		static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
		static void CursorPositionCallback(GLFWwindow* window, double xPos, double yPos);
		static void ScrollWheelCallback(GLFWwindow* window, double xPos, double yPos);

		void Init(GLFWwindow* window);

		bool IsKeyHeldDown(int glfwKeyCode);

		bool WasKeyPressed(int glfwKeyCode);

		void ToggleCursor(GLFWwindow* window);
	};
}