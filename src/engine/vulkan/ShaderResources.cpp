#include <vector>
#include <string>
#include <iostream>
#include <optional>
#include <map>
#include <optional>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/detail/type_vec.hpp>
#include <vulkan/vulkan.h>
#include "engine/vulkan/Queue.hpp"
#include "engine/vulkan/PhysicalDevice.hpp"
#include "engine/vulkan/Image.hpp"
#include "engine/vulkan/Buffer.hpp"
#include "engine/vulkan/ShaderResources.hpp"
#include "engine/structural/IPipelineable.hpp"

namespace Engine::Vulkan
{
    Descriptor::Descriptor(const VkDescriptorType& type, const uint32_t& bindingNumber, const Buffer& buffer, const Image& image)
    {
        if (buffer._handle == VK_NULL_HANDLE || image._imageHandle == VK_NULL_HANDLE) {
            std::cout << "both the image and the buffer must have a valid handle and be fully initialized via a call to vkCreateImage (for the image) and vkCreateBuffer (for the buffer)" << std::endl;
            return;
        }

        _buffer = buffer;
        _image = image;
        _bufferInfo = _buffer.value().GenerateDescriptor();
        _imageInfo = _image.value().GenerateDescriptor();
        _type = type;
        _bindingNumber = bindingNumber;
    }

    Descriptor::Descriptor(const VkDescriptorType& type, const uint32_t& bindingNumber, const Buffer& buffer)
    {
        if (buffer._handle == VK_NULL_HANDLE) {
            std::cout << "the buffer must have a valid handle and be fully initialized via a call to vkCreateBuffer" << std::endl;
            return;
        }

        _buffer = buffer;
        _bufferInfo = _buffer.value().GenerateDescriptor();
        _type = type;
        _bindingNumber = bindingNumber;
    }

    Descriptor::Descriptor(const VkDescriptorType& type, const uint32_t& bindingNumber, const Image& image)
    {
        if (image._imageHandle == VK_NULL_HANDLE) {
            std::cout << "the image must have a valid handle and be fully initialized via a call to vkCreateImage" << std::endl;
            return;
        }

        _image = image;
        _imageInfo = _image.value().GenerateDescriptor();
        _type = type;
        _bindingNumber = bindingNumber;
    }

    void DescriptorSet::SendToGPU()
    {
        std::vector<VkWriteDescriptorSet> writeInfos;
        bool canUpdate = true;

        for (auto& descriptor : _descriptors) {
            if (descriptor._bufferInfo.has_value() || descriptor._imageInfo.has_value()) {
                VkWriteDescriptorSet writeInfo = {};
                writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeInfo.dstSet = _handle;
                writeInfo.descriptorCount = 1;
                writeInfo.descriptorType = descriptor._type;
                writeInfo.pBufferInfo = descriptor._bufferInfo.has_value() ? &descriptor._bufferInfo.value() : writeInfo.pBufferInfo;
                writeInfo.pImageInfo = descriptor._imageInfo.has_value() ? &descriptor._imageInfo.value() : writeInfo.pImageInfo;
                writeInfo.dstBinding = descriptor._bindingNumber;
                writeInfo.pTexelBufferView = descriptor._buffer.has_value() ? descriptor._buffer.value()._viewHandle != nullptr ? &descriptor._buffer.value()._viewHandle : nullptr : nullptr;
                writeInfos.push_back(writeInfo);
            }
            else {
                canUpdate = false;
            }
        }

        if (canUpdate) {
            vkUpdateDescriptorSets(_logicalDevice, (uint32_t)writeInfos.size(), writeInfos.data(), 0, nullptr);
        }
    }

