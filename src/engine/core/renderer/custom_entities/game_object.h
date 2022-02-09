#pragma once

namespace Engine::Core::Renderer::CustomEntities {
	class GameObject {
		std::string name_;
		glm::mat4 transform_; //The matrix that represents the position of the gameobject in 3D space
	public:
		std::string GetName();
		glm::mat4 GetTransform();
		void SetName(std::string name);
		void SetTransform(glm::mat4 transform);
	};
}