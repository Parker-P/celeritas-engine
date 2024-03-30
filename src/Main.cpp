#define GLFW_INCLUDE_VULKAN
#include "LocalIncludes.hpp"

int main()
{
	Settings::GlobalSettings::Instance().Load(Settings::Paths::Settings());
	Engine::Vulkan::VulkanApplication app;
	app.Run();

	return 0;
}