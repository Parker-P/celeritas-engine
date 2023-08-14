#pragma once

namespace Engine::Vulkan
{
    /**
     * @brief Represents a Vulkan image. A Vulkan image uses 2 structures, 1 for storing the data the image contain (VkImage),
     * and 1 for decorating that data with metadata (VkImageView) that Vulkan can use in the graphics pipeline to know how to read it.
     */
    class Image
    {
        /**
         * @brief See Vulkan specification.
         */
        VkDevice _logicalDevice;

        /**
         * @brief Physical device.
         */
        PhysicalDevice _physicalDevice;

        /**
         * @brief Gets the size of a pixel of the image using its format.
         * @param format How the image is stored in memory.
         * @return The size of a pixel in bytes if format is valid, 0 otherwise.
         */
        size_t GetPixelSizeBytes(const VkFormat& format);


    public:

        /**
         * @brief To easily identify the image.
         */
        std::string _name = "";

        /**
         * @brief Handle that identifies a structure that contains the raw image data.
         */
        VkImage _imageHandle;

        /**
         * @brief Handle to the image view. An image view defines how the image's data is accessed,
         * or processed within the shader or pipeline stages. The image view also contains a reference
         * to the VkImage handle. This comment from a reddit user gives an idea of what an image view is used for:
         * Say you have an image which is actually a fairly large atlas of many individual images.
         * You could use an image view to represent a single mip level, or maybe a small region of the atlas, or both,
         * or the whole thing. The point of the abstraction is it allows you to alias the access of an image without
         * needing to muck with the image itself. Think of it as a lens for viewing the image.
         * If you're familiar with C++, this is similar to the string_view concept where you act on ranges of the
         * string without tampering with the source data itself.
         */
        VkImageView _viewHandle;

        /**
         * @brief The way the data for each pixel of the image is stored in memory.
         */
        VkFormat _format;

        /**
         * @brief Width, height and depth (for 3D images) of the image in pixels.
         */
        VkExtent3D _sizePixels;

        /**
         * @brief Image size in bytes.
         */
        size_t _sizeBytes;

        /**
         * @brief Flags that indicate what type of data this image contains.
         */
        VkImageAspectFlagBits _typeFlags;

        /**
         * @brief This is a structure used to specify a subrange of an image's subresources that will be accessed or manipulated. It defines which aspects, mipmap levels, array layers, and usage scopes are included within the range.
         * The VkImageSubresourceRange structure includes the following fields:
         *
         * VkImageAspectFlags aspectMask:
         *
         * Specifies the aspects of the image to be included in the subresource range.
         * Aspects can include color, depth, or stencil components of the image.
         * uint32_t baseMipLevel and uint32_t levelCount:
         *
         * baseMipLevel indicates the starting mipmap level to be included in the subresource range.
         * levelCount specifies the number of mipmap levels to be included in the range.
         * Mipmaps are lower-resolution versions of the original image.
         * uint32_t baseArrayLayer and uint32_t layerCount:
         *
         * baseArrayLayer denotes the starting array layer to be included in the subresource range.
         * layerCount specifies the number of array layers to be included in the range.
         * Array layers can represent individual 2D image slices or cubemap faces.
         */
        VkImageSubresourceRange _subresourceRange{};

        /**
         * @brief Describes how this image is going to be read by the physical GPU texture samplers, which feed textures to shaders.
         */
        VkSampler _sampler;

