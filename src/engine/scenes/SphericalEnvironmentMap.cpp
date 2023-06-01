#include <iostream>
#include <unordered_map>
#include <vector>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <stb/stb_image.h>
#include <vulkan/vulkan.h>
#include <stb/stb_image_write.h>

#include "utils/Utils.hpp"
#include "engine/vulkan/Queue.hpp"
#include "engine/vulkan/PhysicalDevice.hpp"
#include "engine/vulkan/Image.hpp"
#include "engine/vulkan/Buffer.hpp"
#include "engine/scenes/SphericalEnvironmentMap.hpp"

namespace Engine::Scenes
{
	void SphericalEnvironmentMap::LoadFromFile(std::filesystem::path imageFilePath)
	{
		int actualComponents;

		// In the stbi_load() function, comp stands for components. In a PNG image, for example, there are 4 components 
		// for each pixel, red, green, blue and alpha.
		// The image's pixels are read and stored left to right, top to bottom, relative to the image.
		// Each pixel's component is an unsigned char.
		auto image = stbi_load(imageFilePath.string().c_str(), &_width, &_height, &actualComponents, 4);
		auto imageLength = _width * _height * 4;
		auto pixelCount = _width * _height;

		for (int componentIndex = 0; componentIndex < imageLength; ++componentIndex) {
			_pixelColors.push_back(image[componentIndex]);
		}

		// Get some variables ready for conversion.
		glm::vec3 positiveXAxis(1.0f, 0.0f, 0.0f);
		glm::vec3 positiveYAxis(0.0f, 1.0f, 0.0f);
		glm::vec3 positiveZAxis(0.0f, 0.0f, 1.0f);
		glm::vec3 negativeXAxis(-1.0f, 0.0f, 0.0f);
		glm::vec3 negativeYAxis(0.0f, -1.0f, 0.0f);
		glm::vec3 negativeZAxis(0.0f, 0.0f, -1.0f);

		enum class CubeMapFace {
			FRONT,
			RIGHT,
			BACK,
			LEFT,
			UPPER,
			LOWER
		};

		std::unordered_map<CubeMapFace, std::vector<unsigned char>> cubeMapImages;
		cubeMapImages.emplace(CubeMapFace::FRONT, std::vector<unsigned char>{});
		cubeMapImages.emplace(CubeMapFace::RIGHT, std::vector<unsigned char>{});
		cubeMapImages.emplace(CubeMapFace::BACK, std::vector<unsigned char>{});
		cubeMapImages.emplace(CubeMapFace::LEFT, std::vector<unsigned char>{});
		cubeMapImages.emplace(CubeMapFace::UPPER, std::vector<unsigned char>{});
		cubeMapImages.emplace(CubeMapFace::LOWER, std::vector<unsigned char>{});

		for (int componentIndex = 0; componentIndex < imageLength; componentIndex += actualComponents)
		{
			int pixelIndex = componentIndex / actualComponents;
			int imageCoordinateX = pixelIndex % _width;
			int imageCoordinateY = pixelIndex / _width;

			// Mapping the pixel's image coordinates into [0-1] UV coordinates space.
			float uvCoordinateX = (float)imageCoordinateX / (float)_width;
			float uvCoordinateY = 1.0f - ((float)imageCoordinateY / (float)_height);

			// Mapping UV coordinates onto a unit sphere and calculating spherical coordinates.
			float azimuthDegrees = fmodf((360.0f * uvCoordinateX) + 180.0f, 360.0f);
			float zenithDegrees = -((180.0f * uvCoordinateY) - 90.0f);

			// Calculating cartesian (x, y, z) coordinates in world space from spherical coordinates. 
			// Assuming that the origin is the center of the sphere, we are using a left-handed coordinate 
			// system and that the Z vector (forward vector) in world space points to the spherical coordinate 
			// with azimuth = 0 and zenith = 0, or UV coordinates (0.5, 0.5).
			float sphereCoordinateX = sin(glm::radians(azimuthDegrees)) * cos(glm::radians(zenithDegrees));
			float sphereCoordinateY = -sin(glm::radians(zenithDegrees));
			float sphereCoordinateZ = cos(glm::radians(azimuthDegrees)) * cos(glm::radians(zenithDegrees));
			glm::vec3 cartesianCoordinatesOnSphere(sphereCoordinateX, sphereCoordinateY, sphereCoordinateZ);

			// Now we calculate the dot product with all axes and get the maximum (which will be the closest to 1 because
			// both vectors are normalized). By doing this, we find which axis our vector representing cartesian coordinates on the sphere
			// is most aligned with.
			std::vector<std::pair<glm::vec3, float>> dotProducts;
			dotProducts.push_back(std::make_pair(positiveXAxis, glm::dot(cartesianCoordinatesOnSphere, positiveXAxis)));
			dotProducts.push_back(std::make_pair(positiveYAxis, glm::dot(cartesianCoordinatesOnSphere, positiveYAxis)));
			dotProducts.push_back(std::make_pair(positiveZAxis, glm::dot(cartesianCoordinatesOnSphere, positiveZAxis)));
			dotProducts.push_back(std::make_pair(negativeXAxis, glm::dot(cartesianCoordinatesOnSphere, negativeXAxis)));
			dotProducts.push_back(std::make_pair(negativeYAxis, glm::dot(cartesianCoordinatesOnSphere, negativeYAxis)));
			dotProducts.push_back(std::make_pair(negativeZAxis, glm::dot(cartesianCoordinatesOnSphere, negativeZAxis)));
			glm::vec3 closestAxis = positiveXAxis;
			float maxDotProduct = -1.0f;

			// Loop through the map of dot products to find the closest axis to our cartesian coordinates vector.
			for (auto i = dotProducts.begin(); i != dotProducts.end(); ++i) {
				if (i->second > maxDotProduct) {
					maxDotProduct = i->second;
					closestAxis = i->first;
				}
			}

			// Find the angle between the vector representing the cartesian coordinates on the sphere
			// and the closest axis. This will give us the angle we need to feed into the tan()
			// function to get the X and Y coordinates of the cube map's face we are trying to 
			// calculate XY coordinates for, relative to its center.
			float localXAngle = 0.0f;
			float localYAngle = 0.0f;
			float UVCoordinateX = 0.0f;
			float UVCoordinateY = 0.0f;

			// The pixel belongs to the cube map's face on the right.
			if (closestAxis == positiveXAxis) {
				cubeMapImages[CubeMapFace::RIGHT].push_back(image[componentIndex]);
				cubeMapImages[CubeMapFace::RIGHT].push_back(image[componentIndex+1]);
				cubeMapImages[CubeMapFace::RIGHT].push_back(image[componentIndex+2]);
				cubeMapImages[CubeMapFace::RIGHT].push_back(image[componentIndex+3]);
				localXAngle = acos(dot(glm::vec3(cartesianCoordinatesOnSphere.x, 0.0f, cartesianCoordinatesOnSphere.z), closestAxis));
				localYAngle = acos(dot(glm::vec3(cartesianCoordinatesOnSphere.x, cartesianCoordinatesOnSphere.y, 0.0f), closestAxis));
				localXAngle = cartesianCoordinatesOnSphere.z < 0 ? abs(localXAngle) : -1 * abs(localXAngle);
				localYAngle = cartesianCoordinatesOnSphere.y < 0 ? abs(localYAngle) : -1 * abs(localYAngle);
			}

			// The pixel belongs to the cube map's face above.
			if (closestAxis == positiveYAxis) {
				cubeMapImages[CubeMapFace::UPPER].push_back(image[componentIndex]);
				cubeMapImages[CubeMapFace::UPPER].push_back(image[componentIndex + 1]);
				cubeMapImages[CubeMapFace::UPPER].push_back(image[componentIndex + 2]);
				cubeMapImages[CubeMapFace::UPPER].push_back(image[componentIndex + 3]);
				localXAngle = acos(dot(glm::normalize(glm::vec3(cartesianCoordinatesOnSphere.x, cartesianCoordinatesOnSphere.y, 0.0f)), closestAxis));
				localYAngle = acos(dot(glm::normalize(glm::vec3(0.0f, cartesianCoordinatesOnSphere.y, cartesianCoordinatesOnSphere.z)), closestAxis));
				localXAngle = cartesianCoordinatesOnSphere.x > 0 ? abs(localXAngle) : -1 * abs(localXAngle);
				localYAngle = cartesianCoordinatesOnSphere.z > 0 ? abs(localYAngle) : -1 * abs(localYAngle);
			}

			// The pixel belongs to the cube map's face in front.
			if (closestAxis == positiveZAxis) {
				cubeMapImages[CubeMapFace::FRONT].push_back(image[componentIndex]);
				cubeMapImages[CubeMapFace::FRONT].push_back(image[componentIndex + 1]);
				cubeMapImages[CubeMapFace::FRONT].push_back(image[componentIndex + 2]);
				cubeMapImages[CubeMapFace::FRONT].push_back(image[componentIndex + 3]);
				localXAngle = acos(dot(glm::normalize(glm::vec3(cartesianCoordinatesOnSphere.x, 0.0f, cartesianCoordinatesOnSphere.z)), closestAxis));
				localYAngle = acos(dot(glm::normalize(glm::vec3(0.0f, cartesianCoordinatesOnSphere.y, cartesianCoordinatesOnSphere.z)), closestAxis));
				localXAngle = cartesianCoordinatesOnSphere.x > 0 ? abs(localXAngle) : -1 * abs(localXAngle);
				localYAngle = cartesianCoordinatesOnSphere.y < 0 ? abs(localYAngle) : -1 * abs(localYAngle);
			}

			// The pixel belongs to the cube map's face on the left.
			if (closestAxis == negativeXAxis) {
				cubeMapImages[CubeMapFace::LEFT].push_back(image[componentIndex]);
				cubeMapImages[CubeMapFace::LEFT].push_back(image[componentIndex + 1]);
				cubeMapImages[CubeMapFace::LEFT].push_back(image[componentIndex + 2]);
				cubeMapImages[CubeMapFace::LEFT].push_back(image[componentIndex + 3]);
				localXAngle = acos(dot(glm::normalize(glm::vec3(cartesianCoordinatesOnSphere.x, 0.0f, cartesianCoordinatesOnSphere.z)), closestAxis));
				localYAngle = acos(dot(glm::normalize(glm::vec3(cartesianCoordinatesOnSphere.x, cartesianCoordinatesOnSphere.y, 0.0f)), closestAxis));
				localXAngle = cartesianCoordinatesOnSphere.x > 0 ? abs(localXAngle) : -1 * abs(localXAngle);
				localYAngle = cartesianCoordinatesOnSphere.y < 0 ? abs(localYAngle) : -1 * abs(localYAngle);
			}

			// The pixel belongs to the cube map's face below.
			if (closestAxis == negativeYAxis) {
				cubeMapImages[CubeMapFace::LOWER].push_back(image[componentIndex]);
				cubeMapImages[CubeMapFace::LOWER].push_back(image[componentIndex + 1]);
				cubeMapImages[CubeMapFace::LOWER].push_back(image[componentIndex + 2]);
				cubeMapImages[CubeMapFace::LOWER].push_back(image[componentIndex + 3]);
				localXAngle = acos(dot(glm::normalize(glm::vec3(cartesianCoordinatesOnSphere.x, cartesianCoordinatesOnSphere.y, 0.0f)), closestAxis));
				localYAngle = acos(dot(glm::normalize(glm::vec3(cartesianCoordinatesOnSphere.x, cartesianCoordinatesOnSphere.y, 0.0f)), closestAxis));
				localXAngle = cartesianCoordinatesOnSphere.x > 0 ? abs(localXAngle) : -1 * abs(localXAngle);
				localYAngle = cartesianCoordinatesOnSphere.z < 0 ? abs(localYAngle) : -1 * abs(localYAngle);
			}

			// The pixel belongs to the cube map's face behind.
			if (closestAxis == negativeZAxis) {
				cubeMapImages[CubeMapFace::BACK].push_back(image[componentIndex]);
				cubeMapImages[CubeMapFace::BACK].push_back(image[componentIndex + 1]);
				cubeMapImages[CubeMapFace::BACK].push_back(image[componentIndex + 2]);
				cubeMapImages[CubeMapFace::BACK].push_back(image[componentIndex + 3]);
				localXAngle = acos(dot(glm::normalize(glm::vec3(cartesianCoordinatesOnSphere.x, cartesianCoordinatesOnSphere.y, 0.0f)), closestAxis));
				localYAngle = acos(dot(glm::normalize(glm::vec3(0.0f, cartesianCoordinatesOnSphere.y, cartesianCoordinatesOnSphere.z)), closestAxis));
				localXAngle = cartesianCoordinatesOnSphere.x < 0 ? abs(localXAngle) : -1 * abs(localXAngle);
				localYAngle = cartesianCoordinatesOnSphere.y < 0 ? abs(localYAngle) : -1 * abs(localYAngle);
			}

			// This calculates the UV coordinates relative to the cube map's face we detected that the pixel belongs to.
			UVCoordinateX = (tan(localXAngle) + 1.0f) * 0.5f;
			UVCoordinateY = (tan(localYAngle) + 1.0f) * 0.5f;

			// We have 4 side faces on the cube map, so we divide the width of the spherical HDRi image by 4.
			// That will give us the width in pixels each individual image of the cube map needs to be.

			// Clumping the color and the coordinate of that pixel as if the image was folded on itself into a sphere.
			/*unsigned int color = 0;

			for (int c = actualComponents; c > 0; --c) {
				color |= image[componentIndex + c] << (8 * (c - 1));
			}*/

			/*unsigned int r = image[componentIndex];
			unsigned int g = image[componentIndex+1];
			unsigned int b = image[componentIndex+2];*/

			/*_pixelColors[pixelIndex] = glm::vec3((float)image[componentIndex], (float)image[componentIndex + 1], (float)image[componentIndex + 2]);
			_pixelCoordinatesWorldSpace[pixelIndex] = glm::vec3(sphereCoordinateX, sphereCoordinateY, sphereCoordinateZ);*/
		}



		int c = 0;
		for (auto i = cubeMapImages.begin(); i != cubeMapImages.end(); ++i) {
			stbi_write_png(std::to_string(c).c_str(), _width / 4, _width / 4, 4, &i->second, _width);
			++c;
		}

		std::cout << "Environment map " << imageFilePath.string() << " loaded." << std::endl;
	}
}