#pragma once
namespace Engine::Scenes
{
	class Camera : public GameObject
	{
	public:
		Math::Transform _view;
		Math::Transform _projection;

		// Temp
		float _lastYaw;
		float _lastPitch;
		float _lastRoll;

		float _yaw;
		float _pitch;
		float _roll;

		void Update();

		void GenerateProjectionTransform(const float& viewportWidth, const float& viewportHeight, const float& horizontalFovDegrees, const float& nearClipDistance, const float& farClipDistance);
	};
}