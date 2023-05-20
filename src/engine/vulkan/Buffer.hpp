#pragma once

namespace Engine::Vulkan
{
    /**
     * @brief Buffers represent linear arrays of data which are used for
     * various purposes by binding them to a graphics or compute pipeline via descriptor
     * sets or via certain commands, or by directly specifying them as parameters to certain commands.
     */
    class Buffer
    {
    public:

        /**
         * @brief The handle used by Vulkan to identify this buffer.
         */
        VkBuffer _handle;

        /**
         * @brief Like an image view, a buffer view tells Vulkan how to interpret the data inside of the buffer.
         * This is particularly useful for things like a texture buffer, which could contain data about an image 
         * in any format. For example the image might use 3 or 4 color channels (RGB or RGBA), or it could use
         * use floating point numbers, doubles or regular integers as its pixel data.
         */
        VkBufferView _viewHandle;

        /**
         * @brief Default constructor.
         */
        Buffer() = default;

        /**
         * @brief Constructs a buffer.
         * @param logicalDevice Needed by Vulkan to have info about the GPU so it can make function calls to allocate memory for the buffer.
         * @param physicalDevice Needed to know which types of memory are available on the GPU, so the buffer can be allocated on the
         * correct heap (a.k.a portion of VRAM or RAM).
         * @param usageFlags This tells Vulkan what's the buffer's intended use.
         * @param properties This can be any of the following values:
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
         * @param data Pointer to the start address of the data you want to copy to the buffer.
         * @param sizeInBytes Size in bytes of the data you want to copy to the buffer.
         * @param dataFormat Format/layout of the data. For example, if the buffer is meant to contain image data, it might use 3 or 4 color channels (RGB or RGBA), or it 
         * could use floating point numbers, or regular integers as its pixel data. For instance, VK_FORMAT_R32G32B32_SFLOAT describes a buffer that contains 3 consecutive 
         * blocks (R32G32B32) of 32 bits each, and that each 32 bit block represents a Single precision FLOATing point number.
         */
        Buffer(VkDevice& logicalDevice, PhysicalDevice& physicalDevice, VkBufferUsageFlagBits usageFlags, VkMemoryPropertyFlagBits properties, void* pData = nullptr, size_t sizeInBytes = 0, const VkFormat& dataFormat = VK_FORMAT_MAX_ENUM);

        /**
         * @brief Generates a data structure that Vulkan calls descriptor, which it uses to bind the buffer to a descriptor set.
         */
        VkDescriptorBufferInfo GenerateDescriptor();

        /**
         * @brief Updates the data contained in this buffer. Notes: buffer must be marked as VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT and its
         * memory mapped to the _pBufferData member for this to work. To update a GPU only buffer, you need to use a staging buffer and
         * submit a buffer transfer command to a queue, then have Vulkan execute the command, done by the SendToGPU member function.
         * @param data Address of where the data begins in memory.
         * @param sizeInBytes Size of the data in bytes.
         */
        void UpdateData(void* pData, size_t sizeInBytes);

        /**
         * @brief Sends the buffer to the GPU's VRAM. Will only work if buffer usage is marked as VK_BUFFER_USAGE_TRANSFER_DST_BIT and properties
         * marked as VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT.
         * @param commandPool Command pool used to allocate a command buffer that will contain the commands to send the buffer to VRAM.
         * @param queue Queue that will contain the allocated transfer command buffer.
         */
        void SendToGPU(VkCommandPool& commandPool, Queue& queue);

        /**
         * @brief Destroys the buffer and frees any memory allocated to it.
         **/
        void Destroy();

        /**
         * @brief Logical device used to perform Vulkan calls.
         */
        VkDevice _logicalDevice;

        /**
         * @brief Used to query for GPU hardware properties, to know where the buffer can and cannot be stored in memory.
         */
        PhysicalDevice _physicalDevice;

        /**
         * @brief Contains information about the allocated memory for the buffer which, depending on the memory type flags
         * passed at construction, can be allocated in the GPU (VRAM) or in the RAM.
         */
        VkDeviceMemory _memory;

        /**
         * @brief Vulkan buffers have a portion of memory allocated all for themselves, detached from the data you want to copy to them.
         * This is the starting memory address of the data inside the allocated buffer. This pointer is only assigned to if the buffer
         * is marked as VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, otherwise it's nullptr, as it would likely be an address on VRAM which the CPU
         * cannot access without passing through Vulkan calls.
         */
        void* _pBufferData;

        /**
         * @brief Vulkan buffers have a portion of memory allocated all for themselves, detached from the data you want to copy to them.
         * This is the size, in bytes, of the portion of memory allocated specifically for the buffer.
         */
        size_t _bufferDataSize;

        /**
         * @brief Vulkan buffers have a portion of memory allocated all for themselves, detached from the data you want to copy to them.
         * This is the starting memory address of the data that was to be copied to the allocated buffer. Whether the data has been actually
         * copied into the buffer or not depends on whether _pMappedData is nullptr or not. If _pMappedData has not been assigned, there is
         * no memory allocated for the buffer that is visible to the CPU, thus no CPU-writable memory has been created. This means that the
         * buffer is probably meant to reside in VRAM. To send a buffer to VRAM, use the SendToGPU() member function.
         */
        void* _pData;

        /**
         * @brief The format of the data contained in the buffer. This describes the data's layout.
         */
        VkFormat _dataFormat = VK_FORMAT_MAX_ENUM;

        /**
         * @brief See constructor description.
         */
        VkBufferUsageFlagBits _usageFlags;

        /**
         * @brief See constructor description.
         */
        VkMemoryPropertyFlagBits _propertiesFlags;
    };
}
