#include <iostream>
#include <vector>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <stb/stb_image.h>
#include <vulkan/vulkan.h>

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
		int desiredComponents = 4;
		
		if (auto extension = imageFilePath.extension(); extension.string() == ".jpg") {
			desiredComponents = 3;
		}

		// In the stbi_load() function, comp stands for components. In a PNG image, for example, there are 4 components 
		// for each pixel, red, green, blue and alpha.
		// The image's pixels are read and stored left to right, top to bottom, relative to the image.
		// Each pixel's component is an unsigned char.
		auto image = stbi_load(imageFilePath.string().c_str(), &_width, &_height, &actualComponents, desiredComponents);
		auto imageLength = _width * _height * actualComponents;
		auto pixelCount = _width * _height;

		_pixelColors.resize(pixelCount);
		_pixelCoordinatesWorldSpace.resize(pixelCount);

		for (int componentIndex = 0; componentIndex < imageLength; componentIndex += actualComponents)
		{
			int pixelIndex = componentIndex / actualComponents;
			int imageCoordinateX = pixelIndex % _width;
			int imageCoordinateY = pixelIndex / _width;

			// Mapping the image's coordinates into [0-1] UV coordinate space.
			float uvCoordinateX = (float)imageCoordinateX / (float)_width;
			float uvCoordinateY = 1.0f - ((float)imageCoordinateY / (float)_height);

			// Mapping image coordinates onto a unit sphere and calculating spherical coordinates.
			float azimuthDegrees = 360.0f - (360.0f * uvCoordinateX);
			float zenithDegrees = (180.0f * uvCoordinateY) - 90.0f;

			// Calculating cartesian (x, y, z) coordinates in world space from spherical coordinates. 
			// Assuming that the origin is the center of the sphere, we are using a left-handed coordinate 
			// system and that the Z vector (forward vector) in world space points to the spherical coordinate 
			// with azimuth = 0 and zenith = 0.
			float sphereCoordinateX = sin(glm::radians(azimuthDegrees)) * cos(glm::radians(zenithDegrees));
			float sphereCoordinateY = sin(glm::radians(zenithDegrees));
			float sphereCoordinateZ = cos(glm::radians(azimuthDegrees)) * cos(glm::radians(zenithDegrees));

			// Clumping the color and the coordinate of that pixel as if the image was folded on itself into a sphere.
			unsigned int color = 0;

			for (int c = actualComponents; c > 0; --c) {
				color |= image[componentIndex + c] << (8 * (c - 1));
			}

			unsigned int r = image[componentIndex];
			unsigned int g = image[componentIndex+1];
			unsigned int b = image[componentIndex+2];

			_pixelColors[pixelIndex] = glm::vec3((float)image[componentIndex] / 255.0f, (float)image[componentIndex + 1] / 255.0f, (float)image[componentIndex + 2] / 255.0f);
			_pixelCoordinatesWorldSpace[pixelIndex] = glm::vec3(sphereCoordinateX, sphereCoordinateY, sphereCoordinateZ);
		}

		std::cout << "Environment map " << imageFilePath.string() << " loaded." << std::endl;
	}
}