#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <stb/stb_image.h>
#include <vulkan/vulkan.h>
#include <stb/stb_image_write.h>

#include "utils/Utils.hpp"
#include "settings/Paths.hpp"
#include "engine/vulkan/Queue.hpp"
#include "engine/vulkan/PhysicalDevice.hpp"
#include "engine/vulkan/Image.hpp"
#include "engine/vulkan/Buffer.hpp"
#include "structural/IUpdatable.hpp"
#include "structural/Singleton.hpp"
#include "engine/Time.hpp"
#include "settings/GlobalSettings.hpp"
#include "engine/math/Transform.hpp"
#include "engine/vulkan/PhysicalDevice.hpp"
#include "engine/scenes/Material.hpp"
#include "engine/vulkan/ShaderResources.hpp"
#include "engine/scenes/Vertex.hpp"
#include "engine/structural/IPipelineable.hpp"
#include "engine/scenes/CubicalEnvironmentMap.hpp"
#include "utils/BoxBlur.hpp"

namespace Engine::Scenes
{
	// Returns the 0-based pixel coordinates given the component index of an image data array. Assumes that the
	// input component index starts from 0, and that the origin of the image is at the top left corner, with X
	// increasing to the right, and Y increasing downward.
	glm::vec2 ComponentIndexToCartesian(int componentIndex, int imageWidthPixels) {
		int x = (int(componentIndex * 0.25f) % imageWidthPixels);
		int y = (int(componentIndex * 0.25f) / imageWidthPixels);
		return glm::vec2(x, y);
	}

	// Returns the 0-based component index into an image data array given the pixel coordinates of the image.
	// Assumes that X and Y both start from 0, and the maximum X can be is equal to imageWidthPixels-1.
	int CartesianToComponentIndex(int x, int y, int imageWidthPixels) {
		return (x + (y * imageWidthPixels)) * 4;
	}

	// Blurs an image by averaging the value of each pixel with the value of "samples" pixels at distance of "radiusPixels" pixels in a 
	// circular pattern around it. Returns the blurred image data.
	std::vector<unsigned char> BoxBlurImage(const std::vector<unsigned char>& inImageData, int widthPixels, int heightPixels, int radiusPixels) {
		// Make sure that the input values are valid.
		if (radiusPixels < 1) {
			radiusPixels = 1;
		}
		if (inImageData.data() == nullptr || inImageData.size() <= 0) {
			return std::vector<unsigned char>();
		}

		// Prepare some variables used throughout the function.
		std::vector<unsigned char> outImageData;
		int boxSideLength = (radiusPixels * 2) + 1;
		outImageData.resize(inImageData.size());

		for (int componentIndex = 0; componentIndex < inImageData.size(); componentIndex += 4) {
			auto currentPixelCoordinates = ComponentIndexToCartesian(componentIndex, widthPixels);

			// Start sampling from the top left relative to the current pixel. Coordinates increase going left to right for X
			// and top to bottom for Y.
			int sampleCoordinateY = int(currentPixelCoordinates.y - radiusPixels);
			glm::vec4 averageColor(-1.0f);
			for (int i = 0; i < boxSideLength; ++i) {
				int sampleCoordinateX = int(currentPixelCoordinates.x - radiusPixels);
				for (int j = 0; j < boxSideLength; ++j) {

					// Only sample if the coordinates fall within the range of the image.
					if (auto sampleIndex = CartesianToComponentIndex(sampleCoordinateX, sampleCoordinateY, widthPixels); sampleIndex >= 0 && sampleIndex < inImageData.size()) {
						glm::vec4 sampledColor = glm::vec4(inImageData[sampleIndex], inImageData[sampleIndex + 1], inImageData[sampleIndex + 2], inImageData[sampleIndex + 3]);

						// Only calculate the average if averageColor is initialized, otherwise just use the average color as is, to account for
						// the first time this line of code is reached.
						averageColor = averageColor.x < 0 ? sampledColor : (averageColor + sampledColor);
					}
					++sampleCoordinateX;
				}
				++sampleCoordinateY;
			}

			// Calculate the average.
			averageColor /= (boxSideLength * boxSideLength);

			// Assign the calculated average color.
			outImageData[componentIndex] = int(averageColor.r);
			outImageData[componentIndex + 1] = int(averageColor.g);
			outImageData[componentIndex + 2] = int(averageColor.b);
			outImageData[componentIndex + 3] = int(averageColor.a);
		}

		return outImageData;
	}

