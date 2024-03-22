#define GLFW_INCLUDE_VULKAN

// Image management.
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

#include "Includes.hpp"

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

	/*CubeMapImage::CubeMapImage(int widthHeightPixels, unsigned char* data)
	{
		_widthHeightPixels = widthHeightPixels;
		_data = data;
	}*/

	std::vector<unsigned char> CubicalEnvironmentMap::GenerateFaceImage(CubeMapFace face)
	{
		// The world space unit vector that points in the positive X direction of the image (must be left to right), as if the image was placed in the 3D world on a square plane.
		glm::vec3 imageXWorldSpace;

		// The world space unit vector that points in the positive Y direction of the image (must be bottom to top), as if the image was placed in the 3D world on a square plane.
		glm::vec3 imageYWorldSpace;

		// The origin of the image (must be the bottom left corner) in world space, as if the image was placed in the 3D world on a square plane.
		glm::vec3 imageOriginWorldSpace;

		std::vector<unsigned char> outImage;
		outImage.resize(_faceSizePixels * _faceSizePixels * 4);

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
				outImage[faceComponentIndex] = red;
				outImage[faceComponentIndex + 1] = green;
				outImage[faceComponentIndex + 2] = blue;
				outImage[faceComponentIndex + 3] = alpha;
			}
		}

		return outImage;
	}

	void CubicalEnvironmentMap::WriteImagesToFiles(std::filesystem::path absoluteFolderPath)
	{
		/*if (!std::filesystem::exists(absoluteFolderPath)) {
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
			_faceSizePixels * 4);*/
	}

	Engine::Scenes::CubicalEnvironmentMap::CubicalEnvironmentMap(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice)
	{
		_physicalDevice = physicalDevice;
		_logicalDevice = logicalDevice;
		_faceSizePixels = 512;
	}

	std::vector<unsigned char> ResizeImage(std::vector<unsigned char> image, int oldWidthPixels, int oldHeightPixels, int newWidthPixels, int newHeightPixels)
	{
		std::vector<unsigned char> outImage(newWidthPixels * newHeightPixels * 4);
		int ratioX = oldWidthPixels / newWidthPixels;
		int ratioY = oldHeightPixels / newHeightPixels;
		int oldImageX = 0;
		int oldImageY = 0;
		int newImageX = 0;
		int newImageY = 0;

		for (; newImageY < newHeightPixels; ++newImageY) {
			newImageX = 0;
			for (; newImageX < newWidthPixels; ++newImageX) {
				auto oldImageComponentIndex = CartesianToComponentIndex(newImageX * ratioX, newImageY * ratioY, oldWidthPixels);
				auto newImageComponentIndex = CartesianToComponentIndex(newImageX, newImageY, newWidthPixels);
				outImage[newImageComponentIndex] = image[oldImageComponentIndex];
				outImage[newImageComponentIndex + 1] = image[oldImageComponentIndex + 1];
				outImage[newImageComponentIndex + 2] = image[oldImageComponentIndex + 2];
				outImage[newImageComponentIndex + 3] = image[oldImageComponentIndex + 3];
			}
		}

		return outImage;
	}

	void CubicalEnvironmentMap::LoadFromSphericalHDRI(std::filesystem::path imageFilePath)
	{
		int wantedComponents = 4;
		int componentsDetected;
		int width;
		int height;

		// First, we load the spherical HDRi image.
		// In the stbi_load() function, comp stands for components. In a PNG image, for example, there are 4 components 
		// for each pixel: red, green, blue and alpha.
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

		//auto debugImage = std::vector<unsigned char>(imageLength);

		// Now we start sampling the spherical HDRi for each individual pixel of each
		// face of the cube map we want to generate.
		_front.push_back(GenerateFaceImage(CubeMapFace::FRONT));
		_right.push_back(GenerateFaceImage(CubeMapFace::RIGHT));
		_back.push_back(GenerateFaceImage(CubeMapFace::BACK));
		_left.push_back(GenerateFaceImage(CubeMapFace::LEFT));
		_upper.push_back(GenerateFaceImage(CubeMapFace::UPPER));
		_lower.push_back(GenerateFaceImage(CubeMapFace::LOWER));

		Utils::BoxBlur blurrer;
		auto mipmapIndex = 1;
		auto resolution = _faceSizePixels;
		auto faces = { &_front, &_right, &_back, &_left, &_upper, &_lower };
		for (auto resolution = _faceSizePixels, radius = 2; resolution > 1; resolution /= 2, radius *= 2) {

			for (auto face : faces) {
				std::vector<unsigned char> tmp;
				auto halfResolution = resolution / 2;
				tmp = ResizeImage((*face)[mipmapIndex - 1], resolution, resolution, halfResolution, halfResolution);
				auto blurredImageData = blurrer.Run(_physicalDevice._handle, _logicalDevice, tmp.data(), halfResolution, halfResolution, radius);

				if (blurredImageData == nullptr) {
					std::cout << "failed blurring image\n";
					std::exit(-1);
				}

				auto sizeBytes = (halfResolution) * (halfResolution) * 4;
				(*face).push_back(std::vector<unsigned char>());
				(*face)[mipmapIndex].resize(sizeBytes);
				memcpy((*face)[mipmapIndex].data(), blurredImageData, sizeBytes);
			}

			mipmapIndex++;
		}

		//WriteImagesToFiles(Settings::Paths::TexturesPath());
		blurrer.Destroy();

		/*for (int i = 0; i < blurredFaceImages.size(); ++i) {
			char* buf = (char*)malloc(64);
			if (buf == nullptr) {
				continue;
			}

			if (_itoa_s(i, buf, 64, 10)) {
				free(buf);
				continue;
			}

			auto path = Settings::Paths::TexturesPath() /= std::string("front_") + std::string(buf) + std::string(".png");
			free(buf);
			stbi_write_png(path.string().c_str(), _faceSizePixels, _faceSizePixels, 4, blurredFaceImages[i], _faceSizePixels * 4);
		}*/

		std::cout << "Environment map " << imageFilePath.string() << " loaded." << std::endl;
	}

	VkCommandBuffer CreateCommandBuffer(VkDevice logicalDevice, VkCommandPool commandPool)
	{
		VkCommandBufferAllocateInfo cmdBufInfo = {};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdBufInfo.commandPool = commandPool;
		cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdBufInfo.commandBufferCount = 1;
		VkCommandBuffer commandBuffer;
		if (vkAllocateCommandBuffers(logicalDevice, &cmdBufInfo, &commandBuffer) != VK_SUCCESS) {
			std::cout << "Failed allocating copy command buffer to copy buffer to texture." << std::endl;
			exit(1);
		}

		return commandBuffer;
	}

	void StartRecording(VkCommandBuffer commandBuffer)
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkResetCommandBuffer(commandBuffer, 0);
		vkBeginCommandBuffer(commandBuffer, &beginInfo);
	}

	void StopRecording(VkCommandBuffer commandBuffer) { vkEndCommandBuffer(commandBuffer); }

	/**
	 * @brief Stops the current thread and waits for commands to finish executing on the GPU.
	 * @param commandBuffer A command buffer with recorded commands.
	 * @param queue A queue that is compatible with all commands recorded in the command buffer to execute.
	 */
	void ExecuteCommands(VkCommandBuffer commandBuffer, Vulkan::Queue queue)
	{
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		vkQueueSubmit(queue._handle, 1, &submitInfo, nullptr);
		vkQueueWaitIdle(queue._handle);
	}

	void CubicalEnvironmentMap::CopyFacesToImage(VkDevice logicalDevice, Vulkan::PhysicalDevice& physicalDevice, VkCommandPool commandPool, VkCommandBuffer commandBuffer, Vulkan::Queue& queue)
	{
		auto faces = { _right, _left, _upper, _lower, _front, _back };
		uint32_t resolution = _faceSizePixels;
		uint32_t faceIndex = 0;

		StartRecording(commandBuffer);

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_NONE;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.image = _cubeMapImage._image;
		barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
		_cubeMapImage._currentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		for (auto& face : faces) {
			resolution = _faceSizePixels;

			for (int mipmapIndex = 0; mipmapIndex < face.size(); ++mipmapIndex, resolution /= 2) {

				// Create the image.
				Image image{};
				image._createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
				image._createInfo.imageType = VK_IMAGE_TYPE_2D;
				image._createInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
				image._createInfo.extent = { resolution, resolution, 1 };
				image._createInfo.mipLevels = 1;
				image._createInfo.arrayLayers = 1;
				image._createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
				image._createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
				image._createInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
				vkCreateImage(logicalDevice, &image._createInfo, nullptr, &image._image);

				// Allocate memory for it.
				VkMemoryRequirements reqs;
				VkDeviceMemory mem;
				vkGetImageMemoryRequirements(logicalDevice, image._image, &reqs);
				VkMemoryAllocateInfo allocInfo{};
				allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				allocInfo.allocationSize = reqs.size;
				allocInfo.memoryTypeIndex = physicalDevice.GetMemoryTypeIndex(reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &mem);
				vkBindImageMemory(logicalDevice, image._image, mem, 0);

				// Create the image view.
				image._viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				image._viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				image._viewCreateInfo.image = image._image;
				image._viewCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
				image._viewCreateInfo.subresourceRange.baseMipLevel = 0;
				image._viewCreateInfo.subresourceRange.levelCount = 1;
				image._viewCreateInfo.subresourceRange.baseArrayLayer = 0;
				image._viewCreateInfo.subresourceRange.layerCount = 1;
				image._viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				vkCreateImageView(logicalDevice, &image._viewCreateInfo, nullptr, &image._view);

				// Send the image to the GPU.
				Buffer stagingBuffer{};
				stagingBuffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				stagingBuffer._createInfo.size = face[mipmapIndex].size();
				stagingBuffer._createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
				vkCreateBuffer(_logicalDevice, &stagingBuffer._createInfo, nullptr, &stagingBuffer._buffer);

				// Allocate memory for the buffer.
				VkMemoryRequirements requirements{};
				vkGetBufferMemoryRequirements(_logicalDevice, stagingBuffer._buffer, &requirements);
				stagingBuffer._gpuMemory = physicalDevice.AllocateMemory(logicalDevice, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

				// Map memory to the correct GPU and CPU ranges.
				vkBindBufferMemory(_logicalDevice, stagingBuffer._buffer, stagingBuffer._gpuMemory, 0);
				vkMapMemory(_logicalDevice, stagingBuffer._gpuMemory, 0, face[mipmapIndex].size(), 0, &stagingBuffer._cpuMemory);
				memcpy(stagingBuffer._cpuMemory, face[mipmapIndex].data(), face[mipmapIndex].size());

				// Create the buffer view.
				/*stagingBuffer._viewCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
				stagingBuffer._viewCreateInfo.buffer = stagingBuffer._buffer;
				stagingBuffer._viewCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
				stagingBuffer._viewCreateInfo.range = VK_WHOLE_SIZE;
				vkCreateBufferView(logicalDevice, &stagingBuffer._viewCreateInfo, nullptr, &stagingBuffer._view);*/

				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.srcAccessMask = VK_ACCESS_NONE;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.image = image._image;
				barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
				image._currentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

				// Copy the temporary buffer into the image using copy regions. A copy region is just a structure to tell Vulkan 
				// how to direct the data to the right place when copying it from a buffer to the image or vice-versa.
				{
					VkBufferImageCopy copyRegion = {};
					copyRegion.bufferOffset = 0;
					copyRegion.bufferRowLength = 0;
					copyRegion.bufferImageHeight = 0;
					copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					copyRegion.imageSubresource.mipLevel = 0;
					copyRegion.imageSubresource.baseArrayLayer = 0;
					copyRegion.imageSubresource.layerCount = 1;
					copyRegion.imageExtent = { resolution, resolution, 1 };
					vkCmdCopyBufferToImage(commandBuffer, stagingBuffer._buffer, image._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
				}

				VkImageCopy copyRegion{};
				copyRegion.srcOffset = { 0,0,0 };
				copyRegion.dstOffset = { 0,0,0 };
				copyRegion.extent = { resolution, resolution, 1 };

				copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.srcSubresource.baseArrayLayer = 0;
				copyRegion.srcSubresource.layerCount = 1;
				copyRegion.srcSubresource.mipLevel = 0;

				copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.dstSubresource.baseArrayLayer = faceIndex;
				copyRegion.dstSubresource.layerCount = 1;
				copyRegion.dstSubresource.mipLevel = mipmapIndex;

				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barrier.image = image._image;
				barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
				image._currentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
				
				vkCmdCopyImage(commandBuffer, image._image, image._currentLayout, _cubeMapImage._image, _cubeMapImage._currentLayout, 1, &copyRegion);
			}

			++faceIndex;
		}

		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.image = _cubeMapImage._image;
		barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
		_cubeMapImage._currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		StopRecording(commandBuffer);
		ExecuteCommands(commandBuffer, queue);

		// Get the data of the cube map's images as one serialized data array.
		//auto serializedFaceImages = Serialize();
		//auto singleImageArraySizeBytes = Utils::GetVectorSizeInBytes(serializedFaceImages);

		//// Allocate a temporary buffer for holding image data to upload to VRAM.
		//VkBuffer stagingBuffer{};
		//VkBufferCreateInfo vertexBufferInfo = {};
		//vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		//vertexBufferInfo.size = singleImageArraySizeBytes;
		//vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		//vkCreateBuffer(_logicalDevice, &vertexBufferInfo, nullptr, &stagingBuffer);

		//// Allocate memory for the buffer.
		//VkMemoryRequirements requirements{};
		//vkGetBufferMemoryRequirements(_logicalDevice, stagingBuffer, &requirements);
		//VkDeviceMemory memory = _physicalDevice.AllocateMemory(_logicalDevice, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		//// Creates a reference/connection to the buffer on the GPU side.
		//vkBindBufferMemory(_logicalDevice, stagingBuffer, memory, 0);

		//// Creates a reference/connection to the buffer on the CPU side.
		//void* pData;
		//vkMapMemory(_logicalDevice, memory, 0, singleImageArraySizeBytes, 0, &pData);
		//memcpy(pData, serializedFaceImages.data(), serializedFaceImages.size());

		//StartRecording(commandBuffer);

		//VkBufferImageCopy copyRegion = {};
		//copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		//copyRegion.imageExtent = { (uint32_t)_faceSizePixels, (uint32_t)_faceSizePixels, 1 };
		//vkCmdCopyBufferToImage(commandBuffer,
		//	stagingBuffer,
		//	_cubeMapImage.image,
		//	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		//	1,
		//	&copyRegion);


		//// Stop recording copy commands and submit them for execution.
		//vkEndCommandBuffer(commandBuffer);
		//VkSubmitInfo submitInfo{};
		//submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		//submitInfo.commandBufferCount = 1;
		//submitInfo.pCommandBuffers = &commandBuffer;
		//vkQueueSubmit(queue._handle, 1, &submitInfo, nullptr);
		//vkQueueWaitIdle(queue._handle);
	}

	void CubicalEnvironmentMap::CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& graphicsQueue)
	{
		// Create the cubemap image.
		auto& imageCreateInfo = _cubeMapImage._createInfo;
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.arrayLayers = 6;
		imageCreateInfo.extent = { (uint32_t)_faceSizePixels, (uint32_t)_faceSizePixels, 1 };
		imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		imageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.mipLevels = 10;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		vkCreateImage(_logicalDevice, &imageCreateInfo, nullptr, &_cubeMapImage._image);

		// Allocate memory on the GPU for the image.
		VkMemoryRequirements reqs;
		vkGetImageMemoryRequirements(logicalDevice, _cubeMapImage._image, &reqs);
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = reqs.size;
		allocInfo.memoryTypeIndex = _physicalDevice.GetMemoryTypeIndex(reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VkDeviceMemory mem;
		vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &mem);
		vkBindImageMemory(logicalDevice, _cubeMapImage._image, mem, 0);

		auto& imageViewCreateInfo = _cubeMapImage._viewCreateInfo;
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.components = { {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY} };
		imageViewCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		imageViewCreateInfo.image = _cubeMapImage._image;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 6;
		imageViewCreateInfo.subresourceRange.levelCount = 10;
		vkCreateImageView(_logicalDevice, &imageViewCreateInfo, nullptr, &_cubeMapImage._view);

		auto& samplerCreateInfo = _cubeMapImage._samplerCreateInfo;
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.anisotropyEnable = false;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
		samplerCreateInfo.flags = 0;
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.maxLod = 10;
		samplerCreateInfo.minLod = 0;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		vkCreateSampler(_logicalDevice, &samplerCreateInfo, nullptr, &_cubeMapImage._sampler);

		auto commandBuffer = CreateCommandBuffer(logicalDevice, commandPool);
		CopyFacesToImage(logicalDevice, physicalDevice, commandPool, commandBuffer, graphicsQueue);
	}

	void CubicalEnvironmentMap::UpdateShaderResources()
	{
	}
}