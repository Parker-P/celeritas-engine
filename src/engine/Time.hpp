#pragma once

class Time : public Singleton<Time> {
public:
	float _deltaTime;
};