#include "LocalIncludes.hpp"

using namespace Engine::Vulkan;

namespace Engine::Scenes
{
	std::vector<Material> SceneLoader::LoadMaterials(VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice, tinygltf::Model& gltfScene)
	{
		std::vector<Material> outMaterials;

		for (int i = 0; i < gltfScene.materials.size(); ++i) {
			Scenes::Material m;
			auto& baseColorTextureIndex = gltfScene.materials[i].pbrMetallicRoughness.baseColorTexture.index;
			m._name = gltfScene.materials[i].name;

			if (baseColorTextureIndex >= 0) {
				auto& baseColorImageIndex = gltfScene.textures[baseColorTextureIndex].source;
				auto& baseColorImageData = gltfScene.images[baseColorImageIndex].image;
				unsigned char* copiedImageData = new unsigned char[baseColorImageData.size()];
				memcpy(copiedImageData, baseColorImageData.data(), baseColorImageData.size());
				auto size = VkExtent2D{ (uint32_t)gltfScene.images[baseColorImageIndex].width, (uint32_t)gltfScene.images[baseColorImageIndex].height };

				auto& imageCreateInfo = m._albedo._createInfo;
				imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
				imageCreateInfo.extent = { (uint32_t)size.width, (uint32_t)size.height, 1 };
				imageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
				imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
				imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				imageCreateInfo.arrayLayers = 1;
				imageCreateInfo.mipLevels = 1;
				imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
				imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
				imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
				CheckResult(vkCreateImage(logicalDevice, &imageCreateInfo, nullptr, &m._albedo._image));

				// Allocate memory on the GPU for the image.
				VkMemoryRequirements reqs;
				vkGetImageMemoryRequirements(logicalDevice, m._albedo._image, &reqs);
				VkMemoryAllocateInfo allocInfo{};
				allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				allocInfo.allocationSize = reqs.size;
				allocInfo.memoryTypeIndex = PhysicalDevice::GetMemoryTypeIndex(physicalDevice, reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				VkDeviceMemory mem;
				CheckResult(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &mem));
				CheckResult(vkBindImageMemory(logicalDevice, m._albedo._image, mem, 0));

				auto& imageViewCreateInfo = m._albedo._viewCreateInfo;
				imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				imageViewCreateInfo.components = { {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY} };
				imageViewCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
				imageViewCreateInfo.image = m._albedo._image;
				imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
				imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
				imageViewCreateInfo.subresourceRange.layerCount = 1;
				imageViewCreateInfo.subresourceRange.levelCount = 1;
				CheckResult(vkCreateImageView(logicalDevice, &imageViewCreateInfo, nullptr, &m._albedo._view));

				auto& samplerCreateInfo = m._albedo._samplerCreateInfo;
				samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
				samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
				samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
				samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
				vkCreateSampler(logicalDevice, &samplerCreateInfo, nullptr, &m._albedo._sampler);

				m._albedo._pData = copiedImageData;
				m._albedo._sizeBytes = baseColorImageData.size();

				outMaterials.push_back(m);
			}
		}

		return outMaterials;
	}

