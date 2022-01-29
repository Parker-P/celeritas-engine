#pragma once

namespace Engine::Core::Renderer::CustomEntities {
	//The command pool used to allocate memory for the command buffers that will be submitted to the queue family of the graphics queue. Command pools are opaque objects that command buffer memory is allocated from, and which allow the implementation to amortize the cost of resource creation across multiple command buffers. Command pools are externally synchronized, meaning that a command pool must not be used concurrently in multiple threads. That includes use via recording commands on any command buffers allocated from the pool, as well as operations that allocate, free, and reset command buffers or the pool itself.
	class Object3D {

	public:

	};
}