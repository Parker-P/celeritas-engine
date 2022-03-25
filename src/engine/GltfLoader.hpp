#pragma once

/// <summary>
/// Represents binary gltf data
/// </summary>
class GltfData {
public:
	// Header
	struct {
		uint32_t magic;
		uint32_t version;
		uint32_t fileLength;
	} header;
	
	// Chunk 0 (JSON)
	struct {
		uint32_t chunkLength;
		uint32_t length;
	} json;
	
	// Chunk 1 (Binary data)
	struct {
		uint32_t chunkLength;
		uint32_t length;
	} json;
};

/// <summary>
/// Class used to load .glb or .gltf 3D files
/// </summary>
class GltfLoader {
public:
	static GltfData Load(std::filesystem::path filename);
};