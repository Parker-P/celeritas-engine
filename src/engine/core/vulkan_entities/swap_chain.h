namespace Engine::Core::VulkanEntities {

	//The swap chain is the object that handles the way the images are presented to the screen and is the what allows the output images to go from the frame buffers to the window surface to be presented to the screen. See the FrameBuffer and WindowSurface classes
	//The swap chain handles retrieving and updating the images to be displayed. The swapchain decides when to swap the front and back buffers and contains a queue of images to be drawn
	class SwapChain {
		VkSwapchainKHR swap_chain_; //The KHR at the end indicates that the swap chain is an extension. An extension is simply an additional feature. This additional feature is provided by Khronos (the owners of Vulkan)
		VkPresentModeKHR present_mode_; //The way in which images are going to be presented. This will determine how many frame buffers are going to be used. For example in triple buffering, three frame buffers are used
		WindowSurface window_surface_; //The window surface that is going present the images in this swap chain to the glfw window
		VkExtent2D extent_; //The size of the images in the swap chain
		VkSurfaceFormatKHR format_; //The format and color space of the images in the swap chain
		VkRenderPass render_pass_; //The render pass whose instances generate the images that are output to the framebuffers
		std::vector<VkFramebuffer> frame_buffers_; //The framebuffers from which to take the images output by the render pass
		std::vector<VkImage> images_; //The images the swap chain is going to pass the window surface for presentation
		std::vector<VkImageView> image_views_; //The image views describing the images. Image views are also used by frame buffers. See the FrameBuffer class

		//Chooses which image format and color space to use for this swap chain's images
		VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats);

		//Chooses the size of the images in the swap chain
		VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surface_capabilities, const AppConfig& app_config);

		//Chooses the algorithm to use to present the images (triple buffering or double buffering)
		VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR> presentModes);

		//Creates one image view per image as we need the additional information an image view provides for each image
		void CreateImageViews(LogicalDevice& logical_device);

		//Creates the render pass
		void CreateRenderPass(LogicalDevice logical_device);

		//Creates the frame buffers this swap chain is going to be getting the images to present from
		void CreateFramebuffers(LogicalDevice& logical_device);
	public:
		void CreateSwapChain(PhysicalDevice& physical_device, LogicalDevice& logical_device, WindowSurface& window_surface, AppConfig& app_config);
		VkSwapchainKHR GetSwapChain();
		VkPresentModeKHR GetPresentMode();
		WindowSurface GetWindowSurface();
		VkExtent2D GetExtent();
		VkSurfaceFormatKHR GetFormat();
		std::vector<VkImage> GetImages();
		std::vector<VkImageView> GetImageViews();
	};
}