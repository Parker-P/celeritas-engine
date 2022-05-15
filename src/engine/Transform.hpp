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

	/// <summary>
	/// Translate this transform by "offset". This will modify the fourth column of the _position matrix
	/// </summary>
	void Translate(const glm::vec3& offset) {
		_position[0][3] += offset.x;
		_position[1][3] += offset.y;
		_position[2][3] += offset.z;
	}

	void SetPosition(glm::vec3& position) {
		_position += glm::mat4x4(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), glm::vec4(position, 1.0f));
	}

	glm::vec3 Position() {
		return glm::vec3(_position[0][3], _position[1][3], _position[2][3]);
	}
};