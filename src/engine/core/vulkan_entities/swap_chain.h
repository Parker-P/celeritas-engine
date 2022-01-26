namespace Engine::Core::VulkanEntities {
	class SwapChain {
		VkSwapchainKHR swap_chain_;
		VkPresentModeKHR present_mode_;
		WindowSurface window_surface_;
		VkExtent2D swap_chain_extent_;
		VkSwapchainKHR swap_chain_;
		VkFormat swap_chain_format_;
		std::vector<VkImage> swap_chain_images_;
		std::vector<VkImageView> swap_chain_image_views_;

		VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities, const AppConfig& app_config);
		VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR> presentModes);
		void CreateImageViews(LogicalDevice& logical_device);
	public:
		void CreateSwapChain(PhysicalDevice& physical_device, LogicalDevice& logical_device, WindowSurface& window_surface, AppConfig& app_config);
	};
}