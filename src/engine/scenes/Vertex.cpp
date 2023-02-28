#include <glm/glm.hpp>

#include "engine/scenes/Vertex.hpp"

namespace Engine::Scenes 
{
	size_t Vertex::OffsetOf(const AttributeType& attributeType)
	{
		switch (attributeType) {
		case AttributeType::Position:
			return 0;
		case AttributeType::Normal:
			return sizeof(_position);
		case AttributeType::UV:
			return sizeof(_position) + sizeof(_normal);
		default: return 0;
		}
	}
}