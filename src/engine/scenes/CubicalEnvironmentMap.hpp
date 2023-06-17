#pragma once

namespace Engine::Scenes
{
    /**
     * @brief Used specifically for the CubicalEnvironmentMap class in order to index into its faces.
     */
    enum class CubeMapFace {
        NONE,
        FRONT,
        RIGHT,
        BACK,
        LEFT,
        UPPER,
        LOWER
    };

    /**
     * @brief Represents a cubical environment map, used as an image-based light source
     * in the shaders.
     */
    class CubicalEnvironmentMap
    {
    public:

        /**
         * @brief Front face data.
         */
        std::vector<unsigned char> _front;

        /**
         * @brief Right face data.
         */
        std::vector<unsigned char> _right;

        /**
         * @brief Back face data.
         */
        std::vector<unsigned char> _back;

        /**
         * @brief Left face data.
         */
        std::vector<unsigned char> _left;

        /**
         * @brief Upper face data.
         */
        std::vector<unsigned char> _upper;

        /**
         * @brief Lower face data.
         */
        std::vector<unsigned char> _lower;

        /**
         * @brief Width and height of each face's image.
         */
        unsigned int _faceSizePixels;

        /**
         * @brief Default constructor.
         */
        CubicalEnvironmentMap() = default;

        std::vector<unsigned char>& operator[](const CubeMapFace& face) {
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

        void SetFaceData(const CubeMapFace& face, const std::vector<unsigned char>& data) {
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

        /**
         * @brief Loads an environment map from a spherical HDRi file and converts it to a cubical map.
         */
        void LoadFromSphericalHDRI(std::filesystem::path imageFilePath);

        void CreateShaderResources(VkDevice& logicalDevice, Vulkan::PhysicalDevice& physicalDevice);
    };
}

