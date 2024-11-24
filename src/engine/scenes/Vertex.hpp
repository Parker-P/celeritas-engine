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

		size_t Vertex::OffsetOf(const AttributeType& attributeType)
		{
			switch (attributeType) {
			case AttributeType::Position:
				return 0;
			case AttributeType::Normal:
				return sizeof(_position);
			case AttributeType::UV:
				return sizeof(_position) + sizeof(_normal);
			default: return 0;
			}
		}
	};
}