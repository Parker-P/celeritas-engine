#pragma once

class EventArgs
{

};

class Event
{
public:

	/**
	 * @brief Underlying event's function pointer.
	 */
	void (*_e)(void* caller, EventArgs args);

	void Invoke(void* caller, EventArgs args) 
	{
		_e(caller, args);
	}
};

class IObservable
{
	Invoke() 
	{

	}
};
