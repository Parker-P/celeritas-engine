#pragma once

#define EVENT 

/**
 * @brief Used by implementing classes to mark themselves as a class that is meant to be observed. Classes that implement this
 * interface will have 
 */
class IObservable 
{

public:

	/**
	 * @brief Function called on implementing classes in each iteration of the main loop. The main loop is defined in VulkanApplication.cpp.
	 */
	virtual void Update() = 0;
};
