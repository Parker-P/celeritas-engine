#include <iostream>
#include <algorithm>
#include <vector>
#include <vulkan/vulkan.h>

#include "../src/engine/core/app_config.h"
#include "window_surface.h"
#include "logical_device.h"
#include "physical_device.h"
#include "swap_chain.h"

namespace Engine::Core::VulkanEntities {
	VkSurfaceFormatKHR SwapChain::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
		
		// We can either choose any format
		if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
			return{ VK_FORMAT_R8G8B8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
		}

		// Or go with the standard format - if available
		for (const auto& available_surface_format : availableFormats) {
			if (available_surface_format.format == VK_FORMAT_R8G8B8A8_UNORM) {
				return available_surface_format;
			}
		}

		// Or fall back to the first available one
		return availableFormats[0];
	}

	VkExtent2D SwapChain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surface_capabilities, const AppConfig& app_config) {
		
		//Choose the size of the images in the swapchain based on viewport width and size, and device output image capabilities
		if (surface_capabilities.currentExtent.width == -1) {
			VkExtent2D swapchain_extent = {};
			swapchain_extent.width = std::min(std::max(app_config.settings_.width_, surface_capabilities.minImageExtent.width), surface_capabilities.maxImageExtent.width);
			swapchain_extent.height = std::min(std::max(app_config.settings_.height_, surface_capabilities.minImageExtent.height), surface_capabilities.maxImageExtent.height);
			return swapchain_extent;
		}
		else {
			return surface_capabilities.currentExtent;
		}
	}

	VkPresentModeKHR SwapChain::ChoosePresentMode(const std::vector<VkPresentModeKHR> presentModes) {
		
		//Look for mailbox presentmode (triple buffering mode)
		for (const auto& presentMode : presentModes) {
			if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return presentMode;
			}
		}

		//If mailbox is unavailable, fall back to FIFO (guaranteed to be available)
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	void SwapChain::CreateImageViews(LogicalDevice& logical_device) {
		
		//Create an image view for every image in the swap chain. An image view is an image descriptor, it's just metadata that describes
		//the image (such as knowing what format it is, the type etc...)
		swap_chain_image_views_.resize(swap_chain_images_.size());
		for (size_t i = 0; i < swap_chain_images_.size(); i++) {
			VkImageViewCreateInfo create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			create_info.image = swap_chain_images_[i];
			create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			create_info.format = swap_chain_format_;
			create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			create_info.subresourceRange.baseMipLevel = 0;
			create_info.subresourceRange.levelCount = 1;
			create_info.subresourceRange.baseArrayLayer = 0;
			create_info.subresourceRange.layerCount = 1;
			if (vkCreateImageView(logical_device.GetLogicalDevice(), &create_info, nullptr, &swap_chain_image_views_[i]) != VK_SUCCESS) {
				std::cerr << "failed to create image view for swap chain image #" << i << std::endl;
				exit(1);
			}
		}
		std::cout << "created image views for swap chain images" << std::endl;
	}

	void SwapChain::CreateSwapChain(PhysicalDevice& physical_device, LogicalDevice& logical_device, WindowSurface& window_surface, AppConfig& app_config) {
		
		//Find supported surface formats for the swap chain's images
		uint32_t format_count;
		if (vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device.GetPhysicalDevice(), window_surface.GetWindowSurface(), &format_count, nullptr) != VK_SUCCESS || format_count == 0) {
			std::cerr << "failed to get number of supported surface formats" << std::endl;
			exit(1);
		}
		std::vector<VkSurfaceFormatKHR> surface_formats(format_count);
		if (vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device.GetPhysicalDevice(), window_surface.GetWindowSurface(), &format_count, surface_formats.data()) != VK_SUCCESS) {
			std::cerr << "failed to get supported surface formats" << std::endl;
			exit(1);
		}

		//Find supported present modes
		uint32_t present_mode_count;
		if (vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device.GetPhysicalDevice(), window_surface.GetWindowSurface(), &present_mode_count, nullptr) != VK_SUCCESS || present_mode_count == 0) {
			std::cerr << "failed to get number of supported presentation modes" << std::endl;
			exit(1);
		}
		std::vector<VkPresentModeKHR> present_modes(present_mode_count);
		if (vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device.GetPhysicalDevice(), window_surface.GetWindowSurface(), &present_mode_count, present_modes.data()) != VK_SUCCESS) {
			std::cerr << "failed to get supported presentation modes" << std::endl;
			exit(1);
		}

		//Determine number of images for the swap chain
		VkSurfaceCapabilitiesKHR surface_capabilities = window_surface.GetSurfaceCapabilities();
		uint32_t image_count = surface_capabilities.minImageCount + 1;
		if (surface_capabilities.maxImageCount != 0 && image_count > surface_capabilities.maxImageCount) {
			image_count = surface_capabilities.maxImageCount;
		}
		std::cout << "using " << image_count << " images for swap chain" << std::endl;

		//Select a surface format
		window_surface.SetSurfaceFormat(ChooseSurfaceFormat(surface_formats));

		//Select swap chain size
		swap_chain_extent_ = ChooseSwapExtent(surface_capabilities, app_config);

		//Determine if the images to present should be transformed before being presented (for example rotated 90 degrees or not)
		VkSurfaceTransformFlagBitsKHR surface_transform;
		if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
			surface_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		}
		else {
			surface_transform = surface_capabilities.currentTransform;
		}

		//Choose presentation mode (preferring MAILBOX ~= triple buffering)
		VkPresentModeKHR present_mode = ChoosePresentMode(present_modes);

		//Finally, create the swap chain
		VkSwapchainCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = window_surface.GetWindowSurface();
		createInfo.minImageCount = image_count;
		createInfo.imageFormat = window_surface.GetSurfaceFormat().format;
		createInfo.imageColorSpace = window_surface.GetSurfaceFormat().colorSpace;
		createInfo.imageExtent = swap_chain_extent_;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
		createInfo.preTransform = surface_transform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = present_mode;
		createInfo.clipped = VK_TRUE;
		if (vkCreateSwapchainKHR(logical_device.GetLogicalDevice(), &createInfo, nullptr, &swap_chain_) != VK_SUCCESS) {
			std::cerr << "failed to create swap chain" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created swap chain" << std::endl;
		}
		swap_chain_format_ = window_surface.GetSurfaceFormat().format;

		//Store the images used by the swap chain
		//Note: these are the images that swap chain image indices refer to
		//Note: actual number of images may differ from requested number, since it's a lower bound
		uint32_t actual_image_count = 0;
		if (vkGetSwapchainImagesKHR(logical_device.GetLogicalDevice(), swap_chain_, &actual_image_count, nullptr) != VK_SUCCESS || actual_image_count == 0) {
			std::cerr << "failed to acquire number of swap chain images" << std::endl;
			exit(1);
		}
		swap_chain_images_.resize(actual_image_count);
		if (vkGetSwapchainImagesKHR(logical_device.GetLogicalDevice(), swap_chain_, &actual_image_count, swap_chain_images_.data()) != VK_SUCCESS) {
			std::cerr << "failed to acquire swap chain images" << std::endl;
			exit(1);
		}
		std::cout << "acquired swap chain images" << std::endl;


	}
}