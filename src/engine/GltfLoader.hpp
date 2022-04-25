#pragma once

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
	static GltfData Load(std::filesystem::path filename);
};