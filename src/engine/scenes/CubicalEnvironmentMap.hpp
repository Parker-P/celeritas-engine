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
        std::vector<unsigned char*> _front;

        /**
         * @brief Right face data.
         */
        std::vector<unsigned char*> _right;

        /**
         * @brief Back face data.
         */
        std::vector<unsigned char*> _back;

        /**
         * @brief Left face data.
         */
        std::vector<unsigned char*> _left;

        /**
         * @brief Upper face data.
         */
        std::vector<unsigned char*> _upper;

        /**
         * @brief Lower face data.
         */
        std::vector<unsigned char*> _lower;

        /**
         * @brief Data loaded from the HDRi image file.
         */
        unsigned char* _hdriImageData;

        /**
         * @brief Width and height of the loaded HDRi image.
         */
        VkExtent2D _hdriSizePixels;

        /**
         * @brief Width and height of each face's image in pixels.
         */
        int _faceSizePixels;

        /**
         * @brief Vulkan handle to the cube map image used in the shaders. This image is meant to contain all the cube map's images as a serialized array of pixels.
         * In order to know where, in the array of pixels, each image starts/ends and what format it's in, a sampler and image view are used.
         */
        Vulkan::Image _cubeMapImage;

        /**
         * @brief Number of mipmaps each face has.
         */
        int _mipmapCount;

        /**
         * @brief Default constructor.
         */
        CubicalEnvironmentMap() = default;

        std::vector<unsigned char*>& operator[](const CubeMapFace& face) {
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

        /**
         * Loads an environment map from a spherical HDRi file and converts it to a cubical map.
         * 
         * @param physicalDevice
         * @param logicalDevice
         * @param imageFilePath
         * @param mipmapCount The mipmap count for each cubemap's face, used to simulate material roughness. The higher the number, the higher the quality
         * of the mipmap generation, but the higher the time needed to pre-compute the blurred mipmaps.
         */
        void LoadFromSphericalHDRI(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, std::filesystem::path imageFilePath, int mipmapCount = 0);

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
         * @brief Generates blurred mipmaps for the given image, and returns an array of pointers to the start of each blurred image's data.
         * @param sourceImageData
         */
        std::vector<unsigned char*> GenerateBlurredMipmaps(Vulkan::PhysicalDevice physicalDevice, VkDevice logicalDevice, Utils::BoxBlur& blurrer, unsigned char* sourceImage, int mipmapCount);

        /**
         * @brief Internal method for code-shortening.
         */
        void GenerateFaceImage(CubeMapFace face, std::vector<unsigned char>& outImageData);
    };
}

