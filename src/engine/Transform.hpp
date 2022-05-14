#pragma once

class Transform {
public:
	glm::mat4x4 _transformation;
	glm::mat4x4 _position;
	glm::mat4x4 _rotation;
	glm::mat4x4 _scale;

	/// <summary>
	/// Returns the combined transformation matrix of position, rotation and scale
	/// </summary>
	/// <returns></returns>
	glm::mat4x4 Transformation();

	/// <summary>
	/// Returns a vector in world space that is the world's X axis rotated by the _rotation transformation matrix.
	/// The function is called "Right" because a camera is always pointing towards the -Z axis (in world space)
	/// so the X axis is to the "right" of the camera
	/// </summary>
	glm::vec3 Right();

	/// <summary>
	/// Returns a vector in world space that is the world's Y axis rotated by the _rotation transformation matrix.
	/// </summary>
	glm::vec3 Up();

	/// <summary>
	/// Returns a vector in world space that is the world's Z axis rotated by the _rotation transformation matrix.
	/// </summary>
	glm::vec3 Forward();
};