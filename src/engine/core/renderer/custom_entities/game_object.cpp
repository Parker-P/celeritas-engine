#include <string>
#include <glm/glm.hpp>

#include "game_object.h"

namespace Engine::Core::Renderer::CustomEntities {
	std::string GameObject::GetName() {
		return name_;
	}
	
	glm::mat4 GameObject::GetTransform() {
		return transform_;
	}

	void GameObject::SetName(std::string name)
	{
		name_ = name;
	}

	void GameObject::SetTransform(glm::mat4 transform){
		transform_ = transform;
	}
}