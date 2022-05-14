#include <glm/gtc/matrix_transform.hpp>

#include "Transform.hpp"

glm::mat4x4 Transform::Transformation() {
	_transformation = _scale * _position * _rotation;
	return _transformation;
}

glm::vec3 Transform::Right()
{
	glm::vec4 right = _rotation * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
	return glm::vec3(right.x, right.y, right.z);
}

glm::vec3 Transform::Up()
{
	glm::vec4 up = _rotation * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
	return glm::vec3(up.x, up.y, up.z);
}

glm::vec3 Transform::Forward()
{
	glm::vec4 forward = _rotation * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
	return glm::vec3(forward.x, forward.y, forward.z);
}
