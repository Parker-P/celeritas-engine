#pragma once

namespace Structural
{
	/**
	 * @brief Used by implementing classes to mark themselves as a class that is meant to do work on the main loop of the physics thread.
	 */
	class IPhysicsUpdatable
	{
	public:

		/**
		 * @brief Function called on implementing classes in each iteration of the main loop. The main loop is defined in VulkanApplication.cpp.
		 */
		virtual void PhysicsUpdate() = 0;
	};
}
