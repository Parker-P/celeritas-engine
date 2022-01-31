#pragma once

namespace Engine::Core::Renderer::CustomEntities {
	class Camera : public GameObject {
		glm::mat4 projection_; //The projection matrix of the camera. This matrix tells the vertex shader how to go from vertex 3D positions to 2D positions in the viewport. These 2D positions will then be passed to the fragment shader
	};
}