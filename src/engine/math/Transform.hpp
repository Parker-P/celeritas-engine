#pragma once

namespace Engine::Math
{
	class Transform
	{
		glm::mat4x4 _transformation; // The homogeneous transformation matrix for this transform
		glm::mat4x4 _scale; // The scale is stored separately as it would be impossible to tell from _transformation alone as it mixes with rotation values

	public:
		/// <summary>
		/// Returns the homogeneous transformation matrix that defines position, rotation and scale
		/// </summary>
		/// <returns></returns>
		glm::mat4x4 Transformation();

		void SetTransformation(const glm::mat4x4 transformation);

		glm::mat4x4 GetTransformation();

		/// <summary>
		/// Returns a vector in world space that is the world's X axis rotated by the _transformation transformation matrix.
		/// The function is called "Right" because a camera is always pointing towards the Z axis (in world space)
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
		void Translate(const glm::vec3& offset);


		/// <summary>
		/// Rotate this transform by "angle" (defined in degrees) around "axis" independent of position
		/// </summary>
		void Rotate(const glm::vec3& axis, const float& angleDegrees);

		glm::vec3 Position();

		void SetPosition(glm::vec3& position);

	};
}