	void CubicalEnvironmentMap::GenerateFaceImage(CubeMapFace face)
	{
		// The world space unit vector that points in the positive X direction of the image (must be left to right), as if the image was placed in the 3D world on a square plane.
		glm::vec3 imageXWorldSpace;

		// The world space unit vector that points in the positive Y direction of the image (must be bottom to top), as if the image was placed in the 3D world on a square plane.
		glm::vec3 imageYWorldSpace;

		// The origin of the image (must be the bottom left corner) in world space, as if the image was placed in the 3D world on a square plane.
		glm::vec3 imageOriginWorldSpace;

		switch (face) {
		case CubeMapFace::FRONT:
			imageXWorldSpace = glm::vec3(1.0f, 0.0f, 0.0f);
			imageYWorldSpace = glm::vec3(0.0f, 1.0f, 0.0f);
			imageOriginWorldSpace = glm::vec3(-0.5f, 0.5f, 0.5f);
			break;
		case CubeMapFace::RIGHT:
			imageXWorldSpace = glm::vec3(0.0f, 0.0f, -1.0f);
			imageYWorldSpace = glm::vec3(0.0f, 1.0f, 0.0f);
			imageOriginWorldSpace = glm::vec3(0.5f, 0.5f, 0.5f);
			break;
		case CubeMapFace::BACK:
			imageXWorldSpace = glm::vec3(-1.0f, 0.0f, 0.0f);
			imageYWorldSpace = glm::vec3(0.0f, 1.0f, 0.0f);
			imageOriginWorldSpace = glm::vec3(0.5f, 0.5f, -0.5f);
			break;
		case CubeMapFace::LEFT:
			imageXWorldSpace = glm::vec3(0.0f, 0.0f, 1.0f);
			imageYWorldSpace = glm::vec3(0.0f, 1.0f, 0.0f);
			imageOriginWorldSpace = glm::vec3(-0.5f, 0.5f, -0.5f);
			break;
		case CubeMapFace::UPPER:
			imageXWorldSpace = glm::vec3(1.0f, 0.0f, 0.0f);
			imageYWorldSpace = glm::vec3(0.0f, 0.0f, -1.0f);
			imageOriginWorldSpace = glm::vec3(-0.5f, 0.5f, -0.5f);
			break;
		case CubeMapFace::LOWER:
			imageXWorldSpace = glm::vec3(1.0f, 0.0f, 0.0f);
			imageYWorldSpace = glm::vec3(0.0f, 0.0f, 1.0f);
			imageOriginWorldSpace = glm::vec3(-0.5f, -0.5f, 0.5f);
			break;
		}

		for (auto y = 0; y < _faceSizePixels; ++y) {
			for (auto x = 0; x < _faceSizePixels; ++x) {

				// First we calculate the cartesian coordinate of the pixel of the cube map's face we are considering, in world space.
				glm::vec3 cartesianCoordinatesOnFace;
				cartesianCoordinatesOnFace = imageOriginWorldSpace + (imageXWorldSpace * ((1.0f / _faceSizePixels) * x));
				cartesianCoordinatesOnFace += -imageYWorldSpace * ((1.0f / _faceSizePixels) * y);

				// Then we get the cartesian coordinates on the sphere from the cartesian coordinates on the face. You can
				// imagine a cube that contains a sphere of exactly the same radius. Only the sphere's poles and sides will
				// touch each cube's face exactly at the center. We take advantage of the fact that the radius of the sphere
				// is constant (always equal to 1 for simplicity in our case) so all we need to do is normalize the coordinates
				// on the face and we get the cartesian coordinates on the sphere. This kind of simulates shooting a ray from
				// the current pixel on the cube towards the center of the sphere, intersecting it with the sphere, and taking 
				// the coordinates of the intersection point.
				auto cartesianCoordinatesOnSphere = glm::normalize(cartesianCoordinatesOnFace);

				float localXAngleDegrees = 0.0f;
				float localYAngleDegrees = 0.0f;
				float azimuthDegrees = 0.0f;
				float zenithDegrees = 0.0f;

				// Then we calculate the spherical coordinates by using some trig.
				// Depending on the coordinate, the angles could either be negative or positive, according to the left hand rule.
				// This engine uses a left-handed coordinate system.
				switch (face) {
				case CubeMapFace::FRONT:
					localXAngleDegrees = glm::degrees(atanf(cartesianCoordinatesOnFace.x * 2.0f));
					localYAngleDegrees = (90.0f - glm::degrees(acosf(cartesianCoordinatesOnSphere.y))) * -1.0f;
					azimuthDegrees = cartesianCoordinatesOnFace.x < 0.0f ? abs(localXAngleDegrees) * -1.0f : abs(localXAngleDegrees);
					zenithDegrees = cartesianCoordinatesOnFace.y < 0.0f ? abs(localYAngleDegrees) : abs(localYAngleDegrees) * -1.0f;
					azimuthDegrees = fmodf((360.0f + azimuthDegrees), 360.0f); // This transforms any angle into the [0-360] degree domain.
					break;
				case CubeMapFace::RIGHT:
					localXAngleDegrees = glm::degrees(atanf(cartesianCoordinatesOnFace.z * 2.0f));
					localYAngleDegrees = (90.0f - glm::degrees(acosf(cartesianCoordinatesOnSphere.y))) * -1.0f;
					localXAngleDegrees = cartesianCoordinatesOnFace.z > 0.0f ? abs(localXAngleDegrees) * -1.0f : abs(localXAngleDegrees);
					localYAngleDegrees = cartesianCoordinatesOnFace.y < 0.0f ? abs(localYAngleDegrees) : abs(localYAngleDegrees) * -1.0f;
					localXAngleDegrees += 90;
					azimuthDegrees = fmodf((360.0f + localXAngleDegrees), 360.0f);
					zenithDegrees = localYAngleDegrees;
					break;
				case CubeMapFace::BACK:
					localXAngleDegrees = glm::degrees(atanf(cartesianCoordinatesOnFace.x * 2.0f));
					localYAngleDegrees = (90.0f - glm::degrees(acosf(cartesianCoordinatesOnSphere.y))) * -1.0f;
					localXAngleDegrees = cartesianCoordinatesOnFace.x > 0.0f ? abs(localXAngleDegrees) * -1.0f : abs(localXAngleDegrees);
					localYAngleDegrees = cartesianCoordinatesOnFace.y < 0.0f ? abs(localYAngleDegrees) : abs(localYAngleDegrees) * -1.0f;
					localXAngleDegrees += 180;
					azimuthDegrees = fmodf((360.0f + localXAngleDegrees), 360.0f);
					zenithDegrees = localYAngleDegrees;
					break;
				case CubeMapFace::LEFT:
					localXAngleDegrees = glm::degrees(atanf(cartesianCoordinatesOnFace.z * 2.0f));
					localYAngleDegrees = (90.0f - glm::degrees(acosf(cartesianCoordinatesOnSphere.y))) * -1.0f;
					localXAngleDegrees = cartesianCoordinatesOnFace.z < 0.0f ? abs(localXAngleDegrees) * -1.0f : abs(localXAngleDegrees);
					localYAngleDegrees = cartesianCoordinatesOnFace.y < 0.0f ? abs(localYAngleDegrees) : abs(localYAngleDegrees) * -1.0f;
					localXAngleDegrees += 270;
					azimuthDegrees = fmodf((360.0f + localXAngleDegrees), 360.0f);
					zenithDegrees = localYAngleDegrees;
					break;
				case CubeMapFace::UPPER:
					if (cartesianCoordinatesOnSphere.y < 1.0f) {
						auto temp = glm::normalize(glm::vec3(cartesianCoordinatesOnSphere.x, 0.0f, cartesianCoordinatesOnSphere.z));
						localXAngleDegrees = glm::degrees(acosf(temp.z));
						localYAngleDegrees = glm::degrees(acosf(cartesianCoordinatesOnSphere.y));
						localXAngleDegrees = cartesianCoordinatesOnFace.x < 0.0f ? abs(localXAngleDegrees) * -1.0f : abs(localXAngleDegrees);
						localYAngleDegrees = cartesianCoordinatesOnFace.y < 0.0f ? abs(localYAngleDegrees) : abs(localYAngleDegrees) * -1.0f;
						azimuthDegrees = fmodf((360.0f + localXAngleDegrees), 360.0f);
						zenithDegrees = (90.0f + localYAngleDegrees) * -1.0f;
					}
					else {
						continue;
					}
					break;
				case CubeMapFace::LOWER:
					if (cartesianCoordinatesOnSphere.y < 1.0f) {
						auto temp = glm::normalize(glm::vec3(cartesianCoordinatesOnSphere.x, 0.0f, cartesianCoordinatesOnSphere.z));
						localXAngleDegrees = glm::degrees(acosf(temp.z));
						localYAngleDegrees = glm::degrees(acosf(cartesianCoordinatesOnSphere.y));
						localXAngleDegrees = cartesianCoordinatesOnFace.x < 0.0f ? abs(localXAngleDegrees) * -1.0f : abs(localXAngleDegrees);
						localYAngleDegrees = cartesianCoordinatesOnFace.y < 0.0f ? abs(localYAngleDegrees) : abs(localYAngleDegrees) * -1.0f;
						azimuthDegrees = fmodf((360.0f + localXAngleDegrees), 360.0f);
						zenithDegrees = (90.0f - localYAngleDegrees) * -1.0f;
					}
					else {
						continue;
					}
					break;
				}

				// UV coordinates into the spherical HDRi image.
				auto uCoordinate = fmodf(0.5f + (azimuthDegrees / 360.0f), 1.0f);
				auto vCoordinate = 0.5f + (zenithDegrees / -180.0f);

				// This calculates at which pixel from the left (for U) and from the top (for V)
				// we need to fetch from the spherical HDRi.
				int pixelNumberU = (int)ceil(uCoordinate * _hdriSizePixels.width);
				int pixelNumberV = (int)ceil((1.0f - vCoordinate) * _hdriSizePixels.height);

				// This calculates the index into the spherical HDRi image accounting for 2 things:
				// 1) the pixel numbers calculated above
				// 2) the fact that each pixel is actually stored in 4 separate cells that represent
				//    each channel of each individual pixel. The channels used are always 4 (RGBA).
				int componentIndex = (pixelNumberU * 4) + ((_hdriSizePixels.width * 4) * (pixelNumberV - 1));

				// Fetch the channels from the spherical HDRi based on the calculations made above.
				auto red = _hdriImageData[componentIndex];
				auto green = _hdriImageData[componentIndex + 1];
				auto blue = _hdriImageData[componentIndex + 2];
				auto alpha = _hdriImageData[componentIndex + 3];

				// Assign the color fetched from the HDRi to the cube map face's image we want to generate.
				int faceComponentIndex = (x + (_faceSizePixels * y)) * 4;
				switch (face) {
				case CubeMapFace::FRONT:
					_front[faceComponentIndex] = red;
					_front[faceComponentIndex + 1] = green;
					_front[faceComponentIndex + 2] = blue;
					_front[faceComponentIndex + 3] = alpha;
					break;
				case CubeMapFace::RIGHT:
					_right[faceComponentIndex] = red;
					_right[faceComponentIndex + 1] = green;
					_right[faceComponentIndex + 2] = blue;
					_right[faceComponentIndex + 3] = alpha;
					break;
				case CubeMapFace::BACK:
					_back[faceComponentIndex] = red;
					_back[faceComponentIndex + 1] = green;
					_back[faceComponentIndex + 2] = blue;
					_back[faceComponentIndex + 3] = alpha;
					break;
				case CubeMapFace::LEFT:
					_left[faceComponentIndex] = red;
					_left[faceComponentIndex + 1] = green;
					_left[faceComponentIndex + 2] = blue;
					_left[faceComponentIndex + 3] = alpha;
					break;
				case CubeMapFace::UPPER:
					_upper[faceComponentIndex] = red;
					_upper[faceComponentIndex + 1] = green;
					_upper[faceComponentIndex + 2] = blue;
					_upper[faceComponentIndex + 3] = alpha;
					break;
				case CubeMapFace::LOWER:
					_lower[faceComponentIndex] = red;
					_lower[faceComponentIndex + 1] = green;
					_lower[faceComponentIndex + 2] = blue;
					_lower[faceComponentIndex + 3] = alpha;
					break;
				}
			}
		}
	}

