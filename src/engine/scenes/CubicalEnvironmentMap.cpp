#define GLFW_INCLUDE_VULKAN
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
#include "LocalIncludes.hpp"

using namespace Engine::Vulkan;

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
	// Assumes that X and Y both start from 0, at the top left corner of the image, and the maximum X can be is equal to imageWidthPixels-1.
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

	std::vector<unsigned char> CubicalEnvironmentMap::GenerateFaceImage(CubeMapFace face, int mipIndex)
	{
		// The world space unit vector that points in the positive X direction of the image (must be left to right), as if the image was placed in the 3D world on a square plane.
		glm::vec3 imageXWorldSpace;

		// The world space unit vector that points in the positive Y direction of the image (must be bottom to top), as if the image was placed in the 3D world on a square plane.
		glm::vec3 imageYWorldSpace;

		// The origin of the image (must be the bottom left corner) in world space, as if the image was placed in the 3D world on a square plane.
		glm::vec3 imageOriginWorldSpace;

		auto sizePixels = _faceSizePixels;
		for (int i = 0; i < mipIndex; ++i) {
			sizePixels /= 2;
		}

		std::vector<unsigned char> outImage;
		outImage.resize(sizePixels * sizePixels * 4);

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

		for (auto y = 0; y < sizePixels; ++y) {
			for (auto x = 0; x < sizePixels; ++x) {

				// First we calculate the cartesian coordinate of the pixel of the cube map's face we are considering, in world space.
				glm::vec3 cartesianCoordinatesOnFace;
				cartesianCoordinatesOnFace = imageOriginWorldSpace + (imageXWorldSpace * ((1.0f / sizePixels) * x));
				cartesianCoordinatesOnFace += -imageYWorldSpace * ((1.0f / sizePixels) * y);

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
				int pixelNumberU = (int)ceil(uCoordinate * sizePixels);
				int pixelNumberV = (int)ceil((1.0f - vCoordinate) * sizePixels);

				// This calculates the index into the spherical HDRi image accounting for 2 things:
				// 1) the pixel numbers calculated above
				// 2) the fact that each pixel is actually stored in 4 separate cells that represent
				//    each channel of each individual pixel. The channels used are always 4 (RGBA).
				int componentIndex = (pixelNumberU * 4) + ((sizePixels * 4) * (pixelNumberV - 1));

				// Fetch the channels from the spherical HDRi based on the calculations made above.
				auto red = _hdriImageData[mipIndex][componentIndex];
				auto green = _hdriImageData[mipIndex][componentIndex + 1];
				auto blue = _hdriImageData[mipIndex][componentIndex + 2];
				auto alpha = _hdriImageData[mipIndex][componentIndex + 3];

				// Assign the color fetched from the HDRi to the cube map face's image we want to generate.
				int faceComponentIndex = (x + (sizePixels * y)) * 4;
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

	Engine::Scenes::CubicalEnvironmentMap::CubicalEnvironmentMap(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice)
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

	std::vector<unsigned char> PadImage(std::vector<unsigned char> image, int widthPixels, int heightPixels, int padAmountPixels)
	{
		if (padAmountPixels > std::min(widthPixels, heightPixels)) {
			Utils::Logger::Log("padding cannot exceed smallest image dimension");
			return std::vector<unsigned char>();
		}

		int newWidthPixels = 0, newHeightPixels = 0;
		newWidthPixels = widthPixels + (padAmountPixels * 2);
		newHeightPixels = heightPixels + (padAmountPixels * 2);
		std::vector<unsigned char> outImage(newWidthPixels * newHeightPixels * 4);

		// Fill the output image with the original image data.
		for (int y = 0; y < heightPixels; ++y) {
			for (int x = 0; x < widthPixels; ++x) {
				auto oldImageComponentIndex = CartesianToComponentIndex(x, y, widthPixels);
				auto outImageComponentIndex = CartesianToComponentIndex(x + padAmountPixels, y + padAmountPixels, newWidthPixels);
				outImage[outImageComponentIndex] = image[oldImageComponentIndex];
				outImage[outImageComponentIndex + 1] = image[oldImageComponentIndex + 1];
				outImage[outImageComponentIndex + 2] = image[oldImageComponentIndex + 2];
				outImage[outImageComponentIndex + 3] = image[oldImageComponentIndex + 3];
			}
		}

		// Pad the upper and lower portions of the image by mirroring "padAmountPixels" from the upper and lower borders.
		for (int yUpper = 0, yLower = heightPixels - 1, rowSampled = 0; rowSampled < padAmountPixels; ++yUpper, --yLower, ++rowSampled) {
			int yNewUpper = padAmountPixels - rowSampled - 1;
			int yNewLower = heightPixels + padAmountPixels + rowSampled;

			for (int x = 0; x < widthPixels; ++x) {

				// Mirror the upper pixel.
				auto indexOfColorToCopy = CartesianToComponentIndex(x, yUpper, widthPixels);
				auto indexOfOutImage = CartesianToComponentIndex(x + padAmountPixels, yNewUpper, newWidthPixels);
				outImage[indexOfOutImage] = image[indexOfColorToCopy];
				outImage[indexOfOutImage + 1] = image[indexOfColorToCopy + 1];
				outImage[indexOfOutImage + 2] = image[indexOfColorToCopy + 2];
				outImage[indexOfOutImage + 3] = image[indexOfColorToCopy + 3];

				// Mirror the lower pixel.
				indexOfColorToCopy = CartesianToComponentIndex(x, yLower, widthPixels);
				indexOfOutImage = CartesianToComponentIndex(x + padAmountPixels, yNewLower, newWidthPixels);
				outImage[indexOfOutImage] = image[indexOfColorToCopy];
				outImage[indexOfOutImage + 1] = image[indexOfColorToCopy + 1];
				outImage[indexOfOutImage + 2] = image[indexOfColorToCopy + 2];
				outImage[indexOfOutImage + 3] = image[indexOfColorToCopy + 3];
			}
		}

		// Pad the left and right portions of the image by mirroring "padAmountPixels" from the left and right borders.
		for (int y = 0; y < newHeightPixels; ++y) {
			for (int xLeft = padAmountPixels, xRight = widthPixels + padAmountPixels - 1, columnSampled = 0; columnSampled < padAmountPixels; ++xLeft, --xRight, ++columnSampled) {
				int xNewLeft = padAmountPixels - columnSampled - 1;
				int xNewRight = widthPixels + padAmountPixels + columnSampled;

				// Mirror the left pixel.
				auto sourceIndex = CartesianToComponentIndex(xLeft, y, newWidthPixels);
				auto destinationIndex = CartesianToComponentIndex(xNewLeft, y, newWidthPixels);
				outImage[destinationIndex] = outImage[sourceIndex];
				outImage[destinationIndex + 1] = outImage[sourceIndex + 1];
				outImage[destinationIndex + 2] = outImage[sourceIndex + 2];
				outImage[destinationIndex + 3] = outImage[sourceIndex + 3];

				// Mirror the right pixel.
				sourceIndex = CartesianToComponentIndex(xRight, y, newWidthPixels);
				destinationIndex = CartesianToComponentIndex(xNewRight, y, newWidthPixels);
				outImage[destinationIndex] = outImage[sourceIndex];
				outImage[destinationIndex + 1] = outImage[sourceIndex + 1];
				outImage[destinationIndex + 2] = outImage[sourceIndex + 2];
				outImage[destinationIndex + 3] = outImage[sourceIndex + 3];
			}
		}

		return outImage;
	}

	std::vector<unsigned char> GetImageArea(std::vector<unsigned char> image, int widthPixels, int heightPixels, int xStart, int xFinish, int yStart, int yFinish)
	{
		if (xStart < 0 || yStart < 0 || xFinish > widthPixels || yFinish > heightPixels || xStart >= xFinish || yStart >= yFinish) {
			Utils::Logger::Log("invalid image range");
			return std::vector<unsigned char>();
		}

		auto newWidthPixels = xFinish - xStart;
		auto newHeightPixels = yFinish - yStart;
		std::vector<unsigned char> outImage(newWidthPixels * newHeightPixels * 4);

		// Fill the output image with the original image data.
		for (int y = yStart, newY = 0; newY < newHeightPixels; ++y, ++newY) {
			for (int x = xStart, newX = 0; newX < newWidthPixels; ++x, ++newX) {
				auto oldImageComponentIndex = CartesianToComponentIndex(x, y, widthPixels);
				auto outImageComponentIndex = CartesianToComponentIndex(newX, newY, newWidthPixels);
				outImage[outImageComponentIndex] = image[oldImageComponentIndex];
				outImage[outImageComponentIndex + 1] = image[oldImageComponentIndex + 1];
				outImage[outImageComponentIndex + 2] = image[oldImageComponentIndex + 2];
				outImage[outImageComponentIndex + 3] = image[oldImageComponentIndex + 3];
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
		auto mipCount = 1;
		for (auto resolution = _faceSizePixels; resolution > 1; resolution /= 2, ++mipCount) {}

		_hdriImageData.resize(mipCount);
		auto lodZero = stbi_load(imageFilePath.string().c_str(), &width, &height, &componentsDetected, wantedComponents);
		auto sizeBytes = width * height * 4;
		_hdriImageData[0].resize(sizeBytes);
		memcpy(_hdriImageData[0].data(), lodZero, sizeBytes);
		_hdriSizePixels.width = width;
		_hdriSizePixels.height = height;

		Utils::BoxBlur blurrer;
		for (int resolution = _faceSizePixels, radius = 2, i = 1; resolution > 1; resolution /= 2, radius *= 2, ++i) {
			auto halfResolution = resolution / 2;
			auto tmp = ResizeImage(_hdriImageData[i-1], resolution, resolution, halfResolution, halfResolution);

			int paddingAmountPixels = (int)((halfResolution / 100.0f) * 5.0f);
			auto paddedResolution = halfResolution + (paddingAmountPixels * 2);
			tmp = PadImage(tmp, halfResolution, halfResolution, paddingAmountPixels);

			auto blurredImageData = blurrer.Run(_physicalDevice, _logicalDevice, tmp.data(), paddedResolution, paddedResolution, radius);

			memcpy(tmp.data(), blurredImageData, paddedResolution * paddedResolution * 4);
			_hdriImageData[i] = GetImageArea(tmp, paddedResolution, paddedResolution, paddingAmountPixels, halfResolution + paddingAmountPixels, paddingAmountPixels, halfResolution + paddingAmountPixels);
		
			_front.push_back(GenerateFaceImage(CubeMapFace::FRONT, i));
			_right.push_back(GenerateFaceImage(CubeMapFace::RIGHT, i));
			_back.push_back(GenerateFaceImage(CubeMapFace::BACK, i));
			_left.push_back(GenerateFaceImage(CubeMapFace::LEFT, i));
			_upper.push_back(GenerateFaceImage(CubeMapFace::UPPER, i));
			_lower.push_back(GenerateFaceImage(CubeMapFace::LOWER, i));
		}
		
		blurrer.Destroy();

		//Utils::BoxBlur blurrer;
		//auto mipmapIndex = 1;
		//auto resolution = _faceSizePixels;
		//auto faces = { &_front, &_right, &_back, &_left, &_upper, &_lower };
		//for (auto resolution = _faceSizePixels, radius = 2; resolution > 1; resolution /= 2, radius *= 2) {

		//	int j = 0;
		//	for (auto face : faces) {
		//		std::vector<unsigned char> tmp;
		//		auto halfResolution = resolution / 2;
		//		tmp = ResizeImage((*face)[mipmapIndex - 1], resolution, resolution, halfResolution, halfResolution);

		//		int paddingAmountPixels = (int)((halfResolution / 100.0f) * 5.0f);
		//		auto paddedResolution = halfResolution + (paddingAmountPixels * 2);
		//		tmp = PadImage(tmp, halfResolution, halfResolution, paddingAmountPixels);

		//		auto path = Settings::Paths::TexturesPath() /= "env_map\\face_" + std::to_string(j) + "_" + std::to_string(mipmapIndex) + ".png";
		//		stbi_write_png(path.string().c_str(), paddedResolution, paddedResolution, 4, tmp.data(), paddedResolution * 4);

		//		auto blurredImageData = blurrer.Run(_physicalDevice, _logicalDevice, tmp.data(), paddedResolution, paddedResolution, radius);

		//		if (blurredImageData == nullptr) {
		//			Utils::Exit(1, "failed blurring image\n");
		//		}

		//		memcpy(tmp.data(), blurredImageData, paddedResolution * paddedResolution * 4);
		//		tmp = GetImageArea(tmp, paddedResolution, paddedResolution, paddingAmountPixels, halfResolution + paddingAmountPixels, paddingAmountPixels, halfResolution + paddingAmountPixels);

		//		auto sizeBytes = (halfResolution) * (halfResolution) * 4;
		//		(*face).push_back(std::vector<unsigned char>());
		//		(*face)[mipmapIndex].resize(sizeBytes);
		//		memcpy((*face)[mipmapIndex].data(), tmp.data(), sizeBytes);
		//		//free(blurredImageData);

		//		if (!std::filesystem::exists(Settings::Paths::TexturesPath() /= "env_map")) {
		//			std::filesystem::create_directories(Settings::Paths::TexturesPath() /= "env_map");
		//		}

		//		path = Settings::Paths::TexturesPath() /= "env_map\\face_" + std::to_string(j) + "_" + std::to_string(mipmapIndex) + ".png";
		//		auto ptr = (*face)[mipmapIndex].data();
		//		stbi_write_png(path.string().c_str(), halfResolution, halfResolution, 4, ptr, halfResolution * 4);
		//		++j;
		//	}

		//	mipmapIndex++;
		//}

		//WriteImagesToFiles(Settings::Paths::TexturesPath());
		

		//Utils::Logger::Log("Environment map " + imageFilePath.string() + " loaded.");
	}

	void Engine::Scenes::CubicalEnvironmentMap::CreateImage(VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice, VkCommandPool& commandPool, VkQueue& queue)
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
		vkCreateImage(logicalDevice, &imageCreateInfo, nullptr, &_cubeMapImage._image);

		// Allocate memory on the GPU for the image.
		VkMemoryRequirements reqs;
		vkGetImageMemoryRequirements(logicalDevice, _cubeMapImage._image, &reqs);
		VkMemoryAllocateInfo imageAllocInfo{};
		imageAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		imageAllocInfo.allocationSize = reqs.size;
		imageAllocInfo.memoryTypeIndex = PhysicalDevice::GetMemoryTypeIndex(physicalDevice, reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VkDeviceMemory mem;
		vkAllocateMemory(logicalDevice, &imageAllocInfo, nullptr, &mem);
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
		CopyFacesToImage(logicalDevice, physicalDevice, commandPool, commandBuffer, queue);
	}

	void CubicalEnvironmentMap::CopyFacesToImage(VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice, VkCommandPool& commandPool, VkCommandBuffer& commandBuffer, VkQueue& queue)
	{
		auto faces = { _right, _left, _upper, _lower, _front, _back };
		uint32_t resolution = _faceSizePixels;
		uint32_t faceIndex = 0;

		StartRecording(commandBuffer);

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_NONE;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.oldLayout = _cubeMapImage._currentLayout;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.image = _cubeMapImage._image;
		barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
		_cubeMapImage._currentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		std::vector<Buffer> temporaryBuffers;

		for (auto& face : faces) {
			resolution = _faceSizePixels;

			for (int mipmapIndex = 0; mipmapIndex < face.size(); ++mipmapIndex, resolution /= 2) {
				// Create a temporary buffer.
				Buffer stagingBuffer{};
				stagingBuffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				stagingBuffer._createInfo.size = face[mipmapIndex].size();
				stagingBuffer._createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
				vkCreateBuffer(_logicalDevice, &stagingBuffer._createInfo, nullptr, &stagingBuffer._buffer);

				// Allocate memory for the buffer.
				VkMemoryRequirements requirements{};
				vkGetBufferMemoryRequirements(_logicalDevice, stagingBuffer._buffer, &requirements);
				stagingBuffer._gpuMemory = PhysicalDevice::AllocateMemory(physicalDevice, logicalDevice, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

				// Map memory to the correct GPU and CPU ranges for the buffer.
				vkBindBufferMemory(_logicalDevice, stagingBuffer._buffer, stagingBuffer._gpuMemory, 0);
				vkMapMemory(_logicalDevice, stagingBuffer._gpuMemory, 0, face[mipmapIndex].size(), 0, &stagingBuffer._cpuMemory);
				memcpy(stagingBuffer._cpuMemory, face[mipmapIndex].data(), face[mipmapIndex].size());

				// Copy the buffer to the specific face by defining the subresource range.
				VkBufferImageCopy copyInfo{};
				copyInfo.bufferImageHeight = resolution;
				copyInfo.bufferRowLength = resolution;
				copyInfo.imageExtent = { resolution, resolution, 1 };
				copyInfo.imageOffset = { 0,0,0 };
				copyInfo.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyInfo.imageSubresource.layerCount = 1;
				copyInfo.imageSubresource.baseArrayLayer = faceIndex;
				copyInfo.imageSubresource.mipLevel = mipmapIndex;
				vkCmdCopyBufferToImage(commandBuffer, stagingBuffer._buffer, _cubeMapImage._image, _cubeMapImage._currentLayout, 1, &copyInfo);

				temporaryBuffers.push_back(stagingBuffer);
			}

			++faceIndex;
		}

		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.oldLayout = _cubeMapImage._currentLayout;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.image = _cubeMapImage._image;
		barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
		_cubeMapImage._currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		StopRecording(commandBuffer);
		ExecuteCommands(commandBuffer, queue);

		// Destroy all the buffers used to move data to the cube map image.
		for (auto& buffer : temporaryBuffers) {
			vkUnmapMemory(_logicalDevice, buffer._gpuMemory);
			vkFreeMemory(_logicalDevice, buffer._gpuMemory, nullptr);
			vkDestroyBuffer(_logicalDevice, buffer._buffer, nullptr);
		}
	}

	Vulkan::ShaderResources CubicalEnvironmentMap::CreateDescriptorSets(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, std::vector<Vulkan::DescriptorSetLayout>& layouts)
	{
		auto descriptorSetID = 4;

		// Map the cubemap image to the fragment shader.
		VkDescriptorPool descriptorPool{};
		VkDescriptorPoolSize poolSizes[1] = { VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 } };
		VkDescriptorPoolCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.maxSets = (uint32_t)1;
		createInfo.poolSizeCount = (uint32_t)1;
		createInfo.pPoolSizes = poolSizes;
		vkCreateDescriptorPool(logicalDevice, &createInfo, nullptr, &descriptorPool);

		// Create the descriptor set.
		VkDescriptorSet set{};
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = (uint32_t)1;
		allocInfo.pSetLayouts = &layouts[descriptorSetID]._layout;
		vkAllocateDescriptorSets(logicalDevice, &allocInfo, &set);

		// Update the descriptor set's data with the environment map's image.
		VkDescriptorImageInfo imageInfo{ _cubeMapImage._sampler, _cubeMapImage._view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkWriteDescriptorSet writeInfo = {};
		writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeInfo.dstSet = set;
		writeInfo.descriptorCount = 1;
		writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeInfo.pImageInfo = &imageInfo;
		writeInfo.dstBinding = 0;
		vkUpdateDescriptorSets(logicalDevice, 1, &writeInfo, 0, nullptr);

		auto descriptorSets = std::vector<VkDescriptorSet>{ set };
		_shaderResources._data.try_emplace(layouts[descriptorSetID], descriptorSets);
		return _shaderResources;
	}

	void CubicalEnvironmentMap::UpdateShaderResources()
	{
	}
}