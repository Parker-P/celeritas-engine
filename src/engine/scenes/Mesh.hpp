#pragma once

namespace Engine::Scenes
{
	class Mesh
	{
	public:
		std::string						_name;
		std::vector<glm::vec3>			_vertexPositions;
		std::vector<glm::vec3>			_normals;
		std::vector<glm::vec2>			_uvCoords;
		std::vector<unsigned short>		_faceIndices;
	};
}