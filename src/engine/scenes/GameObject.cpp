#include <string>
#include <vector>
#include <iostream>

#include <vulkan/vulkan.h>
#include <glm/gtc/matrix_transform.hpp>

#include "structural/IUpdatable.hpp"
#include "engine/vulkan/PhysicalDevice.hpp"
#include "engine/vulkan/Queue.hpp"
#include "engine/vulkan/Buffer.hpp"
#include "engine/vulkan/Image.hpp"
#include "engine/vulkan/ShaderResources.hpp"
#include "engine/structural/Pipelineable.hpp"
#include "engine/scenes/Vertex.hpp"
#include "engine/structural/Drawable.hpp"
#include "engine/math/Transform.hpp"
#include "engine/scenes/Material.hpp"
#include "engine/scenes/Vertex.hpp"
#include "engine/scenes/Mesh.hpp"
#include "engine/scenes/GameObject.hpp"

Engine::Scenes::GameObject::GameObject(const std::string& name, Scene& scene)
{
	_name = name;
	_scene = &scene;
}

void Engine::Scenes::GameObject::Update()
{
	_mesh.Update();
}
