#include <iostream>
#include <vector>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

GLFWwindow* g_window = nullptr; //The main application window
VkInstance g_vkInstance{}; //The vulkan instance used to load the API
VkPhysicalDevice g_physicalDevice{}; //The physical graphics card handle
VkDevice g_logicalDevice{}; //The logical graphics card handle. This is what we use to communicate with the GPU
VkQueue g_graphicsQueue;

int main() {
    //Initialize the window with GLFW API
    glfwInit();
    g_window = glfwCreateWindow(1024, 768, "Celeritas engine", nullptr, nullptr);

    //Initialize Vulkan
    VkApplicationInfo vkAppInfo{};
    vkAppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    vkAppInfo.pApplicationName = "Solar System Explorer";
    vkAppInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    vkAppInfo.pEngineName = "Celeritas Engine";
    vkAppInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    vkAppInfo.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo instanceCreateInfo{};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &vkAppInfo;
    vkCreateInstance(&instanceCreateInfo, nullptr, &g_vkInstance);

    //Check for graphics card support and if found assign it to g_physicalDevice
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(g_vkInstance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        std::cout << "failed to find GPUs with Vulkan support!" << std::endl;
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(g_vkInstance, &deviceCount, devices.data());
    for (auto device : devices) {
        g_physicalDevice = device;
    }
    if (nullptr != g_physicalDevice) {
        std::cout << "found your graphics card" << std::endl;
    }

    //Find what queue families are supported by the graphics card.
    //Queue families are queues that are used to organize the order of instruction processing for the GPU.
    //They act like channels of communication with the GPU.
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(g_physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(g_physicalDevice, &queueFamilyCount, queueFamilies.data());
    //We need to find the index of the family queue that has the VK_QUEUE_GRAPHICS_BIT flag as we only want to perform graphics operations for now
    int graphicsQueueFamilyIndex = 0;
    for (int i = 0; i < queueFamilies.size(); ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            std::cout << "found the graphics bit queue family" << std::endl;
            graphicsQueueFamilyIndex = i; break;
        }
    }

    //Initialize logical device
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    VkDeviceCreateInfo logicalDeviceCreateInfo{};
    logicalDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    logicalDeviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    logicalDeviceCreateInfo.queueCreateInfoCount = 1;
    if (vkCreateDevice(g_physicalDevice, &logicalDeviceCreateInfo, nullptr, &g_logicalDevice) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }
    vkGetDeviceQueue(g_logicalDevice, graphicsQueueFamilyIndex, 0, &g_graphicsQueue);

    //Main loop
    while (!glfwWindowShouldClose(g_window)) {
        std::cout << "Main loop" << std::endl;
        glfwPollEvents();
    }

    //Shutdown operations (cleanup)
    vkDestroyDevice(g_logicalDevice, nullptr);
    vkDestroyInstance(g_vkInstance, nullptr);
    glfwDestroyWindow(g_window);
    glfwTerminate();

    return EXIT_SUCCESS;
}