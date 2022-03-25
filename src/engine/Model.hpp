#pragma once

// Vertex layout
struct Vertex {
	float pos[3];
};

class Model {
public:
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	uint32_t verticesSize;
	uint32_t indicesSize;
};