#include <GLFW/glfw3.h>

#include "input.h"

void Input::Init(GLFWwindow* window) {
	glfwSetKeyCallback(window, key_callback);
}