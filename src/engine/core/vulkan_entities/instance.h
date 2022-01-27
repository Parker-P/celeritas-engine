namespace Engine::Core::VulkanEntities {

	//This is an instance of the Vulkan execution system. The instance is our gateway to the vulkan API. The instance is what allows us to use vulkan commands and is the root of the entire Vulkan application context
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