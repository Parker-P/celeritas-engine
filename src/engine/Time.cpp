#include <chrono>

#include "structural/IUpdatable.hpp"
#include "structural/IPhysicsUpdatable.hpp"
#include "structural/Singleton.hpp"
#include "engine/Time.hpp"

Time::Time()
{
	_timeStart = std::chrono::high_resolution_clock::now();
	_deltaTime = 0.0;
	_physicsDeltaTime = 0.0;
	_lastUpdateTime = _timeStart;
	_lastPhysicsUpdateTime = _timeStart;
}

void Time::Update()
{
	auto now = std::chrono::high_resolution_clock::now();
	_deltaTime = (now - _lastUpdateTime).count() * 0.000001;
	_lastUpdateTime = now;
}

void Time::PhysicsUpdate()
{
	auto now = std::chrono::high_resolution_clock::now();
	_physicsDeltaTime = (now - _lastPhysicsUpdateTime).count() * 0.000001;
	_lastPhysicsUpdateTime = now;
}
