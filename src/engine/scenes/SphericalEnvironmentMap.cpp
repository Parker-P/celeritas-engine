#define STB_IMAGE_IMPLEMENTATION

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
#include "engine/scenes/SphericalEnvironmentMap.hpp"

namespace Engine::Scenes
{
	void SphericalEnvironmentMap::LoadFromFile(std::filesystem::path imagePath)
	{
		int width;
		int height;
		int actualComponents;

		// In the stbi_load() function, comp stands for components. In a PNG image, for example, there are 4 components 
		// for each pixel, red, green, blue and alpha.
		// The image's pixels are read and stored left to right, top to bottom, relative to the image.
		// Each pixel's component is an unsigned char.
		auto image = stbi_load(imagePath.string().c_str(), &width, &height, &actualComponents, 4);
		auto imageLength = width * height * actualComponents;
		auto pixelCount = width * height;

		_pixelColors.resize(pixelCount);
		_pixelCoordinatesWorldSpace.resize(pixelCount);

		for (int componentIndex = 0; componentIndex < imageLength; componentIndex += 4)
		{
			int pixelIndex = componentIndex / 4;
			int imageCoordinateX = pixelIndex % width;
			int imageCoordinateY = pixelIndex / width;

			// Mapping the image's coordinates into [0-1] UV coordinate space.
			float uvCoordinateX = (float)imageCoordinateX / (float)width;
			float uvCoordinateY = 1.0f - ((float)imageCoordinateY / (float)height);

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

			// Clumping the color and the coordinate of that pixel as if the image was folden on itself into a sphere.
			glm::vec4 color(image[componentIndex], image[componentIndex + 1], image[componentIndex + 2], image[componentIndex + 3]);
			glm::vec3 coordinatesOnSphere = glm::vec3(sphereCoordinateX, sphereCoordinateY, sphereCoordinateZ);

			_pixelColors[pixelIndex] = color;
			_pixelCoordinatesWorldSpace[pixelIndex] = coordinatesOnSphere;
		}

		std::cout << ""
	}
}