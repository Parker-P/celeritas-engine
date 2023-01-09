#pragma once

/**
 * @brief Used by implementing classes to mark themselves as a class that is meant to bind resources (vertex buffers, index buffer
 * and descriptors) to a graphics pipeline and send draw commands to the Vulkan API.
 */
class IDrawable
{
public:

	/**
	 * @brief Function called on implementing classes whenever you want to draw the data the class contains.
	 */
	virtual void RecordDrawCommands(VkPipeline& pipeline, VkCommandBuffer& commandBuffer) = 0;
};
