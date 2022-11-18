#pragma once

namespace Engine::Scenes
{
	/**
	 * @brief Represents vertex attributes.
	 */
	class Vertex 
	{
	public:
		
		/**
		 * @brief Used to identify vertex attribute types.
		 */
		enum class AttributeType 
		{
			Position, Normal, UV
		};

		/**
		 * @brief Attribute describing the object space position of the vertex.
		 */
		glm::vec3 _position;

		/**
		 * @brief Attribute describing the object space normal vector of the vertex.
		 */
		glm::vec3 _normal;

		/**
		 * @brief Attribute describing the UV coordinates of the vertex.
		 */
		glm::vec2 _uvCoord;
		
		/**
		 * @brief Calculates the offset in bytes of a given attribute given its type.
		 * @param attributeType
		 * @return The offset in bytes of the given type within the Vertex class.
		 */
		static size_t OffsetOf(const AttributeType& attributeType);
	};

	class Mesh
	{
	public:

		/**
		 * @brief Name of the mesh.
		 */
		std::string	_name;

		/**
		 * @brief Array of vertices that make up the mesh.
		 */
		std::vector<Vertex> _vertices;
		
		/**
		 * @brief List of indices, where each index corresponds to a vertex defined in the _vertices array above.
		 * A face (triangle) is defined by three consecutive indices in this array.
		 */
		std::vector<unsigned short> _faceIndices;
	};
}