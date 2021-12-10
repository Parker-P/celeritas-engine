//All boilerplate code from https://vulkan-tutorial.com/Introduction
//Great explanation of vulkan: https://www.youtube.com/watch?v=Y9U9IE0gVHA&list=PL8327DO66nu9qYVKLDmdLW_84-yE4auCR

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_INCLUDE_VULKAN
#include <iostream>
#include <algorithm>
#include <fstream>
#include <vector>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <vulkan/vulkan.h>

GLFWwindow* g_window = nullptr; //The main application window
VkInstance g_vkInstance{}; //The vulkan instance used to load the API
VkPhysicalDevice g_physicalDevice{}; //The physical graphics card handle
VkDevice g_logicalDevice{}; //The logical graphics card handle. This is what we use to communicate with the GPU
VkQueue g_graphicsQueue; //The queue used to send graphics commands to the GPU (actual rendering commands)
VkQueue g_presentQueue; //The queue used to send presentation commands to the GPU (commands to interface with the window)
VkSurfaceKHR g_surface; //The window surface used by vulkan to communicate with the glfw window
VkSwapchainKHR g_swapChain; //The swapchain object. A swapchain is a queue of images waiting to be presented to the screen via framebuffer
std::vector<VkImageView> g_swapChainImageViews; //Image views for the swap chain images. Image views describe how to access images in the swap chain
std::vector<VkImage> g_swapChainImages; //Container for the images in the swap chain queue
VkShaderModule g_vertexShaderModule; //Vulkan container object for the vertex shader
VkShaderModule g_fragmentShaderModule; //Vulkan container object for the fragment shader
VkRenderPass g_renderPass; //Contains information about the render pass
VkPipelineLayout g_pipelineLayout;

//Reads text from a file and returns the content of it
static std::vector<char> readFile(const std::wstring& path) {
	//Open the file and check if it was open
	std::ifstream file(path, std::ios::ate | std::ios::binary);
	if (file.is_open()) {
		//Get file length
		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);
		file.seekg(0);

		//Read contents and return
		file.read(buffer.data(), fileSize);
		file.close();
		return buffer;
	}
	else {
		std::wcout << "error opening file " << path << std::endl;
		return std::vector<char>();
	}
}

