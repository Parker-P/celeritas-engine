#pragma once

class Camera {
public:

	//bool _init = false;
	glm::mat4x4 _rotation = glm::mat4x4();
	glm::vec3 _cameraRight = glm::vec3(1.0f, 0.0f, 0.0f);
	glm::vec3 _cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 _cameraForward = glm::vec3(0.0f, 0.0f, -1.0f);

	glm::vec3 _position;
	glm::mat4 _view;
	glm::mat4 _projection;

	/// <summary>
	/// Initializes a camera.
	/// </summary>
	/// <param name="FOV">The field of view in degrees</param>
	/// <param name="aspectRatio">The aspect ratio of the screen</param>
	/// <param name="nearClipPlaneDistance">The plane closer than which nothing will be rendered</param>
	/// <param name="farClipPlaneDistance">The plane farther than which nothing will be rendered</param>
	//void Init(float FOV, float aspectRatio, float nearClipPlaneDistance, float farClipPlaneDistance);
};