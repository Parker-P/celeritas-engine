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
#include "engine/scenes/SphericalEnvironmentMap.hpp"

namespace Engine::Scenes
{
    enum class CubeMapFace {
        NONE,
        FRONT,
        RIGHT,
        BACK,
        LEFT,
        UPPER,
        LOWER
    };

    class CubicalEnvironmentMap
    {
    public:

        // Image data of each face.
        std::vector<unsigned int> _front;
        std::vector<unsigned int> _right;
        std::vector<unsigned int> _back;
        std::vector<unsigned int> _left;
        std::vector<unsigned int> _upper;
        std::vector<unsigned int> _lower;

        CubicalEnvironmentMap() = default;

        CubicalEnvironmentMap(int faceSizePixels)
        {
            _front = std::vector<unsigned int>(faceSizePixels);
            _right = std::vector<unsigned int>(faceSizePixels);
            _back = std::vector<unsigned int>(faceSizePixels);
            _left = std::vector<unsigned int>(faceSizePixels);
            _upper = std::vector<unsigned int>(faceSizePixels);
            _lower = std::vector<unsigned int>(faceSizePixels);
        }

        std::vector<unsigned int>& operator[](const CubeMapFace& face) {
            switch (face) {
            case CubeMapFace::FRONT:
                return _front;
            case CubeMapFace::RIGHT:
                return _right;
            case CubeMapFace::BACK:
                return _back;
            case CubeMapFace::UPPER:
                return _upper;
            case CubeMapFace::LEFT:
                return _left;
            case CubeMapFace::LOWER:
                return _lower;
            default:
                return _front;
            }
        }

        void SetFaceData(const CubeMapFace& face, const std::vector<unsigned int>& data) {
            switch (face) {
            case CubeMapFace::FRONT:
                _front = data;
                break;
            case CubeMapFace::RIGHT:
                _right = data;
                break;
            case CubeMapFace::BACK:
                _back = data;
                break;
            case CubeMapFace::UPPER:
                _upper = data;
                break;
            case CubeMapFace::LEFT:
                _left = data;
                break;
            case CubeMapFace::LOWER:
                _lower = data;
                break;
            default:
                break;
            }
        }
    };

