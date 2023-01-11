#include <vector>
#include <string>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/detail/type_vec.hpp>
#include <vulkan/vulkan.h>

#include "structural/IUpdatable.hpp"
#include "structural/IDrawable.hpp"
#include "engine/vulkan/PhysicalDevice.hpp"
#include "engine/vulkan/Queue.hpp"
#include "engine/vulkan/Buffer.hpp"
#include "engine/scenes/Material.hpp"
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

    void Mesh::RecordDrawCommands(VkPipeline& pipeline, VkCommandBuffer& commandBuffer)
    {
        
    }

    void Mesh::Update()
    {

    }
}