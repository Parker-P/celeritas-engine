#include <vector>
#include <string>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/detail/type_vec.hpp>
#include <vulkan/vulkan.h>

#include "structural/IUpdatable.hpp"
#include "engine/vulkan/PhysicalDevice.hpp"
#include "engine/vulkan/Queue.hpp"
#include "engine/vulkan/Image.hpp"
#include "engine/vulkan/Buffer.hpp"
#include "engine/vulkan/ShaderResources.hpp"
#include "engine/scenes/Vertex.hpp"
#include "engine/structural/Pipelineable.hpp"
#include "engine/structural/Drawable.hpp"
#include "engine/scenes/Material.hpp"
#include "engine/scenes/Mesh.hpp"
#include <utils/Utils.hpp>

namespace Engine::Scenes
{
    void Mesh::CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice)
    {

    }

    void Mesh::Update()
    {

    }
}