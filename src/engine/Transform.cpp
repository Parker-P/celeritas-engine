#include <functional>
#include <glm/gtc/matrix_transform.hpp>


#include "Transform.hpp"

glm::mat4x4 Transform::Transformation() {
	return _transformation;
}

void Transform::SetTransformation(const glm::mat4x4 transformation) {
	_transformation = transformation;
}

glm::vec3 Transform::Right()
{
	glm::vec4 right = _transformation * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
	return glm::vec3(right.x, right.y, right.z);
}

glm::vec3 Transform::Up()
{
	glm::vec4 up = _transformation * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
	return glm::vec3(up.x, up.y, up.z);
}

glm::vec3 Transform::Forward()
{
	glm::vec4 forward = _transformation * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
	return glm::vec3(forward.x, forward.y, forward.z);
}

/// <summary>
/// Translate this transform by "offset". This will modify the fourth column of the _position matrix
/// </summary>
void Transform::Translate(const glm::vec3& offset) {
	_transformation[0][3] += offset.x;
	_transformation[1][3] += offset.y;
	_transformation[2][3] += offset.z;
}

/// <summary>
/// Rotate this transform by "angle" (defined in degrees) around "axis"
/// </summary>
void Transform::Rotate(const glm::vec3& axis, const float& angleDegrees) {
	_transformation = glm::rotate(_transformation, glm::radians(angleDegrees), axis);
}

void Transform::SetPosition(glm::vec3& position) {
	_transformation += glm::mat4x4(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), glm::vec4(position, 1.0f));
}

glm::vec3 Transform::Position() {
	return glm::vec3(_transformation[0][3], _transformation[1][3], _transformation[2][3]);
}
