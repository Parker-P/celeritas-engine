#include <vector>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include "structural/Singleton.hpp"
#include "engine/vulkan/PhysicalDevice.hpp"
#include "engine/vulkan/Queue.hpp"
#include "engine/vulkan/Buffer.hpp"
#include "engine/vulkan/Image.hpp"
#include "structural/IUpdatable.hpp"
#include "engine/vulkan/ShaderResources.hpp"
#include "engine/structural/Pipelineable.hpp"
#include "engine/scenes/Vertex.hpp"
#include "engine/structural/Drawable.hpp"
#include "engine/vulkan/Image.hpp"

namespace Engine
{
	/**
	 * @brief Choke point for all drawable object used by render passes.
	 */
	class DrawablePool : public ::Structural::Singleton<DrawablePool>
	{
		/**
		 * @brief Contains.
		 */
		std::vector<Engine::Structural::Drawable*> _drawables;

	public:

		/**
		 * @brief .
		 */
		DrawablePool();


	};
}