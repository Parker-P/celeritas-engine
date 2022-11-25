#pragma once

namespace Engine::Math
{
	/**
	 * @brief Represents a column-major 4x4 matrix transform.
	 */
	class Transform
	{

	public:

		/**
		 * @brief Returns the transform to take another transform from engine space (X right, Y up, Z forward)
		 * to Vulkan's space (X right, Y down, Z forward).
		 * The coordinate system that the vertex shader expects is setup as follows:
		 * what happens is that you have a cubic viewing volume right in front of you, within which everything will be rendered.
		 * Just imagine having a cube right in front of you. The face closer to you is your monitor. The volume inside of the cube is
		 * the range within which everything will be rendered.
		 * This viewing volume is a cube that ranges from [-1,1,0] (from the perspective of your monitor, this is the lower-left-close vertex 
		 * of the cube) to [1,-1,1] (upper right far vertex of the cube). Anything you want to render eventually has to fall within this range.
		 * This means that gl_position in the vertex shader needs to have an output coordinate that falls within this range for it to be
		 * within the visible range.
		 */
		static Transform EngineToVulkan() {
			return Transform(glm::mat4x4{
				glm::vec4(1.0f,  0.0f, 0.0f, 0.0f), // Column 1
				glm::vec4(0.0f, -1.0f, 0.0f, 0.0f), // Column 2
				glm::vec4(0.0f,  0.0f, 1.0f, 0.0f), // Column 3
				glm::vec4(0.0f,  0.0f, 0.0f, 1.0f)	// Column 4
			});
		}

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
		 * @brief The scale is stored separately as it would be impossible to tell from _transformation alone as it mixes with rotation values.
		 */
		glm::mat4x4 _scale;

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