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

    void CubicalEnvironmentMap::CreateShaderResources(VkDevice& logicalDevice, Vulkan::PhysicalDevice& physicalDevice)
    {
        // Create the Vulkan image/s for the cubemap.
        VkImageCreateInfo imageCreateInfo{};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageCreateInfo.extent.width = _faceSizePixels;
        imageCreateInfo.extent.height = _faceSizePixels;
        imageCreateInfo.extent.depth = 1;
        imageCreateInfo.arrayLayers = 6;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkImage cubeMapImage;

        if (vkCreateImage(logicalDevice, &imageCreateInfo, nullptr, &cubeMapImage) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }

        // Allocate memory for the cube map on the GPU.
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(logicalDevice, cubeMapImage, &memRequirements);
        auto memoryHandle = physicalDevice.AllocateMemory(logicalDevice, memRequirements);
        vkBindImageMemory(logicalDevice, cubeMapImage, memoryHandle, 0);

        // Create an image view for the cube map that adds some metadata so that Vulkan can optimize it for the GPU.
        VkImageViewCreateInfo viewCreateInfo{};
        viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        viewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewCreateInfo.subresourceRange.baseMipLevel = 0;
        viewCreateInfo.subresourceRange.levelCount = 1;
        viewCreateInfo.subresourceRange.baseArrayLayer = 0;
        viewCreateInfo.subresourceRange.layerCount = 6;
        viewCreateInfo.image = cubeMapImage;

        VkImageView cubeMapImageView;

        if (vkCreateImageView(logicalDevice, &viewCreateInfo, nullptr, &cubeMapImageView) != VK_SUCCESS) {
            std::cout << "failed to create texture image view!" << std::endl;
        }

        // Create a sampler so that Vulkan can tell the GPU how the shaders should interpret the image
        // data in the cube map. This allows functions like texture() to work in GLSL.
        VkSamplerCreateInfo samplerCreateInfo{};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.mipLodBias = 0.0f;
        samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerCreateInfo.minLod = 0.0f;
        samplerCreateInfo.maxLod = 1.0f;
        samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

        VkSampler cubeMapSampler;
        if (vkCreateSampler(logicalDevice, &samplerCreateInfo, nullptr, &cubeMapSampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }
}