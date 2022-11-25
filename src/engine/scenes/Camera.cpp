#include <string>
#include <iostream>
#include <filesystem>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/ext.hpp>
#include <vector>
#include <map>

#include "structural/Singleton.hpp"
#include "engine/Time.hpp"
#include "settings/GlobalSettings.hpp"
#include "engine/input/Input.hpp"
#include "engine/math/Transform.hpp"
#include "Mesh.hpp"
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
	void Camera::Update()
	{
		auto& input = Input::KeyboardMouse::Instance();
		auto& time = Time::Instance();

		_yaw = -input._mouseX * (double)Settings::GlobalSettings::Instance()._mouseSensitivity; // According to the right hand rule, rotating left is positive along the Y axis, but going left with the mouse gives you a negative value. Because we will be using this value as degrees of rotation, we want to negate it.
		_pitch = input._mouseY * (double)Settings::GlobalSettings::Instance()._mouseSensitivity; // Around the X axis (for pitch) the positive rotation is looking upwards, and moving the mouse upwards gives you a positive value, so this stays as is.

		auto _mouseSens = Settings::GlobalSettings::Instance()._mouseSensitivity;


		if (input.IsKeyHeldDown(GLFW_KEY_Q)) {
			_roll -= 0.1f * (float)Time::Instance()._deltaTime;
		}

		if (input.IsKeyHeldDown(GLFW_KEY_E)) {
			_roll += 0.1f * (float)Time::Instance()._deltaTime;
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

		// Then apply yaw rotation.
		_transform.Rotate(_transform.Up(), _deltaYaw);

		// Then pitch.
		_transform.Rotate(_transform.Right(), _deltaPitch);

		// Vulkan's coordinate system is: X points to the right, Y points down, Z points towards you
		// Vulkan's viewport coordinate system is right handed (the x axis points to the right with respect to the z and y axes)
		// and we are doing all our calculations assuming +Z is forward and +X is right, so using a left-handed coordinate system.
		// Supposedly, this is because the screen renders it's images starting from the top left and ending at the bottom right,
		// Infact coordinates (0,0) in Vulkan correspond to the top left of the screen

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
	}

	void Camera::GenerateProjectionTransform(const float& viewportWidth, const float& viewportHeight, const float& horizontalFovDegrees, const float& nearClipDistance, const float& farClipDistance)
	{
		// The idea behind the projection transformation is using the camera as if you were standing behind a glass window: whatever you see out the window gets projected onto
		// the glass.
		// Lets say that YOU are the camera. Now do the following:
		// 1) Go to a window and stand about 1 meter behind it, making sure that your head is roughly at the center of the window.
		// 2) Close one eye so you have only one focal point.
		// 3) Now focus on something outside the window (has to be a specific point, like the tip of the roof of a house or the leaf of a tree) 
		// 4) Next, move your head towards the point you are focusing on in a straight line, untill you eventually hit the window with your head
		// That point on the glass, right in front of your pupil, is the intersection between your screen and the point is space outside your house that you were focusing on.
		// This is exactly how the projection transformation was thought about but in reverse; the point in space outside the window moves towards your pupil untill it
		// eventually hits the window. When it hits the window, you just take the distance from the center of the window to the hit point: these will be your 2D screen
		// coordinates, and this is what the vertex shader always has to do in the end: convert a position from outside your window to some 2D coordinates on the glass
		// of the window. It's a very simple idea but the nature of how GPUs and vertex shaders work make the math a VERY complicated, because
		// you need to understand the entire rendering pipeline to understand it.
		//
		// You need to be aware of the following pipeline to understand why this transformation matrix is setup as it is:
		// 1. The vertex shader runs, and takes the first vertex position from vertexBuffer[0]
		// 2. The vertex shader outputs gl_position as (position.x, position.y, position.z, position.w) this vector is in what is called clip-space.
		// 3. One of the succeeding stages (before the fragment shader) takes gl_position and does this: 
		// ndcCoordinates = vec3(gl_position.x / gl_position.w, gl_position.y / gl_position.w, gl_position.z / gl_position.w)
		// These will be the final coordinates that will be used to place the vertex onto the viewport.
		// We, unfortunately, have no control over this and cannot prevent it from happening. The only thing we can do is to be hacky
		// with the transformation matrices we apply to gl_position in the vertex shader so that we get the values we want AFTER
		// perspective divide.
		_projection._matrix = glm::mat4{
				glm::vec4{nearClipDistance, 0.0f, 0.0f, 0.0f},
				glm::vec4{0.0f, -nearClipDistance, 0.0f, 0.0f},
				glm::vec4{0.0f, 0.0f, (1.0f/(farClipDistance - nearClipDistance)), 1.0f},
				glm::vec4{0.0f, 0.0f, 0.0f, 0.0f}
		};

		//_projection._matrix = projectionTransform._matrix;/*glm::perspectiveFov(glm::radians(horizontalFovDegrees), viewportWidth, viewportHeight, nearClipDistance, farClipDistance)*/;
	}
}