	void CubicalEnvironmentMap::WriteImagesToFiles(std::filesystem::path absoluteFolderPath)
	{
		if (!std::filesystem::exists(absoluteFolderPath)) {
			if (std::filesystem::is_directory(absoluteFolderPath)) {
				std::filesystem::create_directories(absoluteFolderPath);
			}
			else {
				std::cout << "provided path is not a folder" << std::endl;
				return;
			}
		}

		auto frontFaceImagePath = absoluteFolderPath /= std::filesystem::path("FrontFace.png");
		stbi_write_png(frontFaceImagePath.string().c_str(),
			_faceSizePixels,
			_faceSizePixels,
			4,
			_front.data(),
			_faceSizePixels * 4);

		auto rightFaceImagePath = absoluteFolderPath /= std::filesystem::path("RightFace.png");
		stbi_write_png(rightFaceImagePath.string().c_str(),
			_faceSizePixels,
			_faceSizePixels,
			4,
			_right.data(),
			_faceSizePixels * 4);

		auto backFaceImagePath = absoluteFolderPath /= std::filesystem::path("BackFace.png");
		stbi_write_png(backFaceImagePath.string().c_str(),
			_faceSizePixels,
			_faceSizePixels,
			4,
			_back.data(),
			_faceSizePixels * 4);

		auto leftFaceImagePath = absoluteFolderPath /= std::filesystem::path("LeftFace.png");
		stbi_write_png(leftFaceImagePath.string().c_str(),
			_faceSizePixels,
			_faceSizePixels,
			4,
			_left.data(),
			_faceSizePixels * 4);

		auto upperFaceImagePath = absoluteFolderPath /= std::filesystem::path("UpperFace.png");
		stbi_write_png(upperFaceImagePath.string().c_str(),
			_faceSizePixels,
			_faceSizePixels,
			4,
			_upper.data(),
			_faceSizePixels * 4);

		auto lowerFaceImagePath = absoluteFolderPath /= std::filesystem::path("LowerFace.png");
		stbi_write_png(lowerFaceImagePath.string().c_str(),
			_faceSizePixels,
			_faceSizePixels,
			4,
			_lower.data(),
			_faceSizePixels * 4);
	}

