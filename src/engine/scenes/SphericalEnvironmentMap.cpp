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

        for (int componentIndex = 0; componentIndex < imageLength; componentIndex += wantedComponents)
        {



            int pixelIndex = componentIndex / wantedComponents;
            int imageCoordinateX = pixelIndex % _width;
            int imageCoordinateY = pixelIndex / _width;

            // Mapping the pixel's image coordinates into [0-1] UV coordinates space.
            float uvCoordinateX = (float)imageCoordinateX / (float)_width;
            float uvCoordinateY = 1.0f - ((float)imageCoordinateY / (float)_height);

            // Mapping UV coordinates onto a unit sphere and calculating spherical coordinates.
            float azimuthDegrees = fmodf((360.0f * uvCoordinateX) + 180.0f, 360.0f);
            float zenithDegrees = -((180.0f * uvCoordinateY) - 90.0f);


        }

        std::cout << "Environment map " << imageFilePath.string() << " loaded." << std::endl;
    }

    //    void SphericalEnvironmentMap::LoadFromFile(std::filesystem::path imageFilePath)
    //    {
    //        int wantedComponents = 4;
    //        int componentsDetected;
    //
    //        // In the stbi_load() function, comp stands for components. In a PNG image, for example, there are 4 components 
    //        // for each pixel, red, green, blue and alpha.
    //        // The image's pixels are read and stored left to right, top to bottom, relative to the image.
    //        // Each pixel's component is an unsigned char.
    //        auto image = stbi_load(imageFilePath.string().c_str(), &_width, &_height, &componentsDetected, wantedComponents);
    //        auto imageLength = _width * _height * 4;
    //        auto pixelCount = _width * _height;
    //
    //        for (int componentIndex = 0; componentIndex < imageLength; ++componentIndex) {
    //            _pixelColors.push_back(image[componentIndex]);
    //        }
    //
    //        // Get some variables ready for conversion.
    //        glm::vec3 positiveXAxis(1.0f, 0.0f, 0.0f);
    //        glm::vec3 positiveYAxis(0.0f, 1.0f, 0.0f);
    //        glm::vec3 positiveZAxis(0.0f, 0.0f, 1.0f);
    //        glm::vec3 negativeXAxis(-1.0f, 0.0f, 0.0f);
    //        glm::vec3 negativeYAxis(0.0f, -1.0f, 0.0f);
    //        glm::vec3 negativeZAxis(0.0f, 0.0f, -1.0f);
    //
    //        // This map will keep track of each of the cubemap's images we generate from the spherical HDRi.
    //        // The width of the image must be the width of the HDRi divided by 4 (because we have 4 side faces).
    //        // The height will be the same as the width.
    //        auto cubeMapImageWidthAndHeight = _width / 4;
    //        CubicalEnvironmentMap cubeMap(cubeMapImageWidthAndHeight * cubeMapImageWidthAndHeight);
    //        auto debugImage = std::vector<unsigned char>(imageLength);
    //
    //        for (int componentIndex = 0; componentIndex < imageLength; componentIndex += wantedComponents)
    //        {
    //            int pixelIndex = componentIndex / wantedComponents;
    //            int imageCoordinateX = pixelIndex % _width;
    //            int imageCoordinateY = pixelIndex / _width;
    //
    //            // Mapping the pixel's image coordinates into [0-1] UV coordinates space.
    //            float uvCoordinateX = (float)imageCoordinateX / (float)_width;
    //            float uvCoordinateY = 1.0f - ((float)imageCoordinateY / (float)_height);
    //
    //            // Mapping UV coordinates onto a unit sphere and calculating spherical coordinates.
    //            float azimuthDegrees = fmodf((360.0f * uvCoordinateX) + 180.0f, 360.0f);
    //            float zenithDegrees = -((180.0f * uvCoordinateY) - 90.0f);
    //
    //            if (azimuthDegrees == 0.0f && zenithDegrees == 0.0f) {
    //                auto m = 4;
    //            }
    //
    //            // Calculating cartesian (x, y, z) coordinates in world space from spherical coordinates. 
    //            // Assuming that the origin is the center of the sphere, we are using a left-handed coordinate 
    //            // system and that the Z vector (forward vector) in world space points to the spherical coordinate 
    //            // with azimuth = 0 and zenith = 0, or UV coordinates (0.5, 0.5).
    //            float sphereCoordinateX = sin(glm::radians(azimuthDegrees)) * cos(glm::radians(zenithDegrees));
    //            float sphereCoordinateY = -sin(glm::radians(zenithDegrees));
    //            float sphereCoordinateZ = cos(glm::radians(azimuthDegrees)) * cos(glm::radians(zenithDegrees));
    //            glm::vec3 cartesianCoordinatesOnSphere(sphereCoordinateX, sphereCoordinateY, sphereCoordinateZ);
    //            cartesianCoordinatesOnSphere = glm::normalize(cartesianCoordinatesOnSphere);
    //
    //            // Now we calculate the dot product with all axes and get the maximum (which will be the closest to 1 because
    //            // both vectors are normalized). By doing this, we find which axis our vector representing cartesian coordinates on the sphere
    //            // is most aligned with.
    //            std::vector<std::pair<glm::vec3, float>> dotProducts;
    //            dotProducts.push_back(std::make_pair(positiveXAxis, glm::dot(cartesianCoordinatesOnSphere, positiveXAxis)));
    //            dotProducts.push_back(std::make_pair(positiveYAxis, glm::dot(cartesianCoordinatesOnSphere, positiveYAxis)));
    //            dotProducts.push_back(std::make_pair(positiveZAxis, glm::dot(cartesianCoordinatesOnSphere, positiveZAxis)));
    //            dotProducts.push_back(std::make_pair(negativeXAxis, glm::dot(cartesianCoordinatesOnSphere, negativeXAxis)));
    //            dotProducts.push_back(std::make_pair(negativeYAxis, glm::dot(cartesianCoordinatesOnSphere, negativeYAxis)));
    //            dotProducts.push_back(std::make_pair(negativeZAxis, glm::dot(cartesianCoordinatesOnSphere, negativeZAxis)));
    //            glm::vec3 closestAxis = positiveXAxis;
    //            float maxDotProduct = -1.0f;
    //
    //            // Loop through the map of dot products to find the closest axis to our cartesian coordinates vector.
    //            for (auto i = dotProducts.begin(); i != dotProducts.end(); ++i) {
    //                if (i->second > maxDotProduct) {
    //                    maxDotProduct = i->second;
    //                    closestAxis = i->first;
    //                }
    //            }
    //
    //            // Find the vertical and horizontal angles between the vector representing the cartesian coordinates on the sphere
    //            // and the closest axis. This will give us the angles we need to feed into the tan()
    //            // function to get the X and Y coordinates of the cube map's face we are trying to 
    //            // calculate XY coordinates for, relative to its center.
    //            float localXAngle = 0.0f;
    //            float localYAngle = 0.0f;
    //            float UVCoordinateX = 0.0f;
    //            float UVCoordinateY = 0.0f;
    //
    //            CubeMapFace detectedFace = CubeMapFace::NONE;
    //
    //            // The pixel belongs to the cube map's face on the right.
    //            if (closestAxis == positiveXAxis) {
    //                detectedFace = CubeMapFace::RIGHT;
    //                localXAngle = acos(dot(glm::normalize(glm::vec3(cartesianCoordinatesOnSphere.x, 0.0f, cartesianCoordinatesOnSphere.z)), closestAxis));
    //                localYAngle = acos(dot(glm::normalize(glm::vec3(cartesianCoordinatesOnSphere.x, cartesianCoordinatesOnSphere.y, 0.0f)), closestAxis));
    //                localXAngle = cartesianCoordinatesOnSphere.z < 0 ? abs(localXAngle) : -1 * abs(localXAngle);
    //                localYAngle = cartesianCoordinatesOnSphere.y < 0 ? abs(localYAngle) : -1 * abs(localYAngle);
    //            }
    //
    //            // The pixel belongs to the cube map's face above.
    //            if (closestAxis == positiveYAxis) {
    //                detectedFace = CubeMapFace::UPPER;
    //                localXAngle = acos(dot(glm::normalize(glm::vec3(cartesianCoordinatesOnSphere.x, cartesianCoordinatesOnSphere.y, 0.0f)), closestAxis));
    //                localYAngle = acos(dot(glm::normalize(glm::vec3(0.0f, cartesianCoordinatesOnSphere.y, cartesianCoordinatesOnSphere.z)), closestAxis));
    //                localXAngle = cartesianCoordinatesOnSphere.x > 0 ? abs(localXAngle) : -1 * abs(localXAngle);
    //                localYAngle = cartesianCoordinatesOnSphere.z > 0 ? abs(localYAngle) : -1 * abs(localYAngle);
    //            }
    //
    //            // The pixel belongs to the cube map's face in front.
    //            if (closestAxis == positiveZAxis) {
    //                detectedFace = CubeMapFace::FRONT;
    //                localXAngle = acos(dot(glm::normalize(glm::vec3(cartesianCoordinatesOnSphere.x, 0.0f, cartesianCoordinatesOnSphere.z)), closestAxis));
    //                localYAngle = acos(dot(glm::normalize(glm::vec3(0.0f, cartesianCoordinatesOnSphere.y, cartesianCoordinatesOnSphere.z)), closestAxis));
    //                localXAngle = cartesianCoordinatesOnSphere.x > 0 ? abs(localXAngle) : -1 * abs(localXAngle);
    //                localYAngle = cartesianCoordinatesOnSphere.y < 0 ? abs(localYAngle) : -1 * abs(localYAngle);
    //            }
    //
    //            // The pixel belongs to the cube map's face on the left.
    //            if (closestAxis == negativeXAxis) {
    //                detectedFace = CubeMapFace::LEFT;
    //                localXAngle = acos(dot(glm::normalize(glm::vec3(cartesianCoordinatesOnSphere.x, 0.0f, cartesianCoordinatesOnSphere.z)), closestAxis));
    //                localYAngle = acos(dot(glm::normalize(glm::vec3(cartesianCoordinatesOnSphere.x, cartesianCoordinatesOnSphere.y, 0.0f)), closestAxis));
    //                localXAngle = cartesianCoordinatesOnSphere.x > 0 ? abs(localXAngle) : -1 * abs(localXAngle);
    //                localYAngle = cartesianCoordinatesOnSphere.y < 0 ? abs(localYAngle) : -1 * abs(localYAngle);
    //            }
    //
    //            // The pixel belongs to the cube map's face below.
    //            if (closestAxis == negativeYAxis) {
    //                detectedFace = CubeMapFace::LOWER;
    //                localXAngle = acos(dot(glm::normalize(glm::vec3(cartesianCoordinatesOnSphere.x, cartesianCoordinatesOnSphere.y, 0.0f)), closestAxis));
    //                localYAngle = acos(dot(glm::normalize(glm::vec3(0.0f, cartesianCoordinatesOnSphere.y, cartesianCoordinatesOnSphere.z)), closestAxis));
    //                localXAngle = cartesianCoordinatesOnSphere.x > 0 ? abs(localXAngle) : -1 * abs(localXAngle);
    //                localYAngle = cartesianCoordinatesOnSphere.z < 0 ? abs(localYAngle) : -1 * abs(localYAngle);
    //            }
    //
    //            // The pixel belongs to the cube map's face behind.
    //            if (closestAxis == negativeZAxis) {
    //                detectedFace = CubeMapFace::BACK;
    //                localXAngle = acos(dot(glm::normalize(glm::vec3(cartesianCoordinatesOnSphere.x, 0.0f, cartesianCoordinatesOnSphere.z)), closestAxis));
    //                localYAngle = acos(dot(glm::normalize(glm::vec3(0.0f, cartesianCoordinatesOnSphere.y, cartesianCoordinatesOnSphere.z)), closestAxis));
    //                localXAngle = cartesianCoordinatesOnSphere.x < 0 ? abs(localXAngle) : -1 * abs(localXAngle);
    //                localYAngle = cartesianCoordinatesOnSphere.y < 0 ? abs(localYAngle) : -1 * abs(localYAngle);
    //            }
    //
    //#pragma region Debug image
    //
    //            if (detectedFace == CubeMapFace::FRONT) {
    //                debugImage[componentIndex] = 255;
    //                debugImage[componentIndex + 1] = 0;
    //                debugImage[componentIndex + 2] = 0;
    //                debugImage[componentIndex + 3] = 255;
    //            }
    //
    //            if (detectedFace == CubeMapFace::RIGHT) {
    //                debugImage[componentIndex] = 255;
    //                debugImage[componentIndex + 1] = 255;
    //                debugImage[componentIndex + 2] = 0;
    //                debugImage[componentIndex + 3] = 255;
    //            }
    //
    //            if (detectedFace == CubeMapFace::BACK) {
    //                debugImage[componentIndex] = 255;
    //                debugImage[componentIndex + 1] = 255;
    //                debugImage[componentIndex + 2] = 255;
    //                debugImage[componentIndex + 3] = 255;
    //            }
    //
    //            if (detectedFace == CubeMapFace::LEFT) {
    //                debugImage[componentIndex] = 0;
    //                debugImage[componentIndex + 1] = 255;
    //                debugImage[componentIndex + 2] = 0;
    //                debugImage[componentIndex + 3] = 255;
    //            }
    //
    //            if (detectedFace == CubeMapFace::UPPER) {
    //                debugImage[componentIndex] = 0;
    //                debugImage[componentIndex + 1] = 255;
    //                debugImage[componentIndex + 2] = 255;
    //                debugImage[componentIndex + 3] = 255;
    //            }
    //
    //            if (detectedFace == CubeMapFace::LOWER) {
    //                debugImage[componentIndex] = 0;
    //                debugImage[componentIndex + 1] = 0;
    //                debugImage[componentIndex + 2] = 255;
    //                debugImage[componentIndex + 3] = 255;
    //            }
    //
    //#pragma endregion
    //
    //            // This calculates the UV coordinates relative to the cube map's face we detected that the pixel belongs to.
    //            UVCoordinateX = (tan(localXAngle) + 1.0f) * 0.5f;
    //            UVCoordinateY = 1.0f - ((tan(localYAngle) + 1.0f) * 0.5f);
    //
    //            if (UVCoordinateX > 1.0f || UVCoordinateY > 1.0f || UVCoordinateX < 0.0f || UVCoordinateY < 0.0f) {
    //                auto x = 2;
    //            }
    //
    //            // Now we need to force the position of the sampled pixel into an image.
    //            // A spherical HDRi is heavily distorted, because in some areas, especially around the poles
    //            // (so at the top and bottom of the image), very little actual image data needs to be stretched
    //            // out in order to fit the width and height of the rectangular image it's shown in. The same is
    //            // true in reverse. If you were to try and wrap a piece of paper (the image) into a sphere, you will
    //            // find that you will have the most trouble at the poles, because you have to shrink the piece of paper
    //            // in that specific area. This is no different in our case, we will need to discard/overlap
    //            // some sampled pixels. We will copy pixels to each image keeping into account the fact that the data
    //            // in them will be stored top left to bottom right, so we need to come up with a function that takes UV
    //            // coordinates X, a color C and an array of colors I (the image), and places C into I at the correct index
    //            // depending on X.
    //            // This means that if the UV coordinates are (0, 1) we map to index = 0. If the coordinates are (1, 0) we map
    //            // to index = array[array.size-1] (the last element in the array). If we take coordinate (0.5, 0.5) we map to
    //            // index = array[(array.size-1) / 2]. For the reasons above, an index might be calculated from 2 different UV
    //            // coordinates, meaning that we would be trying to fit 2 colors in one; to minimize the impact of this, each time
    //            // 2 pixels of the spherical HDRi map to one pixel of a specific cube map's image, we take the average of the
    //            // 2 colors.
    //            auto& cubeMapImage = cubeMap[detectedFace];
    //            int index = (UVCoordinateX * cubeMapImageWidthAndHeight) + (((cubeMapImageWidthAndHeight * (cubeMapImageWidthAndHeight - 1)) * (1.0f - UVCoordinateY)));
    //            auto red = image[componentIndex];
    //            auto green = image[componentIndex + 1];
    //            auto blue = image[componentIndex + 2];
    //            auto alpha = image[componentIndex + 3];
    //            unsigned int hdriColor = red << 24 | green << 16 | blue << 8 | alpha;
    //
    //            auto existingColor = cubeMapImage[index];
    //            auto averageRed = ((existingColor >> 24 & 255) + red) / 2;
    //            auto averageGreen = ((existingColor >> 16 & 255) + green) / 2;
    //            auto averageBlue = ((existingColor >> 8 & 255) + blue) / 2;
    //            auto averageAlpha = ((existingColor & 255) + alpha) / 2;
    //            unsigned int averageColor = averageRed << 24 | averageGreen << 16 | averageBlue << 8 | averageAlpha;
    //            
    //            cubeMapImage[index] = (cubeMapImage[index] == 0u) ? hdriColor : averageColor;
    //            cubeMap.SetFaceData(detectedFace, cubeMapImage);
    //        }
    //
    //#pragma region Debug image to file
    //
    //        auto debugImagePath = Settings::Paths::TexturesPath() /= std::filesystem::path("Debug.png");
    //        stbi_write_png(debugImagePath.string().c_str(),
    //            _width,
    //            _height,
    //            4,
    //            debugImage.data(),
    //            _width * 4);
    //
    //#pragma endregion
    //
    //#pragma region Writing to file
    //
    //        auto imageData = new unsigned char[cubeMapImageWidthAndHeight * cubeMapImageWidthAndHeight * 4];
    //        int index = 0;
    //
    //        for (int i = 0; i < cubeMap[CubeMapFace::FRONT].size(); ++i) {
    //            auto color = cubeMap[CubeMapFace::FRONT][i];
    //            auto red = color >> 24 & 255;
    //            auto green= color >> 16 & 255;
    //            auto blue= color >> 8 & 255;
    //            auto alpha = color & 255;
    //            imageData[index++] = red;
    //            imageData[index++] = green;
    //            imageData[index++] = blue;
    //            imageData[index++] = alpha;
    //        }
    //
    //        auto path = Settings::Paths::TexturesPath() /= std::filesystem::path("Front.png");
    //        stbi_write_png(path.string().c_str(),
    //            cubeMapImageWidthAndHeight, 
    //            cubeMapImageWidthAndHeight,
    //            4,
    //            imageData,
    //            cubeMapImageWidthAndHeight * 4);
    //
    //        index = 0;
    //
    //        for (int i = 0; i < cubeMap[CubeMapFace::RIGHT].size(); ++i) {
    //            auto color = cubeMap[CubeMapFace::RIGHT][i];
    //            auto red = color >> 24 & 255;
    //            auto green = color >> 16 & 255;
    //            auto blue = color >> 8 & 255;
    //            auto alpha = color & 255;
    //            imageData[index++] = red;
    //            imageData[index++] = green;
    //            imageData[index++] = blue;
    //            imageData[index++] = alpha;
    //        }
    //
    //        path = Settings::Paths::TexturesPath() /= std::filesystem::path("Right.png");
    //        stbi_write_png(path.string().c_str(),
    //            cubeMapImageWidthAndHeight,
    //            cubeMapImageWidthAndHeight,
    //            4,
    //            imageData,
    //            cubeMapImageWidthAndHeight * 4);
    //
    //        index = 0;
    //
    //        for (int i = 0; i < cubeMap[CubeMapFace::BACK].size(); ++i) {
    //            auto color = cubeMap[CubeMapFace::BACK][i];
    //            auto red = color >> 24 & 255;
    //            auto green = color >> 16 & 255;
    //            auto blue = color >> 8 & 255;
    //            auto alpha = color & 255;
    //            imageData[index++] = red;
    //            imageData[index++] = green;
    //            imageData[index++] = blue;
    //            imageData[index++] = alpha;
    //        }
    //
    //        path = Settings::Paths::TexturesPath() /= std::filesystem::path("Back.png");
    //        stbi_write_png(path.string().c_str(),
    //            cubeMapImageWidthAndHeight,
    //            cubeMapImageWidthAndHeight,
    //            4,
    //            imageData,
    //            cubeMapImageWidthAndHeight * 4);
    //
    //        index = 0;
    //
    //        for (int i = 0; i < cubeMap[CubeMapFace::LEFT].size(); ++i) {
    //            auto color = cubeMap[CubeMapFace::LEFT][i];
    //            auto red = color >> 24 & 255;
    //            auto green = color >> 16 & 255;
    //            auto blue = color >> 8 & 255;
    //            auto alpha = color & 255;
    //            imageData[index++] = red;
    //            imageData[index++] = green;
    //            imageData[index++] = blue;
    //            imageData[index++] = alpha;
    //        }
    //
    //        path = Settings::Paths::TexturesPath() /= std::filesystem::path("Left.png");
    //        stbi_write_png(path.string().c_str(),
    //            cubeMapImageWidthAndHeight,
    //            cubeMapImageWidthAndHeight,
    //            4,
    //            imageData,
    //            cubeMapImageWidthAndHeight * 4);
    //
    //        index = 0;
    //
    //        for (int i = 0; i < cubeMap[CubeMapFace::UPPER].size(); ++i) {
    //            auto color = cubeMap[CubeMapFace::UPPER][i];
    //            auto red = color >> 24 & 255;
    //            auto green = color >> 16 & 255;
    //            auto blue = color >> 8 & 255;
    //            auto alpha = color & 255;
    //            imageData[index++] = red;
    //            imageData[index++] = green;
    //            imageData[index++] = blue;
    //            imageData[index++] = alpha;
    //        }
    //
    //        path = Settings::Paths::TexturesPath() /= std::filesystem::path("Upper.png");
    //        stbi_write_png(path.string().c_str(),
    //            cubeMapImageWidthAndHeight,
    //            cubeMapImageWidthAndHeight,
    //            4,
    //            imageData,
    //            cubeMapImageWidthAndHeight * 4);
    //
    //        index = 0;
    //
    //        for (int i = 0; i < cubeMap[CubeMapFace::LOWER].size(); ++i) {
    //            auto color = cubeMap[CubeMapFace::LOWER][i];
    //            auto red = color >> 24 & 255;
    //            auto green = color >> 16 & 255;
    //            auto blue = color >> 8 & 255;
    //            auto alpha = color & 255;
    //            imageData[index++] = red;
    //            imageData[index++] = green;
    //            imageData[index++] = blue;
    //            imageData[index++] = alpha;
    //        }
    //
    //        path = Settings::Paths::TexturesPath() /= std::filesystem::path("Lower.png");
    //        stbi_write_png(path.string().c_str(),
    //            cubeMapImageWidthAndHeight,
    //            cubeMapImageWidthAndHeight,
    //            4,
    //            imageData,
    //            cubeMapImageWidthAndHeight * 4);
    //
    //#pragma endregion
    //
    //        std::cout << "Environment map " << imageFilePath.string() << " loaded." << std::endl;
    //    }
}