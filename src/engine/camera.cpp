#include <string>
#include <iostream>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/ext.hpp>

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
//

std::string Format(float value){
	return (value >= 0.0f) ? " " + std::to_string(value) : std::to_string(value);
}

std::ostream& operator << (std::ostream& stream, const glm::mat4& matrix) {
	stream << Format(matrix[0][0]) << ", " << Format(matrix[0][1]) << ", " << Format(matrix[0][2]) << ", " << Format(matrix[0][3]) << "\n";
	stream << Format(matrix[1][0]) << ", " << Format(matrix[1][1]) << ", " << Format(matrix[1][2]) << ", " << Format(matrix[1][3]) << "\n";
	stream << Format(matrix[2][0]) << ", " << Format(matrix[2][1]) << ", " << Format(matrix[2][2]) << ", " << Format(matrix[2][3]) << "\n";
	stream << Format(matrix[3][0]) << ", " << Format(matrix[3][1]) << ", " << Format(matrix[3][2]) << ", " << Format(matrix[3][3]);
	return stream;
}

std::ostream& operator<< (std::ostream& stream, const glm::vec3& vector) {
	return stream << "(" << vector.x << ", " << vector.y << ", " << vector.z << ")";
}

std::ostream& operator<< (std::ostream& stream, const glm::vec3&& vector) {
	return stream << "(" << vector.x << ", " << vector.y << ", " << vector.z << ")";
}

void Camera::Update()
{
	_yaw = Input::Instance()._mouseX * Settings::Instance()._mouseSensitivity; // According to the right hand rule, rotating left is positive along the Y axis, but going left with the mouse gives you a negative value. Because we will be using this value as degrees of rotation, we want to negate it.
	_pitch = Input::Instance()._mouseY * Settings::Instance()._mouseSensitivity; // Same as above. Around the X axis (for pitch) the positive rotation is looking upwards

	auto _mouseSens = Settings::Instance()._mouseSensitivity;

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
	//_proxy._transform.Rotate(_proxy._transform.Forward(), _deltaRoll);

	// Then apply yaw rotation
	_proxy._transform.Rotate(_proxy._transform.Up(), _deltaYaw);

	// Then pitch
	_proxy._transform.Rotate(_proxy._transform.Right(), _deltaPitch);

	system("cls");
	std::cout << "---------------------------------" << "\n";
	std::cout << "Cumulative yaw is " << _yaw << "\n";
	std::cout << "Delta yaw is " << _deltaYaw << "\n";
	std::cout << "The \"right\" vector is " << _proxy._transform.Right() << "\n";
	std::cout << "The transformation matrix is \n" << _proxy._transform.Transformation() << "\n";

	/*std::cout << "Proxy right is " << _proxy._transform.Right() << std::endl;
	std::cout << "Proxy up is " << _proxy._transform.Up() << std::endl;
	std::cout << "Proxy forward is " << _proxy._transform.Forward() << std::endl;*/

	// Generate the view matrix. This is the matrix that will transform the position of the vertices in the shader
	auto proxyPosition = _proxy._transform.Position();
	glm::vec3 const proxyForward = _proxy._transform.Forward();
	glm::vec3 const proxyRight = _proxy._transform.Right();
	glm::vec3 const proxyUp = _proxy._transform.Up();

	// Vulkan's coordinate system is: X points to the right, Y points down, Z points towards you
	// Vulkan's viewport coordinate system is right handed (the x axis points to the right with respect to the z and y axes)
	// and we are using a right handed coordinate system
	//glm::mat4x4 view;
	glm::mat4x4 view = glm::lookAt(proxyPosition, proxyPosition + proxyForward, proxyUp);

	// Transforms the X axis from world space into camera space
	//view[0][0] = -proxyRight.x;
	//view[1][0] = -proxyRight.y;
	//view[2][0] = -proxyRight.z;
	// 
	//// Transforms the Y axis from world space into camera space
	//view[0][1] = -proxyUp.x;
	//view[1][1] = -proxyUp.y;
	//view[2][1] = -proxyUp.z;
	// 
	//// Transforms the Z axis from world space into camera space
	//view[0][2] = -proxyForward.x;
	//view[1][2] = -proxyForward.y;
	//view[2][2] = -proxyForward.z;
	// 
	//// Makes 
	//view[3][0] = -glm::dot(proxyRight, proxyPosition); // The vertex shader will divide the X vector of th
	//view[3][1] = -glm::dot(proxyUp, proxyPosition);
	//view[3][2] = -glm::dot(proxyForward, proxyPosition);

	/*std::cout << "View matrix is: \n" << view << "\n";
	std::cout << "Proxy position is \n" << _proxy._transform.Transformation() << "\n";*/


	_view.SetTransformation(view);

	/*std::cout.precision(1);
	std::cout << "View is:" << std::endl;
	std::cout << _view._transformation[0][0] << " " << _view._transformation[0][1] << " " << _view._transformation[0][2] << " " << _view._transformation[0][3] << std::endl;
	std::cout << _view._transformation[1][0] << " " << _view._transformation[1][1] << " " << _view._transformation[1][2] << " " << _view._transformation[1][3] << std::endl;
	std::cout << _view._transformation[2][0] << " " << _view._transformation[2][1] << " " << _view._transformation[2][2] << " " << _view._transformation[2][3] << std::endl;
	std::cout << _view._transformation[3][0] << " " << _view._transformation[3][1] << " " << _view._transformation[3][2] << " " << _view._transformation[3][3] << std::endl;
	std::cout << std::endl;*/
}
