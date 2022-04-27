#pragma once

#pragma region AccessorDataTypes
enum class GltfDataType {
	NONE,
	SCALAR,
	VEC2,
	VEC3,
	VEC4,
	MAT2,
	MAT3,
	MAT4
};

enum class ComponentType {
	SIGNED_BYTE = 5120,
	UNSIGNED_BYTE = 5121,
	SIGNED_SHORT = 5122,
	UNSIGNED_SHORT = 5123,
	UNSIGNED_INT = 5125,
	FLOAT = 5126
};
#pragma endregion

#pragma region LocalTypes

class GltfMesh {
public:
	int index; // The index of the mesh in the "meshes" array in the GLTF file
	std::string name;

	// This struct describes where to find information about this mesh inside the GltfScene this mesh is in
	struct Primitive {
		struct {
			int positionsAccessorIndex; // Index of where to find vertex positions in the accessors
			int normalsAccessorIndex;	// Index of where to find vertex normals in the accessors
			int uvCoordsAccessorIndex;	// Index of where to find uv coordinates in the accessors
		} attributes;
		int indicesAccessorIndex;
	};

	std::vector<Primitive> primitives; // Describes the raw meshes that make up the mesh. For example this mesh could be made up of 2 separate cubes
};

class GltfScene {
public:

	struct Accessor {
		int bufferViewIndex;
		int	componentType;
		int	count;
		GltfDataType type;
	};

	struct BufferView {
		int byteLength; // How big the data described by this bufferView is inside the raw gltf buffer
		int	byteOffset; // Where the data starts inside the raw gltf data buffer
	};

	std::vector<GltfMesh> meshes;			// Tells you which mehses are in the scene
	std::vector<Accessor> accessors;		// Tells you how to read and interpret primitive attributes such as vertex positions or vertex normals and which bufferView to find this data in
	std::vector<BufferView> bufferViews;	// Tells you where to find mesh data inside the raw gltf data buffer
};

/// <summary>
/// Represents binary gltf data.
/// Specification at: https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#glb-file-format-specification
/// File structure overview: https://github.com/KhronosGroup/glTF-Tutorials/blob/master/gltfTutorial/gltfTutorial_003_MinimalGltfFile.md
/// </summary>
class GltfData {
public:
	// Header
	struct {
		uint32_t magic;			// Makes the file identifiable as a gltf file, it's a data format identifier
		uint32_t version;
		uint32_t fileLength;	// File size in bytes
	} header;

	// Chunk 0 (JSON)
	struct {
		uint32_t chunkLength;	// How bug this buffer is
		uint32_t chunkType;		// The type of data inside this buffer
		char* data;				// The raw json data
	} json;

	// Chunk 1 (Binary data)
	struct {
		uint32_t chunkLength;	// How bug this buffer is
		uint32_t chunkType;		// The type of data inside this buffer
		char* data;				// The raw gltf buffer
	} binaryBuffer;
};


#pragma endregion

/// <summary>
/// Class used to load .glb or .gltf 3D files
/// </summary>
class GltfLoader {
	/// <summary>
	/// Sums the integer value of each character of string into one integer
	/// </summary>
	/// <param name="string">The string to convert to integer</param>
	/// <returns>The sum of the values</returns>
	//static uint32_t GetIntValue(const char* string);
public:
	static Scene Load(std::filesystem::path filename);
};