	void CubicalEnvironmentMap::LoadFromSphericalHDRI(Vulkan::PhysicalDevice& physicalDevice, std::filesystem::path imageFilePath)
	{
		int wantedComponents = 4;
		int componentsDetected;
		int width;
		int height;

		// First, we load the spherical HDRi image.
		// In the stbi_load() function, comp stands for components. In a PNG image, for example, there are 4 components 
		// for each pixel, red, green, blue and alpha.
		// The image's pixels are read and stored left to right, top to bottom, relative to the image.
		// Each pixel's component is an unsigned char.
		_hdriImageData = stbi_load(imageFilePath.string().c_str(), &width, &height, &componentsDetected, wantedComponents);

		_hdriSizePixels.width = width;
		_hdriSizePixels.height = height;

		if (nullptr == _hdriImageData) {
			std::cout << "failed loading image " << imageFilePath.string() << std::endl;
			std::exit(-1);
		}

		auto imageLength = width * height * 4;
		auto pixelCount = width * height;

		// Then we initialize the image data containers.
		_faceSizePixels = width / 4;
		int dataSize = _faceSizePixels * _faceSizePixels * 4;
		_front = std::vector<unsigned char>(dataSize);
		_right = std::vector<unsigned char>(dataSize);
		_back = std::vector<unsigned char>(dataSize);
		_left = std::vector<unsigned char>(dataSize);
		_upper = std::vector<unsigned char>(dataSize);
		_lower = std::vector<unsigned char>(dataSize);
		//auto debugImage = std::vector<unsigned char>(imageLength);

		// Now we start sampling the spherical HDRi for each individual pixel of each
		// face of the cube map we want to generate.
		GenerateFaceImage(CubeMapFace::FRONT);
		GenerateFaceImage(CubeMapFace::RIGHT);
		GenerateFaceImage(CubeMapFace::BACK);
		GenerateFaceImage(CubeMapFace::LEFT);
		GenerateFaceImage(CubeMapFace::UPPER);
		GenerateFaceImage(CubeMapFace::LOWER);

		//WriteImagesToFiles();
		Utils::BoxBlur blurrer;
		auto blur1 = blurrer.Run(physicalDevice._handle, _front.data(), _faceSizePixels, _faceSizePixels, 10);
		auto blur2 = blurrer.Run(physicalDevice._handle, _front.data(), _faceSizePixels, _faceSizePixels, 20);
		auto blur3 = blurrer.Run(physicalDevice._handle, _front.data(), _faceSizePixels, _faceSizePixels, 40);
		auto blur4 = blurrer.Run(physicalDevice._handle, _front.data(), _faceSizePixels, _faceSizePixels, 80);

		auto path = Settings::Paths::TexturesPath() /= "FrontFaceBlurred1.png";
		stbi_write_png(path.string().c_str(),
			_faceSizePixels,
			_faceSizePixels,
			4,
			blur1,
			_faceSizePixels * 4);

		path = Settings::Paths::TexturesPath() /= "FrontFaceBlurred2.png";
		stbi_write_png(path.string().c_str(),
			_faceSizePixels,
			_faceSizePixels,
			4,
			blur2,
			_faceSizePixels * 4);

		path = Settings::Paths::TexturesPath() /= "FrontFaceBlurred3.png";
		stbi_write_png(path.string().c_str(),
			_faceSizePixels,
			_faceSizePixels,
			4,
			blur3,
			_faceSizePixels * 4);

		path = Settings::Paths::TexturesPath() /= "FrontFaceBlurred4.png";
		stbi_write_png(path.string().c_str(),
			_faceSizePixels,
			_faceSizePixels,
			4,
			blur4,
			_faceSizePixels * 4);

		std::cout << "Environment map " << imageFilePath.string() << " loaded." << std::endl;
	}

