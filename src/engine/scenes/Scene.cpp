#include <vector>
#include <string>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/detail/type_vec.hpp>
#include <vulkan/vulkan.h>

#include "structural/IUpdatable.hpp"
#include "structural/IDrawable.hpp"
#include "structural/IUpdatable.hpp"
#include "engine/scenes/Material.hpp"
#include "engine/math/Transform.hpp"
#include "engine/vulkan/PhysicalDevice.hpp"
#include "engine/vulkan/Queue.hpp"
#include "engine/vulkan/Buffer.hpp"
#include "engine/vulkan/Image.hpp"
#include "engine/vulkan/ShaderResources.hpp"
#include "engine/scenes/Mesh.hpp"
#include "engine/scenes/GameObject.hpp"
#include "engine/scenes/Scene.hpp"

namespace Engine::Scenes
{
	void Scene::Update()
	{
		for (auto gameObject : _gameObjects) {
			gameObject.Update();
		}
	}
}