    DescriptorSet::DescriptorSet(VkDevice& logicalDevice, const VkShaderStageFlagBits& shaderStageFlags, std::vector<Descriptor> descriptors)
    {
        _logicalDevice = logicalDevice;
        _indexNumber = -1;
        _descriptors = descriptors;
        std::vector<VkDescriptorSetLayoutBinding> bindings;

        // Prep work for the descriptor set layout.
        for (auto& descriptor : descriptors) {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = descriptor._bindingNumber;
            binding.descriptorType = descriptor._type;
            binding.descriptorCount = 1;
            binding.stageFlags = shaderStageFlags;
            bindings.push_back(binding);
        }

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.bindingCount = (uint32_t)bindings.size();
        descriptorSetLayoutCreateInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(logicalDevice, &descriptorSetLayoutCreateInfo, nullptr, &_layout) != VK_SUCCESS) {
            std::cerr << "failed to create descriptor set layout" << std::endl;
            exit(1);
        }
        else {
            std::cout << "created descriptor set layout" << std::endl;
        }
    }

    void DescriptorPool::AllocateDescriptorSets()
    {
        // The amount of sets and descriptor types is defined when creating the descriptor pool.
        std::vector<VkDescriptorSetLayout> layouts;

        for (auto& descriptorSet : *_descriptorSets) {
            layouts.push_back(descriptorSet._layout);
        }

        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = _handle;
        allocInfo.descriptorSetCount = (uint32_t)_descriptorSets->size();
        allocInfo.pSetLayouts = layouts.data();

        std::vector<VkDescriptorSet> allocatedDescriptorSetHandles(_descriptorSets->size());

        if (vkAllocateDescriptorSets(_logicalDevice, &allocInfo, allocatedDescriptorSetHandles.data()) != VK_SUCCESS) {
            std::cerr << "failed to allocate descriptor sets" << std::endl;
            exit(1);
        }
        else {
            std::cout << "allocated descriptor sets" << std::endl;

            for (int i = 0; i < allocatedDescriptorSetHandles.size(); ++i) {
                (*_descriptorSets)[i]._handle = allocatedDescriptorSetHandles[i];
            }
        }
    }

    void DescriptorPool::SendDescriptorSetDataToGPU() {
        for (auto& descriptorSet : *_descriptorSets) {
            descriptorSet.SendToGPU();
        }
    }

    DescriptorPool::DescriptorPool(VkDevice& logicalDevice, std::vector<DescriptorSet>& descriptorSets)
    {
        _descriptorSets = &descriptorSets;
        _logicalDevice = logicalDevice;

        // Find the total amount of each descriptor type present in each descriptor of each descriptor set.
        // Vulkan needs this probably because when it needs to allocate memory, it needs to know the type (which 
        // implies its size in bytes) and how many of them there are.
        std::map<VkDescriptorType, int> typeCounts;

        for (auto& descriptorSet : descriptorSets) {
            for (auto& descriptor : descriptorSet._descriptors) {
                if (descriptor._type != VK_DESCRIPTOR_TYPE_MAX_ENUM) {
                    auto existingEntry = typeCounts.find(descriptor._type);

                    if (existingEntry != typeCounts.end()) {
                        typeCounts[existingEntry->first] += 1;
                    }
                    else {
                        typeCounts.emplace(descriptor._type, 1);
                    }
                }
            }
        }

        std::vector<VkDescriptorPoolSize> poolSizes;

        for (auto i = typeCounts.begin(); i != typeCounts.end(); ++i) {
            VkDescriptorPoolSize poolSize{};
            poolSize.type = i->first;
            poolSize.descriptorCount = (uint32_t)i->second;
            poolSizes.push_back(poolSize);
        }

        // maxSets is the maximum number of descriptor sets that can be allocated from the pool.
        // poolSizeCount is the number of elements in pPoolSizes.
        // pPoolSizes is a pointer to an array of VkDescriptorPoolSize structures, each containing a descriptor type and number of descriptors of that type to be allocated in the pool.
        VkDescriptorPoolCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        createInfo.maxSets = (uint32_t)descriptorSets.size();
        createInfo.poolSizeCount = (uint32_t)poolSizes.size();
        createInfo.pPoolSizes = poolSizes.data();

        if (vkCreateDescriptorPool(logicalDevice, &createInfo, nullptr, &_handle) != VK_SUCCESS) {
            std::cerr << "failed to create descriptor pool" << std::endl;
            exit(1);
        }
        else {
            std::cout << "created descriptor pool" << std::endl;
        }

        AllocateDescriptorSets();
        SendDescriptorSetDataToGPU();
    }
}