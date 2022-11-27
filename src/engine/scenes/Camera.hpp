#pragma once
namespace Engine::Scenes
{
	class Camera : public GameObject
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
		 * @brief This is a transform that translates and rotates a vertices from world space into camera space.
		 */
		Math::Transform _view;

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

		void Update();
	};
}