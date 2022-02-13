#pragma once

class Camera {
public:
	glm::mat4 view;
	glm::mat4 projection;

	/// <summary>
	/// Initializes a camera.
	/// </summary>
	/// <param name="FOV">The field of view in degrees</param>
	/// <param name="aspectRatio">The aspect ratio of the screen</param>
	/// <param name="nearClipPlaneDistance">The plane closer than which nothing will be rendered</param>
	/// <param name="farClipPlaneDistance">The plane farther than which nothing will be rendered</param>
	void Init(float FOV, float aspectRatio, float nearClipPlaneDistance, float farClipPlaneDistance);
};