#pragma once

namespace Engine::Scenes
{
#pragma region AccessorDataTypes
	enum class GltfDataType
	{
		NONE,
		SCALAR,
		VEC2,
		VEC3,
		VEC4,
		MAT2,
		MAT3,
		MAT4
	};

	enum class GltfComponentType
	{
		SIGNED_BYTE = 5120,
		UNSIGNED_BYTE = 5121,
		SIGNED_SHORT = 5122,
		UNSIGNED_SHORT = 5123,
		UNSIGNED_INT = 5125,
		FLOAT = 5126
	};
#pragma endregion

#pragma region LocalTypes

	/**
	 * @brief Represents a mesh in a gltf file, that can be made up of primitives such as cubes, spheres or any other shape.
	 */
	class GltfMesh
	{
	public:
		/**
		 * @brief The index of the mesh in the "meshes" array in the GLTF file.
		 */
		int index;

		/**
		 * @brief Name of the mesh as parsed from the file.
		 */
		std::string name;

		/**
		 * @brief This struct describes where to find information about this mesh inside the GltfScene this mesh is in.
		 */
		struct Primitive
		{
			/**
			 * @brief Vertex attributes' accessors.
			 */
			struct
			{
				/**
				 * @brief Index of where to find vertex positions in the accessors.
				 */
				int positionsAccessorIndex;
				
				/**
				 * @brief Index of where to find vertex normals in the accessors.
				 */
				int normalsAccessorIndex;

				/**
				 * @brief Index of where to find uv coordinates in the accessors.
				 */
				int uvCoordsAccessorIndex;

			} vertexAttributes;

			/**
			 * @brief Index of where to find face indices in the accessors.
			 */
			int indicesAccessorIndex;
		};

		/**
		 * @brief Describes the raw shapes that make up the mesh. For example this mesh could be made up of 2 separate cubes.
		 */
		std::vector<Primitive> primitives;
	};

	class GltfScene
	{
	public:

		/**
		 * @brief An accessor object refers to a bufferView and contains properties that define the type and layout of the data of this bufferView.
		 */
		struct Accessor
		{
			/**
			 * @brief Index into the buffer view.
			 */
			int bufferViewIndex;

			/**
			 * @brief See GltfComponentType enum.
			 */
			int	componentType;

			/**
			 * @brief How many elements the buffer view at bufferViewIndex contains.
			 */
			int	count;

			/**
			 * @brief .
			 */
			GltfDataType type;
		};

		/**
		 * @brief A buffer represents a block of raw binary data, without an inherent structure or meaning.
		 * It is the accessor's job to define what each bufferView contains.
		 */
		struct BufferView
		{
			/**
			 * @brief How big the data described by this bufferView is inside the raw gltf buffer.
			 */
			int byteLength;

			/**
			 * @brief Where the data starts inside the raw gltf data buffer.
			 */
			int	byteOffset;
		};

		/**
		 * @brief Tells you which mehses are in the scene.
		 */
		std::vector<GltfMesh> meshes;

		/**
		 * @brief Tells you how to read and interpret primitive attributes such as vertex positions or vertex normals and which bufferView to find this data in.
		 */
		std::vector<Accessor> accessors;

		/**
		 * @brief Tells you where to find mesh data inside the raw gltf data buffer.
		 */
		std::vector<BufferView> bufferViews;
	};

	/**
	 * @brief Represents binary gltf data.
	 * Specification at: @see [file.specification](https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#glb-file-format-specification)
	 * File structure overview: @see [file.structure]https://github.com/KhronosGroup/glTF-Tutorials/blob/master/gltfTutorial/gltfTutorial_003_MinimalGltfFile.md.
	 */
	class GltfData
	{
	public:
		
		/**
		 * @brief Header.
		 */
		struct
		{
			/**
			 * @brief Makes the file identifiable as a gltf file, it's a data format identifier.
			 */
			uint32_t magic;

			/**
			 * @brief Gltf version standard this data conforms to.
			 */
			uint32_t version;

			/**
			 * @brief File size in bytes.
			 */
			uint32_t fileLength;
		} header;

		/**
		 * @brief Describes a generic gltf buffer that will typically contain either JSON or binary data.
		 */
		struct GltfBuffer {
			/**
			 * @brief Buffer size in bytes.
			 */
			uint32_t chunkLength;

			/**
			 * @brief The type of data inside this buffer. Number 1313821514 indicates type JSON type, number 5130562 indicates binary type.
			 */
			uint32_t chunkType;

			/**
			 * @brief The raw json data.
			 */
			char* data;
		};

		/**
		 * @brief Chunk 0 (JSON).
		 */
		GltfBuffer json;

		/**
		 * @brief Chunk 1 (Binary data).
		 */
		GltfBuffer binaryBuffer;
	};


#pragma endregion

	/**
	 * @brief Class used to load .glb or .gltf 3D scene files.
	 */
	class GltfLoader
	{
	public:
		/**
		 * @brief Loads a scene.
		 * @param filename
		 * @return A scene object containing scene data.
		 */
		static Scene LoadScene(std::filesystem::path filename);
	};
}