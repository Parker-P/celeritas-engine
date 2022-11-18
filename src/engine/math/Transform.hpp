#pragma once

namespace Engine::Math
{
	/**
	 * @brief Represents a column-major matrix transform.
	 */
	class Transform
	{
		/**
		 * @brief The homogeneous transformation matrix for this transform.
		 */
		glm::mat4x4 _transformation;

		/**
		 * @brief The scale is stored separately as it would be impossible to tell from _transformation alone as it mixes with rotation values.
		 */
		glm::mat4x4 _scale;

	public:
		
		/**
		 * @brief Returns the homogeneous transformation matrix that defines position, rotation and scale.
		 * @return 
		 */
		glm::mat4x4 Transformation();

		void SetTransformation(const glm::mat4x4 transformation);

		glm::mat4x4 GetTransformation();

		/**
		 * @brief Returns a vector in world space that is the world's X axis rotated by the _transformation transformation matrix.
		 * The function is called "Right" because a camera is always pointing towards the Z axis (in world space)
		 * so the X axis is to the "right" of the camera.
		 * @return 
		 */
		glm::vec3 Right();

		/**
		 * @brief Returns a vector in world space that is the world's Y axis rotated by the _rotation transformation matrix.
		 * @return 
		 */
		glm::vec3 Up();

		/**
		 * @brief Returns a vector in world space that is the world's Z axis rotated by the _rotation transformation matrix.
		 * @return 
		 */
		glm::vec3 Forward();

		/**
		 * @brief Translate this transform by "offset". This will modify the fourth column of the _position matrix.
		 * @param offset
		 */
		void Translate(const glm::vec3& offset);

		/**
		 * @brief Rotate this transform by "angle" (defined in degrees) around "axis" independent of position.
		 * @param axis
		 * @param angleDegrees
		 */
		void Rotate(const glm::vec3& axis, const float& angleDegrees);

		glm::vec3 Position();

		void SetPosition(glm::vec3& position);

	};
}