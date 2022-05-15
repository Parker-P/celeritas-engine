#include <string>
#include <iostream>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "Singleton.hpp"
#include "Time.hpp"
#include "Settings.hpp"
#include "Input.hpp"
#include "Transform.hpp"
#include "GameObject.hpp"
#include "Camera.hpp"
#include "Utils.hpp"

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
		_roll -= 0.1f * (float)Time::Instance()._deltaTime;
	}

	if (Input::Instance().IsKeyHeldDown("e")) {
		_roll += 0.1f * (float)Time::Instance()._deltaTime;
	}

	if (Input::Instance().IsKeyHeldDown("w")) {
		_proxy._transform.Translate(_proxy._transform.Forward() * 0.009f * (float)Time::Instance()._deltaTime);
	}

	if (Input::Instance().IsKeyHeldDown("a")) {
		_proxy._transform.Translate(_proxy._transform.Right() * 0.009f * (float)Time::Instance()._deltaTime);
	}

	if (Input::Instance().IsKeyHeldDown("s")) {
		_proxy._transform.Translate(-_proxy._transform.Forward() * 0.009f * (float)Time::Instance()._deltaTime);
	}

	if (Input::Instance().IsKeyHeldDown("d")) {
		_proxy._transform.Translate(-_proxy._transform.Right() * 0.009f * (float)Time::Instance()._deltaTime);
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
	glm::vec3 const proxyForward = _proxy._transform.Forward();
	glm::vec3 const proxyRight = _proxy._transform.Right();
	glm::vec3 const proxyUp = _proxy._transform.Up();

	_view._transformation[0][0] = proxyRight.x;
	_view._transformation[1][0] = proxyRight.y;
	_view._transformation[2][0] = proxyRight.z;
	_view._transformation[0][1] = proxyUp.x;
	_view._transformation[1][1] = proxyUp.y;
	_view._transformation[2][1] = proxyUp.z;
	_view._transformation[0][2] = -proxyForward.x;
	_view._transformation[1][2] = -proxyForward.y;
	_view._transformation[2][2] = -proxyForward.z;
	_view._transformation[3][0] = -glm::dot(proxyRight, proxyPosition);
	_view._transformation[3][1] = -glm::dot(proxyUp, proxyPosition);
	_view._transformation[3][2] = dot(proxyForward, proxyPosition);

	/*std::cout.precision(1);
	std::cout << "View is:" << std::endl;
	std::cout << _view._transformation[0][0] << " " << _view._transformation[0][1] << " " << _view._transformation[0][2] << " " << _view._transformation[0][3] << std::endl;
	std::cout << _view._transformation[1][0] << " " << _view._transformation[1][1] << " " << _view._transformation[1][2] << " " << _view._transformation[1][3] << std::endl;
	std::cout << _view._transformation[2][0] << " " << _view._transformation[2][1] << " " << _view._transformation[2][2] << " " << _view._transformation[2][3] << std::endl;
	std::cout << _view._transformation[3][0] << " " << _view._transformation[3][1] << " " << _view._transformation[3][2] << " " << _view._transformation[3][3] << std::endl;
	std::cout << std::endl;*/
}