	void CubicalEnvironmentMap::CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& graphicsQueue)
	{
		// Get the data of the cube map's images as one serialized data array.
		auto serializedFaceImages = Serialize();

		// Setup buffer copy regions for each face. A copy region is just a structure to tell Vulkan how to direct the data
		// to the right place when copying it from a buffer to the image or vice-versa.
		// For cube and cube array image views, the layers of the image view starting at baseArrayLayer correspond to 
		// faces in the order +X, -X, +Y, -Y, +Z, -Z, which is, in order, _right, _left, _upper, _lower, _front, _back in our case.
		uint32_t offset = 0;
		std::vector<VkBufferImageCopy> bufferCopyRegions;
		int faceCount = 6;
		auto singleImageArraySize = Utils::GetVectorSizeInBytes(serializedFaceImages);
		for (int faceIndex = 0; faceIndex < faceCount; ++faceIndex) {
			VkBufferImageCopy bufferCopyRegion{};
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel = 0;
			bufferCopyRegion.imageSubresource.baseArrayLayer = faceIndex;
			bufferCopyRegion.imageSubresource.layerCount = 1;
			bufferCopyRegion.imageExtent.width = _faceSizePixels;
			bufferCopyRegion.imageExtent.height = _faceSizePixels;
			bufferCopyRegion.imageExtent.depth = 1;
			bufferCopyRegion.bufferOffset = offset;
			offset += (int)(singleImageArraySize / faceCount);
			bufferCopyRegions.push_back(bufferCopyRegion);
		}

		// Now we need to create the image and image view that will be used in the shaders and stored in VRAM. The image will contain our image data as a single array.
		_cubeMapImage = Vulkan::Image(logicalDevice,
			physicalDevice,
			VK_FORMAT_R8G8B8A8_SRGB,
			VkExtent3D{ (uint32_t)_faceSizePixels, (uint32_t)_faceSizePixels, 1 },
			serializedFaceImages.data(),
			(VkImageUsageFlagBits)(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			6,
			VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
			VK_IMAGE_VIEW_TYPE_CUBE);

		_cubeMapImage.SendToGPU(commandPool, graphicsQueue, bufferCopyRegions);
	}

	std::vector<unsigned char> CubicalEnvironmentMap::Serialize()
	{
		// According to the Vulkan documentation, the faces should be serialized in the following order: +X, -X, +Y, -Y, +Z, -Z.
		// This corresponds to _right, _left, _upper, _lower, _front, _back for our cube map structure that we have in place.
		std::vector<unsigned char> serialized;
		size_t imageSizeBytes = _front.size();
		serialized.resize(imageSizeBytes * 6); // Type is unsigned char so the slot count in the vector is the same as the byte count.
		memcpy(serialized.data(), _right.data(), imageSizeBytes);
		memcpy(serialized.data() + imageSizeBytes, _left.data(), imageSizeBytes);
		memcpy(serialized.data() + (imageSizeBytes * 2), _upper.data(), imageSizeBytes);
		memcpy(serialized.data() + (imageSizeBytes * 3), _lower.data(), imageSizeBytes);
		memcpy(serialized.data() + (imageSizeBytes * 4), _front.data(), imageSizeBytes);
		memcpy(serialized.data() + (imageSizeBytes * 5), _back.data(), imageSizeBytes);
		return serialized;
	}

	void CubicalEnvironmentMap::UpdateShaderResources()
	{
	}
}