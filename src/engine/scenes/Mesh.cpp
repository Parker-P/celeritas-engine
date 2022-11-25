#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/detail/type_vec.hpp>

#include "engine/scenes/Mesh.hpp"

namespace Engine::Scenes
{
    size_t Mesh::Vertex::OffsetOf(const AttributeType& attributeType)
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