	Scene SceneLoader::LoadFile(std::filesystem::path filePath, VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice, VkCommandPool& commandPool, VkQueue& queue)
	{
		auto s = new Scenes::Scene(logicalDevice, physicalDevice);
		auto& scene = *s;
		scene._pointLights.push_back(Scenes::PointLight("DefaultLight"));

		tinygltf::Model gltfScene;
		tinygltf::TinyGLTF loader;
		std::string err;
		std::string warn;

		bool ret = loader.LoadBinaryFromFile(&gltfScene, &err, &warn, filePath.string());
		std::cout << warn << std::endl;
		std::cout << err << std::endl;

		auto materials = LoadMaterials(logicalDevice, physicalDevice, gltfScene);
		scene._materials.insert(scene._materials.end(), materials.begin(), materials.end());

		for (int i = 0; i < gltfScene.nodes.size(); ++i) {
			if (gltfScene.nodes[i].mesh < 0) {
				continue;
			}

			auto& gltfMesh = gltfScene.meshes[gltfScene.nodes[i].mesh];

			for (auto& gltfPrimitive : gltfMesh.primitives) {
				auto gameObject = Scenes::GameObject(gltfScene.nodes[i].name, &scene);
				auto& tr = gltfScene.nodes[i].translation;
				auto& sc = gltfScene.nodes[i].scale;
				auto& rot = gltfScene.nodes[i].rotation;

				auto translation = glm::mat4x4{};
				if (tr.size() == 3) {
					auto translation = glm::mat4x4{
						glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), // Column 1
						glm::vec4(0.0f,  1.0f, 0.0f, 0.0f), // Column 2
						glm::vec4(0.0f,  0.0f, 1.0f, 0.0f), // Column 3
						glm::vec4(-tr[0],  tr[1], tr[2], 1.0f)	// Column 4
					};

					gameObject._transform._matrix *= translation;
				}

				auto r = Math::Transform();
				if (rot.size() == 4) {
					r.Rotate(glm::quat{ -(float)rot[3], (float)rot[0], (float)rot[1], (float)rot[2] });
					gameObject._transform._matrix *=  r._matrix;
				}

				/*if (sc.size() == 3) {
					gameObject._transform.SetScale(glm::vec3{ sc[0], sc[1], sc[2] });
				}*/

				auto faceIndicesAccessorIndex = gltfPrimitive.indices;
				auto vertexPositionsAccessorIndex = gltfPrimitive.attributes["POSITION"];
				auto vertexNormalsAccessorIndex = gltfPrimitive.attributes["NORMAL"];
				auto uvCoords0AccessorIndex = gltfPrimitive.attributes["TEXCOORD_0"];
				//auto uvCoords1AccessorIndex = gltfPrimitive.attributes["TEXCOORD_1"];
				//auto uvCoords2AccessorIndex = gltfPrimitive.attributes["TEXCOORD_2"];

				auto faceIndicesAccessor = gltfScene.accessors[faceIndicesAccessorIndex];
				auto positionsAccessor = gltfScene.accessors[vertexPositionsAccessorIndex];
				auto normalsAccessor = gltfScene.accessors[vertexNormalsAccessorIndex];
				auto uvCoords0Accessor = gltfScene.accessors[uvCoords0AccessorIndex];
				//auto uvCoords1Accessor = gltfScene.accessors[uvCoords1AccessorIndex];
				//auto uvCoords2Accessor = gltfScene.accessors[uvCoords2AccessorIndex];

				// Load face indices.
				auto faceIndicesCount = faceIndicesAccessor.count;
				auto faceIndicesBufferIndex = gltfScene.bufferViews[faceIndicesAccessor.bufferView].buffer;
				auto faceIndicesBufferOffset = gltfScene.bufferViews[faceIndicesAccessor.bufferView].byteOffset;
				auto faceIndicesBufferStride = faceIndicesAccessor.ByteStride(gltfScene.bufferViews[faceIndicesAccessor.bufferView]);
				auto faceIndicesBufferSizeBytes = faceIndicesCount * faceIndicesBufferStride;
				std::vector<unsigned int> faceIndices(faceIndicesCount);
				if (faceIndicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
					for (int i = 0; i < faceIndicesCount; ++i) {
						unsigned short index = 0;
						memcpy(&index, gltfScene.buffers[faceIndicesBufferIndex].data.data() + faceIndicesBufferOffset + i * faceIndicesBufferStride, faceIndicesBufferStride);
						faceIndices[i] = index;
					}
				}
				else if (faceIndicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
					memcpy(faceIndices.data(), gltfScene.buffers[faceIndicesBufferIndex].data.data() + faceIndicesBufferOffset, faceIndicesBufferSizeBytes);
				}

				// Load vertex Positions.
				auto vertexPositionsCount = positionsAccessor.count;
				auto vertexPositionsBufferIndex = gltfScene.bufferViews[positionsAccessor.bufferView].buffer;
				auto vertexPositionsBufferOffset = gltfScene.bufferViews[positionsAccessor.bufferView].byteOffset;
				auto vertexPositionsBufferStride = positionsAccessor.ByteStride(gltfScene.bufferViews[positionsAccessor.bufferView]);
				auto vertexPositionsBufferSizeBytes = vertexPositionsCount * vertexPositionsBufferStride;
				std::vector<glm::vec3> vertexPositions(vertexPositionsCount);
				memcpy(vertexPositions.data(), gltfScene.buffers[vertexPositionsBufferIndex].data.data() + vertexPositionsBufferOffset, vertexPositionsBufferSizeBytes);

				// Load vertex normals.
				auto vertexNormalsCount = normalsAccessor.count;
				auto vertexNormalsBufferIndex = gltfScene.bufferViews[normalsAccessor.bufferView].buffer;
				auto vertexNormalsBufferOffset = gltfScene.bufferViews[normalsAccessor.bufferView].byteOffset;
				auto vertexNormalsBufferStride = normalsAccessor.ByteStride(gltfScene.bufferViews[normalsAccessor.bufferView]);
				auto vertexNormalsBufferSizeBytes = vertexNormalsCount * vertexNormalsBufferStride;
				std::vector<glm::vec3> vertexNormals(vertexNormalsCount);
				memcpy(vertexNormals.data(), gltfScene.buffers[vertexNormalsBufferIndex].data.data() + vertexNormalsBufferOffset, vertexNormalsBufferSizeBytes);

				// Load UV coordinates for UV slot 0.
				auto uvCoords0Count = uvCoords0Accessor.count;
				auto uvCoords0BufferIndex = gltfScene.bufferViews[uvCoords0Accessor.bufferView].buffer;
				auto uvCoords0BufferOffset = gltfScene.bufferViews[uvCoords0Accessor.bufferView].byteOffset;
				auto uvCoords0BufferStride = uvCoords0Accessor.ByteStride(gltfScene.bufferViews[uvCoords0Accessor.bufferView]);
				auto uvCoords0BufferSize = uvCoords0Count * uvCoords0BufferStride;
				std::vector<glm::vec2> uvCoords0(uvCoords0Count);
				memcpy(uvCoords0.data(), gltfScene.buffers[uvCoords0BufferIndex].data.data() + uvCoords0BufferOffset, uvCoords0BufferSize);

				gameObject._pMesh = new Scenes::Mesh{ &scene };
				auto& mesh = gameObject._pMesh;

				bool found = false;
				if (gltfPrimitive.material >= 0) {
					for (int i = 0; i < scene._materials.size() && !found; ++i) {
						if (scene._materials[i]._name == gltfScene.materials[gltfPrimitive.material].name) {
							mesh->_materialIndex = i;
							found = true;
						}
					}
					if (!found) {
						mesh->_materialIndex = 0;
					}
				}

				// Gather vertices and face indices.
				std::vector<Scenes::Vertex> vertices;
				vertices.resize(vertexPositions.size());
				for (int i = 0; i < vertexPositions.size(); ++i) {
					Scenes::Vertex v;

					// Transform all 3D space vectors into the engine's coordinate system (X Right, Y Up, Z forward).
					auto position = Math::Transform::GltfToEngine()._matrix * glm::vec4(vertexPositions[i], 1.0f);
					auto normal = Math::Transform::GltfToEngine()._matrix * glm::vec4(vertexNormals[i], 1.0f);;

					// Set the vertex attributes.
					v._position = glm::vec3(position);
					v._normal = glm::vec3(normal);
					v._uvCoord = uvCoords0[i];
					vertices[i] = v;
				}

				// Copy vertices to the GPU.
				mesh->CreateVertexBuffer(physicalDevice, logicalDevice, commandPool, queue, vertices);

				// Copy face indices to the GPU.
				mesh->CreateIndexBuffer(physicalDevice, logicalDevice, commandPool, queue, faceIndices);

				scene._gameObjects.push_back(gameObject);
				mesh->_gameObjectIndex = (unsigned int)(scene._gameObjects.size() - 1);
			}
		}

		return scene;
	}
}
