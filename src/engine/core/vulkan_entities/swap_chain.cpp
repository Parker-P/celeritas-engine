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
	VkSurfaceFormatKHR SwapChain::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) {

		//We can either choose any format
		if (available_formats.size() == 1 && available_formats[0].format == VK_FORMAT_UNDEFINED) {
			return{ VK_FORMAT_R8G8B8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
		}

		//Or go with the standard format - if available
		for (const auto& available_surface_format : available_formats) {
			if (available_surface_format.format == VK_FORMAT_R8G8B8A8_UNORM) {
				return available_surface_format;
			}
		}

		//Or fall back to the first available one
		return available_formats[0];
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

		//Create an image view for every image in the swap chain. An image view is an image descriptor, it's just metadata that describes the image
		image_views_.resize(images_.size());
		for (size_t i = 0; i < images_.size(); i++) {
			VkImageViewCreateInfo create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			create_info.image = images_[i];
			create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			create_info.format = format_.format;
			create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			create_info.subresourceRange.baseMipLevel = 0;
			create_info.subresourceRange.levelCount = 1;
			create_info.subresourceRange.baseArrayLayer = 0;
			create_info.subresourceRange.layerCount = 1;
			if (vkCreateImageView(logical_device.GetLogicalDevice(), &create_info, nullptr, &image_views_[i]) != VK_SUCCESS) {
				std::cerr << "failed to create image view for swap chain image #" << i << std::endl;
				exit(1);
			}
		}
		std::cout << "created image views for swap chain images" << std::endl;
	}

	void SwapChain::CreateRenderPass(LogicalDevice logical_device) {

		//Define the description for the attachments
		VkAttachmentDescription attachment_description = {};
		attachment_description.format = format_.format;
		attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_description.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		attachment_description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		//Note: hardware will automatically transition attachment to the specified layout
		//Note: index refers to attachment descriptions array
		VkAttachmentReference color_attachment_reference = {};
		color_attachment_reference.attachment = 0;
		color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		//Note: this is a description of how the attachments of the render pass will be used in this sub pass
		//e.g. if they will be read in shaders and/or drawn to
		VkSubpassDescription sub_pass_description = {};
		sub_pass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		sub_pass_description.colorAttachmentCount = 1;
		sub_pass_description.pColorAttachments = &color_attachment_reference;

		//Create the render pass
		VkRenderPassCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		create_info.attachmentCount = 1;
		create_info.pAttachments = &attachment_description;
		create_info.subpassCount = 1;
		create_info.pSubpasses = &sub_pass_description;
		if (vkCreateRenderPass(logical_device.GetLogicalDevice(), &create_info, nullptr, &render_pass_) != VK_SUCCESS) {
			std::cerr << "failed to create render pass" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created render pass" << std::endl;
		}
	}

	void SwapChain::CreateFramebuffers(LogicalDevice& logical_device) {

		//Remember, a buffer is just an aera of memory. A framebuffer is no different: a frame buffer is
		//an area of memory that contains a frame. A frame is another buffer that contains a list of buffers, called attachments (in this context). 
		//An attachment contains an image view. Recall that an image view is just an image descriptor. 
		//This descriptor contains the image itself but also adds other information such as the type of image and the format.
		//The image view in an attachment can be a color, depth or stencil buffer for example.
		//The color buffer is just your regular image, but what are are depth and stencil buffers? 
		//Think of a stencil buffer as a portion of memory that represents an image that for each pixel contains a value. 
		//That value represents the masking, therefore it acts as a stencil.
		//It's just like cutting a hole in a piece of paper, then placing it on a surface you want to spray paint and using
		//it as a mask to spray that exact pattern you cut out of the piece of paper on that surface.
		//You could accomplish the same with just saying: if this pixel has value 0, don't draw it: if the pixel has value 1
		//then you can draw it. This can and is used to occlude certain areas of a model being rendered.
		//A depth buffer is an area of memory that represents an image that for each pixel has a value that represents the
		//distance from the camera to the surface in the scene that that pixel belongs to. Lets say you have a wall in front
		//of you in the scene that is exactly one meter away and you are looking at it from a perfectly perpendicular angle
		//with your camera in orthogonal mode: for each pixel that represents the rendered wall, the depth buffer will contain that one meter
		//distance value to that area on the wall represented by that pixel. This information is useful and actually fundamental
		//for making sure to draw what is visible and not what is not theoretically visible. If you have 2 overlapping planes
		//you can't know which one to render if you don't have the depth information.

		//Create a framebuffer for each image in the swap chain
		frame_buffers_.resize(images_.size());
		for (size_t i = 0; i < images_.size(); i++) {
			VkFramebufferCreateInfo create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			create_info.renderPass = render_pass_;
			create_info.attachmentCount = 1;
			create_info.pAttachments = &image_views_[i];
			create_info.width = extent_.width;
			create_info.height = extent_.height;
			create_info.layers = 1;
			if (vkCreateFramebuffer(logical_device.GetLogicalDevice(), &create_info, nullptr, &frame_buffers_[i]) != VK_SUCCESS) {
				std::cerr << "failed to create framebuffer for swap chain image view #" << i << std::endl;
				exit(1);
			}
		}
		std::cout << "created framebuffers for swap chain image views" << std::endl;
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

		//Select the format and size of the output images
		format_ = ChooseSurfaceFormat(surface_formats);
		extent_ = ChooseSwapExtent(surface_capabilities, app_config);

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
		createInfo.imageFormat = format_.format;
		createInfo.imageColorSpace = format_.colorSpace;
		createInfo.imageExtent = extent_;
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

		//Store the images used by the swap chain
		//Note: these are the images that swap chain image indices refer to
		//Note: actual number of images may differ from requested number, since it's a lower bound
		uint32_t actual_image_count = 0;
		if (vkGetSwapchainImagesKHR(logical_device.GetLogicalDevice(), swap_chain_, &actual_image_count, nullptr) != VK_SUCCESS || actual_image_count == 0) {
			std::cerr << "failed to acquire number of swap chain images" << std::endl;
			exit(1);
		}
		images_.resize(actual_image_count);
		if (vkGetSwapchainImagesKHR(logical_device.GetLogicalDevice(), swap_chain_, &actual_image_count, images_.data()) != VK_SUCCESS) {
			std::cerr << "failed to acquire swap chain images" << std::endl;
			exit(1);
		}
		std::cout << "acquired swap chain images" << std::endl;

		//Create the render pass first as the framebuffers depend on it
		CreateRenderPass(logical_device);

		//Then create the frame buffers
		CreateFramebuffers(logical_device);
	}

	VkSwapchainKHR SwapChain::GetSwapChain() {
		return swap_chain_;
	}

	VkPresentModeKHR SwapChain::GetPresentMode() {
		return present_mode_;
	}

	WindowSurface SwapChain::GetWindowSurface() {
		return window_surface_;
	}

	VkExtent2D SwapChain::GetExtent() {
		return extent_;
	}

	VkSurfaceFormatKHR SwapChain::GetFormat() {
		return format_;
	}

	std::vector<VkImage> SwapChain::GetImages() {
		return images_;
	}

	std::vector<VkImageView> SwapChain::GetImageViews() {
		return image_views_;
	}
}