        /**
         * @brief  This can be any of the following values:
         * 1) VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT: this means GPU memory, so VRAM. If this is not set, then regular RAM is assumed.
         * 2) VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT: this means that the CPU will be able to read and write from the allocated memory IF YOU CALL vkMapMemory() FIRST.
         * 3) VK_MEMORY_PROPERTY_HOST_CACHED_BIT: this means that the memory will be cached so that when the CPU writes to this buffer, if the data is small enough to fit in its
         * cache (which is much faster to access) it will do that instead. Only problem is that this way, if your GPU needs to access that data, it won't be able to unless it's
         * also marked as HOST_COHERENT.
         * 4) VK_MEMORY_PROPERTY_HOST_COHERENT_BIT: this means that anything that the CPU writes to the buffer will be able to be read by the GPU as well, effectively
         * granting the GPU access to the CPU's cache (if the buffer is also marked as HOST_CACHED). COHERENT stands for consistency across memories, so it basically means
         * that the CPU, GPU or any other device will see the same memory if trying to access the buffer. If you don't have this flag set, and you try to access the
         * buffer from the GPU while the buffer is marked HOST_CACHED, you may not be able to get the data or even worse, you may end up reading the wrong chunk of memory.
         */
        VkMemoryPropertyFlagBits _memoryPropertiesFlags;

        /**
         * @brief Raw data of the image.
         */
        void* _pData = nullptr;

        /**
         * @brief True if the image has successfully been copied to the memory readable by your GPU.
         */
        bool _sentToGPU = false;

        Image() = default;

        /**
         * @brief Constructs a Vulkan image.
         * @param logicalDevice
         * @param physicalDevice
         * @param imageFormat
         * @param sizePixels Width, height and width of the image in pixels.
         * @param data Pointer to where in memory the data for the image is stored. Use nullptr if the image is intended to be a renderpass attachment.
         * @param usageFlags Gives Vulkan information about how it's going to be used. For example an image could be used to store depth information, denoted
         * by using VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, or as an image that can be read and sampled by shaders with VK_IMAGE_USAGE_SAMPLED_BIT.
         * @param typeFlags By using these flags, Vulkan allows for fine-grained control over which aspects of an image are involved in operations, ensuring efficient and 
         * accurate utilization of image data. For example VK_IMAGE_ASPECT_COLOR_BIT specifies the color aspect of an image. It represents the image's color data and can 
         * be used for color rendering or sampling operations.
         * @param memoryPropertiesFlags This can be any of the following values:
         * 1) VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT: this means GPU memory, so VRAM. If this is not set, then regular RAM is assumed.
         * 2) VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT: this means that the CPU will be able to read and write from the allocated memory IF YOU CALL vkMapMemory() FIRST.
         * 3) VK_MEMORY_PROPERTY_HOST_CACHED_BIT: this means that the memory will be cached so that when the CPU writes to this buffer, if the data is small enough to fit in its
         * cache (which is much faster to access) it will do that instead. Only problem is that this way, if your GPU needs to access that data, it won't be able to unless it's
         * also marked as HOST_COHERENT.
         * 4) VK_MEMORY_PROPERTY_HOST_COHERENT_BIT: this means that anything that the CPU writes to the buffer will be able to be read by the GPU as well, effectively
         * granting the GPU access to the CPU's cache (if the buffer is also marked as HOST_CACHED). COHERENT stands for consistency across memories, so it basically means
         * that the CPU, GPU or any other device will see the same memory if trying to access the buffer. If you don't have this flag set, and you try to access the
         * buffer from the GPU while the buffer is marked HOST_CACHED, you may not be able to get the data or even worse, you may end up reading the wrong chunk of memory.
         * Further read: https://asawicki.info/news_1740_vulkan_memory_types_on_pc_and_how_to_use_them
         * @param arrayLayerCount An image can have multiple array layers, allowing for the storage and access of different images or slices within a single image object.
         * Each array layer represents a separate 2D image that can be individually addressed or accessed. These array layers are useful for various purposes, such as storing
         * texture array data, managing cubemaps, or implementing layered rendering techniques.
         * @param creationFlags Special flags that tell the shaders how the image can be used. For example the VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT flag signals to the shader that
         * the image can be used as a cube map, or the VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT that indicates that allows the image to be used as a source or destination for
         * operations involving 2D array textures.
         * @param imageViewType Specifies the type of image view. An image view defines how the image's data is accessed, or processed within the shader or pipeline stages.
         */
        Image(VkDevice& logicalDevice,
            PhysicalDevice& physicalDevice,
            const VkFormat& imageFormat,
            const VkExtent3D& sizePixels,
            void* data,
            const VkImageUsageFlagBits& usageFlags,
            const VkImageAspectFlagBits& typeFlags,
            const VkMemoryPropertyFlagBits& memoryPropertiesFlags,
            const uint32_t& arrayLayerCount = 1,
            const VkImageCreateFlags& creationFlags = 0,
            const VkImageViewType& imageViewType = VK_IMAGE_VIEW_TYPE_2D);

