#pragma once

namespace Engine::Math
{
	/**
	 * @brief Represents a column-major 4x4 matrix transform in a left handed X right, Y up, Z forward coordinate system.
	 */
	class Transform
	{

	public:

		/**
		 * @brief Returns the transform to take another transform from gltf space (X left, Y up, Z forward)
		 * to engine space (X right, Y up, Z forward).
		 */
		static Transform GltfToEngine() {
			return Transform(glm::mat4x4{
				glm::vec4(-1.0f, 0.0f, 0.0f, 0.0f), // Column 1
				glm::vec4(0.0f,  1.0f, 0.0f, 0.0f), // Column 2
				glm::vec4(0.0f,  0.0f, 1.0f, 0.0f), // Column 3
				glm::vec4(0.0f,  0.0f, 0.0f, 1.0f)	// Column 4
				});
		}

		/**
		 * @brief Constructs a transform using the identity matrix for both _scale and _matrix.
		 */
		Transform() = default;

		/**
		 * @brief Constructs a transform from a matrix. Uses the identity matrix for _scale.
		 * @param matrix The underlying homogeneous matrix that represents the transform.
		 */
		Transform(glm::mat4x4 matrix);

		/**
		 * @brief The homogeneous transformation matrix for this transform.
		 */
		glm::mat4x4 _matrix;

		/**
		 * @brief The scale is stored separately as it would be impossible to tell from _matrix alone as it mixes with rotation values.
		 */
		glm::vec3 _scale;

		/**
		 * @brief Returns a vector in world space that is the world's X axis rotated by the _matrix transformation matrix.
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
		 * @brief Translate this transform by "offset". This will modify the fourth column of the _matrix member.
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

		/**
		 * @brief Sets the homogeneous column of the transform matrix, used for position.
		 * @param position Three-dimentional position.
		 */
		void SetPosition(const glm::vec3& position);

		glm::vec3 Scale();

		/**
		 * @brief Sets the homogeneous column of the transform matrix, used for position.
		 * @param position Three-dimentional position.
		 */
		void SetScale(const glm::vec3& scale);
	};
}