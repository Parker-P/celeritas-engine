#pragma once

namespace Engine::Scenes
{
	class GameObject;

	/**
	 * @brief Represents a collection of vertices and face indices as triangles.
	 */
	class Mesh : public IDrawable
	{
	public:

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
			 * @brief Attribute describing the object-space position of the vertex in the engine's coordinate system (X right, Y up, Z forward).
			 */
			glm::vec3 _position;

			/**
			 * @brief Attribute describing the object-space normal vector of the vertex in the engine's coordinate system (X right, Y up, Z forward).
			 */
			glm::vec3 _normal;

			/**
			 * @brief Attribute describing the UV coordinates of the vertex. A vertex might be part of a UV seam, so it could have multiple
			 * UV coordinates.
			 */
			glm::vec2 _uvCoord;

			/**
			 * @brief Calculates the offset in bytes of a given attribute given its type.
			 * @param attributeType
			 * @return The offset in bytes of the given type within the Vertex class.
			 */
			static size_t OffsetOf(const AttributeType& attributeType);
		};

		/**
		 * @brief Index into the current scene's game object list.
		 */
		unsigned int _gameObject;

		/**
		 * @brief List of vertices that make up the mesh.
		 */
		std::vector<Vertex> _vertices;
		
		/**
		 * @brief List of indices, where each index corresponds to a vertex defined in the _vertices array above.
		 * A face (triangle) is defined by three consecutive indices in this array.
		 */
		std::vector<unsigned int> _faceIndices;

		/**
		 * @brief Index into the current scene's material list.
		 */
		unsigned int _materialIndex;

		/**
		 * @brief Uses Vulkan to draw the mesh.
		 */
		void Draw() override;
	};
}