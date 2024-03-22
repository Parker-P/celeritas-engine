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

	class Image
	{
	public:
		VkImageCreateInfo _createInfo{};
		VkImageViewCreateInfo _viewCreateInfo{};
		VkSamplerCreateInfo _samplerCreateInfo{};

		VkImage _image{};
		VkImageView _view{};
		VkSampler _sampler{};

		VkImageLayout _currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	};

	class Buffer
	{
	public:
		VkBufferCreateInfo _createInfo{};
		VkBufferViewCreateInfo _viewCreateInfo{};

		VkBuffer _buffer{};
		VkBufferView _view{};

		VkDeviceMemory _gpuMemory = nullptr;
		void* _cpuMemory = nullptr;
	};

	/**
	 * @brief Represents a cubical environment map, used as an image-based light source
	 * in the shaders.
	 */
	class CubicalEnvironmentMap : public Structural::IPipelineable
	{

	public:

		/**
		 * @brief Front face data, including all mipmaps.
		 */
		std::vector<std::vector<unsigned char>> _front;

		/**
		 * @brief Right face data, including all mipmaps.
		 */
		std::vector<std::vector<unsigned char>> _right;

		/**
		 * @brief Back face data, including all mipmaps.
		 */
		std::vector<std::vector<unsigned char>> _back;

		/**
		 * @brief Left face data, including all mipmaps.
		 */
		std::vector<std::vector<unsigned char>> _left;

		/**
		 * @brief Upper face data, including all mipmaps.
		 */
		std::vector<std::vector<unsigned char>> _upper;

		/**
		 * @brief Lower face data, including all mipmaps.
		 */
		std::vector<std::vector<unsigned char>> _lower;

		/**
		 * @brief Data loaded from the HDRi image file.
		 */
		unsigned char* _hdriImageData = 0;

		/**
		 * @brief Width and height of the loaded HDRi image.
		 */
		VkExtent2D _hdriSizePixels{};

		/**
		 * @brief Width and height of each face's image.
		 */
		int _faceSizePixels = 0;

		/**
		 * @brief Vulkan handle to the cube map image used in the shaders. This image is meant to contain all the cube map's images as a serialized array of pixels.
		 * In order to know where, in the array of pixels, each image starts/ends and what format it's in, a sampler and image view are used.
		 */
		Image _cubeMapImage{};

		/**
		 * @brief Number of mipmaps. This is dynamically caluclated.
		 */
		int _mipmapCount = 0;

		/**
		 * @brief Default constructor.
		 */
		CubicalEnvironmentMap() = default;

		/**
		 * @brief Constructor.
		 */
		CubicalEnvironmentMap(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice);

		/**
		 * Loads an environment map from a spherical HDRi file and converts it to a cubical map.
		 *
		 * @param physicalDevice
		 * @param logicalDevice
		 * @param imageFilePath
		 * @param mipmapCount The mipmap count for each cubemap's face, used to simulate material roughness. The higher the number, the higher the quality
		 * of the mipmap generation, but the higher the time needed to pre-compute the blurred mipmaps.
		 */
		void LoadFromSphericalHDRI(std::filesystem::path imageFilePath);

		void CopyFacesToImage(VkDevice logicalDevice, Vulkan::PhysicalDevice& physicalDevice, VkCommandPool commandPool, VkCommandBuffer commandBuffer, Vulkan::Queue& queue);

		/**
		 * @brief Writes each cube map face's image to 6 separate files.
		 * @param absoluteFolderPath Folder in which each of the 6 images of the cube map will be written. Defaults to the current working directory
		 * of the executable running the code.
		 */
		void WriteImagesToFiles(std::filesystem::path absoluteFolderPath);

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
		 * @brief Physical device.
		 */
		Vulkan::PhysicalDevice _physicalDevice{};

		/**
		 * @brief Logical device.
		 */
		VkDevice _logicalDevice = nullptr;

		/**
		 * @brief Internal method for code-shortening.
		 */
		std::vector<unsigned char> GenerateFaceImage(CubeMapFace face);
	};
}