        /**
         * @brief Constructs an image using a pre-existing image handle.
         * @param image The pre-existing image handle.
         * @param imageFormat
         * @param size
         */
        Image(VkDevice& logicalDevice, VkImage& image, const VkFormat& imageFormat);

        /**
         * @brief Sends the image to the GPU.
         * @param commandPool Command pool used to allocate a command buffer that will contain the commands to send the image to VRAM.
         * @param queue Queue that will contain the allocated transfer command buffer.
         * @param bufferCopyRegions When sending data to the GPU, a temporary CPU-only buffer is used to store the data to be transferred, for many reasons.
         * This structure is used to describe regions of data transfer between the temporary buffer and the image in VRAM. 
         * It specifies how portions of the temporary transfer buffer are mapped to a specific regions within the image during the data transfer.
         * In short: it is just a structure to tell Vulkan how to direct the data in the temporary buffer to the right place when copying it to the image.
         */
        void SendToGPU(VkCommandPool& commandPool, Queue& queue, const std::vector<VkBufferImageCopy>& bufferCopyRegions = { VkBufferImageCopy{} });

        /**
         * @brief Generates an image descriptor. An image descriptor is bound to a sampler, which tells Vulkan how to instruct
         * the actual GPU hardware samplers on how to read and sample the particular texture.
         * 
         * @param filteringMode Texture filtering is a parameter used by the shader when given the instruction to read the color of a texture at a specific UV coordinate. 
         * Positions on a texture are identified by integer values (pixel coordinates) whereas the texture() function in a shader takes float values. 
         * Say we have a 2x2 pixel image: if we sample from UV (0.35, 0.35) with origin at the bottom left corner, the texture() function will have to 
         * give back the color of the bottom left pixel, because UV coordinates (0.35, 0.35) fall in the bottom left pixel of our 2x2 image. However, 
         * provided that the image has only 1 sample per pixel, the exact center of that pixel is represented by UV coordinates (0.25, 0.25). UV coordinates 
         * (0.35, 0.35) are skewed towards the upper right of our bottom left pixel in our 2x2 image, so the texture() function uses the filtering parameter 
         * to determine how to calculate the color it gives back. In the case of linear filtering, the color the texture() function gives back will be a blend 
         * of the 4 closest pixels, weighted by how close the input coordinate (0.35, 0.35) is to each pixel, represented in this case by VK_FILTER_LINEAR.
         * 
         * @param addressMode This indicates how the sampler is going to behave when it receives coordinates that are out of the 0-1 UV range. For example,
         * VK_SAMPLER_ADDRESS_MODE_REPEAT will cause the sampler to give back the color of the texture at UV coordinates (0.25, 0.25) when given coordinates (1.25, 1.25), 
         * which mimics the textures being placed side by side, hence the REPEAT suffix.
         * 
         * @param anisotropyLevel Anisotropy is another, more advanced filtering technique that is most effective when the surface onto which the texture being sampled is at
         * a steep angle. It's aimed at preserving sharp features of textures, and maintaining visual fidelity when sampling textures at steep angles.
         * The word anisotropy comes from Greek and literally means "not the same angle".
         * The algorithm for anisotropic filtering looks like the following:
         * 
         * 1) We first need a way to know the angle of the texture. This could be easily achieved by passing the normal vector on from the vertex shader to
         * the fragment shader, but in most cases the derivatives of the texture are used. Texture derivatives represent the rate of change of texture
         * coordinates relative to the position of the pixel being rendered on-screen. To understand this, imagine you are rendering a plane that is
         * very steeply angled from your view, so steeply angled that you can only see 2 pixels, so it looks more like a line (but is infact the plane
         * being rendered almost from the side). If we take the texture coordinate of the 2 neighbouring pixels (on the shorter side) we will get the 2
         * extremes of the UV space. By comparing the rate of change of the on-screen pixel coordinates and the resulting texture coordinates, the shaders
         * can calculate the derivative that will be needed to sample around the main sampling position. In our extreme case, a 1 pixel change in position on screen
         * results in the 2 extremes of the UV space in the corresponding texture coordinates. This means that the magnitude of the derivative is at the
         * maximum it can be for one of the axes.
         * 
         * 2) Now that we have the magnitude of the derivative (the rate of change of the texture coordinates relative to the on-screen pixel coordinates)
         * we can start sampling around the color at the UV coordinate. In anisotropic filtering, the sampling is typically done with an ellyptical pattern
         * or with a cylindrical pattern. In the ellyptical pattern, the samples are taken around the UV coordinate following the line created by an
         * imaginary ellyptical circumference around the sample point. The number of samples will increase if the texture is on a very steeply-angled
         * surface. That's why we needed the angle of the texture, to guide the amount of samples being taken around the main sampling position.
         * 
         * 3) At this point we can calculate a weighted average of all colors that have been sampled around the main sampling position.
         * 
         * @param minLod Represents the minimum LOD (or mipmap). The fractional part of the floating-point values represents interpolation between adjacent 
         * mipmap levels. For example, if you set minLod to 1.5, the sampler may perform texture sampling by interpolating between LOD 1 and LOD 2, blending 
         * the textures from these two levels to get an intermediate level of detail.
         * 
         * @param maxLod Represents the minimum LOD (or mipmap).
         * 
         * @param mipMapMode VK_SAMPLER_MIPMAP_MODE_NEAREST: This mode specifies that the nearest mipmap level should be selected for sampling. 
         * When using this mode, the sampler will not perform any interpolation between mipmaps. Instead, it will directly use the texel data from the nearest 
         * mipmap level to the desired LOD (Level of Detail). This mode is useful when you want a sharp, blocky appearance for textures at different viewing 
         * distances or when using pixel art textures.
         * 
         * VK_SAMPLER_MIPMAP_MODE_LINEAR: This mode specifies that linear interpolation should be performed between mipmaps when sampling. The sampler will 
         * use a weighted average of texel data from two adjacent mipmap levels to obtain the final sampled color. Linear mipmap filtering provides smoother 
         * transitions between LODs and is commonly used to improve visual quality when textures are viewed at varying distances.
         */
        VkDescriptorImageInfo GenerateDescriptor(const VkFilter& filteringMode = VK_FILTER_NEAREST,
            const VkSamplerAddressMode& addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            const float& anisotropyLevel = 0.0f,
            const float& minLod = 0.0f,
            const float& maxLod = 0.0f,
            const VkSamplerMipmapMode& mipMapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST);

        /**
         * @brief Creates a 1x1 pixels image with the one pixel being of the color specified with the red, blue, green and alpha values provided.
         * @param logicalDevice Needed to create the image with Vulkan calls.
         * @param physicalDevice Needed to create the image with Vulkan calls.
         * @param red Red channel value, domain is 0-255.
         * @param green Green channel value, domain is 0-255.
         * @param blue Blue channel value, domain is 0-255.
         * @param alpha Alpha channel value, domain is 0-255.
         * @return
         */
        static Image SolidColor(VkDevice& logicalDevice, 
            PhysicalDevice& physicalDevice, 
            const unsigned char& red, 
            const unsigned char& green, 
            const unsigned char& blue, 
            const unsigned char& alpha);

        /**
         * @brief Uses Vulkan calls to deallocate and remove the contents of the image from memory.
         */
        void Destroy();
    };
}
