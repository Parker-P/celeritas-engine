#pragma once

template <typename T>
class Singleton {
public:
	static T& Instance() {
		static T _instance;
		return _instance;
	}
};
