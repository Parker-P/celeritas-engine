#include <iostream>
#include "../engine/vulkan_application.h"
#include "../engine/vulkan_factory.h"

int main() {
	VulkanApplication game = VulkanFactory::GetInstance().CreateApplication("Solar System Explorer", 1024, 768);
	game.Run("Solar System Explorer");
}