//Main function. This is the entry point.
int main() {
	//Initialize the window with GLFW API
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
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
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
	instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	instanceCreateInfo.ppEnabledExtensionNames = extensions.data();
	if (vkCreateInstance(&instanceCreateInfo, nullptr, &g_vkInstance) != VK_SUCCESS) {
		std::cout << "failed to create vulkan instance" << std::endl;
	}

	//Create a window surface. A window surface is a way that vulkan uses to communicate with
	//the window where it's going to render things
	VkWin32SurfaceCreateInfoKHR windowSurfaceCreateInfo{};
	windowSurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	windowSurfaceCreateInfo.hwnd = glfwGetWin32Window(g_window);
	windowSurfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
	if(glfwCreateWindowSurface(g_vkInstance, g_window, nullptr, &g_surface) != VK_SUCCESS) {
		std::cout << "failed to create vulkan window surface" << std::endl;
	};

	//Check for graphics card support and if found assign it to g_physicalDevice
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(g_vkInstance, &deviceCount, nullptr);
	if (deviceCount == 0) {
		std::cout << "failed to find GPUs with vulkan support!" << std::endl;
	}
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(g_vkInstance, &deviceCount, devices.data());
	for (auto device : devices) {
		g_physicalDevice = device;
	}

	//Find what queue families are supported by the graphics card.
	//Queue families are queues that are used to organize the order of instruction processing for the GPU.
	//They act like channels of communication with the GPU.
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(g_physicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(g_physicalDevice, &queueFamilyCount, queueFamilies.data());
	uint32_t graphicsQueueFamilyIndex = 0;
	uint32_t presentQueueFamilyIndex = 0;
	for (int i = 0; i < queueFamilies.size(); ++i) {
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			graphicsQueueFamilyIndex = i; break;
		}
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(g_physicalDevice, i, g_surface, &presentSupport);
		if (presentSupport) {
			presentQueueFamilyIndex = i;
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
	logicalDeviceCreateInfo.enabledExtensionCount = 1;
	const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	logicalDeviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	if (vkCreateDevice(g_physicalDevice, &logicalDeviceCreateInfo, nullptr, &g_logicalDevice) != VK_SUCCESS) {
		throw std::runtime_error("failed to create logical device!");
	}
	vkGetDeviceQueue(g_logicalDevice, graphicsQueueFamilyIndex, 0, &g_graphicsQueue);
	vkGetDeviceQueue(g_logicalDevice, presentQueueFamilyIndex, 0, &g_presentQueue);

	//Create the swapchain. The swapchain is a queue of images that are waiting
	//to end up in the frame buffer to then be shown on screen.
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_physicalDevice, g_surface, &surfaceCapabilities);
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(g_physicalDevice, g_surface, &formatCount, nullptr);
	std::vector<VkSurfaceFormatKHR> formats;
	if (formatCount != 0) {
		formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(g_physicalDevice, g_surface, &formatCount, formats.data());
	}
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(g_physicalDevice, g_surface, &presentModeCount, nullptr);
	std::vector<VkPresentModeKHR> presentModes;
	if (presentModeCount != 0) {
		presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(g_physicalDevice, g_surface, &presentModeCount, presentModes.data());
	}
	VkSwapchainCreateInfoKHR swapChainCreateInfo{};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = g_surface;
	uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
	if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount) {
		imageCount = surfaceCapabilities.maxImageCount;
	}
	swapChainCreateInfo.minImageCount = imageCount;
	swapChainCreateInfo.imageFormat = formats[0].format;
	swapChainCreateInfo.imageColorSpace = formats[0].colorSpace;
	int width, height;
	glfwGetFramebufferSize(g_window, &width, &height);
	VkExtent2D actualExtent;
	actualExtent.width = std::clamp(static_cast<uint32_t>(width), surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
	actualExtent.height = std::clamp(static_cast<uint32_t>(height), surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
	swapChainCreateInfo.imageExtent = actualExtent;
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	uint32_t queueFamilyIndices[] = { graphicsQueueFamilyIndex, presentQueueFamilyIndex };
	if (graphicsQueueFamilyIndex != presentQueueFamilyIndex) {
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapChainCreateInfo.queueFamilyIndexCount = 2;
		swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}
	swapChainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainCreateInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
	swapChainCreateInfo.clipped = VK_TRUE;
	swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
	if (vkCreateSwapchainKHR(g_logicalDevice, &swapChainCreateInfo, nullptr, &g_swapChain) != VK_SUCCESS) {
		std::cout << "failed to create swapchain" << std::endl;
	}
	vkGetSwapchainImagesKHR(g_logicalDevice, g_swapChain, &imageCount, nullptr);
	g_swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(g_logicalDevice, g_swapChain, &imageCount, g_swapChainImages.data());

	//Create an image view. An image view describes how to access an image and which 
	//part of the image to access in the swap chain. For example if it should be treated 
	//as a 2D texture depth texture without any mipmapping levels. Here we are creating
	//an image view for each image in the swap chain queue
	for (int i = 0; i < g_swapChainImages.size(); ++i) {
		VkImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		std::vector<VkImageView> g_swapChainImageViews;
		imageViewCreateInfo.image = g_swapChainImages[i];
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = formats[0].format;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		g_swapChainImageViews.resize(g_swapChainImages.size());
		if (vkCreateImageView(g_logicalDevice, &imageViewCreateInfo, nullptr, &g_swapChainImageViews[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image view");
		}
	}

	//Compile and load the vertex and fragment shaders
	system(".\\src\\shaderCompiler.bat");
	std::vector<char> vertexShaderCode = readFile(L".\\src\\vertexShader.spv");
	std::vector<char> fragmentShaderCode = readFile(L".\\src\\fragmentShader.spv");

	//Initialize shader modules. A shader module is a vulkan container for all shaders
	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo{};
	vertexShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderModuleCreateInfo.codeSize = vertexShaderCode.size();
	vertexShaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vertexShaderCode.data());
	vkCreateShaderModule(g_logicalDevice, &vertexShaderModuleCreateInfo, nullptr, &g_vertexShaderModule);
	VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo{};
	fragmentShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderModuleCreateInfo.codeSize = fragmentShaderCode.size();
	fragmentShaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(fragmentShaderCode.data());
	vkCreateShaderModule(g_logicalDevice, &fragmentShaderModuleCreateInfo, nullptr, &g_fragmentShaderModule);

	//Add the vertex and fragment shaders to the shader pipeline
	VkPipelineShaderStageCreateInfo vertexShaderStageInfo{};
	vertexShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageInfo.module = g_vertexShaderModule;
	vertexShaderStageInfo.pName = "main";
	VkPipelineShaderStageCreateInfo fragmentShaderStageInfo{};
	fragmentShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageInfo.module = g_fragmentShaderModule;
	fragmentShaderStageInfo.pName = "main";
	VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderStageInfo, fragmentShaderStageInfo };

	//Tell vulkan how to interpret the vertex data that we provide it
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

	//Tell vulkan how we are going to draw geometry (triangles)
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	//Configure the vulkan viewport and tell it what area of the viewport to render via scissors.
	//All a scissor is is a subarea of the viewport you can render in without affecting the coordinates
	//of the polygons being drawn, whereas if you change the viewport size you also change the coordinates
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapChainCreateInfo.imageExtent.width;
	viewport.height = (float)swapChainCreateInfo.imageExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapChainCreateInfo.imageExtent;
	VkPipelineViewportStateCreateInfo viewportState{}; //This object combines the viewport and scissor area to tell vulkan where to render
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	//Configure the vulkan rasterizer. The rasterizer is a component that takes screen positions and fills them with colored pixels.
	//This is where the fragment shader comes in. The fragment shader enables us to control what color those pixels will be.
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	//Configure multisampling
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	//Configure color blending. After a fragment shader has returned a color, it needs to be combined with the color that 
	//is already in the framebuffer. This transformation is known as color blending.
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	//Configure dynamic states. Dynamic states are used for when you want to change the render pipeline parameters
	//such as the viewport width and height without recreating the entire pipeline.
	VkDynamicState dynamicStates[] = {
	VK_DYNAMIC_STATE_VIEWPORT,
	VK_DYNAMIC_STATE_LINE_WIDTH
	};
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	//Create the pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0; // Optional
	pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
	pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
	pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional
	if (vkCreatePipelineLayout(g_logicalDevice, &pipelineLayoutInfo, nullptr, &g_pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout!");
	}

	//Before we can finish creating the pipeline, we need to tell Vulkan about the framebuffer 
	//attachments that will be used while rendering.We need to specify how many colorand depth 
	//buffers there will be, how many samples to use for each of themand how their contents should 
	//be handled throughout the rendering operations.All of this information is wrapped in a render 
	//pass object, for which we'll create a new createRenderPass function. Call this function from 
	//initVulkan before createGraphicsPipeline.
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = ;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	//A single render pass can consist of multiple subpasses. Subpasses are subsequent rendering 
	//operations that depend on the contents of framebuffers in previous passes, for example a 
	//sequence of post-processing effects that are applied one after another. If you group these 
	//rendering operations into one render pass, then Vulkan is able to reorder the operations and 
	//conserve memory bandwidth for possibly better performance. For our very first triangle, however, 
	//we'll stick to a single subpass.
	//Every subpass references one or more of the attachments that we've described using the structure 
	//in the previous sections. These references are themselves VkAttachmentReference structs that look like this:
	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	//Create the renderpass object
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	if (vkCreateRenderPass(g_logicalDevice, &renderPassInfo, nullptr, &g_renderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass!");
	}

	//Main loop
	while (!glfwWindowShouldClose(g_window)) {
		//std::cout << "Main loop" << std::endl;
		glfwPollEvents();
	}

	//Shutdown operations (cleanup)
	vkDestroyPipelineLayout(g_logicalDevice, g_pipelineLayout, nullptr);
	for (auto imageView : g_swapChainImageViews) {
		vkDestroyImageView(g_logicalDevice, imageView, nullptr);
	}
	vkDestroyRenderPass(g_logicalDevice, g_renderPass, nullptr);
	vkDestroyDevice(g_logicalDevice, nullptr);
	vkDestroySurfaceKHR(g_vkInstance, g_surface, nullptr);
	vkDestroyInstance(g_vkInstance, nullptr);
	glfwDestroyWindow(g_window);
	glfwTerminate();

	return EXIT_SUCCESS;
}