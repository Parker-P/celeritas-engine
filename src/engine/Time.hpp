#pragma once

class Time : public Structural::Singleton<Time>, public Structural::IUpdatable
{
public:
	
	/**
	 * @brief The time this instance was created, assigned to in the constructor.
	 */
	std::chrono::high_resolution_clock::time_point _timeStart;

	/**
	 * @brief Time last frame started.
	 */
	std::chrono::high_resolution_clock::time_point _lastUpdateTime;

	/**
	 * @brief The amount of time since last frame started in milliseconds.
	 */
	double _deltaTime;

	/**
	 * @brief Constructor.
	 */
	Time();

	/**
	 * @brief See IUpdatable.
	 */
	void Update() override;
};