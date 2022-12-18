#include <string>
#include <iostream>
#include <filesystem>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/ext.hpp>
#include <vector>
#include <map>

#include "structural/IUpdatable.h"
#include "structural/Singleton.hpp"
#include "engine/Time.hpp"
#include "settings/GlobalSettings.hpp"
#include "engine/input/Input.hpp"
#include "engine/math/Transform.hpp"
#include "engine/scenes/Mesh.hpp"
#include "engine/scenes/GameObject.hpp"
#include "engine/scenes/Camera.hpp"

//void Camera::Init(float FOV, float aspectRatio, float nearClipPlaneDistance, float farClipPlaneDistance) {
//	//view = glm::lookAt(glm::vec3(1, 1, 1), glm::vec3(0, 0, 0), glm::vec3(0, 0, -1));
//	init = true;
//	position = glm::vec3(0.0f, 0.0f, 0.0f);
//	projection = glm::perspective(glm::radians(FOV), aspectRatio, nearClipPlaneDistance, farClipPlaneDistance);
//}
//
namespace Engine::Scenes
{
	Camera::Camera()
	{
		_horizontalFov = 55.0f;
		_nearClippingDistance = 0.1f;
		_farClippingDistance = 200.0f;
		_up = glm::vec3(0.0f, 1.0f, 0.0f);
	}

	Camera::Camera(float horizontalFov, float nearClippingDistance, float farClippingDistance)
	{
		_horizontalFov = horizontalFov;
		_nearClippingDistance = nearClippingDistance;
		_farClippingDistance = farClippingDistance;
		_up = glm::vec3(0.0f, 1.0f, 0.0f);
	}

	void Camera::Update()
	{
		auto& input = Input::KeyboardMouse::Instance();
		auto& time = Time::Instance();
		auto mouseSens = Settings::GlobalSettings::Instance()._mouseSensitivity;

		_yaw += input._deltaMouseX * mouseSens;
		if ((_pitch + (input._deltaMouseY * mouseSens)) > -90 && (_pitch + (input._deltaMouseY * mouseSens)) < 90) {
			_pitch += input._deltaMouseY * mouseSens;
		}

		if (input.IsKeyHeldDown(GLFW_KEY_Q)) {
			_roll += 0.1f * (float)Time::Instance()._deltaTime;
		}

		if (input.IsKeyHeldDown(GLFW_KEY_E)) {
			_roll -= 0.1f * (float)Time::Instance()._deltaTime;
		}

		if (input.IsKeyHeldDown(GLFW_KEY_W)) {
			_transform.Translate(_transform.Forward() * 0.009f * (float)time._deltaTime);
		}

		if (input.IsKeyHeldDown(GLFW_KEY_A)) {
			_transform.Translate(-_transform.Right() * 0.009f * (float)time._deltaTime);
		}

		if (input.IsKeyHeldDown(GLFW_KEY_S)) {
			_transform.Translate(-_transform.Forward() * 0.009f * (float)time._deltaTime);
		}

		if (input.IsKeyHeldDown(GLFW_KEY_D)) {
			_transform.Translate(_transform.Right() * 0.009f * (float)time._deltaTime);
		}

		if (input.IsKeyHeldDown(GLFW_KEY_SPACE)) {
			_transform.Translate(_transform.Up() * 0.009f * (float)time._deltaTime);
		}

		if (input.IsKeyHeldDown(GLFW_KEY_LEFT_CONTROL)) {
			_transform.Translate(-_transform.Up() * 0.009f * (float)time._deltaTime);
		}

		float _deltaYaw = (_yaw - _lastYaw);
		float _deltaPitch = (_pitch - _lastPitch);
		float _deltaRoll = (_roll - _lastRoll);

		_lastYaw = _yaw;
		_lastPitch = _pitch;
		_lastRoll = _roll;

		// First apply roll rotation.
		_transform.Rotate(_transform.Forward(), _deltaRoll);

		// Transform the up vector according to delta roll but not delta pitch, so the
		// up vector is not affected by looking up and down, but it is by rolling the camera
		// left and right.
		auto axis = _transform.Forward(); // This gets the pitch-independent forward vector.
		auto angleRadians = glm::radians(_deltaRoll);
		auto cosine = cos(angleRadians / 2.0f);
		auto sine = sin(angleRadians / 2.0f);
		_up = glm::quat(cosine, axis.x * sine, axis.y * sine, axis.z * sine) * _up;

		/*std::cout << "Up is: " << _up.x << ", " << _up.y << ", " << _up.z << std::endl;
		std::cout << "Axis is: " << axis.x << ", " << axis.y << ", " << axis.z << std::endl;
		std::cout << "Roll is: " << _roll << std::endl;*/

		// Then apply yaw rotation.
		_transform.Rotate(_up, _deltaYaw);

		// Then pitch.
		_transform.Rotate(_transform.Right(), _deltaPitch);

		// Vulkan's coordinate system is: X points to the right, Y points down, Z points into the screen.
		// Vulkan's viewport coordinate system is right handed and we are doing all our calculations assuming 
		// +Z is forward, +Y is up and +X is right, so using a left-handed coordinate system.

		// We use the inverse of the camera transformation matrix because this matrix will be applied to each vertex of each object in the scene in the vertex shader.
		// There is no such thing as a camera, because in the end we use the position of the vertices to draw stuff on screen. We can only modify the position of the
		// vertices as if they were viewed from the camera, because in the end the physical position of the vertex ends up in the vertex shader, which then calculates
		// its position on 2D screen using the projection matrix.
		// Suppose that our vertex V is at (0,10,0) (so above your head if you are looking down the z axis from the origin)
		// Also suppose that our camera is at (0,0,-10). This means that the camera is behind us by 10 units and can likely see the object
		// somewhat in the upper area of its screen. If we want to see vertex V as if we viewed it from the camera, we would have to move
		// vertex V 10 units forward from its current position. If instead of moving the camera BACK 10 units we move vertex V FORWARD 10 units, 
		// we get the exact same effect as if we actually had a camera and moved it backwards. That's it, very simple idea behind the view matrix.

		_view._matrix = glm::inverse(_transform._matrix);

		float _deltaScrollY = (input._scrollY - _lastScrollY);
		_horizontalFov -= _deltaScrollY;
		_lastScrollY = input._scrollY;
	}
}
