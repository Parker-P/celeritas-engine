#pragma once
#include <vector>

typedef void (*EventCallback)(void* caller, EventArgs args);

/**
 * @brief Type for sending arguments in event callbacks.
 */
class EventArgs
{

};


/**
 * @brief Class that gives a type that is able to invoke.
 */
class Event
{
	/**
	 * @brief Underlying event's function pointers.
	 */
	std::vector<EventCallback> _callbacks;

public:

	/**
	 * @brief Executes the functions pointed to by the underlying function pointers.
	 * @param caller
	 * @param args
	 */
	void Invoke(void* caller, EventArgs args)
	{
		for (auto& callback : _callbacks) {
			callback(caller, args);
		}
	}

	/**
	 * @brief Adds the underlying function pointers to the given callback.
	 * @param callback The callback function.
	 */
	void Subscribe(EventCallback c)
	{
		_callbacks.push_back(c);
	}

	/**
	 * @brief Removes the given function from the functio.
	 *
	 */
	void Unsubscribe(EventCallback c) {
		_callbacks.erase(std::remove(_callbacks.begin(), _callbacks.end(), c), _callbacks.end());
	}
};
