#include <chrono>

#include "structural/IUpdatable.hpp"
#include "structural/Singleton.hpp"
#include "engine/Time.hpp"

Time::Time()
{
	_timeStart = std::chrono::high_resolution_clock::now();
	_deltaTime = 0.0;
}

void Time::Update()
{
	_deltaTime = (std::chrono::high_resolution_clock::now() - _lastUpdateTime).count() * 0.000001f;
	_lastUpdateTime = std::chrono::high_resolution_clock::now();
}
