namespace Engine::Core::VulkanEntities {
	class Instance {
		VkInstance instance_; //The Vulkan instance
		VkDebugReportCallbackEXT callback_; //Extension callback used for debugging purposes with validation layers
		void CreateDebugCallback(AppConfig& app_config);
	public:
		//Creates the vulkan instance
		void CreateInstance(Engine::Core::AppConfig& app_config);

		//Returns the vulkan instance. Please create the vulkan instance first with CreateInstance
		VkInstance GetVulkanInstance();
	};
}