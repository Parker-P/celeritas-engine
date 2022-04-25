class Mesh {
public:
	std::string				name;
	std::vector<glm::vec3>  vertexPositions;
	std::vector<glm::vec3>  normals;
	std::vector<glm::vec2>  uvCoords;
	std::vector<int>		faceIndices;
};