#pragma once
namespace Engine::Scenes
{
	class Camera : public GameObject
	{
	public:

		float _horizontalFov;
		float _nearClippingDistance;
		float _farClippingDistance;

		/**
		 * @brief This is a transform that translates and rotates a vertex from world space into camera space.
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

		void Update();
	};
}