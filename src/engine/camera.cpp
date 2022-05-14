#include <string>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "Time.hpp"
#include "Settings.hpp"
#include "Singleton.hpp"
#include "Input.hpp"
#include "Transform.hpp"
#include "GameObject.hpp"
#include "Camera.hpp"

//void Camera::Init(float FOV, float aspectRatio, float nearClipPlaneDistance, float farClipPlaneDistance) {
//	//view = glm::lookAt(glm::vec3(1, 1, 1), glm::vec3(0, 0, 0), glm::vec3(0, 0, -1));
//	init = true;
//	position = glm::vec3(0.0f, 0.0f, 0.0f);
//	projection = glm::perspective(glm::radians(FOV), aspectRatio, nearClipPlaneDistance, farClipPlaneDistance);
//}

void Camera::Update()
{
	_yaw = Input::Instance()._mouseX * Settings::Instance()._mouseSensitivity; // According to the right hand rule, rotating left is positive along the Y axis, but going left with the mouse gives you a negative value. Because we will be using this value as degrees of rotation, we want to negate it.
	_pitch = Input::Instance()._mouseY * Settings::Instance()._mouseSensitivity; // Same as above. Around the X axis (for pitch) the positive rotation is looking upwards

	if (Input::Instance().IsKeyHeldDown("q")) {
		//std::cout << "d key is being held down\n";
		_roll -= 0.1f * (float)Time::Instance()._deltaTime;
	}

	if (Input::Instance().IsKeyHeldDown("e")) {
		//std::cout << "d key is being held down\n";
		_roll += 0.1f * (float)Time::Instance()._deltaTime;
	}

	if (Input::Instance().IsKeyHeldDown("w")) {
		//std::cout << "w key is being held down\n";
		_proxy._transform._position += _proxy._transform.Forward() * 0.009f * (float)Time::Instance()._deltaTime;
	}

	if (Input::Instance().IsKeyHeldDown("a")) {
		//std::cout << "a key is being held down\n";
		_proxy._transform._position += _proxy._transform.Right() * 0.009f * (float)Time::Instance()._deltaTime;
	}

	if (Input::Instance().IsKeyHeldDown("s")) {
		//std::cout << "s key is being held down\n";
		_proxy._transform._position += -_proxy._transform.Forward() * 0.009f * (float)Time::Instance()._deltaTime;
	}

	if (Input::Instance().IsKeyHeldDown("d")) {
		//std::cout << "d key is being held down\n";
		_proxy._transform._position += -_proxy._transform.Right() * 0.009f * (float)Time::Instance()._deltaTime;
	}

	_deltaYaw = _yaw - _lastYaw;
	_deltaPitch = _pitch - _lastPitch;
	_deltaRoll = _roll - _lastRoll;

	_lastYaw = _yaw;
	_lastPitch = _pitch;
	_lastRoll = _roll;


	// First apply roll rotation
	_proxy._transform._rotation = glm::rotate(_proxy._transform._rotation, glm::radians(_deltaRoll), _proxy._transform.Forward());

	// Then apply yaw rotation
	_proxy._transform._rotation = glm::rotate(_proxy._transform._rotation, glm::radians(_deltaYaw), _proxy._transform.Up());

	// Then pitch
	_proxy._transform._rotation = glm::rotate(_proxy._transform._rotation, glm::radians(_deltaPitch), _proxy._transform.Right());

	// Generate the view matrix. This is the matrix that will transform the position of the vertices in the shader
	auto proxyPosition = glm::vec3(_proxy._transform._position[0][3], _proxy._transform._position[1][3], _proxy._transform._position[2][3]);
	glm::vec3 const cameraForward = _proxy._transform.Forward();
	glm::vec3 const cameraRight = _proxy._transform.Right();
	glm::vec3 const cameraUp = _proxy._transform.Up();

	_view._transformation[0][0] = cameraRight.x;
	_view._transformation[1][0] = cameraRight.y;
	_view._transformation[2][0] = cameraRight.z;
	_view._transformation[0][1] = cameraUp.x;
	_view._transformation[1][1] = cameraUp.y;
	_view._transformation[2][1] = cameraUp.z;
	_view._transformation[0][2] = -cameraForward.x;
	_view._transformation[1][2] = -cameraForward.y;
	_view._transformation[2][2] = -cameraForward.z;
	_view._transformation[3][0] = -glm::dot(cameraRight, proxyPosition);
	_view._transformation[3][1] = -glm::dot(cameraUp, proxyPosition);
	_view._transformation[3][2] = dot(cameraForward, proxyPosition);
}
