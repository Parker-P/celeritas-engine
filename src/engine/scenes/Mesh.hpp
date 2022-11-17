#pragma once

namespace Engine::Scenes
{
	/// <summary>
	/// Represents vertex attributes.
	/// </summary>
	class Vertex 
	{
	public:
		/// <summary>
		/// Attribute describing the object space position of the vertex.
		/// </summary>
		std::vector<glm::vec3> _vertexPositions;

		/// <summary>
		/// Attribute describing the object space normal vector of the vertex.
		/// </summary>
		std::vector<glm::vec3> _normals;

		/// <summary>
		/// Attribute describing the UV coordinates of the vertex.
		/// </summary>
		std::vector<glm::vec2> _uvCoords;
	};

	class Mesh
	{
	public:

		/// <summary>
		/// Name of the mesh.
		/// </summary>
		std::string	_name;

		/// <summary>
		/// Array of vertices that make up the mesh.
		/// </summary>
		std::vector<Vertex> _vertices;
		
		/// <summary>
		/// List of indices, where each index corresponds to a vertex defined in the _vertices array above.
		/// A face (triangle) is defined by three consecutive indices in this array.
		/// </summary>
		std::vector<unsigned short> _faceIndices;
	};
}