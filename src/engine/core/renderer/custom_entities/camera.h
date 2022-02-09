#pragma once

namespace Engine::Core::Renderer::CustomEntities {
	class Camera : public GameObject {
		glm::mat4 view_; //The view matrix. This matrix is used to transform gameobjects' vertices in a way that makes it look as if the camera was in this position
		glm::mat4 projection_; //The projection matrix of the camera. This matrix tells the vertex shader how to go from vertex 3D positions to 2D positions in the viewport. These 2D positions will then be passed to the fragment shader to make sure the right pixels are colored
	};
}