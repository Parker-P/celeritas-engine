#include <functional>
#include <glm/gtc/matrix_transform.hpp>


#include "Transform.hpp"
#include <iostream>

glm::mat4x4 Transform::Transformation() {
	return _transformation;
}

void Transform::SetTransformation(const glm::mat4x4 transformation) {
	_transformation = transformation;
}

glm::vec3 Transform::Right() {
	glm::vec4 right = _transformation * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
	return glm::vec3(right.x, right.y, right.z);
}

glm::vec3 Transform::Up() {
	glm::vec4 up = _transformation * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
	return glm::vec3(up.x, up.y, up.z);
}

glm::vec3 Transform::Forward() {
	glm::vec4 forward = _transformation * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
	return glm::vec3(forward.x, forward.y, forward.z);
}

void Transform::Translate(const glm::vec3& offset) {
	_transformation[0][3] += offset.x;
	_transformation[1][3] += offset.y;
	_transformation[2][3] += offset.z;
}

void Transform::Rotate(const glm::vec3& axis, const float& angleDegrees) {
	/*auto position = Position();
	_transformation[0][3] = 0.0f;
	_transformation[1][3] = 0.0f;
	_transformation[2][3] = 0.0f;
	_transformation = glm::rotate(_transformation, glm::radians(angleDegrees), axis);
	SetPosition(position);*/

	enum Axis {
		X, Y, Z
	};

	// First, align the axis of rotation with one of the world axes. 
	// To do it, first we need to choose which world axis to align to. It makes sense to choose the world axis that has
	// the smallest angle difference to the rotation axis, in other words the world axis that the rotation axis is most aligned with. 
	// We can use the dot product do that. The bigger the dot product, the closer the direction of the vectors thus the smaller the angle between them
	auto dotX = glm::dot(axis, glm::vec3(1.0f, 0.0f, 0.0f));
	auto dotY = glm::dot(axis, glm::vec3(0.0f, 1.0f, 0.0f));
	auto dotZ = glm::dot(axis, glm::vec3(0.0f, 0.0f, 1.0f));
	auto axisToAlignTo = (dotX >= dotZ) ? ((dotX >= dotY) ? Axis::X : Axis::Y) : ((dotZ >= dotY) ? Axis::Z : Axis::Y);

	// Then, we need to find the angles by which to rotate the rotation axis in order for it to align to the chosen world axis.
	// To do it we need 2 angles, regardless of which axis we choose. For example, if we wanted to align the rotation axis to 
	// the world's X axis, we would have to rotate the rotation axis around the world's Y and Z axes, so that's 2 angles we need. 
	// Again, imagining that we want to align the rotation axis with world X axis, to find these angles we can imagine shining a 
	// light on the rotation axis from the Y and Z directions so that the rotation axis casts a shadow on the XY and XZ planes respectively.
	// We would then take the angle between its cast shadows and the X axis.
	if (axisToAlignTo == Axis::X) {
		
		// Find the angle between world X and the projection of the rotation axis on the world XY plane
		// Using SOH CAH TOA, we can use CAH to calculate the angle, that is cos(angle) = adjacent / hypotenuse.
		// The found angle will be the angle by which you need to rotate the rotation axis around world Z to align with the world X axis
		auto adjacent = glm::dot(glm::vec2(1.0f, 0.0f), glm::normalize(glm::vec2(axis.x, axis.y)));
		auto angleZ = glm::degrees(acos(adjacent)); // We don't divide by the hypotenuse as it's always 1
		std::cout << angleZ;

		// We do the same for the projection on the XZ plane
		adjacent = glm::dot(glm::vec2(1.0f, 0.0f), glm::normalize(glm::vec2(axis.x, axis.z)));
		auto angleY = glm::degrees(acos(adjacent));
		std::cout << angleY;
	}

	// Then apply rotation around the world axis you aligned your rotation axis with

	// Now realign your rotation axis to what you had originally, so you would take the
	// inverse transformation of step 1
}

void Transform::SetPosition(glm::vec3& position) {
	_transformation[0][3] = position.x;
	_transformation[1][3] = position.y;
	_transformation[2][3] = position.z;
}

glm::vec3 Transform::Position() {
	return glm::vec3(_transformation[0][3], _transformation[1][3], _transformation[2][3]);
}