    void SphericalEnvironmentMap::LoadFromFile(std::filesystem::path imageFilePath)
    {
        int wantedComponents = 4;
        int componentsDetected;

        // In the stbi_load() function, comp stands for components. In a PNG image, for example, there are 4 components 
        // for each pixel, red, green, blue and alpha.
        // The image's pixels are read and stored left to right, top to bottom, relative to the image.
        // Each pixel's component is an unsigned char.
        auto image = stbi_load(imageFilePath.string().c_str(), &_width, &_height, &componentsDetected, wantedComponents);
        auto imageLength = _width * _height * 4;
        auto pixelCount = _width * _height;

        std::vector<std::vector<unsigned char>> easierImage(_height);
        unsigned long long rowIndex = (unsigned long long)image;

        // This map will keep track of each of the cubemap's images we generate from the spherical HDRi.
        // The width of the image must be the width of the HDRi divided by 4 (because we have 4 side faces).
        // The height will be the same as the width.
        auto cubeMapFaceSizePixels = _width / 4;
        CubicalEnvironmentMap cubeMap(cubeMapFaceSizePixels * cubeMapFaceSizePixels);
        auto debugImage = std::vector<unsigned char>(imageLength);

        // Front face.
        {
            auto frontFace = std::vector<unsigned char>((cubeMapFaceSizePixels * cubeMapFaceSizePixels) * 4);

            for (int y = 0; y < cubeMapFaceSizePixels; ++y) {
                for (int x = 0; x < cubeMapFaceSizePixels; ++x) {

                    // First we calculate the cartesian coordinate of the pixel of the cube map's face we are considering.
                    glm::vec3 cartesianCoordinatesOnFace;
                    cartesianCoordinatesOnFace.x = -0.5f + ((1.0f / cubeMapFaceSizePixels) * x);
                    cartesianCoordinatesOnFace.y = 0.5f - ((1.0f / cubeMapFaceSizePixels) * y);
                    cartesianCoordinatesOnFace.z = 0.5f;

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
                    int pixelNumberU = ceil(uCoordinate * _width);
                    int pixelNumberV = ceil((1.0f - vCoordinate) * _height);

                    // This calculates the index into the spherical HDRi image accounting for 2 things:
                    // 1) the pixel numbers calculated above
                    // 2) the fact that each pixel is actually stored in 4 separate cells that represent
                    //    each channel of each individual pixel. The channels used are always 4 (RGBA).
                    int componentIndex = (pixelNumberU * 4) + ((_width * 4) * (pixelNumberV - 1));

                    // Fetch the channels from the spherical HDRi based on the calculations made above.
                    auto red = image[componentIndex];
                    auto green = image[componentIndex + 1];
                    auto blue = image[componentIndex + 2];
                    auto alpha = image[componentIndex + 3];

                    // Assign the color fetched from the HDRi to the cube map face's image we want to generate.
                    int faceComponentIndex = (x + (cubeMapFaceSizePixels * y)) * 4;
                    frontFace[faceComponentIndex] = red;
                    frontFace[faceComponentIndex + 1] = green;
                    frontFace[faceComponentIndex + 2] = blue;
                    frontFace[faceComponentIndex + 3] = alpha;
                }
            }

            auto frontFaceImagePath = Settings::Paths::TexturesPath() /= std::filesystem::path("FrontFace.png");
            stbi_write_png(frontFaceImagePath.string().c_str(),
                cubeMapFaceSizePixels,
                cubeMapFaceSizePixels,
                4,
                frontFace.data(),
                cubeMapFaceSizePixels * 4);
        }

        // Right face.
        {
            auto rightFace = std::vector<unsigned char>((cubeMapFaceSizePixels * cubeMapFaceSizePixels) * 4);

            // The world space unit vector that points in the positive X direction of the image (must be left to right), as if the image was placed in the 3D world on a square plane.
            auto imageXWorldSpace = glm::vec3(0.0f, 0.0f, -1.0f);

            // The world space unit vector that points in the positive Y direction of the image (must be bottom to top), as if the image was placed in the 3D world on a square plane.
            auto imageYWorldSpace = glm::vec3(0.0f, 1.0f, 0.0f);

            // The origin of the image (must be the bottom left corner) in world space, as if the image was placed in the 3D world on square plane.
            auto imageOriginWorldSpace = glm::vec3(0.5f, 0.5f, 0.5f);

            for (int y = 0; y < cubeMapFaceSizePixels; ++y) {
                for (int x = 0; x < cubeMapFaceSizePixels; ++x) {

                    // First we calculate the cartesian coordinate of the pixel of the cube map's face we are considering, in world space.
                    glm::vec3 cartesianCoordinatesOnFace;
                    cartesianCoordinatesOnFace = imageOriginWorldSpace + (imageXWorldSpace * ((1.0f / cubeMapFaceSizePixels) * x));
                    cartesianCoordinatesOnFace += -imageYWorldSpace * ((1.0f / cubeMapFaceSizePixels) * y);

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
                    int pixelNumberU = ceil(uCoordinate * _width);
                    int pixelNumberV = ceil((1.0f - vCoordinate) * _height);

                    // This calculates the index into the spherical HDRi image accounting for 2 things:
                    // 1) the pixel numbers calculated above
                    // 2) the fact that each pixel is actually stored in 4 separate cells that represent
                    //    each channel of each individual pixel. The channels used are always 4 (RGBA).
                    int componentIndex = (pixelNumberU * 4) + ((_width * 4) * (pixelNumberV - 1));

                    // Fetch the channels from the spherical HDRi based on the calculations made above.
                    auto red = image[componentIndex];
                    auto green = image[componentIndex + 1];
                    auto blue = image[componentIndex + 2];
                    auto alpha = image[componentIndex + 3];

                    // Assign the color fetched from the HDRi to the cube map face's image we want to generate.
                    int faceComponentIndex = (x + (cubeMapFaceSizePixels * y)) * 4;
                    rightFace[faceComponentIndex] = red;
                    rightFace[faceComponentIndex + 1] = green;
                    rightFace[faceComponentIndex + 2] = blue;
                    rightFace[faceComponentIndex + 3] = alpha;
                }
            }

            auto rightFaceImagePath = Settings::Paths::TexturesPath() /= std::filesystem::path("RightFace.png");
            stbi_write_png(rightFaceImagePath.string().c_str(),
                cubeMapFaceSizePixels,
                cubeMapFaceSizePixels,
                4,
                rightFace.data(),
                cubeMapFaceSizePixels * 4);
        }

        // Back face.
        {
            auto backFace = std::vector<unsigned char>((cubeMapFaceSizePixels * cubeMapFaceSizePixels) * 4);

            // The world space unit vector that points in the positive X direction of the image (must be left to right), as if the image was placed in the 3D world on a square plane.
            auto imageXWorldSpace = glm::vec3(-1.0f, 0.0f, 0.0f);

            // The world space unit vector that points in the positive Y direction of the image (must be bottom to top), as if the image was placed in the 3D world on a square plane.
            auto imageYWorldSpace = glm::vec3(0.0f, 1.0f, 0.0f);

            // The origin of the image (must be the bottom left corner) in world space, as if the image was placed in the 3D world on square plane.
            auto imageOriginWorldSpace = glm::vec3(0.5f, 0.5f, -0.5f);

            for (int y = 0; y < cubeMapFaceSizePixels; ++y) {
                for (int x = 0; x < cubeMapFaceSizePixels; ++x) {

                    // First we calculate the cartesian coordinate of the pixel of the cube map's face we are considering, in world space.
                    glm::vec3 cartesianCoordinatesOnFace;
                    cartesianCoordinatesOnFace = imageOriginWorldSpace + (imageXWorldSpace * ((1.0f / cubeMapFaceSizePixels) * x));
                    cartesianCoordinatesOnFace += -imageYWorldSpace * ((1.0f / cubeMapFaceSizePixels) * y);

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
                    int pixelNumberU = ceil(uCoordinate * _width);
                    int pixelNumberV = ceil((1.0f - vCoordinate) * _height);

                    // This calculates the index into the spherical HDRi image accounting for 2 things:
                    // 1) the pixel numbers calculated above
                    // 2) the fact that each pixel is actually stored in 4 separate cells that represent
                    //    each channel of each individual pixel. The channels used are always 4 (RGBA).
                    int componentIndex = (pixelNumberU * 4) + ((_width * 4) * (pixelNumberV - 1));

                    // Fetch the channels from the spherical HDRi based on the calculations made above.
                    auto red = image[componentIndex];
                    auto green = image[componentIndex + 1];
                    auto blue = image[componentIndex + 2];
                    auto alpha = image[componentIndex + 3];

                    // Assign the color fetched from the HDRi to the cube map face's image we want to generate.
                    int faceComponentIndex = (x + (cubeMapFaceSizePixels * y)) * 4;
                    backFace[faceComponentIndex] = red;
                    backFace[faceComponentIndex + 1] = green;
                    backFace[faceComponentIndex + 2] = blue;
                    backFace[faceComponentIndex + 3] = alpha;
                }
            }

            auto backFaceImagePath = Settings::Paths::TexturesPath() /= std::filesystem::path("BackFace.png");
            stbi_write_png(backFaceImagePath.string().c_str(),
                cubeMapFaceSizePixels,
                cubeMapFaceSizePixels,
                4,
                backFace.data(),
                cubeMapFaceSizePixels * 4);
        }

        // Left face.
        {
            auto leftFace = std::vector<unsigned char>((cubeMapFaceSizePixels * cubeMapFaceSizePixels) * 4);

            // The world space unit vector that points in the positive X direction of the image (must be left to right), as if the image was placed in the 3D world on a square plane.
            auto imageXWorldSpace = glm::vec3(0.0f, 0.0f, 1.0f);

            // The world space unit vector that points in the positive Y direction of the image (must be bottom to top), as if the image was placed in the 3D world on a square plane.
            auto imageYWorldSpace = glm::vec3(0.0f, 1.0f, 0.0f);

            // The origin of the image (must be the top left corner) in world space, as if the image was placed in the 3D world on square plane.
            auto imageOriginWorldSpace = glm::vec3(-0.5f, 0.5f, -0.5f);

            for (int y = 0; y < cubeMapFaceSizePixels; ++y) {
                for (int x = 0; x < cubeMapFaceSizePixels; ++x) {

                    // First we calculate the cartesian coordinate of the pixel of the cube map's face we are considering, in world space.
                    glm::vec3 cartesianCoordinatesOnFace;
                    cartesianCoordinatesOnFace = imageOriginWorldSpace + (imageXWorldSpace * ((1.0f / cubeMapFaceSizePixels) * x));
                    cartesianCoordinatesOnFace += -imageYWorldSpace * ((1.0f / cubeMapFaceSizePixels) * y);

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
                    int pixelNumberU = ceil(uCoordinate * _width);
                    int pixelNumberV = ceil((1.0f - vCoordinate) * _height);

                    // This calculates the index into the spherical HDRi image accounting for 2 things:
                    // 1) the pixel numbers calculated above
                    // 2) the fact that each pixel is actually stored in 4 separate cells that represent
                    //    each channel of each individual pixel. The channels used are always 4 (RGBA).
                    int componentIndex = (pixelNumberU * 4) + ((_width * 4) * (pixelNumberV - 1));

                    // Fetch the channels from the spherical HDRi based on the calculations made above.
                    auto red = image[componentIndex];
                    auto green = image[componentIndex + 1];
                    auto blue = image[componentIndex + 2];
                    auto alpha = image[componentIndex + 3];

                    // Assign the color fetched from the HDRi to the cube map face's image we want to generate.
                    int faceComponentIndex = (x + (cubeMapFaceSizePixels * y)) * 4;
                    leftFace[faceComponentIndex] = red;
                    leftFace[faceComponentIndex + 1] = green;
                    leftFace[faceComponentIndex + 2] = blue;
                    leftFace[faceComponentIndex + 3] = alpha;
                }
            }

            auto leftFaceImagePath = Settings::Paths::TexturesPath() /= std::filesystem::path("LeftFace.png");
            stbi_write_png(leftFaceImagePath.string().c_str(),
                cubeMapFaceSizePixels,
                cubeMapFaceSizePixels,
                4,
                leftFace.data(),
                cubeMapFaceSizePixels * 4);
        }

        // Upper face.
        {
            auto upperFace = std::vector<unsigned char>((cubeMapFaceSizePixels * cubeMapFaceSizePixels) * 4);

            // The world space unit vector that points in the positive X direction of the image (must be left to right), as if the image was placed in the 3D world on a square plane.
            auto imageXWorldSpace = glm::vec3(1.0f, 0.0f, 0.0f);

            // The world space unit vector that points in the positive Y direction of the image (must be bottom to top), as if the image was placed in the 3D world on a square plane.
            auto imageYWorldSpace = glm::vec3(0.0f, 0.0f, -1.0f);

            // The origin of the image (must be the top left corner) in world space, as if the image was placed in the 3D world on square plane.
            auto imageOriginWorldSpace = glm::vec3(-0.5f, 0.5f, -0.5f);

            for (int y = 0; y < cubeMapFaceSizePixels; ++y) {
                for (int x = 0; x < cubeMapFaceSizePixels; ++x) {

                    // First we calculate the cartesian coordinate of the pixel of the cube map's face we are considering, in world space.
                    glm::vec3 cartesianCoordinatesOnFace;
                    cartesianCoordinatesOnFace = imageOriginWorldSpace + (imageXWorldSpace * ((1.0f / cubeMapFaceSizePixels) * x));
                    cartesianCoordinatesOnFace += -imageYWorldSpace * ((1.0f / cubeMapFaceSizePixels) * y);

                    if (cartesianCoordinatesOnFace.z == 0) {
                        auto n = 4;
                    }

                    // Then we get the cartesian coordinates on the sphere from the cartesian coordinates on the face. You can
                    // imagine a cube that contains a sphere of exactly the same radius. Only the sphere's poles and sides will
                    // touch each cube's face exactly at the center. We take advantage of the fact that the radius of the sphere
                    // is constant (always equal to 1 for simplicity in our case) so all we need to do is normalize the coordinates
                    // on the face and we get the cartesian coordinates on the sphere. This kind of simulates shooting a ray from
                    // the current pixel on the cube towards the center of the sphere, intersecting it with the sphere, and taking 
                    // the coordinates of the intersection point.
                    auto cartesianCoordinatesOnSphere = glm::normalize(cartesianCoordinatesOnFace);

                    if (cartesianCoordinatesOnSphere.y < 1.0f) {

                        auto temp = glm::normalize(glm::vec3(cartesianCoordinatesOnSphere.x, 0.0f, cartesianCoordinatesOnSphere.z));

                        // Then we calculate the spherical coordinates by using some trig.
                        auto localXAngleDegrees = glm::degrees(acosf(temp.z));


                        auto localYAngleDegrees = glm::degrees(acosf(cartesianCoordinatesOnSphere.y));

                        // Depending on the coordinate, the angles could either be negative or positive, depending on the left hand rule.
                        // This engine uses a left-handed coordinate system.
                        localXAngleDegrees = cartesianCoordinatesOnFace.x < 0.0f ? abs(localXAngleDegrees) * -1.0f : abs(localXAngleDegrees);
                        localYAngleDegrees = cartesianCoordinatesOnFace.y < 0.0f ? abs(localYAngleDegrees) : abs(localYAngleDegrees) * -1.0f;



                        // Now we make the azimuth respect the [0-360] degree domain.
                        auto azimuthDegrees = fmodf((360.0f + localXAngleDegrees), 360.0f);
                        auto zenithDegrees = (90.0f + localYAngleDegrees) * -1.0f;

                        if (zenithDegrees < -89.0f && azimuthDegrees == 270.0f) {
                            auto m = 0;
                        }

                        // UV coordinates into the spherical HDRi image.
                        auto uCoordinate = fmodf(0.5f + (azimuthDegrees / 360.0f), 1.0f);
                        auto vCoordinate = 0.5f + (zenithDegrees / -180.0f);

                        // This calculates at which pixel from the left (for U) and from the top (for V)
                        // we need to fetch from the spherical HDRi.
                        int pixelNumberU = ceil(uCoordinate * _width);
                        int pixelNumberV = ceil((1.0f - vCoordinate) * _height);

                        // This calculates the index into the spherical HDRi image accounting for 2 things:
                        // 1) the pixel numbers calculated above
                        // 2) the fact that each pixel is actually stored in 4 separate cells that represent
                        //    each channel of each individual pixel. The channels used are always 4 (RGBA).
                        int componentIndex = (pixelNumberU * 4) + ((_width * 4) * (pixelNumberV - 1));

                        // Fetch the channels from the spherical HDRi based on the calculations made above.
                        auto red = image[componentIndex];
                        auto green = image[componentIndex + 1];
                        auto blue = image[componentIndex + 2];
                        auto alpha = image[componentIndex + 3];

                        // Assign the color fetched from the HDRi to the cube map face's image we want to generate.
                        int faceComponentIndex = (x + (cubeMapFaceSizePixels * y)) * 4;
                        upperFace[faceComponentIndex] = red;
                        upperFace[faceComponentIndex + 1] = green;
                        upperFace[faceComponentIndex + 2] = blue;
                        upperFace[faceComponentIndex + 3] = alpha;
                    }
                }
            }

            auto upperFaceImagePath = Settings::Paths::TexturesPath() /= std::filesystem::path("UpperFace.png");
            stbi_write_png(upperFaceImagePath.string().c_str(),
                cubeMapFaceSizePixels,
                cubeMapFaceSizePixels,
                4,
                upperFace.data(),
                cubeMapFaceSizePixels * 4);
        }

        // Lower face.
        {
            auto lowerFace = std::vector<unsigned char>((cubeMapFaceSizePixels * cubeMapFaceSizePixels) * 4);

            // The world space unit vector that points in the positive X direction of the image (must be left to right), as if the image was placed in the 3D world on a square plane.
            auto imageXWorldSpace = glm::vec3(1.0f, 0.0f, 0.0f);

            // The world space unit vector that points in the positive Y direction of the image (must be bottom to top), as if the image was placed in the 3D world on a square plane.
            auto imageYWorldSpace = glm::vec3(0.0f, 0.0f, 1.0f);

            // The origin of the image (must be the top left corner) in world space, as if the image was placed in the 3D world on square plane.
            auto imageOriginWorldSpace = glm::vec3(-0.5f, -0.5f, 0.5f);

            for (int y = 0; y < cubeMapFaceSizePixels; ++y) {
                for (int x = 0; x < cubeMapFaceSizePixels; ++x) {

                    // First we calculate the cartesian coordinate of the pixel of the cube map's face we are considering, in world space.
                    glm::vec3 cartesianCoordinatesOnFace;
                    cartesianCoordinatesOnFace = imageOriginWorldSpace + (imageXWorldSpace * ((1.0f / cubeMapFaceSizePixels) * x));
                    cartesianCoordinatesOnFace += -imageYWorldSpace * ((1.0f / cubeMapFaceSizePixels) * y);

                    if (cartesianCoordinatesOnFace.z == 0) {
                        auto n = 4;
                    }

                    // Then we get the cartesian coordinates on the sphere from the cartesian coordinates on the face. You can
                    // imagine a cube that contains a sphere of exactly the same radius. Only the sphere's poles and sides will
                    // touch each cube's face exactly at the center. We take advantage of the fact that the radius of the sphere
                    // is constant (always equal to 1 for simplicity in our case) so all we need to do is normalize the coordinates
                    // on the face and we get the cartesian coordinates on the sphere. This kind of simulates shooting a ray from
                    // the current pixel on the cube towards the center of the sphere, intersecting it with the sphere, and taking 
                    // the coordinates of the intersection point.
                    auto cartesianCoordinatesOnSphere = glm::normalize(cartesianCoordinatesOnFace);

                    if (cartesianCoordinatesOnSphere.y < 1.0f) {

                        auto temp = glm::normalize(glm::vec3(cartesianCoordinatesOnSphere.x, 0.0f, cartesianCoordinatesOnSphere.z));

                        // Then we calculate the spherical coordinates by using some trig.
                        auto localXAngleDegrees = glm::degrees(acosf(temp.z));


                        auto localYAngleDegrees = glm::degrees(acosf(cartesianCoordinatesOnSphere.y));

                        // Depending on the coordinate, the angles could either be negative or positive, depending on the left hand rule.
                        // This engine uses a left-handed coordinate system.
                        localXAngleDegrees = cartesianCoordinatesOnFace.x < 0.0f ? abs(localXAngleDegrees) * -1.0f : abs(localXAngleDegrees);
                        localYAngleDegrees = cartesianCoordinatesOnFace.y < 0.0f ? abs(localYAngleDegrees) : abs(localYAngleDegrees) * -1.0f;



                        // Now we make the azimuth respect the [0-360] degree domain.
                        auto azimuthDegrees = fmodf((360.0f + localXAngleDegrees), 360.0f);
                        auto zenithDegrees = (90.0f - localYAngleDegrees) * -1.0f;

                        if (zenithDegrees < -89.0f && azimuthDegrees == 270.0f) {
                            auto m = 0;
                        }

                        // UV coordinates into the spherical HDRi image.
                        auto uCoordinate = fmodf(0.5f + (azimuthDegrees / 360.0f), 1.0f);
                        auto vCoordinate = 0.5f + (zenithDegrees / -180.0f);

                        // This calculates at which pixel from the left (for U) and from the top (for V)
                        // we need to fetch from the spherical HDRi.
                        int pixelNumberU = ceil(uCoordinate * _width);
                        int pixelNumberV = ceil((1.0f - vCoordinate) * _height);

                        // This calculates the index into the spherical HDRi image accounting for 2 things:
                        // 1) the pixel numbers calculated above
                        // 2) the fact that each pixel is actually stored in 4 separate cells that represent
                        //    each channel of each individual pixel. The channels used are always 4 (RGBA).
                        int componentIndex = (pixelNumberU * 4) + ((_width * 4) * (pixelNumberV - 1));

                        // Fetch the channels from the spherical HDRi based on the calculations made above.
                        auto red = image[componentIndex];
                        auto green = image[componentIndex + 1];
                        auto blue = image[componentIndex + 2];
                        auto alpha = image[componentIndex + 3];

                        // Assign the color fetched from the HDRi to the cube map face's image we want to generate.
                        int faceComponentIndex = (x + (cubeMapFaceSizePixels * y)) * 4;
                        lowerFace[faceComponentIndex] = red;
                        lowerFace[faceComponentIndex + 1] = green;
                        lowerFace[faceComponentIndex + 2] = blue;
                        lowerFace[faceComponentIndex + 3] = alpha;
                    }
                }
            }

            auto lowerFaceImagePath = Settings::Paths::TexturesPath() /= std::filesystem::path("LowerFace.png");
            stbi_write_png(lowerFaceImagePath.string().c_str(),
                cubeMapFaceSizePixels,
                cubeMapFaceSizePixels,
                4,
                lowerFace.data(),
                cubeMapFaceSizePixels * 4);
        }

        std::cout << "Environment map " << imageFilePath.string() << " loaded." << std::endl;
    }
}