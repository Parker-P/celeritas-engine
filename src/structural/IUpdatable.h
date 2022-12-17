#pragma once

/**
 * @brief Used by implementing classes to mark themselves as a class that is meant to do work on each iteration of the main loop.
 */
class IUpdatable 
{
public:

	/**
	 * @brief Function called on implementing classes in each iteration of the main loop, which is defined in VulkanApplication.cpp.
	 */
	virtual void Update() = 0;
};
