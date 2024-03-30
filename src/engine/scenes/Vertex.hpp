#pragma once

namespace Engine::Scenes
{
	/**
	  * @brief Represents vertex attributes, such as positions, normals and UV coordinates.
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
		 * @brief Attribute describing the object-space position of the vertex in the engine's coordinate system (X right, Y up, Z forward).
		 */
		glm::vec3 _position;

		/**
		 * @brief Attribute describing the object-space normal vector of the vertex in the engine's coordinate system (X right, Y up, Z forward).
		 */
		glm::vec3 _normal;

		/**
		 * @brief Attribute describing the UV coordinates of the vertex in the UV coordinate system, where the origin (U = 0, V = 0) is the bottom left of the 2D space.
		 */
		glm::vec2 _uvCoord;

		/**
		 * @brief Calculates the offset in bytes of a given attribute given its type.
		 * @param attributeType
		 * @return The offset in bytes of the given type within the Vertex class.
		 */
		static size_t OffsetOf(const AttributeType& attributeType);
	};
}