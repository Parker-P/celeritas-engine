#pragma once

namespace Engine::Vulkan
{
	/**
	 * @brief A queue is an ordered collection of commands that gets passed into a command buffer.
	 */
	struct Queue
	{
		/**
		 * @brief Identifier for Vulkan.
		 */
		VkQueue _handle;

		/**
		 * @brief A queue family index enables Vulkan to group queues that serve similar purposes (a.k.a. have the same properties).
		 */
		uint32_t _familyIndex;
	};
}
