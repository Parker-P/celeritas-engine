#include <functional>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>


#include "engine/math/Transform.hpp"
#include <iostream>

namespace Engine::Math
{
	glm::vec3 Transform::Right() 
	{
		glm::vec4 right = _matrix * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
		return glm::vec3(right.x, right.y, right.z);
	}

	glm::vec3 Transform::Up() 
	{
		glm::vec4 up = _matrix * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
		return glm::vec3(up.x, up.y, up.z);
	}

	glm::vec3 Transform::Forward() 
	{
		glm::vec4 forward = _matrix * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
		return glm::vec3(forward.x, forward.y, forward.z);
	}

	void Transform::Translate(const glm::vec3& offset) 
	{
		_matrix[3][0] += offset.x;
		_matrix[3][1] += offset.y;
		_matrix[3][2] += offset.z;
	}

	void Transform::Rotate(const glm::vec3& axis, const float& angleDegrees) 
	{
		// Construct a quaternion to rotate any point or vector around the chosen axis by the angle provided.
		auto normalizedAxis = glm::normalize(axis);
		auto angleRadians = glm::radians(angleDegrees);
		auto cosine = cos(angleRadians / 2.0f);
		auto sine = sin(angleRadians / 2.0f);
		glm::quat rotation(cosine, axis.x * sine, axis.y * sine, axis.z * sine);

		// Rotate each individual axis of the transformation by the quaternion.
		auto newX = rotation * glm::vec3(_matrix[0][0], _matrix[0][1], _matrix[0][2]);
		auto newY = rotation * glm::vec3(_matrix[1][0], _matrix[1][1], _matrix[1][2]);
		auto newZ = rotation * glm::vec3(_matrix[2][0], _matrix[2][1], _matrix[2][2]);

		// Set the new axes starting with X.
		_matrix[0][0] = newX.x;
		_matrix[0][1] = newX.y;
		_matrix[0][2] = newX.z;

		// Then Y.
		_matrix[1][0] = newY.x;
		_matrix[1][1] = newY.y;
		_matrix[1][2] = newY.z;

		// And finally Z.
		_matrix[2][0] = newZ.x;
		_matrix[2][1] = newZ.y;
		_matrix[2][2] = newZ.z;
	}

	void Transform::SetPosition(glm::vec3& position) 
	{
		_matrix[3][0] = position.x;
		_matrix[3][1] = position.y;
		_matrix[3][2] = position.z;
	}

	glm::vec3 Transform::Position() 
	{
		return glm::vec3(_matrix[3][0], _matrix[3][1], _matrix[3][2]);
	}
}