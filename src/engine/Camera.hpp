#pragma once

class Camera : public GameObject {
public:
	GameObject _proxy;

	Transform _view;
	Transform _projection;

	// Temp
	float _lastYaw;
	float _lastPitch;
	float _lastRoll;
	float _deltaYaw;
	float _deltaPitch;
	float _deltaRoll;

	float _yaw;
	float _pitch;
	float _roll;

	void Update();

	void GenerateProjectionTransform(const float& viewportWidth, const float& viewportHeight, const float& horizontalFovDegrees, const float& nearClipDistance, const float& farClipDistance);

	/// <summary>
	/// Initializes a camera.
	/// </summary>
	/// <param name="FOV">The field of view in degrees</param>
	/// <param name="aspectRatio">The aspect ratio of the screen</param>
	/// <param name="nearClipPlaneDistance">The plane closer than which nothing will be rendered</param>
	/// <param name="farClipPlaneDistance">The plane farther than which nothing will be rendered</param>
	//void Init(float FOV, float aspectRatio, float nearClipPlaneDistance, float farClipPlaneDistance);
};