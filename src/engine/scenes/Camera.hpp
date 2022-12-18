#pragma once
namespace Engine::Scenes
{
	/**
	 * @brief Represents a general-purpose camera.
	 */
	class Camera : public GameObject, public IUpdatable
	{
	public:

		/**
		 * @brief Horizontal FOV in degrees.
		 */
		float _horizontalFov;

		/**
		 * @brief Lower bound that maps to normalizedDeviceCoordinates.z = 0 in the vertex shader, in meters.
		 * Anything closer than this will not be rendered by the graphics pipeline.
		 */
		float _nearClippingDistance;

		/**
		 * @brief Upper bound that maps to normalizedDeviceCoordinates.z = 1 in the vertex shader, in meters.
		 * Anything farther than this will not be rendered by the graphics pipeline.
		 */
		float _farClippingDistance;

		/**
		 * @brief This is a transform passed to the vertex shader that translates vertices from world space into camera space.
		 */
		Math::Transform _view;

		/**
		 * @brief Up direction of the camera.
		 */
		glm::vec3 _up;

		// Temp
		float _lastYaw;
		float _lastPitch;
		float _lastRoll;

		float _yaw;
		float _pitch;
		float _roll;

		float _lastScrollY;

		Camera();

		Camera(float horizontalFov, float nearClippingDistance, float farClippingDistance);

		/**
		 * @brief See IUpdatable.
		 */
		void Update() override;
	};
}