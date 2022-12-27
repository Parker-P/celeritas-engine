#pragma once
#include <vector>

/**
 * @brief Type for sending arguments in event callbacks.
 */
class EventArgs
{

};

/**
 * @brief Class that wraps an event, which is just a collection of function pointers.
 */
template <typename ArgType> class Event
{
public:

	typedef void (*EventCallback)(void* caller, ArgType args);

	/**
	 * @brief Underlying event's function pointers.
	 */
	std::vector<EventCallback> _subscribers;

	/**
	 * @brief Executes the functions pointed to by the underlying function pointers.
	 * @param caller
	 * @param args
	 */
	void Invoke(void* caller, ArgType args)
	{
		for (auto& subscriber : _subscribers) {
			subscriber(caller, args);
		}
	}

	/**
	 * @brief Adds the given callback function pointer to the underlying function pointers vector.
	 * @param callback The callback function.
	 */
	void Subscribe(EventCallback subscriber)
	{
		_subscribers.push_back(subscriber);
	}

	/**
	 * @brief Removes the given function from the underlying function pointers vector.
	 */
	void Unsubscribe(EventCallback unsubscriber) {
		_subscribers.erase(std::remove(_subscribers.begin(), _subscribers.end(), unsubscriber), _subscribers.end());
	}
};
