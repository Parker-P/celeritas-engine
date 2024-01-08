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
    class CubicalEnvironmentMap : public Structural::IPipelineable
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
         * @brief Data loaded from the HDRi image file.
         */
        unsigned char* _hdriImageData;

        /**
         * @brief Width and height of the loaded HDRi image.
         */
        VkExtent2D _hdriSizePixels;

        /**
         * @brief Width and height of each face's image.
         */
        int _faceSizePixels;

        /**
         * @brief Vulkan handle to the cube map image used in the shaders. This image is meant to contain all the cube map's images as a serialized array of pixels.
         * In order to know where, in the array of pixels, each image starts/ends and what format it's in, a sampler and image view are used.
         */
        Vulkan::Image _cubeMapImage;

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
        void LoadFromSphericalHDRI(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, std::filesystem::path imageFilePath);

        /**
         * @brief Writes each cube map face's image to 6 separate files.
         * @param absoluteFolderPath Folder in which each of the 6 images of the cube map will be written. Defaults to the current working directory
         * of the executable running the code.
         */
        void WriteImagesToFiles(std::filesystem::path absoluteFolderPath);

        /**
         * @brief Serializes the data of all the faces' images and returns a vector that contains all images, in this specific order:
         * right, left, upper, lower, front, back.
         */
        std::vector<unsigned char> Serialize();

        /**
         * @brief See IPipelineable.
         */
        virtual void CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& graphicsQueue) override;

        /**
         * @brief See IPipelineable.
         */
        virtual void UpdateShaderResources() override;

    private:

        /**
         * @brief Internal method for code-shortening.
         */
        void GenerateFaceImage(CubeMapFace face);
    };
}

