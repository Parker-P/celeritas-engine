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
#include "settings/Paths.hpp"
#include "engine/vulkan/Queue.hpp"
#include "engine/vulkan/PhysicalDevice.hpp"
#include "engine/vulkan/Image.hpp"
#include "engine/vulkan/Buffer.hpp"
#include "engine/scenes/CubicalEnvironmentMap.hpp"
#include "structural/IUpdatable.hpp"
#include "structural/Singleton.hpp"
#include "engine/Time.hpp"
#include "settings/GlobalSettings.hpp"
#include "engine/math/Transform.hpp"
#include "engine/vulkan/PhysicalDevice.hpp"

namespace Engine::Scenes
{

	void CubicalEnvironmentMap::LoadFromSphericalHDRI(std::filesystem::path imageFilePath)
	{
		// The code that follows is an absolute disgrace. Feel free to refactor if it makes you angry.
		int wantedComponents = 4;
		int componentsDetected;
		int width;
		int height;

		// First, we load the spherical HDRi image.
		// In the stbi_load() function, comp stands for components. In a PNG image, for example, there are 4 components 
		// for each pixel, red, green, blue and alpha.
		// The image's pixels are read and stored left to right, top to bottom, relative to the image.
		// Each pixel's component is an unsigned char.
		auto image = stbi_load(imageFilePath.string().c_str(), &width, &height, &componentsDetected, wantedComponents);

		if (nullptr == image) {
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

		// TODO: make the conversion for each face into one function that handles all cases
		// given the right parameters.

		// Front face.
		{
			// The world space unit vector that points in the positive X direction of the image (must be left to right), as if the image was placed in the 3D world on a square plane.
			auto imageXWorldSpace = glm::vec3(1.0f, 0.0f, 0.0f);

			// The world space unit vector that points in the positive Y direction of the image (must be bottom to top), as if the image was placed in the 3D world on a square plane.
			auto imageYWorldSpace = glm::vec3(0.0f, 1.0f, 0.0f);

			// The origin of the image (must be the bottom left corner) in world space, as if the image was placed in the 3D world on square plane.
			auto imageOriginWorldSpace = glm::vec3(-0.5f, 0.5f, 0.5f);

			for (int y = 0; y < _faceSizePixels; ++y) {
				for (int x = 0; x < _faceSizePixels; ++x) {

					// First we calculate the cartesian coordinate of the pixel of the cube map's face we are considering.
					glm::vec3 cartesianCoordinatesOnFace;
					cartesianCoordinatesOnFace = imageOriginWorldSpace + (imageXWorldSpace * ((1.0f / _faceSizePixels) * x));
					cartesianCoordinatesOnFace += -imageYWorldSpace * ((1.0f / _faceSizePixels) * y);

					// Then we get the cartesian coordinates on the sphere from the cartesian coordinates on the face. You can
					// imagine a cube that contains a sphere of exactly the same radius. Only the sphere's poles and sides will
					// touch each cube's face exactly at the center. We take advantage of the fact that the radius of the sphere
					// is constant (always equal to 1 for simplicity in our case) so all we need to do is normalize the coordinates
					// on the face and we get the cartesian coordinates on the sphere. This kind of simulates shooting a ray from
					// the current pixel on the cube, intersecting it with the sphere, and taking the coordinates of the intersection
					// point.
					auto cartesianCoordinatesOnSphere = glm::normalize(cartesianCoordinatesOnFace);

					// Then we calculate the spherical coordinates by using some trig.
					auto azimuthDegrees = glm::degrees(atanf(cartesianCoordinatesOnFace.x * 2.0f));
					auto zenithDegrees = (90.0f - glm::degrees(acosf(cartesianCoordinatesOnSphere.y))) * -1.0f;

					// Depending on the coordinate, the angles could either be negative or positive, depending on the left hand rule.
					// This engine uses a left-handed coordinate system.
					azimuthDegrees = cartesianCoordinatesOnFace.x < 0.0f ? abs(azimuthDegrees) * -1.0f : abs(azimuthDegrees);
					zenithDegrees = cartesianCoordinatesOnFace.y < 0.0f ? abs(zenithDegrees) : abs(zenithDegrees) * -1.0f;

					// Now we make the azimuth respect the [0-360] degree domain.
					azimuthDegrees = fmodf((360.0f + azimuthDegrees), 360.0f);

					// UV coordinates into the spherical HDRi image.
					auto uCoordinate = fmodf(0.5f + (azimuthDegrees / 360.0f), 1.0f);
					auto vCoordinate = 0.5f + (zenithDegrees / -180.0f);

					// This calculates at which pixel from the left (for U) and from the top (for V)
					// we need to fetch from the spherical HDRi.
					int pixelNumberU = (int)ceil(uCoordinate * width);
					int pixelNumberV = (int)ceil((1.0f - vCoordinate) * height);

					// This calculates the index into the spherical HDRi image accounting for 2 things:
					// 1) the pixel numbers calculated above
					// 2) the fact that each pixel is actually stored in 4 separate cells that represent
					//    each channel of each individual pixel. The channels used are always 4 (RGBA).
					int componentIndex = (pixelNumberU * 4) + ((width * 4) * (pixelNumberV - 1));

					// Fetch the channels from the spherical HDRi based on the calculations made above.
					auto red = image[componentIndex];
					auto green = image[componentIndex + 1];
					auto blue = image[componentIndex + 2];
					auto alpha = image[componentIndex + 3];

					// Assign the color fetched from the HDRi to the cube map face's image we want to generate.
					int faceComponentIndex = (x + (_faceSizePixels * y)) * 4;
					_front[faceComponentIndex] = red;
					_front[faceComponentIndex + 1] = green;
					_front[faceComponentIndex + 2] = blue;
					_front[faceComponentIndex + 3] = alpha;
				}
			}

			/*auto frontFaceImagePath = Settings::Paths::TexturesPath() /= std::filesystem::path("FrontFace.png");
			stbi_write_png(frontFaceImagePath.string().c_str(),
				_faceSizePixels,
				_faceSizePixels,
				4,
				frontFace.data(),
				_faceSizePixels * 4);*/
		}

		// Right face.
		{
			// The world space unit vector that points in the positive X direction of the image (must be left to right), as if the image was placed in the 3D world on a square plane.
			auto imageXWorldSpace = glm::vec3(0.0f, 0.0f, -1.0f);

			// The world space unit vector that points in the positive Y direction of the image (must be bottom to top), as if the image was placed in the 3D world on a square plane.
			auto imageYWorldSpace = glm::vec3(0.0f, 1.0f, 0.0f);

			// The origin of the image (must be the bottom left corner) in world space, as if the image was placed in the 3D world on square plane.
			auto imageOriginWorldSpace = glm::vec3(0.5f, 0.5f, 0.5f);

			for (int y = 0; y < _faceSizePixels; ++y) {
				for (int x = 0; x < _faceSizePixels; ++x) {

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

					// Then we calculate the spherical coordinates by using some trig.
					auto localXAngleDegrees = glm::degrees(atanf(cartesianCoordinatesOnFace.z * 2.0f));
					auto localYAngleDegrees = (90.0f - glm::degrees(acosf(cartesianCoordinatesOnSphere.y))) * -1.0f;

					// Depending on the coordinate, the angles could either be negative or positive, depending on the left hand rule.
					// This engine uses a left-handed coordinate system.
					localXAngleDegrees = cartesianCoordinatesOnFace.z > 0.0f ? abs(localXAngleDegrees) * -1.0f : abs(localXAngleDegrees);
					localYAngleDegrees = cartesianCoordinatesOnFace.y < 0.0f ? abs(localYAngleDegrees) : abs(localYAngleDegrees) * -1.0f;
					localXAngleDegrees += 90;

					// Now we make the azimuth respect the [0-360] degree domain.
					auto azimuthDegrees = fmodf((360.0f + localXAngleDegrees), 360.0f);
					auto zenithDegrees = localYAngleDegrees;

					// UV coordinates into the spherical HDRi image.
					auto uCoordinate = fmodf(0.5f + (azimuthDegrees / 360.0f), 1.0f);
					auto vCoordinate = 0.5f + (zenithDegrees / -180.0f);

					// This calculates at which pixel from the left (for U) and from the top (for V)
					// we need to fetch from the spherical HDRi.
					int pixelNumberU = (int)ceil(uCoordinate * width);
					int pixelNumberV = (int)ceil((1.0f - vCoordinate) * height);

					// This calculates the index into the spherical HDRi image accounting for 2 things:
					// 1) the pixel numbers calculated above
					// 2) the fact that each pixel is actually stored in 4 separate cells that represent
					//    each channel of each individual pixel. The channels used are always 4 (RGBA).
					int componentIndex = (pixelNumberU * 4) + ((width * 4) * (pixelNumberV - 1));

					// Fetch the channels from the spherical HDRi based on the calculations made above.
					auto red = image[componentIndex];
					auto green = image[componentIndex + 1];
					auto blue = image[componentIndex + 2];
					auto alpha = image[componentIndex + 3];

					// Assign the color fetched from the HDRi to the cube map face's image we want to generate.
					int faceComponentIndex = (x + (_faceSizePixels * y)) * 4;
					_right[faceComponentIndex] = red;
					_right[faceComponentIndex + 1] = green;
					_right[faceComponentIndex + 2] = blue;
					_right[faceComponentIndex + 3] = alpha;
				}
			}

			/*auto rightFaceImagePath = Settings::Paths::TexturesPath() /= std::filesystem::path("RightFace.png");
			stbi_write_png(rightFaceImagePath.string().c_str(),
				_faceSizePixels,
				_faceSizePixels,
				4,
				rightFace.data(),
				_faceSizePixels * 4);*/
		}

		// Back face.
		{
			auto backFace = std::vector<unsigned char>(_faceSizePixels * _faceSizePixels * 4);

			// The world space unit vector that points in the positive X direction of the image (must be left to right), as if the image was placed in the 3D world on a square plane.
			auto imageXWorldSpace = glm::vec3(-1.0f, 0.0f, 0.0f);

			// The world space unit vector that points in the positive Y direction of the image (must be bottom to top), as if the image was placed in the 3D world on a square plane.
			auto imageYWorldSpace = glm::vec3(0.0f, 1.0f, 0.0f);

			// The origin of the image (must be the bottom left corner) in world space, as if the image was placed in the 3D world on square plane.
			auto imageOriginWorldSpace = glm::vec3(0.5f, 0.5f, -0.5f);

			for (int y = 0; y < _faceSizePixels; ++y) {
				for (int x = 0; x < _faceSizePixels; ++x) {

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

					// Then we calculate the spherical coordinates by using some trig.
					auto localXAngleDegrees = glm::degrees(atanf(cartesianCoordinatesOnFace.x * 2.0f));
					auto localYAngleDegrees = (90.0f - glm::degrees(acosf(cartesianCoordinatesOnSphere.y))) * -1.0f;

					// Depending on the coordinate, the angles could either be negative or positive, depending on the left hand rule.
					// This engine uses a left-handed coordinate system.
					localXAngleDegrees = cartesianCoordinatesOnFace.x > 0.0f ? abs(localXAngleDegrees) * -1.0f : abs(localXAngleDegrees);
					localYAngleDegrees = cartesianCoordinatesOnFace.y < 0.0f ? abs(localYAngleDegrees) : abs(localYAngleDegrees) * -1.0f;
					localXAngleDegrees += 180;

					// Now we make the azimuth respect the [0-360] degree domain.
					auto azimuthDegrees = fmodf((360.0f + localXAngleDegrees), 360.0f);
					auto zenithDegrees = localYAngleDegrees;

					// UV coordinates into the spherical HDRi image.
					auto uCoordinate = fmodf(0.5f + (azimuthDegrees / 360.0f), 1.0f);
					auto vCoordinate = 0.5f + (zenithDegrees / -180.0f);

					// This calculates at which pixel from the left (for U) and from the top (for V)
					// we need to fetch from the spherical HDRi.
					int pixelNumberU = (int)ceil(uCoordinate * width);
					int pixelNumberV = (int)ceil((1.0f - vCoordinate) * height);

					// This calculates the index into the spherical HDRi image accounting for 2 things:
					// 1) the pixel numbers calculated above
					// 2) the fact that each pixel is actually stored in 4 separate cells that represent
					//    each channel of each individual pixel. The channels used are always 4 (RGBA).
					int componentIndex = (pixelNumberU * 4) + ((width * 4) * (pixelNumberV - 1));

					// Fetch the channels from the spherical HDRi based on the calculations made above.
					auto red = image[componentIndex];
					auto green = image[componentIndex + 1];
					auto blue = image[componentIndex + 2];
					auto alpha = image[componentIndex + 3];

					// Assign the color fetched from the HDRi to the cube map face's image we want to generate.
					int faceComponentIndex = (x + (_faceSizePixels * y)) * 4;
					_back[faceComponentIndex] = red;
					_back[faceComponentIndex + 1] = green;
					_back[faceComponentIndex + 2] = blue;
					_back[faceComponentIndex + 3] = alpha;
				}
			}

			/*auto backFaceImagePath = Settings::Paths::TexturesPath() /= std::filesystem::path("BackFace.png");
			stbi_write_png(backFaceImagePath.string().c_str(),
				_faceSizePixels,
				_faceSizePixels,
				4,
				backFace.data(),
				_faceSizePixels * 4);*/
		}

		// Left face.
		{
			// The world space unit vector that points in the positive X direction of the image (must be left to right), as if the image was placed in the 3D world on a square plane.
			auto imageXWorldSpace = glm::vec3(0.0f, 0.0f, 1.0f);

			// The world space unit vector that points in the positive Y direction of the image (must be bottom to top), as if the image was placed in the 3D world on a square plane.
			auto imageYWorldSpace = glm::vec3(0.0f, 1.0f, 0.0f);

			// The origin of the image (must be the top left corner) in world space, as if the image was placed in the 3D world on square plane.
			auto imageOriginWorldSpace = glm::vec3(-0.5f, 0.5f, -0.5f);

			for (int y = 0; y < _faceSizePixels; ++y) {
				for (int x = 0; x < _faceSizePixels; ++x) {

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

					// Then we calculate the spherical coordinates by using some trig.
					auto localXAngleDegrees = glm::degrees(atanf(cartesianCoordinatesOnFace.z * 2.0f));
					auto localYAngleDegrees = (90.0f - glm::degrees(acosf(cartesianCoordinatesOnSphere.y))) * -1.0f;

					// Depending on the coordinate, the angles could either be negative or positive, depending on the left hand rule.
					// This engine uses a left-handed coordinate system.
					localXAngleDegrees = cartesianCoordinatesOnFace.z < 0.0f ? abs(localXAngleDegrees) * -1.0f : abs(localXAngleDegrees);
					localYAngleDegrees = cartesianCoordinatesOnFace.y < 0.0f ? abs(localYAngleDegrees) : abs(localYAngleDegrees) * -1.0f;
					localXAngleDegrees += 270;

					// Now we make the azimuth respect the [0-360] degree domain.
					auto azimuthDegrees = fmodf((360.0f + localXAngleDegrees), 360.0f);
					auto zenithDegrees = localYAngleDegrees;

					// UV coordinates into the spherical HDRi image.
					auto uCoordinate = fmodf(0.5f + (azimuthDegrees / 360.0f), 1.0f);
					auto vCoordinate = 0.5f + (zenithDegrees / -180.0f);

					// This calculates at which pixel from the left (for U) and from the top (for V)
					// we need to fetch from the spherical HDRi.
					int pixelNumberU = (int)ceil(uCoordinate * width);
					int pixelNumberV = (int)ceil((1.0f - vCoordinate) * height);

					// This calculates the index into the spherical HDRi image accounting for 2 things:
					// 1) the pixel numbers calculated above
					// 2) the fact that each pixel is actually stored in 4 separate cells that represent
					//    each channel of each individual pixel. The channels used are always 4 (RGBA).
					int componentIndex = (pixelNumberU * 4) + ((width * 4) * (pixelNumberV - 1));

					// Fetch the channels from the spherical HDRi based on the calculations made above.
					auto red = image[componentIndex];
					auto green = image[componentIndex + 1];
					auto blue = image[componentIndex + 2];
					auto alpha = image[componentIndex + 3];

					// Assign the color fetched from the HDRi to the cube map face's image we want to generate.
					int faceComponentIndex = (x + (_faceSizePixels * y)) * 4;
					_left[faceComponentIndex] = red;
					_left[faceComponentIndex + 1] = green;
					_left[faceComponentIndex + 2] = blue;
					_left[faceComponentIndex + 3] = alpha;
				}
			}

			/*auto leftFaceImagePath = Settings::Paths::TexturesPath() /= std::filesystem::path("LeftFace.png");
			stbi_write_png(leftFaceImagePath.string().c_str(),
				_faceSizePixels,
				_faceSizePixels,
				4,
				leftFace.data(),
				_faceSizePixels * 4);*/
		}

		// Upper face.
		{
			// The world space unit vector that points in the positive X direction of the image (must be left to right), as if the image was placed in the 3D world on a square plane.
			auto imageXWorldSpace = glm::vec3(1.0f, 0.0f, 0.0f);

			// The world space unit vector that points in the positive Y direction of the image (must be bottom to top), as if the image was placed in the 3D world on a square plane.
			auto imageYWorldSpace = glm::vec3(0.0f, 0.0f, -1.0f);

			// The origin of the image (must be the top left corner) in world space, as if the image was placed in the 3D world on square plane.
			auto imageOriginWorldSpace = glm::vec3(-0.5f, 0.5f, -0.5f);

			for (int y = 0; y < _faceSizePixels; ++y) {
				for (int x = 0; x < _faceSizePixels; ++x) {

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

					if (cartesianCoordinatesOnSphere.y < 1.0f) {

						// Then we calculate the spherical coordinates by using some trig.
						auto temp = glm::normalize(glm::vec3(cartesianCoordinatesOnSphere.x, 0.0f, cartesianCoordinatesOnSphere.z));
						auto localXAngleDegrees = glm::degrees(acosf(temp.z));
						auto localYAngleDegrees = glm::degrees(acosf(cartesianCoordinatesOnSphere.y));

						// Depending on the coordinate, the angles could either be negative or positive, depending on the left hand rule.
						// This engine uses a left-handed coordinate system.
						localXAngleDegrees = cartesianCoordinatesOnFace.x < 0.0f ? abs(localXAngleDegrees) * -1.0f : abs(localXAngleDegrees);
						localYAngleDegrees = cartesianCoordinatesOnFace.y < 0.0f ? abs(localYAngleDegrees) : abs(localYAngleDegrees) * -1.0f;

						// Now we make the azimuth respect the [0-360] degree domain.
						auto azimuthDegrees = fmodf((360.0f + localXAngleDegrees), 360.0f);
						auto zenithDegrees = (90.0f + localYAngleDegrees) * -1.0f;

						// UV coordinates into the spherical HDRi image.
						auto uCoordinate = fmodf(0.5f + (azimuthDegrees / 360.0f), 1.0f);
						auto vCoordinate = 0.5f + (zenithDegrees / -180.0f);

						// This calculates at which pixel from the left (for U) and from the top (for V)
						// we need to fetch from the spherical HDRi.
						int pixelNumberU = (int)ceil(uCoordinate * width);
						int pixelNumberV = (int)ceil((1.0f - vCoordinate) * height);

						// This calculates the index into the spherical HDRi image accounting for 2 things:
						// 1) the pixel numbers calculated above
						// 2) the fact that each pixel is actually stored in 4 separate cells that represent
						//    each channel of each individual pixel. The channels used are always 4 (RGBA).
						int componentIndex = (pixelNumberU * 4) + ((width * 4) * (pixelNumberV - 1));

						// Fetch the channels from the spherical HDRi based on the calculations made above.
						auto red = image[componentIndex];
						auto green = image[componentIndex + 1];
						auto blue = image[componentIndex + 2];
						auto alpha = image[componentIndex + 3];

						// Assign the color fetched from the HDRi to the cube map face's image we want to generate.
						int faceComponentIndex = (x + (_faceSizePixels * y)) * 4;
						_upper[faceComponentIndex] = red;
						_upper[faceComponentIndex + 1] = green;
						_upper[faceComponentIndex + 2] = blue;
						_upper[faceComponentIndex + 3] = alpha;
					}
				}
			}

			/*auto upperFaceImagePath = Settings::Paths::TexturesPath() /= std::filesystem::path("UpperFace.png");
			stbi_write_png(upperFaceImagePath.string().c_str(),
				_faceSizePixels,
				_faceSizePixels,
				4,
				upperFace.data(),
				_faceSizePixels * 4);*/
		}

		// Lower face.
		{
			auto lowerFace = std::vector<unsigned char>(_faceSizePixels * _faceSizePixels * 4);

			// The world space unit vector that points in the positive X direction of the image (must be left to right), as if the image was placed in the 3D world on a square plane.
			auto imageXWorldSpace = glm::vec3(1.0f, 0.0f, 0.0f);

			// The world space unit vector that points in the positive Y direction of the image (must be bottom to top), as if the image was placed in the 3D world on a square plane.
			auto imageYWorldSpace = glm::vec3(0.0f, 0.0f, 1.0f);

			// The origin of the image (must be the top left corner) in world space, as if the image was placed in the 3D world on square plane.
			auto imageOriginWorldSpace = glm::vec3(-0.5f, -0.5f, 0.5f);

			for (int y = 0; y < _faceSizePixels; ++y) {
				for (int x = 0; x < _faceSizePixels; ++x) {

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

					if (cartesianCoordinatesOnSphere.y < 1.0f) {

						// Then we calculate the spherical coordinates by using some trig.
						auto temp = glm::normalize(glm::vec3(cartesianCoordinatesOnSphere.x, 0.0f, cartesianCoordinatesOnSphere.z));
						auto localXAngleDegrees = glm::degrees(acosf(temp.z));
						auto localYAngleDegrees = glm::degrees(acosf(cartesianCoordinatesOnSphere.y));

						// Depending on the coordinate, the angles could either be negative or positive, depending on the left hand rule.
						// This engine uses a left-handed coordinate system.
						localXAngleDegrees = cartesianCoordinatesOnFace.x < 0.0f ? abs(localXAngleDegrees) * -1.0f : abs(localXAngleDegrees);
						localYAngleDegrees = cartesianCoordinatesOnFace.y < 0.0f ? abs(localYAngleDegrees) : abs(localYAngleDegrees) * -1.0f;

						// Now we make the azimuth respect the [0-360] degree domain.
						auto azimuthDegrees = fmodf((360.0f + localXAngleDegrees), 360.0f);
						auto zenithDegrees = (90.0f - localYAngleDegrees) * -1.0f;

						// UV coordinates into the spherical HDRi image.
						auto uCoordinate = fmodf(0.5f + (azimuthDegrees / 360.0f), 1.0f);
						auto vCoordinate = 0.5f + (zenithDegrees / -180.0f);

						// This calculates at which pixel from the left (for U) and from the top (for V)
						// we need to fetch from the spherical HDRi.
						int pixelNumberU = (int)ceil(uCoordinate * width);
						int pixelNumberV = (int)ceil((1.0f - vCoordinate) * height);

						// This calculates the index into the spherical HDRi image accounting for 2 things:
						// 1) the pixel numbers calculated above
						// 2) the fact that each pixel is actually stored in 4 separate cells that represent
						//    each channel of each individual pixel. The channels used are always 4 (RGBA).
						int componentIndex = (pixelNumberU * 4) + ((width * 4) * (pixelNumberV - 1));

						// Fetch the channels from the spherical HDRi based on the calculations made above.
						auto red = image[componentIndex];
						auto green = image[componentIndex + 1];
						auto blue = image[componentIndex + 2];
						auto alpha = image[componentIndex + 3];

						// Assign the color fetched from the HDRi to the cube map face's image we want to generate.
						int faceComponentIndex = (x + (_faceSizePixels * y)) * 4;
						_lower[faceComponentIndex] = red;
						_lower[faceComponentIndex + 1] = green;
						_lower[faceComponentIndex + 2] = blue;
						_lower[faceComponentIndex + 3] = alpha;
					}
				}
			}

			/*auto lowerFaceImagePath = Settings::Paths::TexturesPath() /= std::filesystem::path("LowerFace.png");
			stbi_write_png(lowerFaceImagePath.string().c_str(),
				_faceSizePixels,
				_faceSizePixels,
				4,
				lowerFace.data(),
				_faceSizePixels * 4);*/
		}

		std::cout << "Environment map " << imageFilePath.string() << " loaded." << std::endl;
	}

	void CubicalEnvironmentMap::CreateShaderResources(VkDevice& logicalDevice, Vulkan::PhysicalDevice& physicalDevice, VkCommandPool& commandPool, Vulkan::Queue& graphicsQueue)
	{

#pragma region 1) Create a temporary buffer that contains the data of each image of the cube map as one serialized data array.

		// Create a host-visible staging buffer that contains the serialized raw data of all faces' images.
		VkBuffer stagingBuffer;
		VkBufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = _faceSizePixels * _faceSizePixels * 4; // Size in bytes.
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT; // This buffer is used as a transfer source for the buffer copy
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		vkCreateBuffer(logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer);

		// Allocate memory for the buffer.
		VkMemoryRequirements memoryRequirements;
		vkGetBufferMemoryRequirements(logicalDevice, stagingBuffer, &memoryRequirements);
		auto stagingBufferMemoryHandle = physicalDevice.AllocateMemory(logicalDevice, memoryRequirements, (VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
		vkBindBufferMemory(logicalDevice, stagingBuffer, stagingBufferMemoryHandle, 0);

		// Copy texture data into the staging buffer as a single data array.
		uint8_t* data;
		vkMapMemory(logicalDevice, stagingBufferMemoryHandle, 0, memoryRequirements.size, 0, (void**)&data);
		auto serializedFaceImages = Serialize();
		memcpy(data, serializedFaceImages.data(), Utils::GetVectorSizeInBytes(serializedFaceImages));
		vkUnmapMemory(logicalDevice, stagingBufferMemoryHandle);

#pragma endregion

#pragma region 2) Then create buffer copy regions, to instruct Vulkan on how to copy our data in the temporary buffer to the allocated GPU memory.

		// Setup buffer copy regions for each face. A copy region is just a structure to tell Vulkan how to direct the data
		// to the right place when copying it to VRAM.
		// For cube and cube array image views, the layers of the image view starting at baseArrayLayer correspond to 
		// faces in the order +X, -X, +Y, -Y, +Z, -Z.
		// Right image.
		uint32_t offset = 0;
		std::vector<VkBufferImageCopy> bufferCopyRegions;

		for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
			VkBufferImageCopy bufferCopyRegion{};
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel = 1;
			bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
			bufferCopyRegion.imageSubresource.layerCount = 1;
			bufferCopyRegion.imageExtent.width = _faceSizePixels;
			bufferCopyRegion.imageExtent.height = _faceSizePixels;
			bufferCopyRegion.imageExtent.depth = 1;
			bufferCopyRegion.bufferOffset = offset;
			offset += Utils::GetVectorSizeInBytes(_front);
			bufferCopyRegions.push_back(bufferCopyRegion);
		}

#pragma endregion

#pragma region 3) Create a Vulkan image that will contain all 6 images that we will copy to it. This is the image that will be passed onto the shaders.

		// Create optimal tiled target image.
		VkImageCreateInfo imageCreateInfo{};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.extent = { _faceSizePixels, _faceSizePixels, 1 };
		imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCreateInfo.arrayLayers = 6; // Cube faces count as array layers in Vulkan.
		imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT; // This flag is required for cube map images.
		VkImage cubeMapImageGlobal; // Cube map image that contains all cube map images. This is the image that will be sampled by the shaders.
		vkCreateImage(logicalDevice, &imageCreateInfo, nullptr, &cubeMapImageGlobal);

		// Allocate VRAM memory for the image.
		vkGetImageMemoryRequirements(logicalDevice, cubeMapImageGlobal, &memoryRequirements);
		auto globalImageMemoryHandle = physicalDevice.AllocateMemory(logicalDevice, memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkBindImageMemory(logicalDevice, cubeMapImageGlobal, globalImageMemoryHandle, 0);

#pragma endregion

#pragma region 4) Copy the data from the temporary buffer into the created image.

		// Create a command buffer to tell Vulkan to copy the images to VRAM.
		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.commandBufferCount = 1;
		allocateInfo.commandPool = commandPool;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		VkCommandBuffer copyCommandBuffer;
		vkAllocateCommandBuffers(logicalDevice, &allocateInfo, &copyCommandBuffer);

		// This tells Vulkan to start recording commands into copyCommandBuffer. Any Vulkan API call from now on, on this thread, that is prefixed
		// with vkCmd will be part of copyCommandBuffer, until vkEndCommandBuffer() is called.
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(copyCommandBuffer, &beginInfo);

		// Now we need to use a barrier to tell Vulkan that we want to change the memory layout of the image into the transfer-receive layout.
		// GPUs use specific memory layouts (the way data is arranged and stored on disk, for example an image could be stored one row of pixels
		// after another row of pixels and so on, or column after column). By using specific layouts, certain operations are significantly faster.
		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = 6;
		VkImageMemoryBarrier imageBarrierUndefinedToTransfer = {};
		imageBarrierUndefinedToTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarrierUndefinedToTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageBarrierUndefinedToTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrierUndefinedToTransfer.image = cubeMapImageGlobal;
		imageBarrierUndefinedToTransfer.subresourceRange = subresourceRange;
		imageBarrierUndefinedToTransfer.srcAccessMask = 0;
		imageBarrierUndefinedToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		vkCmdPipelineBarrier(copyCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrierUndefinedToTransfer);

		// Copy the cube map faces from the staging buffer to the global cube map image. Remember that our staging buffer contains the cube map's images serialized as one
		// array, the way that the Serialize function in this class serialized it, in the following order: _right, _left, _upper, _lower, _front, _back.
		vkCmdCopyBufferToImage(
			copyCommandBuffer,
			stagingBuffer,
			cubeMapImageGlobal,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(bufferCopyRegions.size()),
			bufferCopyRegions.data()
		);

		// Change texture image layout to shader read after all faces have been copied, so that the image is in a specific memory layout that allows faster reading from shaders.
		VkImageMemoryBarrier imageBarrierTransferToShaderRead = {};
		imageBarrierTransferToShaderRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarrierTransferToShaderRead.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageBarrierTransferToShaderRead.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrierTransferToShaderRead.image = cubeMapImageGlobal;
		imageBarrierTransferToShaderRead.subresourceRange = subresourceRange;
		imageBarrierTransferToShaderRead.srcAccessMask = 0;
		imageBarrierTransferToShaderRead.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		vkCmdPipelineBarrier(copyCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrierTransferToShaderRead);
		
		// End recording commands into copyCommandBuffer.
		vkEndCommandBuffer(copyCommandBuffer);

		// Destroy the staging buffer that is no longer needed.
		vkFreeMemory(logicalDevice, stagingBufferMemoryHandle, nullptr);
		vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);

#pragma endregion

#pragma region 4) Create a sampler and link it to the image/image view created above; this tells the shaders how to read the data in the cube map image we created above.

		// Create sampler to instruct the shaders on how to interpret the data in the global cube map image.
		VkSamplerCreateInfo samplerCreateInfo{};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
		samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 1;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		VkSampler sampler;
		vkCreateSampler(logicalDevice, &samplerCreateInfo, nullptr, &sampler);

		// Create an image view to inform Vulkan on what type of image this is.
		VkImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE; // Cube map view type.
		imageViewCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; 
		imageViewCreateInfo.subresourceRange.layerCount = 6; // 6 array layers (faces).
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.image = cubeMapImageGlobal;
		VkImageView imageView;
		vkCreateImageView(logicalDevice, &imageViewCreateInfo, nullptr, &imageView);

#pragma endregion

	}

	std::vector<unsigned char> CubicalEnvironmentMap::Serialize()
	{
		// According to the Vulkan documentation, the faces should be serialized in the following order: +X, -X, +Y, -Y, +Z, -Z.
		// This corresponds to _right, _left, _upper, _lower, _front, _back for our cube map structure that we have in place.

		std::vector<unsigned char> serialized;

		if (_front.size() == _right.size() == _back.size() == _left.size() == _upper.size() == _lower.size()) {
			size_t imageSizeBytes = _front.size();
			serialized.reserve(imageSizeBytes); // Type is unsigned char so the slot count in the vector is the same as the byte count.
			memcpy(serialized.data(), _right.data(), imageSizeBytes);							
			memcpy(serialized.data() + imageSizeBytes + 1, _left.data(), imageSizeBytes);		
			memcpy(serialized.data() + (imageSizeBytes * 2) + 1 , _upper.data(), imageSizeBytes);
			memcpy(serialized.data() + (imageSizeBytes * 3) + 1, _lower.data(), imageSizeBytes);
			memcpy(serialized.data() + (imageSizeBytes * 4) + 1, _front.data(), imageSizeBytes);
			memcpy(serialized.data() + (imageSizeBytes * 5) + 1, _back.data(), imageSizeBytes);	
		}

		return serialized;
	}
}