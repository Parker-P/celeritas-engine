#pragma warning(disable:4005)
#define GLFW_INCLUDE_VULKAN
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#define _CRT_SECURE_NO_WARNINGS
#define __STDC_LIB_EXT1__ 1
#define NK_IMPLEMENTATION
#define NK_GLFW_VULKAN_IMPLEMENTATION
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_GLFW_VULKAN_IMPLEMENTATION
#define NK_KEYSTATE_BASED_INPUT
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>
#include <functional>
#include <optional>
#include <filesystem>
#include <map>
#include <thread>
#include <bitset>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <tinygltf/tiny_gltf.h>
#include <vulkan/vulkan.h>
#include <Json.h>
#include <nuklear/nuklear.h>
#include <nuklear/nuklear_glfw_vulkan.h>

namespace Engine {

	using namespace Engine;

	bool windowResized = false;
	bool windowMinimized = false;

	template <typename T>
	class Singleton {
	public:

		/**
		 * @brief Returns a static instance of the implementing class.
		 * @return
		 */
		static T& Instance() {
			static T _instance;
			return _instance;
		}
	};

	class Logger : public Singleton<Logger> {
	public:
		static void Log(const char* message) {
			std::cout << message << std::endl;
		}

		static void Log(std::string message) {
			std::cout << message << std::endl;
		}

		static void Log(const wchar_t* message) {
			std::wcout << message << std::endl;
		}

		static void Log(const std::wstring message) {
			std::wcout << message << std::endl;
		}
	};

	void Exit(int errorCode, const char* message) {
		Logger::Log(message);
		throw std::exception(message);
		std::exit(errorCode);
	}

	void CheckResult(VkResult result) {
		if (result != VK_SUCCESS) {
			std::string message = "ERROR: code " + std::to_string(result);
			Exit(result, message.c_str());
		}
	}

	/**
	 * @brief Returns true if rayVector intersects the plane formed by the triangle formed by v1, v2, v3 and the intersection point falls within said triangle.
	 * @param rayOrigin The origin point of the ray, in local space.
	 * @param rayVector The ray vector. Can be normalized or not.
	 * @param v1 Coordinates of vertex 1 in local space.
	 * @param v2 Coordinates of vertex 2 in local space.
	 * @param v3 Coordinates of vertex 3 in local space.
	 * @param outIntersectionPoint Intersection point in local space. This will remain unchanged if the function returns false.
	 */
	bool IsRayIntersectingTriangle(glm::vec3 rayOrigin, glm::vec3 rayVector, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, glm::vec3& outIntersectionPoint) {
		/**
		 * This is an implementation of the Trumbore-Moller ray-triangle intersection algorithm, source paper on the study is under the docs folder.
		 *
		 * In summary, the algorithm is based around the idea of creating a transformation matrix to create a new ad-hoc coordinate system so it's easier
		 * to calculate the point of intersection. In code, this is not apparent, as the algorithm also uses some careful and clever optimizations based on
		 * linear algebra concepts after some careful observations that Trumbore and Moller made when analyzing the math involved.
		 * The paper shows how these optimizations were derived at section 2.5 and 2.6.
		 *
		 * After an initial implementation I made myself from their theory, I decided to take their implementation directly, as it uses some extra optimizations
		 * as described above.
		 * This implementation is copy-pasted from Wikipedia, but it's pretty much exactly the same as the one in the original paper.
		 *
		 * The only addition to make the function more useful would be to add the ability to choose whether to ignore back-facing triangles.
		 */

		constexpr float epsilon = std::numeric_limits<float>::epsilon();

		glm::vec3 edge1 = v2 - v1;
		glm::vec3 edge2 = v3 - v1;

		glm::vec3 rayCrossE2 = glm::cross(rayVector, edge2);
		float determinant = dot(edge1, rayCrossE2);

		if (determinant > -epsilon && determinant < epsilon)
			return false;    // This ray is parallel to this triangle.

		float inverseDeterminant = 1.0f / determinant;
		glm::vec3 s = rayOrigin - v1;
		float u = inverseDeterminant * dot(s, rayCrossE2);

		if (u < 0 || u > 1)
			return false;

		glm::vec3 sCrossE1 = cross(s, edge1);
		float v = inverseDeterminant * glm::dot(rayVector, sCrossE1);

		if (v < 0 || u + v > 1)
			return false;

		// At this stage we can compute t to find out where the intersection point is along the ray.
		float t = inverseDeterminant * dot(edge2, sCrossE1);

		if (t > epsilon) {
			outIntersectionPoint = rayOrigin + rayVector * t;
			return true;
		}
		else // This means that the origin of the ray is inside the triangle.
			return false;
	}

	/**
	 * @brief Get the value of an enum as an integer.
	 * @param value
	 * @return
	 */
	template <typename Enumeration>
	inline auto AsInteger(Enumeration const value)
		-> typename std::underlying_type<Enumeration>::type {
		return static_cast<typename std::underlying_type<Enumeration>::type>(value);
	}

	/**
	 * @brief Returns the size of a vector in bytes.
	 * @param vector
	 * @return
	 */
	template <typename T>
	inline size_t GetVectorSizeInBytes(std::vector<T> vector) {
		return sizeof(decltype(vector)::value_type) * vector.size();
	}

	/**
	 * @brief Represents all the needed, or neeeded in order to obtain, information to run any call in the Vulkan API.
	 */
	class VulkanContext {
	public:
		VkInstance _instance = nullptr;
		VkDevice _logicalDevice = nullptr;
		VkPhysicalDevice _physicalDevice = nullptr;
		VkCommandPool _commandPool;
		VkQueue _queue;
		int _queueFamilyIndex;
	};

	/**
	 * @brief Used by implementing classes to mark themselves as a class that is meant to do work on each iteration of the main loop.
	 */
	class IVulkanUpdatable {
	public:

		/**
		 * @brief Function called on implementing classes in each iteration of the main loop. The main loop is defined in VulkanApplication.cpp.
		 */
		virtual void Update(VulkanContext& context) = 0;
	};

	/**
	 * @brief Used by implementing classes to mark themselves as a class that is meant to do work on each iteration of the main loop.
	 */
	class IUpdatable {
	public:

		/**
		 * @brief Function called on implementing classes in each iteration of the main loop. The main loop is defined in VulkanApplication.cpp.
		 */
		virtual void Update() = 0;
	};

	/**
		 * @brief Used by implementing classes to mark themselves as a class that is meant to do work on the main loop of the physics thread.
		 */
	class IPhysicsUpdatable {
	public:

		/**
		 * @brief Function called on implementing classes in each iteration of the main loop. The main loop is defined in VulkanApplication.cpp.
		 */
		virtual void PhysicsUpdate() = 0;
	};

	class Helper {
	public:

		/**
		 * @brief Convert values. Returns the converted value.
		 * @param value
		 * @return
		 */
		template <typename FromType, typename ToType>
		static ToType Convert(FromType value) {
			return Convert<ToType>(value);
		}

		/**
		 * @brief Converts uint32_t to float.
		 * @param value
		 * @return
		 */
		template<>
		static float Convert(uint32_t value) {
			int intermediateValue = 0;
			for (unsigned short i = 32; i > 0; --i) {
				unsigned char ithBitFromRight = ((uint32_t)value >> i) & 1;
				intermediateValue |= (ithBitFromRight << i);
			}
			return static_cast<float>(intermediateValue);
		}

		/**
		 * @brief Converts string to bool.
		 * @param value
		 * @return true if the value is either "true" (case insensitive) or 1, false otherwise.
		 */
		template<>
		static bool Convert(std::string value) {
			std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });
			if (value == "true" || value == "1") {
				return true;
			}
			return false;
		}

		/**
		 * @brief Converts string to int.
		 * @param value
		 * @return
		 */
		template<>
		static int Convert(std::string value) {
			return std::stoi(value);
		}

		/**
		 * @brief Converts string to float.
		 * @param value
		 * @return
		 */
		template<>
		static inline float Convert(std::string value) {
			return std::stof(value);
		}

		/**
		 * @brief Reads an ASCII or UNICODE text file.
		 * @param absolutePath
		 * @return The text the file contains as a std::string.
		 */
		static std::string GetFileText(std::filesystem::path absolutePath) {
			std::fstream textFile{ absolutePath, std::ios::in };
			std::string fileText{};

			if (textFile.is_open()) {
				char nextChar = textFile.get();

				while (nextChar >= 0) {
					fileText += nextChar;
					nextChar = textFile.get();
				}
			}
			return fileText;
		}

		static void SaveImageAsPng(std::filesystem::path absolutePath, void* data, uint32_t width, uint32_t height) {
			stbi_write_png(absolutePath.string().c_str(), width, height, 4, data, width * 4);
		}
	};

	class GlobalSettings : public Singleton<GlobalSettings> {
	public:

		/**
		 * @brief Flag for enabling or disabling validation layers when the Vulkan instance is created.
		 */
		bool _enableValidationLayers;

		/**
		 * @brief Instance validation layers to report problems with Vulkan usage.
		 */
		std::vector<const char*> _pValidationLayers;

		/**
		 * @brief Window width in pixels.
		 */
		uint32_t _windowWidth;

		/**
		 * @brief Window height in pixels.
		 */
		uint32_t _windowHeight;

		/**
		 * @brief Mouse sensitivity multiplier.
		 */
		float _mouseSensitivity;


		/**
		 * @brief Trims the ends of a string by removing the first and last characters from it.
		 * @param quotedString
		 * @return A new string with the 2 ends trimmed by one character.
		 */
		std::string TrimEnds(std::string quotedString) {
			auto length = quotedString.length();
			quotedString = quotedString.substr(1, length - 2);
			return quotedString;
		}

		/**
		 * @brief Loads global settings from the json file.
		 * @param absolutePathToJson
		 */
		void Load(const std::filesystem::path& absolutePathToJson) {
			// Read the json file and parse it.
			auto text = Helper::GetFileText(absolutePathToJson);

			sjson::parsing::parse_results json = sjson::parsing::parse(text.data());
			sjson::jobject rootObj = sjson::jobject::parse(json.value);

			// Get the values from the parsed result.
			auto evlJson = rootObj.get("EnableValidationLayers");
			_enableValidationLayers = Helper::Convert<std::string, bool>(TrimEnds(evlJson));

			auto validationLayers = sjson::jobject::parse(rootObj.get("ValidationLayers"));
			_pValidationLayers.resize(validationLayers.size());

			for (int i = 0; i < validationLayers.size(); ++i) {
				auto str = validationLayers.array(i).as_string();
				auto length = strlen(str.data()) + 1;
				char* ch = new char[length];
				ch[length] = 0;
				memcpy(ch, str.data(), length);
				_pValidationLayers[i] = ch;
			}

			auto windowSize = sjson::jobject::parse(rootObj.get("WindowSize"));
			auto width = windowSize.get("Width");
			auto height = windowSize.get("Height");
			_windowWidth = Helper::Convert<std::string, int>(width);
			_windowHeight = Helper::Convert<std::string, int>(height);

			auto input = sjson::jobject::parse(rootObj.get("Input"));
			auto sens = input.get("MouseSensitivity");
			_mouseSensitivity = Helper::Convert<std::string, float>(sens);
		}
	};

	/**
	 * @brief Contains all paths to files needed in the project.
	 */
	class Paths {
	public:

		/**
		 * @brief Every executable is passed a "working directory" parameter when starting it. This function returns the
		 * current working directory supplied to the executable that is running the code in this function, if provided,
		 * otherwise returns the folder, on disk, where the executable resides.
		 */
		static inline auto CurrentWorkingDirectory = []() -> std::filesystem::path { return std::filesystem::current_path(); };

		// I.N. Always use backward slashes for Windows, otherwise file reading may fail because of inconsistent path separators.
		// i.e. don't use C:\path/to\file, always use C:\path\to\file.

		/**
		 * @brief Function returning the path to the global settings json file.
		 */
		static inline auto Settings = []() -> std::filesystem::path { return CurrentWorkingDirectory() /= L"src\\GlobalSettings.json"; };

		/**
		 * @brief Function returning the path to the folder containing shaders.
		 */
		static inline auto ShadersPath = []() -> std::filesystem::path { return CurrentWorkingDirectory() /= L"src\\shaders"; };

		/**
		 * @brief Function returning the path to the compiled vertex shader file.
		 */
		static inline auto VertexShaderPath = []() -> std::filesystem::path { return ShadersPath() /= L"Graphics\\VertexShader.spv"; };

		/**
		 * @brief Function returning the path to the compiled fragment shader file.
		 */
		static inline auto FragmentShaderPath = []() -> std::filesystem::path { return ShadersPath() /= L"Graphics\\FragmentShader.spv"; };

		/**
		 * @brief Function returning the path to the textures folder.
		 */
		static inline auto TexturesPath = []() -> std::filesystem::path { return CurrentWorkingDirectory() /= L"textures"; };

		/**
		 * @brief Function returning the path to the models folder.
		 */
		static inline auto ModelsPath = []() -> std::filesystem::path { return CurrentWorkingDirectory() /= L"models"; };
	};

	class Time : public Singleton<Time>, public IUpdatable, public IPhysicsUpdatable {
	public:

		/**
		 * @brief The time this instance was created, assigned to in the constructor.
		 */
		std::chrono::high_resolution_clock::time_point _timeStart;

		/**
		 * @brief Time last frame started.
		 */
		std::chrono::high_resolution_clock::time_point _lastUpdateTime;

		/**
		 * @brief Time last physics update happened.
		 */
		std::chrono::high_resolution_clock::time_point _lastPhysicsUpdateTime;

		/**
		 * @brief The amount of time since last frame started in milliseconds.
		 */
		double _deltaTime;

		/**
		 * @brief The amount of time since the last physics simulation update in milliseconds.
		 */
		double _physicsDeltaTime;

		/**
		 * @brief Constructor.
		 */
		Time() {
			_timeStart = std::chrono::high_resolution_clock::now();
			_deltaTime = 0.0;
			_physicsDeltaTime = 0.0;
			_lastUpdateTime = _timeStart;
			_lastPhysicsUpdateTime = _timeStart;
		}

		/**
		 * @brief See IUpdatable.
		 */
		void Update() {
			auto now = std::chrono::high_resolution_clock::now();
			_deltaTime = (now - _lastUpdateTime).count() * 0.000001;
			_lastUpdateTime = now;
		}

		/**
		 * @brief See IPhysicsUpdatable.
		 */
		void PhysicsUpdate() {
			auto now = std::chrono::high_resolution_clock::now();
			_physicsDeltaTime = (now - _lastPhysicsUpdateTime).count() * 0.000001;
			_lastPhysicsUpdateTime = now;
		}
	};

	class Key {
	public:
		bool _isHeldDown;
		bool _wasPressed;
		int _code;
		Key() = default;
		Key(int code) : _code(code), _isHeldDown(false), _wasPressed(false) {}
	};

	class KeyCombo {
	public:
		std::vector<Key> _keys;
		bool IsActive() { return false; }
	};

	class KeyboardMouse : public Singleton<KeyboardMouse>, public IUpdatable {
		double _lastMouseX;

		double _lastMouseY;

		GLFWwindow* _window;

	public:

		/**
		 * @brief .
		 */
		std::map<int, Key> _keys;

		/**
		 * @brief True if the cursor is visible.
		 */
		bool _cursorEnabled = false;

		/**
		 * @brief Cumulative value of all horizontal mouse movements since a callback function was registered with glfwSetCursorPosCallback.
		 */
		double _mouseX;

		/**
		 * @brief Cumulative value of all vertical mouse movements since a callback function was registered with glfwSetCursorPosCallback.
		 */
		double _mouseY;

		/**
		 * @brief The difference between the current frame's mouseX and the mouseX registered on the last Update() call.
		 */
		double _deltaMouseX;

		/**
		 * @brief The difference between the current frame's mouseY and the mouseY registered on the last Update() call.
		 */
		double _deltaMouseY;

		//double _scrollX; // Only used for scroll-pads. For vertical scroll wheels, only the Y value is actually used.
		double _scrollY;

		static void KeyCallback(GLFWwindow* pWindow, int key, int scancode, int action, int mods) {
			Instance()._keys.try_emplace(key, Key(key));

			if (action == GLFW_PRESS) {
				Instance()._keys[key]._wasPressed = true;
				Instance()._keys[key]._isHeldDown = true;
			}
			else if (action == GLFW_REPEAT) {
				Instance()._keys[key]._isHeldDown = true;
			}
			else {
				Instance()._keys[key]._isHeldDown = false;
			}
		}

		static void CursorPositionCallback(GLFWwindow* pWindow, double xPos, double yPos) {
			if (!Instance()._cursorEnabled) {
				Instance()._mouseX = xPos;
				Instance()._mouseY = yPos;
			}
		}

		static void ScrollWheelCallback(GLFWwindow* pWindow, double xPos, double yPos) {
			if (!Instance()._cursorEnabled) {
				//Instance()._scrollX = xPos;
				Instance()._scrollY += yPos;
			}
		}

		/**
		 * @brief Constructor.
		 */
		KeyboardMouse() = default;

		KeyboardMouse(GLFWwindow* window) {
			if (nullptr != window) {
				_window = window;

				// Keyboard init
				glfwSetKeyCallback(window, KeyCallback);

				// Mouse init
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				if (glfwRawMouseMotionSupported()) {
					glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
				}
				glfwSetCursorPosCallback(window, CursorPositionCallback);
				glfwSetScrollCallback(window, ScrollWheelCallback);
			}
		}

		bool IsKeyHeldDown(int glfwKeyCode) {
			if (!_keys[glfwKeyCode]._isHeldDown) {
				_keys[glfwKeyCode]._wasPressed = false;
			}
			return _keys[glfwKeyCode]._isHeldDown;
		}

		bool WasKeyPressed(int glfwKeyCode) {
			bool wasPressed = _keys[glfwKeyCode]._wasPressed;
			_keys[glfwKeyCode]._wasPressed = false;
			return wasPressed;
		}

		void ToggleCursor(GLFWwindow* window) {
			if (_cursorEnabled) {
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			}
			else {
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			}
			_cursorEnabled = !_cursorEnabled;
		}

		void Update() {
			_deltaMouseX = (_mouseX - _lastMouseX);
			_deltaMouseY = (_mouseY - _lastMouseY);

			_lastMouseX = _mouseX;
			_lastMouseY = _mouseY;

			if (WasKeyPressed(GLFW_KEY_ESCAPE)) {
				ToggleCursor(_window);
			}
		}
	};

	/**
	 * @brief Vulkan's representation of a GPU.
	 */
	class PhysicalDevice {

	public:

		/**
		 * @brief Allocates memory according to the requirements, and returns a handle to be used strictly via the Vulkan
		 * API to access the allocated memory.
		 * @param memoryType This can be any of the following values:
		 * 1) VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT: this means GPU memory, so VRAM. If this is not set, then regular RAM is assumed.
		 * 2) VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT: this means that the CPU will be able to read and write from the allocated memory IF YOU CALL vkMapMemory() FIRST.
		 * 3) VK_MEMORY_PROPERTY_HOST_CACHED_BIT: this means that the memory will be cached so that when the CPU writes to this buffer, if the data is small enough to fit in its
		 * cache (which is much faster to access) it will do that instead. Only problem is that this way, if your GPU needs to access that data, it won't be able to unless it's
		 * also marked as HOST_COHERENT.
		 * 4) VK_MEMORY_PROPERTY_HOST_COHERENT_BIT: this means that anything that the CPU writes to the buffer will be able to be read by the GPU as well, effectively
		 * granting the GPU access to the CPU's cache (if the buffer is also marked as HOST_CACHED). COHERENT stands for consistency across memories, so it basically means
		 * that the CPU, GPU or any other device will see the same memory if trying to access the buffer. If you don't have this flag set, and you try to access the
		 * buffer from the GPU while the buffer is marked HOST_CACHED, you may not be able to get the data or even worse, you may end up reading the wrong chunk of memory.
		 * Further read: https://asawicki.info/news_1740_vulkan_memory_types_on_pc_and_how_to_use_them
		 */
		static VkDeviceMemory AllocateMemory(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, const VkMemoryRequirements& memoryRequirements, const VkMemoryPropertyFlagBits& memoryType) {
			VkMemoryAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memoryRequirements.size;
			allocInfo.memoryTypeIndex = GetMemoryTypeIndex(physicalDevice, memoryRequirements.memoryTypeBits, memoryType);

			VkDeviceMemory handleToAllocatedMemory;

			if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &handleToAllocatedMemory) != VK_SUCCESS) {
				std::cout << "failed allocating memory of size " << allocInfo.allocationSize << std::endl;
				std::exit(-1);
			}

			return handleToAllocatedMemory;
		}

		static bool SupportsSwapchains(VkPhysicalDevice& physicalDevice) {
			uint32_t extensionCount = 0;
			vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

			if (extensionCount == 0) {
				std::cerr << "physical device doesn't support any extensions" << std::endl;
				exit(1);
			}

			std::vector<VkExtensionProperties> deviceExtensions(extensionCount);
			vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, deviceExtensions.data());

			for (const auto& extension : deviceExtensions) {
				if (strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
					std::cout << "physical device supports swap chains" << std::endl;
					return true;
				}
			}

			return false;
		}

		static bool SupportsSurface(VkPhysicalDevice& physicalDevice, uint32_t& queueFamilyIndex, VkSurfaceKHR& surface) {
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, surface, &presentSupport);
			return presentSupport == 0 ? false : true;
		}

		static VkPhysicalDeviceMemoryProperties GetMemoryProperties(VkPhysicalDevice& physicalDevice) {
			VkPhysicalDeviceMemoryProperties props;
			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &props);
			return props;
		}

		static uint32_t GetMemoryTypeIndex(VkPhysicalDevice& physicalDevice, uint32_t typeBits, VkMemoryPropertyFlags properties) {
			auto props = GetMemoryProperties(physicalDevice);

			for (uint32_t i = 0; i < 32; i++) {
				if ((typeBits & 1) == 1) {
					if ((props.memoryTypes[i].propertyFlags & properties) == properties) {
						return i;
					}
				}
				typeBits >>= 1;
			}

			return -1;
		}

		static std::vector<VkQueueFamilyProperties> GetAllQueueFamilyProperties(VkPhysicalDevice& physicalDevice) {
			uint32_t queueFamilyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

			std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

			return queueFamilies;
		}

		static void GetQueueFamilyIndices(VkPhysicalDevice& physicalDevice, const VkQueueFlagBits& queueFlags, bool needsPresentationSupport, const VkSurfaceKHR& surface) {
			// Check queue families
			auto queueFamilyProperties = GetAllQueueFamilyProperties(physicalDevice);

			std::vector<uint32_t> queueFamilyIndices;

			for (uint32_t i = 0; i < queueFamilyProperties.size(); i++) {
				VkBool32 presentationSupported = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentationSupported);

				if (queueFamilyProperties[i].queueCount > 0 && queueFamilyProperties[i].queueFlags & queueFlags) {
					if (needsPresentationSupport) {
						if (presentationSupported) {
							queueFamilyIndices.push_back(i);
						}
					}
					else {
						queueFamilyIndices.push_back(i);
					}
				}
			}
		}

		static VkSurfaceCapabilitiesKHR GetSurfaceCapabilities(VkPhysicalDevice& physicalDevice, VkSurfaceKHR& windowSurface) {
			VkSurfaceCapabilitiesKHR surfaceCapabilities;
			if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, windowSurface, &surfaceCapabilities) != VK_SUCCESS) {
				std::cerr << "failed to acquire presentation surface capabilities" << std::endl;
			}
			return surfaceCapabilities;
		}

		static std::vector<VkSurfaceFormatKHR> GetSupportedFormatsForSurface(VkPhysicalDevice& physicalDevice, VkSurfaceKHR& windowSurface) {
			uint32_t formatCount;
			if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, windowSurface, &formatCount, nullptr) != VK_SUCCESS || formatCount == 0) {
				std::cerr << "failed to get number of supported surface formats" << std::endl;
			}

			std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
			if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, windowSurface, &formatCount, surfaceFormats.data()) != VK_SUCCESS) {
				std::cerr << "failed to get supported surface formats" << std::endl;
			}

			return surfaceFormats;
		}

		static std::vector<VkPresentModeKHR> GetSupportedPresentModesForSurface(VkPhysicalDevice& physicalDevice, VkSurfaceKHR& windowSurface) {
			uint32_t presentModeCount;
			if (vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, windowSurface, &presentModeCount, nullptr) != VK_SUCCESS || presentModeCount == 0) {
				std::cerr << "failed to get number of supported presentation modes" << std::endl;
				exit(1);
			}

			std::vector<VkPresentModeKHR> presentModes(presentModeCount);
			if (vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, windowSurface, &presentModeCount, presentModes.data()) != VK_SUCCESS) {
				std::cerr << "failed to get supported presentation modes" << std::endl;
				exit(1);
			}

			return presentModes;
		}
	};

	class VkHelper {
	public:
		static VkCommandPool CreateCommandPool(VkDevice& logicalDevice, uint32_t& queueFamilyIndex) {
			VkCommandPoolCreateInfo poolCreateInfo = {};
			poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			poolCreateInfo.queueFamilyIndex = queueFamilyIndex;
			poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			VkCommandPool outPool;
			vkCreateCommandPool(logicalDevice, &poolCreateInfo, nullptr, &outPool);
			return outPool;
		}

		static int FindQueueFamilyIndex(VkPhysicalDevice& physicalDevice, VkQueueFlagBits queueFlags) {
			auto queueFamilyProperties = PhysicalDevice::GetAllQueueFamilyProperties(physicalDevice);

			for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyProperties.size(); queueFamilyIndex++) {
				if (queueFamilyProperties[queueFamilyIndex].queueCount > 0 && queueFamilyProperties[queueFamilyIndex].queueFlags & queueFlags) {
					return queueFamilyIndex;
				}
			}

			return -1;
		}

		static void StartRecording(VkCommandBuffer& commandBuffer) {
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			vkResetCommandBuffer(commandBuffer, 0);
			vkBeginCommandBuffer(commandBuffer, &beginInfo);
		}

		static void StopRecording(VkCommandBuffer& commandBuffer) { vkEndCommandBuffer(commandBuffer); }

		static void ExecuteCommands(VkCommandBuffer& commandBuffer, VkQueue& queue) {
			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffer;
			vkQueueSubmit(queue, 1, &submitInfo, nullptr);
			vkQueueWaitIdle(queue);
		}

		static VkCommandBuffer CreateCommandBuffer(VkDevice& logicalDevice, VkCommandPool& commandPool) {
			VkCommandBufferAllocateInfo cmdBufInfo = {};
			cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			cmdBufInfo.commandPool = commandPool;
			cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			cmdBufInfo.commandBufferCount = 1;
			VkCommandBuffer commandBuffer;
			vkAllocateCommandBuffers(logicalDevice, &cmdBufInfo, &commandBuffer);
			return commandBuffer;
		}

		static void UnmapAndDestroyStagingBuffer(VkDevice logicalDevice, VkDeviceMemory stagingMemory, VkBuffer stagingBuffer) {
			vkUnmapMemory(logicalDevice, stagingMemory);
			vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
			vkFreeMemory(logicalDevice, stagingMemory, nullptr);
		}

		static void* DownloadImage(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue, VkImage image, uint32_t width, uint32_t height, VkDeviceMemory outStagingMemory, VkBuffer outStagingBuffer) {
			VkDeviceMemory stagingMemory;
			VkBuffer stagingBuffer;

			CreateBuffer(logicalDevice, physicalDevice, 4 * width * height, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT /*| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT*/,
				&stagingBuffer, &stagingMemory);

			TransitionImageLayout(logicalDevice, commandPool, queue, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			CopyImageToBuffer(logicalDevice, commandPool, queue, image, stagingBuffer, width, height);

			void* data;
			vkMapMemory(logicalDevice, stagingMemory, 0, 4 * width * height, 0, &data);
			return data;
		}

		static void CreateBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* bufferMemory) {
			VkBufferCreateInfo bufferInfo{};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = size;
			bufferInfo.usage = usage;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Single queue family access
			CheckResult(vkCreateBuffer(device, &bufferInfo, nullptr, buffer));
			*bufferMemory = AllocateGpuMemoryForBuffer(device, physicalDevice, *buffer, properties);
			vkBindBufferMemory(device, *buffer, *bufferMemory, 0);
		}

		static void TransitionImageLayout(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {

			VkCommandBuffer commandBuffer = CreateCommandBuffer(device, commandPool);
			StartRecording(commandBuffer);

			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = oldLayout;
			barrier.newLayout = newLayout;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = image;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;

			VkPipelineStageFlags sourceStage;
			VkPipelineStageFlags destinationStage;

			if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) { barrier.srcAccessMask = 0; sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; }
			if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) { barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT; }
			if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) { barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT; sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT; }

			if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) { barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT; destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT; }
			if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) { barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT; }
			if (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) { barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT; destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; }
			if (newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) { barrier.dstAccessMask = 0; destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; }

			vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
			vkEndCommandBuffer(commandBuffer);
			ExecuteCommands(commandBuffer, queue);
			vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
		}

		static void CopyImageToBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkImage image, VkBuffer buffer, uint32_t width, uint32_t height) {
			VkCommandBufferAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandPool = commandPool;
			allocInfo.commandBufferCount = 1;

			VkCommandBuffer commandBuffer = CreateCommandBuffer(device, commandPool);
			StartRecording(commandBuffer);

			VkBufferImageCopy region{};
			region.bufferOffset = 0;
			region.bufferRowLength = 0;  // Tightly packed
			region.bufferImageHeight = 0; // Tightly packed
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;
			region.imageOffset = { 0, 0, 0 };
			region.imageExtent = { width, height, 1 };

			vkCmdCopyImageToBuffer(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);
			vkEndCommandBuffer(commandBuffer);
			ExecuteCommands(commandBuffer, queue);
			vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
		}

		static VkDeviceMemory AllocateGpuMemory(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkMemoryRequirements memRequirements, VkMemoryPropertyFlags requiredMemoryProperties) {
			VkMemoryAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memRequirements.size;
			allocInfo.memoryTypeIndex = PhysicalDevice::GetMemoryTypeIndex(physicalDevice, memRequirements.memoryTypeBits, requiredMemoryProperties);
			VkDeviceMemory allocatedMemory;
			CheckResult(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &allocatedMemory));
			return allocatedMemory;
		}

		static VkDeviceMemory AllocateGpuMemoryForImage(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkImage imageToAllocate, VkMemoryPropertyFlags requiredMemoryProperties) {
			VkMemoryRequirements memRequirements;
			vkGetImageMemoryRequirements(logicalDevice, imageToAllocate, &memRequirements);
			return AllocateGpuMemory(logicalDevice, physicalDevice, memRequirements, requiredMemoryProperties);
		}

		static VkDeviceMemory AllocateGpuMemoryForBuffer(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkBuffer buffer, VkMemoryPropertyFlags requiredMemoryProperties) {
			VkMemoryRequirements memRequirements;
			vkGetBufferMemoryRequirements(logicalDevice, buffer, &memRequirements);
			return AllocateGpuMemory(logicalDevice, physicalDevice, memRequirements, requiredMemoryProperties);
		}

		static VkDescriptorSet AllocateDescriptorSet(VkDevice logicalDevice, VkDescriptorPool descriptorPool, VkDescriptorSetLayout setLayout) {
			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = (uint32_t)1;
			allocInfo.pSetLayouts = &setLayout;
			VkDescriptorSet descriptorSet;
			CheckResult(vkAllocateDescriptorSets(logicalDevice, &allocInfo, &descriptorSet));
			return descriptorSet;
		}

		static void CopyImage(VkDevice logicalDevice, VkCommandPool commandPool, VkQueue queue,
			VkImage srcImage, VkImage dstImage, VkExtent2D extent,
			uint32_t mipLevel = 0, uint32_t baseArrayLayer = 0, uint32_t layerCount = 1) {
			VkCommandBuffer commandBuffer = CreateCommandBuffer(logicalDevice, commandPool);
			StartRecording(commandBuffer);

			VkImageSubresourceLayers subresource = {};
			subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // Assuming color images
			subresource.mipLevel = mipLevel;
			subresource.baseArrayLayer = baseArrayLayer;
			subresource.layerCount = layerCount;

			VkImageCopy copyRegion = {};
			copyRegion.srcSubresource = subresource;
			copyRegion.dstSubresource = subresource;
			copyRegion.srcOffset = { 0, 0, 0 }; // Copy from the start of the source image
			copyRegion.dstOffset = { 0, 0, 0 }; // Copy to the start of the destination image
			copyRegion.extent = { extent.width, extent.height, 1 };       // The size of the image to copy

			// Record the copy command
			vkCmdCopyImage(commandBuffer,
				srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &copyRegion);

			// End the command buffer
			CheckResult(vkEndCommandBuffer(commandBuffer));

			ExecuteCommands(commandBuffer, queue);
			vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
		}
	};

	class Buffer {
	public:
		VkBufferCreateInfo _createInfo{};
		VkBufferViewCreateInfo _viewCreateInfo{};

		VkBuffer _buffer{};
		VkBufferView _view{};

		/**
		 * @brief Vulkan-only handle that Vulkan uses to handle the buffer on GPU memory.
		 */
		VkDeviceMemory _gpuMemory = nullptr;

		/**
		 * @brief Pointer to CPU-accessible memory that Vulkan uses to read/write memory from/to the buffer. This is separate from _pData because Vulkan might need to
		 * also tell the GPU where this data is stored, and by making this being able to be set only via Vulkan calls, it ensures that it catches all changes to the data
		 * and maintains it coherent across CPU and GPU.
		 */
		void* _cpuMemory = nullptr;

		/**
		 * @brief Pointer to CPU-only visible data.
		 */
		void* _pData = nullptr;

		size_t _sizeBytes = 0;

		Buffer() = default;

		Buffer(void* pData, size_t sizeBytes) {
			_pData = pData;
			_sizeBytes = sizeBytes;
		}

		static void CopyToDeviceMemory(VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice, VkCommandPool commandPool, VkQueue queue, VkBuffer buffer, void* pData, size_t sizeBytes) {
			// Create a temporary buffer.
			Buffer stagingBuffer{};
			stagingBuffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			stagingBuffer._createInfo.size = sizeBytes;
			stagingBuffer._createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			vkCreateBuffer(logicalDevice, &stagingBuffer._createInfo, nullptr, &stagingBuffer._buffer);

			// Allocate memory for the buffer.
			VkMemoryRequirements requirements{};
			vkGetBufferMemoryRequirements(logicalDevice, stagingBuffer._buffer, &requirements);
			stagingBuffer._gpuMemory = PhysicalDevice::AllocateMemory(physicalDevice, logicalDevice, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

			// Map memory to the correct GPU and CPU ranges for the buffer.
			vkBindBufferMemory(logicalDevice, stagingBuffer._buffer, stagingBuffer._gpuMemory, 0);
			vkMapMemory(logicalDevice, stagingBuffer._gpuMemory, 0, sizeBytes, 0, &stagingBuffer._cpuMemory);
			memcpy(stagingBuffer._cpuMemory, pData, sizeBytes);

			auto copyCommandBuffer = VkHelper::CreateCommandBuffer(logicalDevice, commandPool);
			VkHelper::StartRecording(copyCommandBuffer);

			VkBufferCopy copyRegion = {};
			copyRegion.size = sizeBytes;
			vkCmdCopyBuffer(copyCommandBuffer, stagingBuffer._buffer, buffer, 1, &copyRegion);

			VkHelper::StopRecording(copyCommandBuffer);
			VkHelper::ExecuteCommands(copyCommandBuffer, queue);

			vkFreeCommandBuffers(logicalDevice, commandPool, 1, &copyCommandBuffer);
			vkDestroyBuffer(logicalDevice, stagingBuffer._buffer, nullptr);
		}
	};

	void CopyImageToDeviceMemory(VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice, VkCommandPool& commandPool, VkQueue& queue, VkImage& image, int width, int height, int depth, void* pData, size_t sizeBytes) {
		// Create a temporary buffer.
		Buffer stagingBuffer{};
		stagingBuffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingBuffer._createInfo.size = sizeBytes;
		stagingBuffer._createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		vkCreateBuffer(logicalDevice, &stagingBuffer._createInfo, nullptr, &stagingBuffer._buffer);

		// Allocate memory for the buffer.
		VkMemoryRequirements requirements{};
		vkGetBufferMemoryRequirements(logicalDevice, stagingBuffer._buffer, &requirements);
		stagingBuffer._gpuMemory = PhysicalDevice::AllocateMemory(physicalDevice, logicalDevice, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		// Map memory to the correct GPU and CPU ranges for the buffer.
		vkBindBufferMemory(logicalDevice, stagingBuffer._buffer, stagingBuffer._gpuMemory, 0);
		vkMapMemory(logicalDevice, stagingBuffer._gpuMemory, 0, sizeBytes, 0, &stagingBuffer._cpuMemory);
		memcpy(stagingBuffer._cpuMemory, pData, sizeBytes);

		auto commandBuffer = VkHelper::CreateCommandBuffer(logicalDevice, commandPool);
		VkHelper::StartRecording(commandBuffer);

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_NONE;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.image = image;
		barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		VkBufferImageCopy copyInfo{};
		copyInfo.bufferImageHeight = height;
		copyInfo.bufferRowLength = width;
		copyInfo.imageExtent = { (uint32_t)width, (uint32_t)height, (uint32_t)depth };
		copyInfo.imageOffset = { 0,0,0 };
		copyInfo.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		vkCmdCopyBufferToImage(commandBuffer, stagingBuffer._buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyInfo);

		VkHelper::StopRecording(commandBuffer);
		VkHelper::ExecuteCommands(commandBuffer, queue);

		vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
		vkDestroyBuffer(logicalDevice, stagingBuffer._buffer, nullptr);
	}

	inline std::string Format(float value) {
		return (value >= 0.0f) ? " " + std::to_string(value) : std::to_string(value);
	}

	inline std::ostream& operator << (std::ostream& stream, const glm::mat4& matrix) {
		stream << Format(matrix[0][0]) << ", " << Format(matrix[0][1]) << ", " << Format(matrix[0][2]) << ", " << Format(matrix[0][3]) << "\n";
		stream << Format(matrix[1][0]) << ", " << Format(matrix[1][1]) << ", " << Format(matrix[1][2]) << ", " << Format(matrix[1][3]) << "\n";
		stream << Format(matrix[2][0]) << ", " << Format(matrix[2][1]) << ", " << Format(matrix[2][2]) << ", " << Format(matrix[2][3]) << "\n";
		stream << Format(matrix[3][0]) << ", " << Format(matrix[3][1]) << ", " << Format(matrix[3][2]) << ", " << Format(matrix[3][3]);
		return stream;
	}

	inline std::ostream& operator<< (std::ostream& stream, const glm::vec3& vector) {
		return stream << "(" << vector.x << ", " << vector.y << ", " << vector.z << ")";
	}

	inline std::ostream& operator<< (std::ostream& stream, const glm::vec3&& vector) {
		return stream << "(" << vector.x << ", " << vector.y << ", " << vector.z << ")";
	}

	/**
	 * @brief Prints a message to a stream given a function that does the printing and a message.
	 * Default function prints to standard output (console).
	 * @param message The message to print.
	 * @param logFunction Logging function. Defaults to a function printing to the console window.
	 */
	inline void Print(const std::string& message, void(*logFunction) (const std::string&) = [](const std::string& message) { std::cout << message << std::endl; }) {
		logFunction(message);
	}

	/**
	 * @brief Represents a column-major 4x4 matrix transform in a left handed X right, Y up, Z forward coordinate system.
	 */
	class Transform {

	public:

		glm::mat4x4 _matrix;
		glm::vec3 _scale;

		/**
		 * @brief Returns the transform to take another transform from right-handed gltf space (X left, Y up, Z forward)
		 * to left-handed engine space (X right, Y up, Z forward).
		 */
		static Transform GltfToEngine() {
			return Transform(glm::mat4x4{
				glm::vec4(-1.0f, 0.0f, 0.0f, 0.0f), // Column 1
				glm::vec4(0.0f,  1.0f, 0.0f, 0.0f), // Column 2
				glm::vec4(0.0f,  0.0f, 1.0f, 0.0f), // Column 3
				glm::vec4(0.0f,  0.0f, 0.0f, 1.0f)	// Column 4
				});
		}

		/**
		 * @brief Constructs a transform using the identity matrix for both _scale and _matrix.
		 */
		Transform() = default;

		Transform(glm::mat4x4 matrix) : _matrix(matrix) {}

		glm::vec3 Right() {
			glm::vec4 right = _matrix * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
			return glm::vec3(right.x, right.y, right.z);
		}

		glm::vec3 Up() {
			glm::vec4 up = _matrix * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
			return glm::vec3(up.x, up.y, up.z);
		}

		glm::vec3 Forward() {
			glm::vec4 forward = _matrix * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
			return glm::vec3(forward.x, forward.y, forward.z);
		}

		void Translate(const glm::vec3& offset) {
			_matrix[3][0] += offset.x;
			_matrix[3][1] += offset.y;
			_matrix[3][2] += offset.z;
		}

		void RotateAroundPosition(const glm::vec3& position, const glm::vec3& axis, const float& angleRadians) {
			if (angleRadians == 0) { return; }

			auto rotation = MakeQuaternionRotation(axis, angleRadians);
			auto currentPosition = Position();
			SetPosition(position + (rotation * (currentPosition - position)));
			Rotate(rotation);
		}

		glm::quat MakeQuaternionRotation(const glm::vec3& axis, const float& angleRadians) {
			auto cosine = cos(angleRadians / 2.0f);
			auto sine = sin(angleRadians / 2.0f);
			return glm::quat(cosine, axis.x * sine, axis.y * sine, axis.z * sine);
		}

		void Rotate(const glm::quat& rotation) {
			// Rotate each individual axis of the transformation by the quaternion.
			auto newX = rotation * glm::vec3(_matrix[0][0], _matrix[0][1], _matrix[0][2]);
			auto newY = rotation * glm::vec3(_matrix[1][0], _matrix[1][1], _matrix[1][2]);
			auto newZ = rotation * glm::vec3(_matrix[2][0], _matrix[2][1], _matrix[2][2]);

			// Set the new axes starting with X.
			_matrix[0][0] = newX.x;
			_matrix[0][1] = newX.y;
			_matrix[0][2] = newX.z;

			// Then Y.
			_matrix[1][0] = newY.x;
			_matrix[1][1] = newY.y;
			_matrix[1][2] = newY.z;

			// And finally Z.
			_matrix[2][0] = newZ.x;
			_matrix[2][1] = newZ.y;
			_matrix[2][2] = newZ.z;
		}

		void RotateR(const glm::vec3& axis, const float& angleRadians) {
			if (angleRadians == 0.0f) { return; }
			Rotate(MakeQuaternionRotation(axis, angleRadians));
		}

		void Rotate(const glm::vec3& axis, const float& angleDegrees) {
			if (angleDegrees == 0.0f) { return; }
			RotateR(axis, glm::radians(angleDegrees));
		}

		void SetPosition(const glm::vec3& position) {
			_matrix[3][0] = position.x;
			_matrix[3][1] = position.y;
			_matrix[3][2] = position.z;
		}

		void SetScale(const glm::vec3& scale) {
			_scale = scale;
			_matrix[0][0] *= scale.x;
			_matrix[1][1] *= scale.y;
			_matrix[2][2] *= scale.z;
		}

		glm::vec3 Position() {
			return glm::vec3(_matrix[3][0], _matrix[3][1], _matrix[3][2]);
		}
	};

	class Image {
	public:
		VkImageCreateInfo _createInfo{};
		VkImageViewCreateInfo _viewCreateInfo{};
		VkSamplerCreateInfo _samplerCreateInfo{};

		VkImage _image{};
		VkImageView _view{};
		VkSampler _sampler{};

		VkImageLayout _currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkDeviceMemory _gpuMemory = nullptr;
		void* _pData = nullptr;
		size_t _sizeBytes = 0;

		Image() = default;

		Image(void* pData, size_t sizeBytes) {
			_pData = pData;
			_sizeBytes = sizeBytes;
		}

		static Image SolidColor(VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
			Image image;

			image._sizeBytes = 4;
			image._pData = malloc(image._sizeBytes);
			unsigned char imageData[4] = { r, g, b, a };
			memcpy(image._pData, imageData, image._sizeBytes);

			auto& imageCreateInfo = image._createInfo;
			imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
			imageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
			imageCreateInfo.extent = { 1, 1, 1 };
			imageCreateInfo.mipLevels = 1;
			imageCreateInfo.arrayLayers = 1;
			imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			vkCreateImage(logicalDevice, &imageCreateInfo, nullptr, &image._image);

			VkMemoryRequirements reqs;
			vkGetImageMemoryRequirements(logicalDevice, image._image, &reqs);

			VkMemoryAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = reqs.size;
			allocInfo.memoryTypeIndex = PhysicalDevice::GetMemoryTypeIndex(physicalDevice, reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			VkDeviceMemory mem;
			vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &mem);
			vkBindImageMemory(logicalDevice, image._image, mem, 0);

			auto& imageViewCreateInfo = image._viewCreateInfo;
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imageViewCreateInfo.image = image._image;
			imageViewCreateInfo.format = imageCreateInfo.format;
			imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
			imageViewCreateInfo.subresourceRange.levelCount = 1;
			imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			imageViewCreateInfo.subresourceRange.layerCount = 1;
			imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			vkCreateImageView(logicalDevice, &imageViewCreateInfo, nullptr, &image._view);

			auto& samplerCreateInfo = image._samplerCreateInfo;
			samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
			samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
			samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCreateInfo.anisotropyEnable = VK_FALSE;
			samplerCreateInfo.maxAnisotropy = 1.0f;
			samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
			samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
			samplerCreateInfo.compareEnable = VK_FALSE;
			samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
			samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerCreateInfo.mipLodBias = 0.0f;
			samplerCreateInfo.minLod = 0.0f;
			samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
			vkCreateSampler(logicalDevice, &samplerCreateInfo, nullptr, &image._sampler);

			return image;
		}
	};

	/**
	 * @brief Represents a scene-level PBR material.
	 */
	class Material {

	public:

		/**
		 * @brief Identifier for debug purposes.
		 */
		std::string _name;

		/**
		 * @brief Base color texture data.
		 */
		Image _albedo;

		/**
		 * @brief Roughness texture data.
		 */
		Image _roughness;

		/**
		 * @brief Metalness texture data.
		 */
		Image _metalness;

		/**
		 * @brief Default constructor.
		 */
		Material() = default;

		Material(VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice) {
			_name = "DefaultMaterial";

			_albedo = Image::SolidColor(logicalDevice, physicalDevice, 255, 0, 255, 255);
			_roughness = Image::SolidColor(logicalDevice, physicalDevice, 125, 125, 125, 255);
			_metalness = Image::SolidColor(logicalDevice, physicalDevice, 125, 125, 125, 255);
		}
	};

	/**
	  * @brief Represents vertex attributes, such as positions, normals and UV coordinates.
	  */
	class Vertex {
	public:

		/**
		 * @brief Used to identify vertex attribute types.
		 */
		enum class AttributeType {
			Position, Normal, UV
		};

		/**
		 * @brief Attribute describing the object-space position of the vertex in the engine's coordinate system (X right, Y up, Z forward).
		 */
		glm::vec3 _position;

		/**
		 * @brief Attribute describing the object-space normal vector of the vertex in the engine's coordinate system (X right, Y up, Z forward).
		 */
		glm::vec3 _normal;

		/**
		 * @brief Attribute describing the UV coordinates of the vertex in the UV coordinate system, where the origin (U = 0, V = 0) is the bottom left of the 2D space.
		 */
		glm::vec2 _uvCoord;

		static size_t OffsetOf(const AttributeType& attributeType) {
			switch (attributeType) {
			case AttributeType::Position:
				return 0;
			case AttributeType::Normal:
				return sizeof(_position);
			case AttributeType::UV:
				return sizeof(_position) + sizeof(_normal);
			default: return 0;
			}
		}
	};

	/**
	 * @brief Describes the structure of a single descriptor set to provide context on how the shader should treat the descriptor set.
	 * To make an analogy: if descriptor sets were cars, the blueprint used to fabricate them would be the descriptor set layout, and the
	 * people inside the car would be the descriptors (the data the sets contain).
	 * A descriptor set is a group of descriptors. Each descriptor in the set is an entry in the shader's input variables, and can be either
	 * a buffer or an image.
	 */
	class DescriptorSetLayout {
	public:

		/**
		 * @brief To make it more recognizable when debugging.
		 */
		std::string _name = "";

		/**
		 * @brief ID used to make the pipeline layout and the order the descriptor sets are bound to the pipeline (via vkBindDescriptorSets) match.
		 * In shaders, this corresponds to the "set" decorator when defining an input variable, like in this line of GLSL:
		 * layout(set = 3, binding = 0) uniform sampler2D albedoMap;
		 */
		int _id;

		/**
		 * @brief Layout handle.
		 */
		VkDescriptorSetLayout _layout;

		/**
		 * @brief Used for ordering pipeline layouts in map structures.
		 */
		bool operator<(const DescriptorSetLayout& other) const {
			return _id < other._id;
		}

		/**
		 * @brief Used for ordering pipeline layouts in map structures.
		 */
		bool operator==(const DescriptorSetLayout& other) const {
			return _id == other._id;
		}
	};

	/**
	 * @brief Represents a description of how data from CPU-side memory is bound to input variables in the shaders.
	 */
	class ShaderResources {
	public:

		std::map<DescriptorSetLayout, std::vector<VkDescriptorSet>> _data;

		/**
		 * @brief Merges this shader resources instance with another.
		 */
		void MergeResources(ShaderResources& other) {
			for (const auto& pair : other._data) {
				auto it = _data.find(pair.first);
				if (it != _data.end()) {
					// Here, you can decide how to merge the values.
					// For simplicity, I'm replacing the value in map1 with the value from map2.
					it->second = pair.second;
				}
				else {
					_data.insert(pair);
				}
			}
		}

		/**
		 * @brief Operator overload to index into _data.
		 */
		std::vector<VkDescriptorSet>& operator[](const int& index) {
			auto it = std::find_if(_data.begin(), _data.end(), [index](const auto& pair) { return pair.first._id == index; });

			if (it == _data.end()) {
				Exit(1, "index not found");
			}

			return it->second;
		}
	};

	/**
	 * @brief Used by deriving classes to be able to bind shader resources (descriptor sets and push constants) to GPU-visible memory, to then be used by a pipeline.
	 * Information can be sent from application (run by the CPU) accessible memory to shader (run by the GPU) accessible memory, in order for it to be used in
	 * certain ways in the shader programs. This data structure encapsulates all Vulkan calls needed to enable that. There are 2 ways that data can be sent to
	 * the shaders: using push constants, or using descriptors.
	 * The most common and flexible way is using descriptors, but push constant are fastest.
	 */
	class IPipelineable {

	public:

		/**
		 * @brief Buffers to be used in descriptors for shader resources.
		 */
		std::vector<Buffer> _buffers;

		/**
		 * @brief Images to be used in descriptors for shader resources.
		 */
		std::vector<Image> _images;

		/**
		 * @brief See ShaderResources.
		 */
		ShaderResources _shaderResources;

		/**
		 * @brief Function that is meant for deriving classes to create shader resources and send them to GPU-visible memory (could be either RAM or VRAM).
		 * Shader resources can either be push constants or descriptors. See ShaderResources.cpp.
		 * @param physicalDevice Intended to be used to gather GPU information when allocating buffers or images.
		 * @param logicalDevice Intended to be used for binding created buffers, images, descriptors, descriptor sets etc. to the GPU.
		 */
		virtual ShaderResources CreateDescriptorSets(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, std::vector<DescriptorSetLayout>& layouts) = 0;

		/**
		 * @brief Function that is meant for deriving classes to update the shader resources that have been created with CreateDescriptorSets.
		 */
		virtual void UpdateShaderResources() = 0;
	};

	/**
	 * @brief Used by deriving classes to mark themselves as a class that is meant to bind drawing resources (vertex buffers, index buffer)
	 * to a graphics pipeline and send draw commands to the Vulkan API.
	 */
	class IDrawable {

	public:

		/**
		 * @brief Wrapper that contains a GPU-only buffer that contains vertices for drawing operations, and the vertex information.
		 */
		struct {
			/**
			 * @brief List of vertices that make up the mesh.
			 */
			std::vector<Vertex> _vertexData;

			/**
			 * @brief Buffer that stores vertex attributes. A vertex attribute is a piece of data
			 * that decorates the vertex with more information, so that the vertex shader can
			 * do more work based on it. For example a vertex attribute could be a position or a normal vector.
			 * Based on the normal vector, the vertex shader can perform lighting calculations by computing
			 * the angle between the source of the light and the normal.
			 * At the hardware level, the contents of the vertex buffer are fed into the array of shader cores,
			 * and each vertex, along with its attributes, are processed in parallel by multiple instances of the
			 * vertex shader on each thread of the cores.
			 * This buffer is intended to contain _vertexData to be bound to the graphics pipeline just before
			 * drawing the mesh in a render pass.
			 */
			Buffer _vertexBuffer;

		} _vertices;

		/**
		 * @brief Wrapper that contains a GPU-only buffer that contains face indices for drawing operations, and the face indices' information.
		 */
		struct {
			/**
			 * @brief List of indices, where each index corresponds to a vertex defined in the _vertices array above.
			 * A face (triangle) is defined by three consecutive indices in this array.
			 */
			std::vector<unsigned int> _indexData;

			/**
			 * @brief This buffer is used by Vulkan when drawing using the vkCmdDrawIndexed command; it gives Vulkan
			 * information about the order in which to draw vertices, and is intended to contain _indexData to be bound
			 * to the graphics pipeline just before drawing the mesh in a render pass.
			 */
			Buffer _indexBuffer;

		} _faceIndices;

		void CreateVertexBuffer(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, const std::vector<Vertex>& vertices) {
			_vertices._vertexData = vertices;

			// Create a temporary buffer.
			auto& buffer = _vertices._vertexBuffer;
			auto bufferSizeBytes = GetVectorSizeInBytes(vertices);
			buffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer._createInfo.size = bufferSizeBytes;
			buffer._createInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			vkCreateBuffer(logicalDevice, &buffer._createInfo, nullptr, &buffer._buffer);

			// Allocate memory for the buffer.
			VkMemoryRequirements requirements{};
			vkGetBufferMemoryRequirements(logicalDevice, buffer._buffer, &requirements);
			buffer._gpuMemory = PhysicalDevice::PhysicalDevice::AllocateMemory(physicalDevice, logicalDevice, requirements, (VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

			vkMapMemory(logicalDevice, buffer._gpuMemory, 0, GetVectorSizeInBytes(vertices), 0, &buffer._cpuMemory);

			// Map memory to the correct GPU and CPU ranges for the buffer.
			vkBindBufferMemory(logicalDevice, buffer._buffer, buffer._gpuMemory, 0);

			// Send the buffer to GPU.
			buffer._pData = (void*)vertices.data();
			buffer._sizeBytes = bufferSizeBytes;
			Buffer::CopyToDeviceMemory(logicalDevice, physicalDevice, commandPool, queue, buffer._buffer, buffer._pData, buffer._sizeBytes);
		}

		void CreateIndexBuffer(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, const std::vector<unsigned int>& indices) {
			_faceIndices._indexData = indices;

			// Create a temporary buffer.
			auto& buffer = _faceIndices._indexBuffer;
			auto bufferSizeBytes = GetVectorSizeInBytes(indices);
			buffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer._createInfo.size = bufferSizeBytes;
			buffer._createInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			vkCreateBuffer(logicalDevice, &buffer._createInfo, nullptr, &buffer._buffer);

			// Allocate memory for the buffer.
			VkMemoryRequirements requirements{};
			vkGetBufferMemoryRequirements(logicalDevice, buffer._buffer, &requirements);
			buffer._gpuMemory = PhysicalDevice::AllocateMemory(physicalDevice, logicalDevice, requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			// Map memory to the correct GPU and CPU ranges for the buffer.
			vkBindBufferMemory(logicalDevice, buffer._buffer, buffer._gpuMemory, 0);

			// TODO: send the buffer to GPU.
			buffer._pData = (void*)indices.data();
			buffer._sizeBytes = bufferSizeBytes;
			Buffer::CopyToDeviceMemory(logicalDevice, physicalDevice, commandPool, queue, buffer._buffer, buffer._pData, buffer._sizeBytes);
		}

		/**
		 * @brief Deriving classes should implement this method to bind their vertex and index buffers to a graphics pipeline and draw them via Vulkan draw calls.
		 */
		virtual void Draw(VkPipelineLayout& pipelineLayout, VkCommandBuffer& drawCommandBuffer) = 0;
	};

	/**
	 * @brief Represents an infinitesimally small light source.
	 */
	class PointLight : public IVulkanUpdatable, public IPipelineable {
	public:

		/**
		 * @brief Name of the light.
		 */
		std::string _name;

		/**
		 * @brief .
		 */
		Transform _transform;

		/**
		 * @brief Data to be sent to the shaders.
		 */
		struct {
			glm::vec3 position;
			glm::vec4 colorIntensity;
		} _lightData;

		/**
		 * @brief X, Y, Z represent red, blue and green for light color, while the W component represents
		 * light intensity.
		 */
		glm::vec4 _colorIntensity;

		/**
		 * @brief Default constructor.
		 */
		PointLight() = default;

		PointLight(std::string name) {
			_name = name;
			_transform.SetPosition(glm::vec3(3.0f, 10.0f, -10.0f));
		}

		ShaderResources CreateDescriptorSets(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, std::vector<DescriptorSetLayout>& layouts) {
			auto descriptorSetID = 2;

			// Create a temporary buffer.
			Buffer buffer{};
			auto bufferSizeBytes = sizeof(_lightData);
			buffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer._createInfo.size = bufferSizeBytes;
			buffer._createInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			vkCreateBuffer(logicalDevice, &buffer._createInfo, nullptr, &buffer._buffer);

			// Allocate memory for the buffer.
			VkMemoryRequirements requirements{};
			vkGetBufferMemoryRequirements(logicalDevice, buffer._buffer, &requirements);
			buffer._gpuMemory = PhysicalDevice::AllocateMemory(physicalDevice, logicalDevice, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

			// Map memory to the correct GPU and CPU ranges for the buffer.
			vkBindBufferMemory(logicalDevice, buffer._buffer, buffer._gpuMemory, 0);
			vkMapMemory(logicalDevice, buffer._gpuMemory, 0, bufferSizeBytes, 0, &buffer._cpuMemory);
			memcpy(buffer._cpuMemory, &_lightData, bufferSizeBytes);

			_buffers.push_back(buffer);

			VkDescriptorPool descriptorPool{};
			VkDescriptorPoolSize poolSizes[1] = { VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 } };
			VkDescriptorPoolCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			createInfo.maxSets = (uint32_t)1;
			createInfo.poolSizeCount = (uint32_t)1;
			createInfo.pPoolSizes = poolSizes;
			vkCreateDescriptorPool(logicalDevice, &createInfo, nullptr, &descriptorPool);

			// Create the descriptor set.
			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = (uint32_t)1;
			allocInfo.pSetLayouts = &layouts[descriptorSetID]._layout;
			VkDescriptorSet descriptorSet;
			vkAllocateDescriptorSets(logicalDevice, &allocInfo, &descriptorSet);

			// Update the descriptor set's data.
			VkDescriptorBufferInfo bufferInfo{ buffer._buffer, 0, buffer._createInfo.size };
			VkWriteDescriptorSet writeInfo = {};
			writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeInfo.dstSet = descriptorSet;
			writeInfo.descriptorCount = 1;
			writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writeInfo.pBufferInfo = &bufferInfo;
			writeInfo.dstBinding = 0;
			vkUpdateDescriptorSets(logicalDevice, 1, &writeInfo, 0, nullptr);

			auto descriptorSets = std::vector<VkDescriptorSet>{ descriptorSet };
			_shaderResources._data.try_emplace(layouts[descriptorSetID], descriptorSets);
			return _shaderResources;
		}

		void UpdateShaderResources() {
			_lightData.position = _transform.Position();
			_lightData.colorIntensity = glm::vec4(1.0f, 1.0f, 1.0f, 15000.0f);
			memcpy(_buffers[0]._cpuMemory, &_lightData, sizeof(_lightData));
		}

		void Update(VulkanContext& vkContext) {
			auto& input = KeyboardMouse::Instance();

			if (input.IsKeyHeldDown(GLFW_KEY_UP)) {
				_transform.Translate(_transform.Forward() * 1.5f);
			}

			if (input.IsKeyHeldDown(GLFW_KEY_DOWN)) {
				_transform.Translate(_transform.Forward() * -1.5f);
			}

			if (input.IsKeyHeldDown(GLFW_KEY_LEFT)) {
				_transform.Translate(_transform.Right() * -1.5f);
			}

			if (input.IsKeyHeldDown(GLFW_KEY_RIGHT)) {
				_transform.Translate(_transform.Right() * 1.5f);
			}

			auto pos = _transform.Position();
			UpdateShaderResources();
		}
	};

	// All the data that this boxblur application needs to do its thing.
	class BoxBlur {

	private:
		// Input image size.
		unsigned int _imageWidthPixels;
		unsigned int _imageHeightPixels;
		unsigned int _radiusPixels = 60;
		unsigned char* _loadedImage;
		VkPhysicalDevice _physicalDevice; // A handle for the graphics card used in the application.

		uint32_t _workGroupCount[3]; // Size of the 3D lattice of workgroups.
		uint32_t _workGroupSize[3];  // Size of the 3D lattice of threads in each workgroup.
		uint32_t _coalescedMemory;

		// Bridging information, that allows shaders to freely access resources like buffers and images.
		VkDescriptorPool _descriptorPool;
		VkDescriptorSetLayout _descriptorSetLayout;
		VkDescriptorSet _descriptorSet;

		// Pipeline handles.
		VkPipelineLayout _pipelineLayout;
		VkPipeline _pipeline;

		// Input and output buffers.
		uint32_t _inputBufferCount;
		VkBuffer _inputBuffer;
		VkDeviceMemory _inputBufferDeviceMemory;
		uint32_t _outputBufferCount;
		VkBuffer _outputBuffer;
		VkDeviceMemory _outputBufferDeviceMemory;

		// Vulkan dependencies
		VkInstance _instance;                                                // A connection between the application and the Vulkan library .
		VkDevice _device;                                                    // A logical device, interacting with the physical device.
		VkPhysicalDeviceProperties _physicalDeviceProperties;                // Basic device properties.
		VkPhysicalDeviceMemoryProperties _physicalDeviceMemoryProperties;    // Basic memory properties of the device.
		VkDebugUtilsMessengerEXT _debugMessenger;                            // Extension for debugging.
		uint32_t _queueFamilyIndex;                                          // If multiple queues are available, specify the used one.
		VkQueue _queue;                                                      // A place, where all operations are submitted.
		VkCommandPool _commandPool;                                          // An opaque object that command buffer memory is allocated from.
		VkFence _fence;                                                      // A fence used to synchronize dispatches.

		static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
			std::cout << "validation layer: " << pCallbackData->pMessage << std::endl;
			return VK_FALSE;
		}

		VkResult SetupDebugUtilsMessenger() {
			VkDebugUtilsMessengerCreateInfoEXT
				debugUtilsMessengerCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
					(const void*)NULL,
					(VkDebugUtilsMessengerCreateFlagsEXT)0,
					(VkDebugUtilsMessageSeverityFlagsEXT)VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
					(VkDebugUtilsMessageTypeFlagsEXT)VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
					(PFN_vkDebugUtilsMessengerCallbackEXT)debugCallback,
					(void*)NULL };

			PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_instance, "vkCreateDebugUtilsMessengerEXT");

			if (func != NULL) {
				if (func(_instance, &debugUtilsMessengerCreateInfo, NULL, &_debugMessenger) != VK_SUCCESS) {
					return VK_ERROR_INITIALIZATION_FAILED;
				}
			}
			else {
				return VK_ERROR_EXTENSION_NOT_PRESENT;
			}

			return VK_SUCCESS;
		}

		void DestroyDebugUtilsMessengerEXT(VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
			//pointer to the function, as it is not part of the core. Function destroys debugging messenger
			PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_instance, "vkDestroyDebugUtilsMessengerEXT");
			if (func != NULL) {
				func(_instance, debugMessenger, pAllocator);
			}
		}

		VkResult CreateComputePipeline(VkBuffer* shaderBuffersArray, VkDeviceSize* arrayOfSizesOfEachBuffer, const char* shaderFilename) {
			// Create an application interface to Vulkan. This function binds the shader to the compute pipeline, so it can be used as a part of the command buffer later.
			VkResult res = VK_SUCCESS;
			uint32_t descriptorCount = 2;

			// We have two storage buffer objects in one set in one pool.
			VkDescriptorPoolSize descriptorPoolSize = { (VkDescriptorType)VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
														   (uint32_t)descriptorCount };

			const VkDescriptorType descriptorTypes[2] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
															  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };

			VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
											   (const void*)NULL,
											   (VkDescriptorPoolCreateFlags)0,
											   (uint32_t)1,
											   (uint32_t)1,
											   (const VkDescriptorPoolSize*)&descriptorPoolSize };

			res = vkCreateDescriptorPool(_device, &descriptorPoolCreateInfo, NULL, &_descriptorPool);
			if (res != VK_SUCCESS) return res;

			// Specify each object from the set as a storage buffer.
			VkDescriptorSetLayoutBinding* descriptorSetLayoutBindings =
				(VkDescriptorSetLayoutBinding*)malloc(descriptorCount * sizeof(VkDescriptorSetLayoutBinding));
			for (uint32_t i = 0; i < descriptorCount; ++i) {
				descriptorSetLayoutBindings[i].binding = (uint32_t)i;
				descriptorSetLayoutBindings[i].descriptorType = (VkDescriptorType)descriptorTypes[i];
				descriptorSetLayoutBindings[i].descriptorCount = (uint32_t)1;
				descriptorSetLayoutBindings[i].stageFlags = (VkShaderStageFlags)VK_SHADER_STAGE_COMPUTE_BIT;
				descriptorSetLayoutBindings[i].pImmutableSamplers = (const VkSampler*)NULL;
			}

			VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
													(const void*)NULL,
													(VkDescriptorSetLayoutCreateFlags)0,
													(uint32_t)descriptorCount,
													(const VkDescriptorSetLayoutBinding*)descriptorSetLayoutBindings };
			//create layout
			res = vkCreateDescriptorSetLayout(_device, &descriptorSetLayoutCreateInfo, NULL, &_descriptorSetLayout);
			if (res != VK_SUCCESS) return res;
			free(descriptorSetLayoutBindings);

			//provide the layout with actual buffers and their sizes
			VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
												(const void*)NULL,
												(VkDescriptorPool)_descriptorPool,
												(uint32_t)1,
												(const VkDescriptorSetLayout*)&_descriptorSetLayout };
			res = vkAllocateDescriptorSets(_device, &descriptorSetAllocateInfo, &_descriptorSet);
			if (res != VK_SUCCESS) return res;

			for (uint32_t i = 0; i < descriptorCount; ++i) {

				VkDescriptorBufferInfo descriptorBufferInfo = { shaderBuffersArray[i],
												 0,
												 arrayOfSizesOfEachBuffer[i] };

				VkWriteDescriptorSet writeDescriptorSet = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
												 (const void*)NULL,
												 (VkDescriptorSet)_descriptorSet,
												 (uint32_t)i,
												 (uint32_t)0,
												 (uint32_t)1,
												 (VkDescriptorType)descriptorTypes[i],
												 0,
												 (const VkDescriptorBufferInfo*)&descriptorBufferInfo,
												 (const VkBufferView*)NULL };
				vkUpdateDescriptorSets((VkDevice)_device,
					(uint32_t)1,
					(const VkWriteDescriptorSet*)&writeDescriptorSet,
					(uint32_t)0,
					(const VkCopyDescriptorSet*)NULL);
			}

			VkPushConstantRange range;
			range.offset = 0;
			range.size = sizeof(unsigned int) * 3;
			range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
											   (const void*)NULL,
											   (VkPipelineLayoutCreateFlags)0,
											   (uint32_t)1,
											   (const VkDescriptorSetLayout*)&_descriptorSetLayout,
											   (uint32_t)1,
											   (const VkPushConstantRange*)&range };

			//create pipeline layout
			res = vkCreatePipelineLayout(_device, &pipelineLayoutCreateInfo, NULL, &_pipelineLayout);
			if (res != VK_SUCCESS) return res;

			// Define the specialization constant values
			VkSpecializationMapEntry* specMapEntry = (VkSpecializationMapEntry*)malloc(sizeof(VkSpecializationMapEntry) * 3);
			VkSpecializationMapEntry specConstant0 = { 0, 0, sizeof(uint32_t) };
			VkSpecializationMapEntry specConstant1 = { 1, sizeof(uint32_t), sizeof(uint32_t) };
			VkSpecializationMapEntry specConstant2 = { 2, sizeof(uint32_t) * 2, sizeof(uint32_t) };
			specMapEntry[0] = specConstant0;
			specMapEntry[1] = specConstant1;
			specMapEntry[2] = specConstant2;
			VkSpecializationInfo specializationInfo = { 3, specMapEntry, sizeof(uint32_t) * 3, _workGroupSize };

			std::ifstream file(shaderFilename, std::ios::ate | std::ios::binary);
			VkShaderModule shaderModule = nullptr;
			if (file.is_open()) {
				std::vector<char> fileBytes(file.tellg());
				file.seekg(0, std::ios::beg);
				file.read(fileBytes.data(), fileBytes.size());
				file.close();

				VkShaderModuleCreateInfo createInfo = {};
				createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
				createInfo.codeSize = fileBytes.size();
				createInfo.pCode = (uint32_t*)fileBytes.data();

				if (vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
					std::cout << "failed to create shader module for " << shaderFilename << std::endl;
				}
			}
			else {
				std::cout << "failed to open file " << shaderFilename << std::endl;
			}

			VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
													(const void*)NULL,
													(VkPipelineShaderStageCreateFlags)0,
													(VkShaderStageFlagBits)VK_SHADER_STAGE_COMPUTE_BIT,
													(VkShaderModule)shaderModule,
													(const char*)"main",
													(VkSpecializationInfo*)&specializationInfo };

			VkComputePipelineCreateInfo computePipelineCreateInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
												(const void*)NULL,
												(VkPipelineCreateFlags)0,
												pipelineShaderStageCreateInfo,
												(VkPipelineLayout)_pipelineLayout,
												(VkPipeline)NULL,
												(int32_t)0 };

			//create pipeline
			res = vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, NULL, &_pipeline);
			if (res != VK_SUCCESS) return res;

			vkDestroyShaderModule(_device, pipelineShaderStageCreateInfo.module, NULL);
			return res;
		}

		VkResult Dispatch() {
			VkResult res = VK_SUCCESS;
			VkCommandBuffer commandBuffer = { 0 };

			// Create a command buffer to be executed on the GPU.
			VkCommandBufferAllocateInfo commandBufferAllocateInfo;
			commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			commandBufferAllocateInfo.pNext = (const void*)NULL;
			commandBufferAllocateInfo.commandPool = _commandPool;
			commandBufferAllocateInfo.level = (VkCommandBufferLevel)VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			commandBufferAllocateInfo.commandBufferCount = (uint32_t)1;
			res = vkAllocateCommandBuffers(_device, &commandBufferAllocateInfo, &commandBuffer);

			// Begin command buffer recording.
			VkCommandBufferBeginInfo commandBufferBeginInfo;
			commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			commandBufferBeginInfo.pNext = (const void*)NULL;
			commandBufferBeginInfo.flags = (VkCommandBufferUsageFlags)VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			commandBufferBeginInfo.pInheritanceInfo = (const VkCommandBufferInheritanceInfo*)NULL;
			res = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);

			if (res != VK_SUCCESS) return res;

			unsigned int pushConstants[3] = { _imageWidthPixels, _imageHeightPixels, _radiusPixels };
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _pipeline);
			vkCmdPushConstants(commandBuffer, _pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(unsigned int) * 3, pushConstants);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _pipelineLayout, 0, 1, &_descriptorSet, 0, NULL);
			vkCmdDispatch(commandBuffer, _workGroupCount[0], _workGroupCount[1], _workGroupCount[2]);

			// End command buffer recording.
			res = vkEndCommandBuffer(commandBuffer);
			if (res != VK_SUCCESS) return res;

			// Submit the command buffer.
			VkSubmitInfo submitInfo;
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.pNext = (const void*)NULL;
			submitInfo.waitSemaphoreCount = (uint32_t)0;
			submitInfo.pWaitSemaphores = (const VkSemaphore*)NULL;
			submitInfo.pWaitDstStageMask = (const VkPipelineStageFlags*)NULL;
			submitInfo.commandBufferCount = (uint32_t)1;
			submitInfo.pCommandBuffers = (const VkCommandBuffer*)&commandBuffer;
			submitInfo.signalSemaphoreCount = (uint32_t)0;
			submitInfo.pSignalSemaphores = (const VkSemaphore*)NULL;
			clock_t t;
			t = clock();
			res = vkQueueSubmit(_queue, 1, &submitInfo, _fence);
			if (res != VK_SUCCESS) return res;

			// Wait for the fence that was configured to signal when execution has finished.
			res = vkWaitForFences(_device, 1, &_fence, VK_TRUE, 30000000000);
			if (res != VK_SUCCESS) return res;

			// Calculate the time in milliseconds it took to execute and print the result to the console.
			t = clock() - t;
			double time = ((double)t) / CLOCKS_PER_SEC * 1000;

			// Destroy the command buffer and reset the fence's status. A fence can be signalled or unsignalled. Once a fence is signalled, control is given back to the
			// the code that called vkWaitForFences().
			res = vkResetFences(_device, 1, &_fence);
			if (res != VK_SUCCESS) return res;

			vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);
			return res;
		}

		VkResult FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t memoryTypeBits, VkMemoryPropertyFlags memoryPropertyFlags, uint32_t* memoryTypeIndex) {
			//find memory with specified properties
			VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties = { 0 };

			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);

			for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; ++i) {
				if ((memoryTypeBits & (1 << i)) && ((physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags)) {
					memoryTypeIndex[0] = i;
					return VK_SUCCESS;
				}
			}
			return VK_ERROR_INITIALIZATION_FAILED;
		}

		VkResult AllocateGPUOnlyBuffer(VkBufferUsageFlags bufferUsageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize bufferSizeBytes, VkBuffer* outBuffer, VkDeviceMemory* outDeviceMemory) {
			//allocate the buffer used by the GPU with specified properties
			VkResult res = VK_SUCCESS;
			uint32_t queueFamilyIndices;
			VkBufferCreateInfo bufferCreateInfo;
			bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferCreateInfo.pNext = (const void*)NULL;
			bufferCreateInfo.flags = (VkBufferCreateFlags)0;
			bufferCreateInfo.size = (VkDeviceSize)bufferSizeBytes;
			bufferCreateInfo.usage = (VkBufferUsageFlags)bufferUsageFlags;
			bufferCreateInfo.sharingMode = (VkSharingMode)VK_SHARING_MODE_EXCLUSIVE;
			bufferCreateInfo.queueFamilyIndexCount = (uint32_t)1;
			bufferCreateInfo.pQueueFamilyIndices = (const uint32_t*)&queueFamilyIndices;

			res = vkCreateBuffer(_device, &bufferCreateInfo, NULL, outBuffer);
			if (res != VK_SUCCESS) return res;

			VkMemoryRequirements memoryRequirements = { 0 };
			vkGetBufferMemoryRequirements(_device, *outBuffer, &memoryRequirements);

			//find memory with specified properties
			uint32_t memoryTypeIndex = 0xFFFFFFFF;
			vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &_physicalDeviceMemoryProperties);

			for (uint32_t i = 0; i < _physicalDeviceMemoryProperties.memoryTypeCount; ++i) {
				if ((memoryRequirements.memoryTypeBits & (1 << i)) && ((_physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags)) {
					memoryTypeIndex = i;
					break;
				}
			}
			if (0xFFFFFFFF == memoryTypeIndex) return VK_ERROR_INITIALIZATION_FAILED;

			VkMemoryAllocateInfo memoryAllocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
										 (const void*)NULL,
										 (VkDeviceSize)memoryRequirements.size,
										 (uint32_t)memoryTypeIndex };

			res = vkAllocateMemory(_device, &memoryAllocateInfo, NULL, outDeviceMemory);
			if (res != VK_SUCCESS) return res;

			res = vkBindBufferMemory(_device, *outBuffer, *outDeviceMemory, 0);
			return res;
		}

		VkResult UploadDataToGPU(void* data, VkBuffer* outBuffer, VkDeviceSize bufferSizeBytes) {
			VkResult res = VK_SUCCESS;

			//a function that transfers data from the CPU to the GPU using staging buffer,
				//because the GPU memory is not host-coherent
			VkDeviceSize stagingBufferSize = bufferSizeBytes;
			VkBuffer stagingBuffer = { 0 };
			VkDeviceMemory stagingBufferMemory = { 0 };
			res = AllocateGPUOnlyBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				stagingBufferSize,
				&stagingBuffer,
				&stagingBufferMemory);
			if (res != VK_SUCCESS) return res;

			void* stagingData;
			res = vkMapMemory(_device, stagingBufferMemory, 0, stagingBufferSize, 0, &stagingData);
			if (res != VK_SUCCESS) return res;
			memcpy(stagingData, data, stagingBufferSize);
			vkUnmapMemory(_device, stagingBufferMemory);

			VkCommandBufferAllocateInfo commandBufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
												(const void*)NULL,
												(VkCommandPool)_commandPool,
												(VkCommandBufferLevel)VK_COMMAND_BUFFER_LEVEL_PRIMARY,
												(uint32_t)1 };
			VkCommandBuffer commandBuffer = { 0 };
			res = vkAllocateCommandBuffers(_device, &commandBufferAllocateInfo, &commandBuffer);
			if (res != VK_SUCCESS) return res;



			VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
											 (const void*)NULL,
											 (VkCommandBufferUsageFlags)VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
											 (const VkCommandBufferInheritanceInfo*)NULL };
			res = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
			if (res != VK_SUCCESS) return res;
			VkBufferCopy copyRegion = { 0, 0, stagingBufferSize };
			vkCmdCopyBuffer(commandBuffer, stagingBuffer, *outBuffer, 1, &copyRegion);
			res = vkEndCommandBuffer(commandBuffer);
			if (res != VK_SUCCESS) return res;

			VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO,
								 (const void*)NULL,
								 (uint32_t)0,
								 (const VkSemaphore*)NULL,
								 (const VkPipelineStageFlags*)NULL,
								 (uint32_t)1,
								 (const VkCommandBuffer*)&commandBuffer,
								 (uint32_t)0,
								 (const VkSemaphore*)NULL };

			res = vkQueueSubmit(_queue, 1, &submitInfo, _fence);
			if (res != VK_SUCCESS) return res;


			res = vkWaitForFences(_device, 1, &_fence, VK_TRUE, 100000000000);
			if (res != VK_SUCCESS) return res;

			res = vkResetFences(_device, 1, &_fence);
			if (res != VK_SUCCESS) return res;

			vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);
			vkDestroyBuffer(_device, stagingBuffer, NULL);
			vkFreeMemory(_device, stagingBufferMemory, NULL);
			return res;
		}

		VkResult DownloadDataFromGPU(void* data, VkBuffer* outBuffer, VkDeviceSize bufferSize) {
			VkResult res = VK_SUCCESS;
			VkCommandBuffer commandBuffer = { 0 };

			// A function that transfers data from the GPU to the CPU using staging buffer, because GPU memory is not host-coherent (meaning it is
			// not synced with the contents of CPU memory).
			VkDeviceSize stagingBufferSize = bufferSize;
			VkBuffer stagingBuffer = { 0 };
			VkDeviceMemory stagingBufferMemory = { 0 };
			res = AllocateGPUOnlyBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				stagingBufferSize,
				&stagingBuffer,
				&stagingBufferMemory);
			if (res != VK_SUCCESS) return res;

			VkCommandBufferAllocateInfo commandBufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
												(const void*)NULL,
												(VkCommandPool)_commandPool,
												(VkCommandBufferLevel)VK_COMMAND_BUFFER_LEVEL_PRIMARY,
												(uint32_t)1 };
			res = vkAllocateCommandBuffers(_device, &commandBufferAllocateInfo, &commandBuffer);
			if (res != VK_SUCCESS) return res;

			VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
											 (const void*)NULL,
											 (VkCommandBufferUsageFlags)VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
											 (const VkCommandBufferInheritanceInfo*)NULL };
			res = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
			if (res != VK_SUCCESS) return res;

			VkBufferCopy copyRegion = { (VkDeviceSize)0,
								 (VkDeviceSize)0,
								 (VkDeviceSize)stagingBufferSize };

			vkCmdCopyBuffer(commandBuffer, outBuffer[0], stagingBuffer, 1, &copyRegion);
			vkEndCommandBuffer(commandBuffer);

			VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO,
								 (const void*)NULL,
								 (uint32_t)0,
								 (const VkSemaphore*)NULL,
								 (const VkPipelineStageFlags*)NULL,
								 (uint32_t)1,
								 (const VkCommandBuffer*)&commandBuffer,
								 (uint32_t)0,
								 (const VkSemaphore*)NULL };

			res = vkQueueSubmit(_queue, 1, &submitInfo, _fence);
			if (res != VK_SUCCESS) return res;

			res = vkWaitForFences(_device, 1, &_fence, VK_TRUE, 100000000000);
			if (res != VK_SUCCESS) return res;

			res = vkResetFences(_device, 1, &_fence);
			if (res != VK_SUCCESS) return res;

			vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);

			void* stagingData;
			res = vkMapMemory(_device, stagingBufferMemory, 0, stagingBufferSize, 0, &stagingData);
			if (res != VK_SUCCESS) return res;

			memcpy(data, stagingData, stagingBufferSize);
			vkUnmapMemory(_device, stagingBufferMemory);

			vkDestroyBuffer(_device, stagingBuffer, NULL);
			vkFreeMemory(_device, stagingBufferMemory, NULL);
			return res;
		}

		VkResult GetComputeQueueFamilyIndex() {
			//find a queue family for a selected GPU, select the first available for use
			uint32_t queueFamilyCount;
			vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyCount, NULL);

			VkQueueFamilyProperties* queueFamilies = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);

			vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyCount, queueFamilies);
			uint32_t i = 0;
			for (; i < queueFamilyCount; i++) {
				VkQueueFamilyProperties props = queueFamilies[i];

				if (props.queueCount > 0 && (props.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
					break;
				}
			}
			free(queueFamilies);
			if (i == queueFamilyCount) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}
			_queueFamilyIndex = i;
			return VK_SUCCESS;
		}

		VkResult CreateLogicalDevice() {
			//create logical device representation
			VkResult res = VK_SUCCESS;
			res = GetComputeQueueFamilyIndex();
			if (res != VK_SUCCESS) return res;
			vkGetDeviceQueue(_device, _queueFamilyIndex, 0, &_queue);
			return res;

			float queuePriorities = 1.0;
			VkDeviceQueueCreateInfo deviceQueueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					(const void*)NULL,
					(VkDeviceQueueCreateFlags)0,
					(uint32_t)_queueFamilyIndex,
					(uint32_t)1,
					(const float*)&queuePriorities };

			VkPhysicalDeviceFeatures physicalDeviceFeatures = { 0 };
			physicalDeviceFeatures.shaderFloat64 = VK_TRUE;//this enables double precision support in shaders 

			VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
					(const void*)NULL,
					(VkDeviceCreateFlags)0,
					(uint32_t)1,
					(const VkDeviceQueueCreateInfo*)&deviceQueueCreateInfo,
					(uint32_t)0,
					(const char* const*)NULL,
					(uint32_t)0,
					(const char* const*)NULL,
					(const VkPhysicalDeviceFeatures*)&physicalDeviceFeatures };

			res = vkCreateDevice(_physicalDevice, &deviceCreateInfo, NULL, &_device);
			if (res != VK_SUCCESS) return res;

		}

		void InitializeVulkan() {
			VkResult res = VK_SUCCESS;

			// Create the instance - a connection between the application and the Vulkan library.
			/*res = CreateInstance(vkGPU);
			std::cout << "Instance creation failed, error code: " << res;*/

			// Set up the debugging messenger.
			/*res = SetupDebugUtilsMessenger(vkGPU);
			std::cout << "Debug utils messenger creation failed, error code: " << res;*/

			// Check if there are GPUs that support Vulkan and select one.
			/*res = FindGPU(vkGPU);
			std::cout << "Physical device not found, error code: " << res;*/

			// Create logical device representation.
			if (CreateLogicalDevice() != VK_SUCCESS) {
				std::cout << "Logical device creation failed." << std::endl;
			}

			// Create a fence for synchronization.
			VkFenceCreateInfo fenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, 0 };
			if (vkCreateFence(_device, &fenceCreateInfo, NULL, &_fence)) {
				std::cout << "Fence creation failed." << std::endl;
			}

			// Create a structure from which command buffer memory is allocated from.
			VkCommandPoolCreateInfo commandPoolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, _queueFamilyIndex };
			if (vkCreateCommandPool(_device, &commandPoolCreateInfo, NULL, &_commandPool)) {
				std::cout << "Command Pool Creation failed." << std::endl;
			}
		}

		void CalculateWorkGroupCountAndSize() {
			// Prepare variables.
			uint32_t maxWorkGroupInvocations = _physicalDeviceProperties.limits.maxComputeWorkGroupInvocations;
			uint32_t* maxWorkGroupSize = _physicalDeviceProperties.limits.maxComputeWorkGroupSize;
			uint32_t* maxWorkGroupCount = _physicalDeviceProperties.limits.maxComputeWorkGroupCount;
			uint32_t workGroupSize[3] = { 1, 1, 1 };
			uint32_t workGroupCount[3] = { 1, 1, 1 };

			// Use the work group size first, as that directly controls the amount of threads 1 to 1.
			uint32_t totalWorkGroupSize = workGroupSize[0] * workGroupSize[1] * workGroupSize[2];
			for (int i = 0; i < 3; ++i) {
				for (; workGroupSize[i] < maxWorkGroupSize[i]; ++workGroupSize[i]) {
					totalWorkGroupSize = workGroupSize[0] * workGroupSize[1] * workGroupSize[2];
					if (totalWorkGroupSize >= _inputBufferCount || totalWorkGroupSize == maxWorkGroupInvocations) { break; }
				}
			}

			// If one workgroup still doesn't do it, use multiple workgroups with the size of each one calculated earlier.
			if (totalWorkGroupSize < _inputBufferCount) {
				for (int i = 0; i < 3; ++i) {
					for (; workGroupCount[i] < maxWorkGroupCount[i]; ++workGroupCount[i]) {
						if (((workGroupCount[0] * workGroupCount[1] * workGroupCount[2]) * totalWorkGroupSize) >= _inputBufferCount) { break; }
					}
				}
			}

			// Return the calculated size and count in the output params.
			_workGroupCount[0] = workGroupCount[0];
			_workGroupCount[1] = workGroupCount[1];
			_workGroupCount[2] = workGroupCount[2];
			_workGroupSize[0] = workGroupSize[0];
			_workGroupSize[1] = workGroupSize[1];
			_workGroupSize[2] = workGroupSize[2];
		}

	public:
		unsigned char* Run(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, unsigned char* loadedImage, unsigned int imageWidthPixels, unsigned int imageHeightPixels, unsigned int radiusPixels = 15) {
			_physicalDevice = physicalDevice;
			_device = logicalDevice;
			_imageWidthPixels = imageWidthPixels;
			_imageHeightPixels = imageHeightPixels;
			_radiusPixels = radiusPixels;

			InitializeVulkan();

			// Get device properties and memory properties, if needed.
			vkGetPhysicalDeviceProperties(physicalDevice, &_physicalDeviceProperties);
			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &_physicalDeviceMemoryProperties);

			// Load the image.
			int wantedComponents = 4;

			// In the stbi_load() function, comp stands for components. In a PNG image, for example, there are 4 components 
			// for each pixel, red, green, blue and alpha.
			// The image's pixels are read and stored left to right, top to bottom, relative to the image.
			// Each pixel's component is an unsigned char.
			int width = imageWidthPixels;
			int height = imageHeightPixels;

			//unsigned char* loadedImage = stbi_load("C:\\Users\\paolo.parker\\source\\repos\\celeritas-engine\\textures\\LeftFace.png", &width, &height, &componentsDetected, wantedComponents);

			// The most optimal memory has the VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT flag and is usually not accessible by the CPU on dedicated graphics cards. 
			// The memory type that allows us to access it from the CPU may not be the most optimal memory type for the graphics card itself to read from.
			// Generate some random data in the array to use as input for the compute shader.
			uint32_t inputAndOutputBufferSizeInBytes = (uint32_t)(imageWidthPixels * imageHeightPixels) * 4;

			// Prepare the input and output buffers.
			_inputBufferCount = (uint32_t)(imageWidthPixels * imageHeightPixels);
			_inputBuffer;
			_inputBufferDeviceMemory;
			_outputBufferCount = _inputBufferCount;
			_outputBuffer;
			_outputBufferDeviceMemory;

			// Calculate how many workgroups and the size of each workgroup we are going to use.
			// We want one GPU thread to operate on a single value from the input buffer, so the required thread size is the input buffer size.
			CalculateWorkGroupCountAndSize();

			//use default values if coalescedMemory = 0
			if (_coalescedMemory == 0) {
				switch (_physicalDeviceProperties.vendorID) {
				case 0x10DE://NVIDIA - change to 128 before Pascal
					_coalescedMemory = 32;
					break;
				case 0x8086://INTEL
					_coalescedMemory = 64;
					break;
				case 0x13B5://AMD
					_coalescedMemory = 64;
					break;
				default:
					_coalescedMemory = 64;
					break;
				}
			}

			// Create the input buffer.
			VkResult res = VK_SUCCESS;
			if (AllocateGPUOnlyBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_HEAP_DEVICE_LOCAL_BIT, //device local memory
				inputAndOutputBufferSizeInBytes,
				&_inputBuffer,
				&_inputBufferDeviceMemory) != VK_SUCCESS) {
				std::cout << "Input buffer allocation failed." << std::endl;
			}

			// Create the output buffer.
			if (AllocateGPUOnlyBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_HEAP_DEVICE_LOCAL_BIT, //device local memory
				inputAndOutputBufferSizeInBytes,
				&_outputBuffer,
				&_outputBufferDeviceMemory) != VK_SUCCESS) {
				std::cout << "Output buffer allocation failed." << std::endl;
			}

			// Transfer data to GPU staging buffer and thereafter sync the staging buffer with GPU local memory.
			if (UploadDataToGPU(loadedImage, &_inputBuffer, inputAndOutputBufferSizeInBytes) != VK_SUCCESS) {
				std::cout << "Failed uploading image to GPU." << std::endl;
			}

			VkBuffer buffers[2] = { _inputBuffer, _outputBuffer };
			VkDeviceSize buffersSize[2] = { inputAndOutputBufferSizeInBytes, inputAndOutputBufferSizeInBytes };

			// Create 
			//const char* shaderPath = "C:\\code\\vulkan-compute\\shaders\\Shader.spv";
			auto shaderPath = Paths::ShadersPath() /= L"compute\\BoxBlur.spv";
			if (CreateComputePipeline(buffers, buffersSize, shaderPath.string().c_str()) != VK_SUCCESS) {
				std::cout << "Application creation failed." << std::endl;
			}

			if (Dispatch() != VK_SUCCESS) {
				std::cout << "Application run failed." << std::endl;
			}

			unsigned char* shaderOutputBufferData = (unsigned char*)malloc(inputAndOutputBufferSizeInBytes);

			res = vkGetFenceStatus(_device, _fence);

			//Transfer data from GPU using staging buffer, if needed
			if (DownloadDataFromGPU(shaderOutputBufferData,
				&_outputBuffer,
				inputAndOutputBufferSizeInBytes) != VK_SUCCESS) {
				std::cout << "Failed downloading image from GPU." << std::endl;
			}

			// Print data for debugging.
			/*printf("Output buffer result: [");
			uint32_t startAndEndSize = 50;
			uint32_t i = 0;
			for (; i < outputBufferCount - 1 && i < startAndEndSize; ++i) {
				printf("%d, ", shaderOutputBufferData[i]);
			}
			if (i >= startAndEndSize) {
				printf("..., ");
				i = outputBufferCount - startAndEndSize + 1;
				for (; i < outputBufferCount - 1; ++i)
					printf("%d, ", shaderOutputBufferData[i]);
			}
			printf("%d]\n", shaderOutputBufferData[outputBufferCount - 1]);*/

			// Free resources.
			vkDestroyBuffer(_device, _inputBuffer, nullptr);
			vkFreeMemory(_device, _inputBufferDeviceMemory, nullptr);
			vkDestroyBuffer(_device, _outputBuffer, nullptr);
			vkFreeMemory(_device, _outputBufferDeviceMemory, nullptr);

			return shaderOutputBufferData;
		}

		void Destroy() {
			vkDestroyFence(_device, _fence, nullptr);
			vkDestroyDescriptorPool(_device, _descriptorPool, NULL);
			vkDestroyDescriptorSetLayout(_device, _descriptorSetLayout, NULL);
			vkDestroyPipelineLayout(_device, _pipelineLayout, NULL);
			vkDestroyPipeline(_device, _pipeline, NULL);
		}
	};

	/**
	 * @brief Used specifically for the CubicalEnvironmentMap class in order to index into its faces.
	 */
	enum class CubeMapFace {
		NONE,
		FRONT,
		RIGHT,
		BACK,
		LEFT,
		UPPER,
		LOWER
	};

	/**
	 * @brief Represents a cubical environment map, used as an image-based light source
	 * in the shaders.
	 */
	class CubicalEnvironmentMap : public IPipelineable {

	public:

		/**
		 * @brief Physical device.
		 */
		VkPhysicalDevice _physicalDevice{};

		/**
		 * @brief Logical device.
		 */
		VkDevice _logicalDevice = nullptr;

		/**
		 * @brief Front face data, including all mipmaps.
		 */
		std::vector<std::vector<unsigned char>> _front;

		/**
		 * @brief Right face data, including all mipmaps.
		 */
		std::vector<std::vector<unsigned char>> _right;

		/**
		 * @brief Back face data, including all mipmaps.
		 */
		std::vector<std::vector<unsigned char>> _back;

		/**
		 * @brief Left face data, including all mipmaps.
		 */
		std::vector<std::vector<unsigned char>> _left;

		/**
		 * @brief Upper face data, including all mipmaps.
		 */
		std::vector<std::vector<unsigned char>> _upper;

		/**
		 * @brief Lower face data, including all mipmaps.
		 */
		std::vector<std::vector<unsigned char>> _lower;

		/**
		 * @brief Data loaded from the HDRi image file.
		 */
		std::vector<std::vector<unsigned char>> _hdriImageData;

		/**
		 * @brief Width and height of the loaded HDRi image.
		 */
		VkExtent2D _hdriSizePixels{};

		/**
		 * @brief Width and height of each face's image.
		 */
		int _faceSizePixels = 0;

		/**
		 * @brief Vulkan handle to the cube map image used in the shaders. This image is meant to contain all the cube map's images as a serialized array of pixels.
		 * In order to know where, in the array of pixels, each image starts/ends and what format it's in, a sampler and image view are used.
		 */
		Image _cubeMapImage{ nullptr, 0 };

		/**
		 * @brief Number of mipmaps. This is dynamically caluclated.
		 */
		int _mipmapCount = 0;

		// Returns the 0-based pixel coordinates given the component index of an image data array. Assumes that the
		// input component index starts from 0, and that the origin of the image is at the top left corner, with X
		// increasing to the right, and Y increasing downward.
		glm::vec2 ComponentIndexToCartesian(int componentIndex, int imageWidthPixels) {
			int x = (int(componentIndex * 0.25f) % imageWidthPixels);
			int y = (int(componentIndex * 0.25f) / imageWidthPixels);
			return glm::vec2(x, y);
		}

		// Returns the 0-based component index into an image data array given the pixel coordinates of the image.
		// Assumes that X and Y both start from 0, at the top left corner of the image, and the maximum X can be is equal to imageWidthPixels-1.
		int CartesianToComponentIndex(int x, int y, int imageWidthPixels) {
			return (x + (y * imageWidthPixels)) * 4;
		}

		// Blurs an image by averaging the value of each pixel with the value of "samples" pixels at distance of "radiusPixels" pixels in a 
		// circular pattern around it. Returns the blurred image data.
		std::vector<unsigned char> BoxBlurImage(const std::vector<unsigned char>& inImageData, int widthPixels, int heightPixels, int radiusPixels) {
			// Make sure that the input values are valid.
			if (radiusPixels < 1) {
				radiusPixels = 1;
			}
			if (inImageData.data() == nullptr || inImageData.size() <= 0) {
				return std::vector<unsigned char>();
			}

			// Prepare some variables used throughout the function.
			std::vector<unsigned char> outImageData;
			int boxSideLength = (radiusPixels * 2) + 1;
			outImageData.resize(inImageData.size());

			for (int componentIndex = 0; componentIndex < inImageData.size(); componentIndex += 4) {
				auto currentPixelCoordinates = ComponentIndexToCartesian(componentIndex, widthPixels);

				// Start sampling from the top left relative to the current pixel. Coordinates increase going left to right for X
				// and top to bottom for Y.
				int sampleCoordinateY = int(currentPixelCoordinates.y - radiusPixels);
				glm::vec4 averageColor(-1.0f);
				for (int i = 0; i < boxSideLength; ++i) {
					int sampleCoordinateX = int(currentPixelCoordinates.x - radiusPixels);
					for (int j = 0; j < boxSideLength; ++j) {

						// Only sample if the coordinates fall within the range of the image.
						if (auto sampleIndex = CartesianToComponentIndex(sampleCoordinateX, sampleCoordinateY, widthPixels); sampleIndex >= 0 && sampleIndex < inImageData.size()) {
							glm::vec4 sampledColor = glm::vec4(inImageData[sampleIndex], inImageData[sampleIndex + 1], inImageData[sampleIndex + 2], inImageData[sampleIndex + 3]);

							// Only calculate the average if averageColor is initialized, otherwise just use the average color as is, to account for
							// the first time this line of code is reached.
							averageColor = averageColor.x < 0 ? sampledColor : (averageColor + sampledColor);
						}
						++sampleCoordinateX;
					}
					++sampleCoordinateY;
				}

				// Calculate the average.
				averageColor /= (boxSideLength * boxSideLength);

				// Assign the calculated average color.
				outImageData[componentIndex] = int(averageColor.r);
				outImageData[componentIndex + 1] = int(averageColor.g);
				outImageData[componentIndex + 2] = int(averageColor.b);
				outImageData[componentIndex + 3] = int(averageColor.a);
			}

			return outImageData;
		}

		std::vector<unsigned char> GenerateFaceImage(CubeMapFace face, int mipIndex, int width, int height) {
			// The world space unit vector that points in the positive X direction of the image (must be left to right), as if the image was placed in the 3D world on a square plane.
			glm::vec3 imageXWorldSpace;

			// The world space unit vector that points in the positive Y direction of the image (must be bottom to top), as if the image was placed in the 3D world on a square plane.
			glm::vec3 imageYWorldSpace;

			// The origin of the image (must be the bottom left corner) in world space, as if the image was placed in the 3D world on a square plane.
			glm::vec3 imageOriginWorldSpace;

			auto sizePixels = _faceSizePixels;
			for (int i = 0; i < mipIndex; ++i) {
				sizePixels /= 2;
			}

			std::vector<unsigned char> outImage;
			outImage.resize(sizePixels * sizePixels * 4);

			switch (face) {
			case CubeMapFace::FRONT:
				imageXWorldSpace = glm::vec3(1.0f, 0.0f, 0.0f);
				imageYWorldSpace = glm::vec3(0.0f, 1.0f, 0.0f);
				imageOriginWorldSpace = glm::vec3(-0.5f, 0.5f, 0.5f);
				break;
			case CubeMapFace::RIGHT:
				imageXWorldSpace = glm::vec3(0.0f, 0.0f, -1.0f);
				imageYWorldSpace = glm::vec3(0.0f, 1.0f, 0.0f);
				imageOriginWorldSpace = glm::vec3(0.5f, 0.5f, 0.5f);
				break;
			case CubeMapFace::BACK:
				imageXWorldSpace = glm::vec3(-1.0f, 0.0f, 0.0f);
				imageYWorldSpace = glm::vec3(0.0f, 1.0f, 0.0f);
				imageOriginWorldSpace = glm::vec3(0.5f, 0.5f, -0.5f);
				break;
			case CubeMapFace::LEFT:
				imageXWorldSpace = glm::vec3(0.0f, 0.0f, 1.0f);
				imageYWorldSpace = glm::vec3(0.0f, 1.0f, 0.0f);
				imageOriginWorldSpace = glm::vec3(-0.5f, 0.5f, -0.5f);
				break;
			case CubeMapFace::UPPER:
				imageXWorldSpace = glm::vec3(1.0f, 0.0f, 0.0f);
				imageYWorldSpace = glm::vec3(0.0f, 0.0f, -1.0f);
				imageOriginWorldSpace = glm::vec3(-0.5f, 0.5f, -0.5f);
				break;
			case CubeMapFace::LOWER:
				imageXWorldSpace = glm::vec3(1.0f, 0.0f, 0.0f);
				imageYWorldSpace = glm::vec3(0.0f, 0.0f, 1.0f);
				imageOriginWorldSpace = glm::vec3(-0.5f, -0.5f, 0.5f);
				break;
			}

			for (auto y = 0; y < sizePixels; ++y) {
				for (auto x = 0; x < sizePixels; ++x) {

					// First we calculate the cartesian coordinate of the pixel of the cube map's face we are considering, in world space.
					glm::vec3 cartesianCoordinatesOnFace;
					cartesianCoordinatesOnFace = imageOriginWorldSpace + (imageXWorldSpace * ((1.0f / sizePixels) * x));
					cartesianCoordinatesOnFace += -imageYWorldSpace * ((1.0f / sizePixels) * y);

					// Then we get the cartesian coordinates on the sphere from the cartesian coordinates on the face. You can
					// imagine a cube that contains a sphere of exactly the same radius. Only the sphere's poles and sides will
					// touch each cube's face exactly at the center. We take advantage of the fact that the radius of the sphere
					// is constant (always equal to 1 for simplicity in our case) so all we need to do is normalize the coordinates
					// on the face and we get the cartesian coordinates on the sphere. This kind of simulates shooting a ray from
					// the current pixel on the cube towards the center of the sphere, intersecting it with the sphere, and taking 
					// the coordinates of the intersection point.
					auto cartesianCoordinatesOnSphere = glm::normalize(cartesianCoordinatesOnFace);

					float localXAngleDegrees = 0.0f;
					float localYAngleDegrees = 0.0f;
					float azimuthDegrees = 0.0f;
					float zenithDegrees = 0.0f;

					// Then we calculate the spherical coordinates by using some trig.
					// Depending on the coordinate, the angles could either be negative or positive, according to the left hand rule.
					// This engine uses a left-handed coordinate system.
					switch (face) {
					case CubeMapFace::FRONT:
						localXAngleDegrees = glm::degrees(atanf(cartesianCoordinatesOnFace.x * 2.0f));
						localYAngleDegrees = (90.0f - glm::degrees(acosf(cartesianCoordinatesOnSphere.y))) * -1.0f;
						azimuthDegrees = cartesianCoordinatesOnFace.x < 0.0f ? abs(localXAngleDegrees) * -1.0f : abs(localXAngleDegrees);
						zenithDegrees = cartesianCoordinatesOnFace.y < 0.0f ? abs(localYAngleDegrees) : abs(localYAngleDegrees) * -1.0f;
						azimuthDegrees = fmodf((360.0f + azimuthDegrees), 360.0f); // This transforms any angle into the [0-360] degree domain.
						break;
					case CubeMapFace::RIGHT:
						localXAngleDegrees = glm::degrees(atanf(cartesianCoordinatesOnFace.z * 2.0f));
						localYAngleDegrees = (90.0f - glm::degrees(acosf(cartesianCoordinatesOnSphere.y))) * -1.0f;
						localXAngleDegrees = cartesianCoordinatesOnFace.z > 0.0f ? abs(localXAngleDegrees) * -1.0f : abs(localXAngleDegrees);
						localYAngleDegrees = cartesianCoordinatesOnFace.y < 0.0f ? abs(localYAngleDegrees) : abs(localYAngleDegrees) * -1.0f;
						localXAngleDegrees += 90;
						azimuthDegrees = fmodf((360.0f + localXAngleDegrees), 360.0f);
						zenithDegrees = localYAngleDegrees;
						break;
					case CubeMapFace::BACK:
						localXAngleDegrees = glm::degrees(atanf(cartesianCoordinatesOnFace.x * 2.0f));
						localYAngleDegrees = (90.0f - glm::degrees(acosf(cartesianCoordinatesOnSphere.y))) * -1.0f;
						localXAngleDegrees = cartesianCoordinatesOnFace.x > 0.0f ? abs(localXAngleDegrees) * -1.0f : abs(localXAngleDegrees);
						localYAngleDegrees = cartesianCoordinatesOnFace.y < 0.0f ? abs(localYAngleDegrees) : abs(localYAngleDegrees) * -1.0f;
						localXAngleDegrees += 180;
						azimuthDegrees = fmodf((360.0f + localXAngleDegrees), 360.0f);
						zenithDegrees = localYAngleDegrees;
						break;
					case CubeMapFace::LEFT:
						localXAngleDegrees = glm::degrees(atanf(cartesianCoordinatesOnFace.z * 2.0f));
						localYAngleDegrees = (90.0f - glm::degrees(acosf(cartesianCoordinatesOnSphere.y))) * -1.0f;
						localXAngleDegrees = cartesianCoordinatesOnFace.z < 0.0f ? abs(localXAngleDegrees) * -1.0f : abs(localXAngleDegrees);
						localYAngleDegrees = cartesianCoordinatesOnFace.y < 0.0f ? abs(localYAngleDegrees) : abs(localYAngleDegrees) * -1.0f;
						localXAngleDegrees += 270;
						azimuthDegrees = fmodf((360.0f + localXAngleDegrees), 360.0f);
						zenithDegrees = localYAngleDegrees;
						break;
					case CubeMapFace::UPPER:
						if (cartesianCoordinatesOnSphere.y < 1.0f) {
							auto temp = glm::normalize(glm::vec3(cartesianCoordinatesOnSphere.x, 0.0f, cartesianCoordinatesOnSphere.z));
							localXAngleDegrees = glm::degrees(acosf(temp.z));
							localYAngleDegrees = glm::degrees(acosf(cartesianCoordinatesOnSphere.y));
							localXAngleDegrees = cartesianCoordinatesOnFace.x < 0.0f ? abs(localXAngleDegrees) * -1.0f : abs(localXAngleDegrees);
							localYAngleDegrees = cartesianCoordinatesOnFace.y < 0.0f ? abs(localYAngleDegrees) : abs(localYAngleDegrees) * -1.0f;
							azimuthDegrees = fmodf((360.0f + localXAngleDegrees), 360.0f);
							zenithDegrees = (90.0f + localYAngleDegrees) * -1.0f;
						}
						else {
							continue;
						}
						break;
					case CubeMapFace::LOWER:
						if (cartesianCoordinatesOnSphere.y < 1.0f) {
							auto temp = glm::normalize(glm::vec3(cartesianCoordinatesOnSphere.x, 0.0f, cartesianCoordinatesOnSphere.z));
							localXAngleDegrees = glm::degrees(acosf(temp.z));
							localYAngleDegrees = glm::degrees(acosf(cartesianCoordinatesOnSphere.y));
							localXAngleDegrees = cartesianCoordinatesOnFace.x < 0.0f ? abs(localXAngleDegrees) * -1.0f : abs(localXAngleDegrees);
							localYAngleDegrees = cartesianCoordinatesOnFace.y < 0.0f ? abs(localYAngleDegrees) : abs(localYAngleDegrees) * -1.0f;
							azimuthDegrees = fmodf((360.0f + localXAngleDegrees), 360.0f);
							zenithDegrees = (90.0f - localYAngleDegrees) * -1.0f;
						}
						else {
							continue;
						}
						break;
					}

					// UV coordinates into the spherical HDRi image.
					auto uCoordinate = fmodf(0.5f + (azimuthDegrees / 360.0f), 1.0f);
					auto vCoordinate = 0.5f + (zenithDegrees / -180.0f);

					// This calculates at which pixel from the left (for U) and from the top (for V)
					// we need to fetch from the spherical HDRi.
					auto coordinateU = uCoordinate * width;
					auto coordinateV = (1.0f - vCoordinate) * height;

					int pixelNumberU = (int)ceil(coordinateU);
					if (pixelNumberU >= width || pixelNumberU < 0) {
						pixelNumberU = (int)floor(coordinateU);
						if (pixelNumberU >= width || pixelNumberU < 0) {
							continue;
						}
					}

					int pixelNumberV = (int)ceil(coordinateV);
					if (pixelNumberV >= height || pixelNumberU < 0) {
						pixelNumberV = (int)floor(coordinateV);
						if (pixelNumberV >= height || pixelNumberV < 0) {
							continue;
						}
					}

					// This calculates the index into the spherical HDRi image accounting for 2 things:
					// 1) the pixel numbers calculated above
					// 2) the fact that each pixel is actually stored in 4 separate cells that represent
					//    each channel of each individual pixel. The channels used are always 4 (RGBA).
					int componentIndex = CartesianToComponentIndex(pixelNumberU, pixelNumberV, width);

					// Fetch the channels from the spherical HDRi based on the calculations made above.
					auto red = _hdriImageData[mipIndex][componentIndex];
					auto green = _hdriImageData[mipIndex][componentIndex + 1];
					auto blue = _hdriImageData[mipIndex][componentIndex + 2];
					auto alpha = _hdriImageData[mipIndex][componentIndex + 3];

					// Assign the color fetched from the HDRi to the cube map face's image we want to generate.
					int faceComponentIndex = (x + (sizePixels * y)) * 4;
					outImage[faceComponentIndex] = red;
					outImage[faceComponentIndex + 1] = green;
					outImage[faceComponentIndex + 2] = blue;
					outImage[faceComponentIndex + 3] = alpha;
				}
			}

			return outImage;
		}

		void WriteImagesToFiles(std::filesystem::path absoluteFolderPath) {
			if (!std::filesystem::exists(absoluteFolderPath)) {
				if (std::filesystem::is_directory(absoluteFolderPath)) {
					std::filesystem::create_directories(absoluteFolderPath);
				}
				else {
					Logger::Log("provided path is not valid");
					return;
				}
			}

			auto mipmapIndex = 1;
			auto resolution = _faceSizePixels;
			auto faces = { &_front, &_right, &_back, &_left, &_upper, &_lower };
			for (auto resolution = _faceSizePixels, radius = 2; resolution > 1; resolution /= 2, radius *= 2) {

				int j = 0;
				for (auto face : faces) {
					auto halfResolution = resolution / 2;

					auto path = absoluteFolderPath / (std::string("face_") + std::to_string(j) + "_" + std::to_string(mipmapIndex) + ".png");
					auto ptr = (*face)[mipmapIndex].data();
					stbi_write_png(path.string().c_str(), halfResolution, halfResolution, 4, ptr, halfResolution * 4);
					++j;
				}

				mipmapIndex++;
			}
		}

		CubicalEnvironmentMap() = default;

		CubicalEnvironmentMap(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice) {
			_physicalDevice = physicalDevice;
			_logicalDevice = logicalDevice;
			_faceSizePixels = 512;
		}

		std::vector<unsigned char> ResizeImage(std::vector<unsigned char> image, int oldWidthPixels, int oldHeightPixels, int newWidthPixels, int newHeightPixels) {
			std::vector<unsigned char> outImage(newWidthPixels * newHeightPixels * 4);
			int ratioX = oldWidthPixels / newWidthPixels;
			int ratioY = oldHeightPixels / newHeightPixels;
			int oldImageX = 0;
			int oldImageY = 0;
			int newImageX = 0;
			int newImageY = 0;

			for (; newImageY < newHeightPixels; ++newImageY) {
				newImageX = 0;
				for (; newImageX < newWidthPixels; ++newImageX) {
					auto oldImageComponentIndex = CartesianToComponentIndex(newImageX * ratioX, newImageY * ratioY, oldWidthPixels);
					auto newImageComponentIndex = CartesianToComponentIndex(newImageX, newImageY, newWidthPixels);
					outImage[newImageComponentIndex] = image[oldImageComponentIndex];
					outImage[newImageComponentIndex + 1] = image[oldImageComponentIndex + 1];
					outImage[newImageComponentIndex + 2] = image[oldImageComponentIndex + 2];
					outImage[newImageComponentIndex + 3] = image[oldImageComponentIndex + 3];
				}
			}

			return outImage;
		}

		std::vector<unsigned char> PadImage(std::vector<unsigned char> image, int widthPixels, int heightPixels, int padAmountPixels) {
			if (padAmountPixels > std::min(widthPixels, heightPixels)) {
				Logger::Log("padding cannot exceed smallest image dimension");
				return std::vector<unsigned char>();
			}

			int newWidthPixels = 0, newHeightPixels = 0;
			newWidthPixels = widthPixels + (padAmountPixels * 2);
			newHeightPixels = heightPixels + (padAmountPixels * 2);
			std::vector<unsigned char> outImage(newWidthPixels * newHeightPixels * 4);

			// Fill the output image with the original image data.
			for (int y = 0; y < heightPixels; ++y) {
				for (int x = 0; x < widthPixels; ++x) {
					auto oldImageComponentIndex = CartesianToComponentIndex(x, y, widthPixels);
					auto outImageComponentIndex = CartesianToComponentIndex(x + padAmountPixels, y + padAmountPixels, newWidthPixels);
					outImage[outImageComponentIndex] = image[oldImageComponentIndex];
					outImage[outImageComponentIndex + 1] = image[oldImageComponentIndex + 1];
					outImage[outImageComponentIndex + 2] = image[oldImageComponentIndex + 2];
					outImage[outImageComponentIndex + 3] = image[oldImageComponentIndex + 3];
				}
			}

			// Pad the upper and lower portions of the image by mirroring "padAmountPixels" from the upper and lower borders.
			for (int yUpper = 0, yLower = heightPixels - 1, rowSampled = 0; rowSampled < padAmountPixels; ++yUpper, --yLower, ++rowSampled) {
				int yNewUpper = padAmountPixels - rowSampled - 1;
				int yNewLower = heightPixels + padAmountPixels + rowSampled;

				for (int x = 0; x < widthPixels; ++x) {

					// Mirror the upper pixel.
					auto indexOfColorToCopy = CartesianToComponentIndex(x, yUpper, widthPixels);
					auto indexOfOutImage = CartesianToComponentIndex(x + padAmountPixels, yNewUpper, newWidthPixels);
					outImage[indexOfOutImage] = image[indexOfColorToCopy];
					outImage[indexOfOutImage + 1] = image[indexOfColorToCopy + 1];
					outImage[indexOfOutImage + 2] = image[indexOfColorToCopy + 2];
					outImage[indexOfOutImage + 3] = image[indexOfColorToCopy + 3];

					// Mirror the lower pixel.
					indexOfColorToCopy = CartesianToComponentIndex(x, yLower, widthPixels);
					indexOfOutImage = CartesianToComponentIndex(x + padAmountPixels, yNewLower, newWidthPixels);
					outImage[indexOfOutImage] = image[indexOfColorToCopy];
					outImage[indexOfOutImage + 1] = image[indexOfColorToCopy + 1];
					outImage[indexOfOutImage + 2] = image[indexOfColorToCopy + 2];
					outImage[indexOfOutImage + 3] = image[indexOfColorToCopy + 3];
				}
			}

			// Pad the left and right portions of the image by mirroring "padAmountPixels" from the left and right borders.
			for (int y = 0; y < newHeightPixels; ++y) {
				for (int xLeft = padAmountPixels, xRight = widthPixels + padAmountPixels - 1, columnSampled = 0; columnSampled < padAmountPixels; ++xLeft, --xRight, ++columnSampled) {
					int xNewLeft = padAmountPixels - columnSampled - 1;
					int xNewRight = widthPixels + padAmountPixels + columnSampled;

					// Mirror the left pixel.
					auto sourceIndex = CartesianToComponentIndex(xLeft, y, newWidthPixels);
					auto destinationIndex = CartesianToComponentIndex(xNewLeft, y, newWidthPixels);
					outImage[destinationIndex] = outImage[sourceIndex];
					outImage[destinationIndex + 1] = outImage[sourceIndex + 1];
					outImage[destinationIndex + 2] = outImage[sourceIndex + 2];
					outImage[destinationIndex + 3] = outImage[sourceIndex + 3];

					// Mirror the right pixel.
					sourceIndex = CartesianToComponentIndex(xRight, y, newWidthPixels);
					destinationIndex = CartesianToComponentIndex(xNewRight, y, newWidthPixels);
					outImage[destinationIndex] = outImage[sourceIndex];
					outImage[destinationIndex + 1] = outImage[sourceIndex + 1];
					outImage[destinationIndex + 2] = outImage[sourceIndex + 2];
					outImage[destinationIndex + 3] = outImage[sourceIndex + 3];
				}
			}

			return outImage;
		}

		std::vector<unsigned char> GetImageArea(std::vector<unsigned char> image, int widthPixels, int heightPixels, int xStart, int xFinish, int yStart, int yFinish) {
			if (xStart < 0 || yStart < 0 || xFinish > widthPixels || yFinish > heightPixels || xStart >= xFinish || yStart >= yFinish) {
				Logger::Log("invalid image range");
				return std::vector<unsigned char>();
			}

			auto newWidthPixels = xFinish - xStart;
			auto newHeightPixels = yFinish - yStart;
			std::vector<unsigned char> outImage(newWidthPixels * newHeightPixels * 4);

			// Fill the output image with the original image data.
			for (int y = yStart, newY = 0; newY < newHeightPixels; ++y, ++newY) {
				for (int x = xStart, newX = 0; newX < newWidthPixels; ++x, ++newX) {
					auto oldImageComponentIndex = CartesianToComponentIndex(x, y, widthPixels);
					auto outImageComponentIndex = CartesianToComponentIndex(newX, newY, newWidthPixels);
					outImage[outImageComponentIndex] = image[oldImageComponentIndex];
					outImage[outImageComponentIndex + 1] = image[oldImageComponentIndex + 1];
					outImage[outImageComponentIndex + 2] = image[oldImageComponentIndex + 2];
					outImage[outImageComponentIndex + 3] = image[oldImageComponentIndex + 3];
				}
			}

			return outImage;
		}

		void LoadFromSphericalHDRI(std::filesystem::path imageFilePath) {
			int wantedComponents = 4;
			int componentsDetected;
			int width;
			int height;

			// First, we load the spherical HDRi image.
			// In the stbi_load() function, comp stands for components. In a PNG image, for example, there are 4 components 
			// for each pixel: red, green, blue and alpha.
			// The image's pixels are read and stored left to right, top to bottom, relative to the image.
			// Each pixel's component is an unsigned char.
			auto mipCount = 1;
			for (auto resolution = _faceSizePixels; resolution > 1; resolution /= 2, ++mipCount) {}

			_hdriImageData.resize(mipCount);
			auto lodZero = stbi_load(imageFilePath.string().c_str(), &width, &height, &componentsDetected, wantedComponents);

			if (!lodZero) {
				std::string message = "failed loading environment map" + imageFilePath.string();
				Exit(1, message.c_str());
			}

			auto sizeBytes = width * height * 4;
			_hdriImageData[0].resize(sizeBytes);
			memcpy(_hdriImageData[0].data(), lodZero, sizeBytes);
			_hdriSizePixels.width = width;
			_hdriSizePixels.height = height;

			_front.push_back(GenerateFaceImage(CubeMapFace::FRONT, 0, width, height));
			_right.push_back(GenerateFaceImage(CubeMapFace::RIGHT, 0, width, height));
			_back.push_back(GenerateFaceImage(CubeMapFace::BACK, 0, width, height));
			_left.push_back(GenerateFaceImage(CubeMapFace::LEFT, 0, width, height));
			_upper.push_back(GenerateFaceImage(CubeMapFace::UPPER, 0, width, height));
			_lower.push_back(GenerateFaceImage(CubeMapFace::LOWER, 0, width, height));

			BoxBlur blurrer;
			for (int width = _hdriSizePixels.width, height = _hdriSizePixels.height, radius = 2, i = 1; i < mipCount; width /= 2, height /= 2, radius *= 2, ++i) {
				auto halfWidth = width / 2;
				auto halfHeight = height / 2;
				auto tmp = ResizeImage(_hdriImageData[i - 1], width, height, halfWidth, halfHeight);

				int paddingAmountPixels = (int)((halfWidth / 100.0f) * 5.0f);
				auto paddedWidth = halfWidth + (paddingAmountPixels * 2);
				auto paddedHeight = halfHeight + (paddingAmountPixels * 2);
				tmp = PadImage(tmp, halfWidth, halfHeight, paddingAmountPixels);

				auto blurredImageData = blurrer.Run(_physicalDevice, _logicalDevice, tmp.data(), paddedWidth, paddedHeight, radius);

				memcpy(tmp.data(), blurredImageData, paddedWidth * paddedHeight * 4);
				_hdriImageData[i] = GetImageArea(tmp, paddedWidth, paddedHeight, paddingAmountPixels, halfWidth + paddingAmountPixels, paddingAmountPixels, halfHeight + paddingAmountPixels);

				_front.push_back(GenerateFaceImage(CubeMapFace::FRONT, i, halfWidth, halfHeight));
				_right.push_back(GenerateFaceImage(CubeMapFace::RIGHT, i, halfWidth, halfHeight));
				_back.push_back(GenerateFaceImage(CubeMapFace::BACK, i, halfWidth, halfHeight));
				_left.push_back(GenerateFaceImage(CubeMapFace::LEFT, i, halfWidth, halfHeight));
				_upper.push_back(GenerateFaceImage(CubeMapFace::UPPER, i, halfWidth, halfHeight));
				_lower.push_back(GenerateFaceImage(CubeMapFace::LOWER, i, halfWidth, halfHeight));
			}

			blurrer.Destroy();

			//WriteImagesToFiles(Paths::TexturesPath()/"env_map");

			//Logger::Log("Environment map " + imageFilePath.string() + " loaded.");
		}

		void CreateImage(VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice, VkCommandPool& commandPool, VkQueue& queue) {
			// Create the cubemap image.
			auto& imageCreateInfo = _cubeMapImage._createInfo;
			imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageCreateInfo.arrayLayers = 6;
			imageCreateInfo.extent = { (uint32_t)_faceSizePixels, (uint32_t)_faceSizePixels, 1 };
			imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
			imageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
			imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
			imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageCreateInfo.mipLevels = 10;
			imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			vkCreateImage(logicalDevice, &imageCreateInfo, nullptr, &_cubeMapImage._image);

			// Allocate memory on the GPU for the image.
			VkMemoryRequirements reqs;
			vkGetImageMemoryRequirements(logicalDevice, _cubeMapImage._image, &reqs);
			VkMemoryAllocateInfo imageAllocInfo{};
			imageAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			imageAllocInfo.allocationSize = reqs.size;
			imageAllocInfo.memoryTypeIndex = PhysicalDevice::GetMemoryTypeIndex(physicalDevice, reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VkDeviceMemory mem;
			vkAllocateMemory(logicalDevice, &imageAllocInfo, nullptr, &mem);
			vkBindImageMemory(logicalDevice, _cubeMapImage._image, mem, 0);

			auto& imageViewCreateInfo = _cubeMapImage._viewCreateInfo;
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.components = { {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY} };
			imageViewCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
			imageViewCreateInfo.image = _cubeMapImage._image;
			imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
			imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
			imageViewCreateInfo.subresourceRange.layerCount = 6;
			imageViewCreateInfo.subresourceRange.levelCount = 10;
			vkCreateImageView(_logicalDevice, &imageViewCreateInfo, nullptr, &_cubeMapImage._view);

			auto& samplerCreateInfo = _cubeMapImage._samplerCreateInfo;
			samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCreateInfo.anisotropyEnable = false;
			samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
			samplerCreateInfo.flags = 0;
			samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
			samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
			samplerCreateInfo.maxLod = 10;
			samplerCreateInfo.minLod = 0;
			samplerCreateInfo.mipLodBias = 0.0f;
			samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			vkCreateSampler(_logicalDevice, &samplerCreateInfo, nullptr, &_cubeMapImage._sampler);

			auto commandBuffer = VkHelper::CreateCommandBuffer(logicalDevice, commandPool);
			CopyFacesToImage(logicalDevice, physicalDevice, commandPool, commandBuffer, queue);
		}

		void CopyFacesToImage(VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice, VkCommandPool& commandPool, VkCommandBuffer& commandBuffer, VkQueue& queue) {
			auto faces = { _right, _left, _upper, _lower, _front, _back };
			uint32_t resolution = _faceSizePixels;
			uint32_t faceIndex = 0;

			VkHelper::StartRecording(commandBuffer);

			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.oldLayout = _cubeMapImage._currentLayout;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.image = _cubeMapImage._image;
			barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
			_cubeMapImage._currentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

			std::vector<Buffer> temporaryBuffers;

			for (auto& face : faces) {
				resolution = _faceSizePixels;

				for (int mipmapIndex = 0; mipmapIndex < face.size(); ++mipmapIndex, resolution /= 2) {
					// Create a temporary buffer.
					Buffer stagingBuffer{};
					stagingBuffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
					stagingBuffer._createInfo.size = face[mipmapIndex].size();
					stagingBuffer._createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
					vkCreateBuffer(_logicalDevice, &stagingBuffer._createInfo, nullptr, &stagingBuffer._buffer);

					// Allocate memory for the buffer.
					VkMemoryRequirements requirements{};
					vkGetBufferMemoryRequirements(_logicalDevice, stagingBuffer._buffer, &requirements);
					stagingBuffer._gpuMemory = PhysicalDevice::AllocateMemory(physicalDevice, logicalDevice, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

					// Map memory to the correct GPU and CPU ranges for the buffer.
					vkBindBufferMemory(_logicalDevice, stagingBuffer._buffer, stagingBuffer._gpuMemory, 0);
					vkMapMemory(_logicalDevice, stagingBuffer._gpuMemory, 0, face[mipmapIndex].size(), 0, &stagingBuffer._cpuMemory);
					memcpy(stagingBuffer._cpuMemory, face[mipmapIndex].data(), face[mipmapIndex].size());

					// Copy the buffer to the specific face by defining the subresource range.
					VkBufferImageCopy copyInfo{};
					copyInfo.bufferImageHeight = resolution;
					copyInfo.bufferRowLength = resolution;
					copyInfo.imageExtent = { resolution, resolution, 1 };
					copyInfo.imageOffset = { 0,0,0 };
					copyInfo.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					copyInfo.imageSubresource.layerCount = 1;
					copyInfo.imageSubresource.baseArrayLayer = faceIndex;
					copyInfo.imageSubresource.mipLevel = mipmapIndex;
					vkCmdCopyBufferToImage(commandBuffer, stagingBuffer._buffer, _cubeMapImage._image, _cubeMapImage._currentLayout, 1, &copyInfo);

					temporaryBuffers.push_back(stagingBuffer);
				}

				++faceIndex;
			}

			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.oldLayout = _cubeMapImage._currentLayout;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.image = _cubeMapImage._image;
			barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
			_cubeMapImage._currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

			VkHelper::StopRecording(commandBuffer);
			VkHelper::ExecuteCommands(commandBuffer, queue);

			// Destroy all the buffers used to move data to the cube map image.
			for (auto& buffer : temporaryBuffers) {
				vkUnmapMemory(_logicalDevice, buffer._gpuMemory);
				vkFreeMemory(_logicalDevice, buffer._gpuMemory, nullptr);
				vkDestroyBuffer(_logicalDevice, buffer._buffer, nullptr);
			}
		}

		ShaderResources CreateDescriptorSets(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, std::vector<DescriptorSetLayout>& layouts) {
			auto descriptorSetID = 4;

			// Map the cubemap image to the fragment shader.
			VkDescriptorPool descriptorPool{};
			VkDescriptorPoolSize poolSizes[1] = { VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 } };
			VkDescriptorPoolCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			createInfo.maxSets = (uint32_t)1;
			createInfo.poolSizeCount = (uint32_t)1;
			createInfo.pPoolSizes = poolSizes;
			vkCreateDescriptorPool(logicalDevice, &createInfo, nullptr, &descriptorPool);

			// Create the descriptor set.
			VkDescriptorSet set{};
			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = (uint32_t)1;
			allocInfo.pSetLayouts = &layouts[descriptorSetID]._layout;
			vkAllocateDescriptorSets(logicalDevice, &allocInfo, &set);

			// Update the descriptor set's data with the environment map's image.
			VkDescriptorImageInfo imageInfo{ _cubeMapImage._sampler, _cubeMapImage._view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			VkWriteDescriptorSet writeInfo = {};
			writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeInfo.dstSet = set;
			writeInfo.descriptorCount = 1;
			writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeInfo.pImageInfo = &imageInfo;
			writeInfo.dstBinding = 0;
			vkUpdateDescriptorSets(logicalDevice, 1, &writeInfo, 0, nullptr);

			auto descriptorSets = std::vector<VkDescriptorSet>{ set };
			_shaderResources._data.try_emplace(layouts[descriptorSetID], descriptorSets);
			return _shaderResources;
		}

		void UpdateShaderResources() {
		}
	};

	// Forward declarations for the compiler.
	class GameObject;
	class Scene;
	class RigidBody;
	class Mesh;

	/**
	 * @brief Represents a vertex in the mesh of a physics body.
	 */
	class PhysicsVertex {
	public:

		/**
		 * @brief Position in local space.
		 */
		glm::vec3 _position;
	};

	/**
	 * @brief Physics mesh.
	 */
	class PhysicsMesh {
	public:

		/**
		 * @brief Vertices that form this mesh.
		 */
		std::vector<PhysicsVertex> _vertices;

		/**
		 * @brief Face indices.
		 */
		std::vector<unsigned int> _faceIndices;

		/**
		 * @brief Visual mesh that appears rendered on screen, which this physics mesh class simulates physics for.
		 */
		Mesh* _pMesh;
	};

	/**
	 * @brief Base class for a body that performs physics simulation.
	 */
	class RigidBody : public IPhysicsUpdatable {
	public:

		/**
		 * @brief Whether the body is initialized and valid for simulation.
		 */
		bool _isInitialized = false;

		/**
		 * @brief True if collision detection and resolution is enabled for the body.
		 */
		bool _isCollidable = false;

		/**
		 * @brief The velocity vector of this physics body in units per second.
		 */
		glm::vec3 _velocity = glm::vec3(0.0f, 0.0f, 0.0f);

		/**
		 * @brief The angular velocity in radians per second.
		 */
		glm::vec3 _angularVelocity = glm::vec3(0.0f, 0.0f, 0.0f);

		/**
		 * @brief Mass in kg.
		 */
		float _mass;

		/**
		 * @brief If this is set to true, _overriddenCenterOfMass will be used as center of mass.
		 */
		bool _isCenterOfMassOverridden;

		/**
		 * @brief Overridden center of mass in local space.
		 */
		glm::vec3 _overriddenCenterOfMass;

		/**
		 * @brief Physics mesh used as a bridge between this physics body and its visual counterpart.
		 */
		PhysicsMesh _mesh;

		/**
		 * @brief Physics update implementation for this specific rigidbody.
		 */
		void(*_updateImplementation)(GameObject&);

		/**
		 * @brief Map where the key is the index of a vertex in _pMesh, and the value is a list of vertex indices directly connected to the vertex represented by the key.
		 */
		 //std::map<unsigned int, std::vector<unsigned int>> _neighbors;

		 /**
		  * @brief Constructor.
		  */
		RigidBody() = default;

		/*inline std::ostream& operator<< (std::ostream& stream, const glm::vec3& vector)
		{
			return stream << "(" << vector.x << ", " << vector.y << ", " << vector.z << ")";
		}*/

		bool IsVectorZero(const glm::vec3& vector, float tolerance = 0.0f);

		glm::vec3 CalculateTransmittedForce(const glm::vec3& transmitterPosition, const glm::vec3& force, const glm::vec3& receiverPosition);

		glm::vec3 GetCenterOfMass();

		void AddForceAtPosition(const glm::vec3& force, const glm::vec3& pointOfApplication, bool ignoreTranslation);

		void AddForce(const glm::vec3& force, bool ignoreMass);

		std::vector<glm::vec3> GetContactPoints(const RigidBody& other);

		std::vector< GameObject*> GetAllGameObjects(GameObject* pRoot);

		std::vector<GameObject*> GetOtherGameObjects(GameObject* pGameObject);

		std::vector<glm::vec3> GetContactPoints();

		void Initialize(Mesh* pMesh, const float& mass, const bool& overrideCenterOfMass, const glm::vec3& overriddenCenterOfMass);

		void PhysicsUpdate();
	};

	class Mesh : public IVulkanUpdatable, public IDrawable, public IPipelineable {

	public:

		Mesh() = default;
		int _materialIndex = 0;
		GameObject* _pGameObject = nullptr;

		ShaderResources CreateDescriptorSets(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, std::vector<DescriptorSetLayout>& layouts);
		void UpdateShaderResources();
		void Update(VulkanContext& vkContext);
		void Draw(VkPipelineLayout& pipelineLayout, VkCommandBuffer& drawCommandBuffer);
	};

	/**
	 * @brief Represents a physical object in a celeritas-engine scene.
	 */
	class GameObject : public IVulkanUpdatable, public IPipelineable, public IDrawable {
	public:

		/**
		 * @brief Name of the game object.
		 */
		std::string _name;

		/**
		 * @brief Pointer to the scene so you can use _materialIndex and _gameObjectIndex from this class.
		 */
		Scene* _pScene = nullptr;

		/**
		 * @brief Parent game object pointer.
		 */
		GameObject* _pParent = nullptr;

		/**
		 * @brief Child game object pointers.
		 */
		std::vector<GameObject*> _children;

		/**
		 * @brief Mesh of this game object.
		 */
		Mesh* _pMesh = nullptr;

		/**
		 * @brief Body for physics simulation.
		 */
		RigidBody _body;

		/**
		 * @brief Transform relative to the parent gameobject.
		 */
		Transform _localTransform;

		struct {
			glm::mat4x4 transform;
		} _gameObjectData;

		GameObject() = default;
		GameObject(const std::string& name, Scene* pScene);
		ShaderResources CreateDescriptorSets(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, std::vector<DescriptorSetLayout>& layouts);
		Transform GetWorldSpaceTransform();
		void UpdateShaderResources();
		void Update(VulkanContext& vkContext);
		void Draw(VkPipelineLayout& pipelineLayout, VkCommandBuffer& drawCommandBuffer);
	};

	/**
	 * @brief Represents a celeritas-engine scene.
	 */
	class Scene : public IVulkanUpdatable, public IPipelineable {

	public:
		/**
		 * @brief Collection of point lights.
		 */
		std::vector<PointLight> _pointLights;

		/**
		 * @brief Game object hierarchy.
		 */
		GameObject* _pRootGameObject;

		/**
		 * @brief Collection of materials.
		 */
		std::vector<Material> _materials;

		/**
		 * @brief Environment map used for image-based lighting.
		 */
		CubicalEnvironmentMap _environmentMap;

		/**
		 * @brief Default constructor.
		 */
		Scene() = default;

		Scene(VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice) {
			_materials.push_back(Material(logicalDevice, physicalDevice));
			_pRootGameObject = new GameObject("Root", this);
		}

		Material DefaultMaterial() {
			if (_materials.size() <= 0) {
				std::cout << "a scene object should always have at least a default material" << std::endl;
				std::exit(1);
			}
			return _materials[0];
		}

		void Update(VulkanContext& vkContext) {
			for (auto& light : _pointLights) {
				light.Update(vkContext);
			}

			for (auto& gameObject : _pRootGameObject->_children) {
				gameObject->Update(vkContext);
			}
		}

		ShaderResources CreateDescriptorSets(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, std::vector<DescriptorSetLayout>& layouts) {
			for (auto& gameObject : _pRootGameObject->_children) {
				if (gameObject->_pMesh != nullptr) {
					auto gameObjectResources = gameObject->CreateDescriptorSets(physicalDevice, logicalDevice, commandPool, queue, layouts);
					_shaderResources.MergeResources(gameObjectResources);
				}
			}

			for (auto& light : _pointLights) {
				auto lightResources = light.CreateDescriptorSets(physicalDevice, logicalDevice, commandPool, queue, layouts);
				_shaderResources.MergeResources(lightResources);
				light.UpdateShaderResources();
			}

			auto environmentMapResources = _environmentMap.CreateDescriptorSets(physicalDevice, logicalDevice, commandPool, queue, layouts);
			_shaderResources.MergeResources(environmentMapResources);
			return _shaderResources;
		}

		void UpdateShaderResources() {
			// TODO
		}
	};

	// GameObject
	GameObject::GameObject(const std::string& name, Scene* pScene) {
		_name = name;
		_pScene = pScene;
	}

	ShaderResources GameObject::CreateDescriptorSets(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, std::vector<DescriptorSetLayout>& layouts) {
		auto descriptorSetID = 1;
		auto globalTransform = GetWorldSpaceTransform();

		// Create a temporary buffer.
		Buffer buffer{};
		auto bufferSizeBytes = sizeof(_localTransform._matrix);
		buffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer._createInfo.size = bufferSizeBytes;
		buffer._createInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		vkCreateBuffer(logicalDevice, &buffer._createInfo, nullptr, &buffer._buffer);

		// Allocate memory for the buffer.
		VkMemoryRequirements requirements{};
		vkGetBufferMemoryRequirements(logicalDevice, buffer._buffer, &requirements);
		buffer._gpuMemory = PhysicalDevice::AllocateMemory(physicalDevice, logicalDevice, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		// Map memory to the correct GPU and CPU ranges for the buffer.
		vkBindBufferMemory(logicalDevice, buffer._buffer, buffer._gpuMemory, 0);
		vkMapMemory(logicalDevice, buffer._gpuMemory, 0, bufferSizeBytes, 0, &buffer._cpuMemory);
		memcpy(buffer._cpuMemory, &globalTransform._matrix, bufferSizeBytes);

		_buffers.push_back(buffer);

		VkDescriptorPool descriptorPool{};
		VkDescriptorPoolSize poolSizes[1] = { VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 } };
		VkDescriptorPoolCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.maxSets = (uint32_t)1;
		createInfo.poolSizeCount = (uint32_t)1;
		createInfo.pPoolSizes = poolSizes;
		vkCreateDescriptorPool(logicalDevice, &createInfo, nullptr, &descriptorPool);

		// Create the descriptor set.
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = (uint32_t)1;
		allocInfo.pSetLayouts = &layouts[descriptorSetID]._layout;
		VkDescriptorSet descriptorSet;
		vkAllocateDescriptorSets(logicalDevice, &allocInfo, &descriptorSet);

		// Update the descriptor set's data.
		VkDescriptorBufferInfo bufferInfo{ buffer._buffer, 0, buffer._createInfo.size };
		VkWriteDescriptorSet writeInfo = {};
		writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeInfo.dstSet = descriptorSet;
		writeInfo.descriptorCount = 1;
		writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeInfo.pBufferInfo = &bufferInfo;
		writeInfo.dstBinding = 0;
		vkUpdateDescriptorSets(logicalDevice, 1, &writeInfo, 0, nullptr);

		auto descriptorSets = std::vector<VkDescriptorSet>{ descriptorSet };
		_shaderResources._data.try_emplace(layouts[descriptorSetID], descriptorSets);

		auto meshResources = _pMesh->CreateDescriptorSets(physicalDevice, logicalDevice, commandPool, queue, layouts);
		_shaderResources.MergeResources(meshResources);

		for (auto& child : _children) {
			auto childResources = child->CreateDescriptorSets(physicalDevice, logicalDevice, commandPool, queue, layouts);
		}

		return _shaderResources;
	}

	Transform GameObject::GetWorldSpaceTransform() {
		Transform outTransform;
		GameObject current = *this;
		outTransform._matrix *= current._localTransform._matrix;
		while (current._pParent != nullptr) {
			current = *current._pParent;
			outTransform._matrix *= current._localTransform._matrix;
		}
		return outTransform;
	}

	void GameObject::UpdateShaderResources() {
		_gameObjectData.transform = GetWorldSpaceTransform()._matrix;
		memcpy(_buffers[0]._cpuMemory, &_gameObjectData, sizeof(_gameObjectData));
	}

	void GameObject::Update(VulkanContext& vkContext) {
		if (_pMesh != nullptr) {
			_pMesh->Update(vkContext);
		}

		UpdateShaderResources();

		for (auto& child : _children) {
			child->Update(vkContext);
		}
	}

	void GameObject::Draw(VkPipelineLayout& pipelineLayout, VkCommandBuffer& drawCommandBuffer) {
		vkCmdBindDescriptorSets(drawCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &_shaderResources[1][0], 0, nullptr);

		if (_pMesh != nullptr) {
			_pMesh->Draw(pipelineLayout, drawCommandBuffer);
		}

		for (int i = 0; i < _children.size(); ++i) {
			_children[i]->Draw(pipelineLayout, drawCommandBuffer);
		}
	}

	// Mesh
	ShaderResources Mesh::CreateDescriptorSets(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, std::vector<DescriptorSetLayout>& layouts) {
		auto descriptorSetID = 3;

		// Get the textures to send to the shaders.
		auto pScene = _pGameObject->_pScene;
		auto defaultMaterial = pScene->DefaultMaterial();
		Image albedoMap = defaultMaterial._albedo;
		Image roughnessMap = defaultMaterial._roughness;
		Image metalnessMap = defaultMaterial._metalness;

		if (_materialIndex >= 0) {
			if (VK_NULL_HANDLE != pScene->_materials[_materialIndex]._albedo._image) {
				albedoMap = pScene->_materials[_materialIndex]._albedo;
			}
			if (VK_NULL_HANDLE != pScene->_materials[_materialIndex]._roughness._image) {
				roughnessMap = pScene->_materials[_materialIndex]._roughness;
			}
			if (VK_NULL_HANDLE != pScene->_materials[_materialIndex]._metalness._image) {
				metalnessMap = pScene->_materials[_materialIndex]._metalness;
			}
		}

		// Send the textures to the GPU.
		CopyImageToDeviceMemory(logicalDevice, physicalDevice, commandPool, queue, albedoMap._image, albedoMap._createInfo.extent.width, albedoMap._createInfo.extent.height, albedoMap._createInfo.extent.depth, albedoMap._pData, albedoMap._sizeBytes);
		CopyImageToDeviceMemory(logicalDevice, physicalDevice, commandPool, queue, albedoMap._image, roughnessMap._createInfo.extent.width, roughnessMap._createInfo.extent.height, roughnessMap._createInfo.extent.depth, roughnessMap._pData, roughnessMap._sizeBytes);
		CopyImageToDeviceMemory(logicalDevice, physicalDevice, commandPool, queue, albedoMap._image, metalnessMap._createInfo.extent.width, metalnessMap._createInfo.extent.height, metalnessMap._createInfo.extent.depth, metalnessMap._pData, metalnessMap._sizeBytes);

		auto commandBuffer = VkHelper::CreateCommandBuffer(logicalDevice, commandPool);
		VkHelper::StartRecording(commandBuffer);

		// Transition the images to shader read layout.
		{
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.oldLayout = albedoMap._currentLayout;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.image = albedoMap._image;
			barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
			albedoMap._currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.oldLayout = roughnessMap._currentLayout;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.image = roughnessMap._image;
			barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
			roughnessMap._currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.oldLayout = metalnessMap._currentLayout;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.image = metalnessMap._image;
			barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
			metalnessMap._currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		}

		VkHelper::StopRecording(commandBuffer);
		VkHelper::ExecuteCommands(commandBuffer, queue);

		_images.push_back(albedoMap);
		_images.push_back(roughnessMap);
		_images.push_back(metalnessMap);

		VkDescriptorPool descriptorPool{};
		VkDescriptorPoolSize poolSizes[1] = { VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 } };
		VkDescriptorPoolCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.maxSets = (uint32_t)1;
		createInfo.poolSizeCount = (uint32_t)1;
		createInfo.pPoolSizes = poolSizes;
		vkCreateDescriptorPool(logicalDevice, &createInfo, nullptr, &descriptorPool);

		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = (uint32_t)1;
		allocInfo.pSetLayouts = &layouts[descriptorSetID]._layout;
		VkDescriptorSet descriptorSet;
		vkAllocateDescriptorSets(logicalDevice, &allocInfo, &descriptorSet);

		VkDescriptorImageInfo imageInfo[3];
		imageInfo[0] = { albedoMap._sampler, albedoMap._view, albedoMap._currentLayout };
		imageInfo[1] = { roughnessMap._sampler, roughnessMap._view, roughnessMap._currentLayout };
		imageInfo[2] = { metalnessMap._sampler, metalnessMap._view, metalnessMap._currentLayout };
		VkWriteDescriptorSet writeInfo = {};
		writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeInfo.dstSet = descriptorSet;
		writeInfo.descriptorCount = 3;
		writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeInfo.pImageInfo = imageInfo;
		writeInfo.dstBinding = 0;
		vkUpdateDescriptorSets(logicalDevice, 1, &writeInfo, 0, nullptr);

		auto descriptorSets = std::vector<VkDescriptorSet>{ descriptorSet };
		_shaderResources._data.try_emplace(layouts[descriptorSetID], descriptorSets);
		return _shaderResources;
	}

	void Mesh::UpdateShaderResources() {
		// TODO
	}

	void Mesh::Update(VulkanContext& vkContext) {
		auto& vkBuffer = _vertices._vertexBuffer._buffer;
		auto& vertexData = _vertices._vertexData;

		// Update the Vulkan-only visible memory that is mapped to the GPU so that the updated data is also visible in the vertex shader.
		memcpy(_vertices._vertexBuffer._cpuMemory, _vertices._vertexData.data(), GetVectorSizeInBytes(_vertices._vertexData));
	}

	void Mesh::Draw(VkPipelineLayout& pipelineLayout, VkCommandBuffer& drawCommandBuffer) {
		VkDescriptorSet sets[] = { _shaderResources[3][0] };
		vkCmdBindDescriptorSets(drawCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 3, 1, sets, 0, nullptr);

		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(drawCommandBuffer, 0, 1, &_vertices._vertexBuffer._buffer, &offset);
		vkCmdBindIndexBuffer(drawCommandBuffer, _faceIndices._indexBuffer._buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(drawCommandBuffer, (uint32_t)_faceIndices._indexData.size(), 1, 0, 0, 0);
	}

	// Rigidbody
	bool RigidBody::IsVectorZero(const glm::vec3& vector, float tolerance) {
		return (vector.x >= -tolerance && vector.x <= tolerance &&
			vector.y >= -tolerance && vector.y <= tolerance &&
			vector.z >= -tolerance && vector.z <= tolerance);
	}

	glm::vec3 RigidBody::CalculateTransmittedForce(const glm::vec3& transmitterPosition, const glm::vec3& force, const glm::vec3& receiverPosition) {
		if (IsVectorZero(receiverPosition - transmitterPosition, 0.001f)) {
			return force;
		}

		auto effectiveForce = glm::normalize(receiverPosition - transmitterPosition);
		auto scaleFactor = glm::dot(effectiveForce, force);
		return effectiveForce * scaleFactor;
	}

	glm::vec3 RigidBody::GetCenterOfMass() {
		if (_isCenterOfMassOverridden) {
			return _overriddenCenterOfMass;
		}

		auto& vertices = _mesh._vertices;
		int vertexCount = (int)vertices.size();
		float totalX = 0.0f;
		float totalY = 0.0f;
		float totalZ = 0.0f;

		for (int i = 0; i < vertexCount; ++i) {
			totalX += vertices[i]._position.x;
			totalY += vertices[i]._position.y;
			totalZ += vertices[i]._position.z;
		}

		return glm::vec3(totalX / vertexCount, totalY / vertexCount, totalZ / vertexCount);
	}

	void RigidBody::AddForceAtPosition(const glm::vec3& force, const glm::vec3& pointOfApplication, bool ignoreTranslation) {
		auto& time = Time::Instance();
		float deltaTimeSeconds = (float)time._physicsDeltaTime * 0.001f;
		auto vertexCount = _mesh._pMesh->_vertices._vertexData.size();
		auto& vertices = _mesh._pMesh->_vertices._vertexData;
		auto worldSpaceTransform = _mesh._pMesh->_pGameObject->GetWorldSpaceTransform()._matrix;

		// First calculate the translation component of the force to apply.
		auto worldSpaceCom = glm::vec3(worldSpaceTransform * glm::vec4(GetCenterOfMass(), 1.0f));
		auto worldSpacePointOfApplication = glm::vec3(worldSpaceTransform * glm::vec4(pointOfApplication, 1.0f));

		if (!ignoreTranslation) {
			glm::vec3 translationForce = CalculateTransmittedForce(worldSpacePointOfApplication, force, worldSpaceCom);
			glm::vec3 translationAcceleration = translationForce / _mass;
			glm::vec3 translationDelta = translationForce * deltaTimeSeconds;
			_velocity += translationDelta;
		}

		// Now calculate the rotation component.
		auto positionToCom = worldSpaceCom - worldSpacePointOfApplication;
		if (IsVectorZero(positionToCom, 0.001f)) {
			return;
		}

		auto rotationAxis = -glm::normalize(glm::cross(positionToCom, force));
		if (glm::isnan(rotationAxis.x) || glm::isnan(rotationAxis.y) || glm::isnan(rotationAxis.x)) {
			return;
		}

		auto comPerpendicularDirection = glm::normalize(glm::cross(positionToCom, rotationAxis));
		auto rotationalForce = comPerpendicularDirection * glm::dot(comPerpendicularDirection, force);

		// Calculate or approximate rotational inertia.
		auto rotationalInertia = 0.0f;
		auto singleVertexMass = _mass / vertexCount;
		for (int i = 0; i < vertexCount; ++i) {
			auto worldSpaceVertexPosition = glm::vec3(worldSpaceTransform * glm::vec4(vertices[i]._position, 1.0f));

			if (worldSpaceVertexPosition == worldSpaceCom) {
				continue;
			}

			auto comToVertexDirection = glm::normalize(worldSpaceVertexPosition - worldSpaceCom);
			auto cathetus = comToVertexDirection * glm::dot(rotationAxis, comToVertexDirection);

			if (IsVectorZero(cathetus), 0.001f) {
				rotationalInertia += (/*vertex.mass **/ glm::length(worldSpaceVertexPosition - worldSpaceCom));
				continue;
			}

			auto endPosition = worldSpaceCom + cathetus;
			auto perpDistance = glm::length(endPosition - worldSpaceVertexPosition);
			rotationalInertia += (singleVertexMass * (perpDistance * perpDistance));
		}

		auto angularAcceleration = glm::cross(rotationalForce, positionToCom) / rotationalInertia;
		auto rotationDelta = angularAcceleration * deltaTimeSeconds;
		_angularVelocity += rotationDelta;
	}

	void RigidBody::AddForce(const glm::vec3& force, bool ignoreMass) {
		auto& time = Time::Instance();
		float deltaTimeSeconds = (float)time._physicsDeltaTime * 0.001f;
		auto translationDelta = ignoreMass ? (force * deltaTimeSeconds) : ((force / _mass) * deltaTimeSeconds);
		_velocity += translationDelta;
		_mesh._pMesh->_pGameObject->_localTransform.Translate(translationDelta);
	}

	std::vector<glm::vec3> RigidBody::GetContactPoints(const RigidBody& other) {
		std::vector<glm::vec3> outContactPoints;
		auto worldSpaceOther = other._mesh._pMesh->_pGameObject->GetWorldSpaceTransform();
		auto worldSpaceCurrent = _mesh._pMesh->_pGameObject->GetWorldSpaceTransform();

		for (int i = 0; i < other._mesh._faceIndices.size(); i += 3) {
			for (int j = 0; j < _mesh._faceIndices.size(); j += 3) {

				auto v1Other = glm::vec3(worldSpaceOther._matrix * glm::vec4(other._mesh._vertices[other._mesh._faceIndices[i]]._position, 1.0f));
				auto v2Other = glm::vec3(worldSpaceOther._matrix * glm::vec4(other._mesh._vertices[other._mesh._faceIndices[i + 1]]._position, 1.0f));
				auto v3Other = glm::vec3(worldSpaceOther._matrix * glm::vec4(other._mesh._vertices[other._mesh._faceIndices[i + 2]]._position, 1.0f));

				auto v1 = glm::vec3(worldSpaceCurrent._matrix * glm::vec4(_mesh._vertices[_mesh._faceIndices[j]]._position, 1.0f));
				auto v2 = glm::vec3(worldSpaceCurrent._matrix * glm::vec4(_mesh._vertices[_mesh._faceIndices[j + 1]]._position, 1.0f));
				auto v3 = glm::vec3(worldSpaceCurrent._matrix * glm::vec4(_mesh._vertices[_mesh._faceIndices[j + 2]]._position, 1.0f));

				glm::vec3 intersectionPoint1;
				glm::vec3 intersectionPoint2;
				glm::vec3 intersectionPoint3;

				if (IsRayIntersectingTriangle(v1Other, v2Other - v1Other, v1, v2, v3, intersectionPoint1)) {
					outContactPoints.push_back(intersectionPoint1);
				}

				if (IsRayIntersectingTriangle(v1Other, v3Other - v1Other, v1, v2, v3, intersectionPoint2)) {
					outContactPoints.push_back(intersectionPoint2);
				}

				if (IsRayIntersectingTriangle(v3Other, v2Other - v3Other, v1, v2, v3, intersectionPoint3)) {
					outContactPoints.push_back(intersectionPoint3);
				}
			}
		}
		return outContactPoints;
	}

	std::vector< GameObject*> RigidBody::GetAllGameObjects(GameObject* pRoot) {
		std::vector<GameObject*> outGameObjects;
		outGameObjects.push_back(pRoot);
		for (int i = 0; i < pRoot->_children.size(); ++i) {
			auto objects = GetAllGameObjects(pRoot->_children[i]);
			outGameObjects.insert(outGameObjects.end(), objects.begin(), objects.end());
		}
		return outGameObjects;
	}

	std::vector<GameObject*> RigidBody::GetOtherGameObjects(GameObject* pGameObject) {
		auto allGameObjects = GetAllGameObjects(pGameObject->_pScene->_pRootGameObject);
		for (int i = 0; i < allGameObjects.size(); ++i) {
			if (pGameObject == allGameObjects[i]) {
				allGameObjects.erase(allGameObjects.begin() + i);
			}
		}
		return allGameObjects;
	}

	std::vector<glm::vec3> RigidBody::GetContactPoints() {
		auto otherGameObjects = GetOtherGameObjects(_mesh._pMesh->_pGameObject);
		std::vector<glm::vec3> outContactPoints;
		for (int i = 0; i < otherGameObjects.size(); ++i) {
			if (otherGameObjects[i]->_body._mesh._pMesh == nullptr) {
				continue;
			}
			auto contactPoints = GetContactPoints(otherGameObjects[i]->_body);
			outContactPoints.insert(outContactPoints.end(), contactPoints.begin(), contactPoints.end());
		}
		return outContactPoints;
	}

	void RigidBody::Initialize(Mesh* pMesh, const float& mass, const bool& overrideCenterOfMass, const glm::vec3& overriddenCenterOfMass) {
		if (pMesh == nullptr || mass <= 0.001f) {
			return;
		}

		_mass = mass;
		_mesh._pMesh = pMesh;
		_isCenterOfMassOverridden = overrideCenterOfMass;
		_overriddenCenterOfMass = overriddenCenterOfMass;
		auto& vertices = pMesh->_vertices._vertexData;
		auto& indices = pMesh->_faceIndices._indexData;
		_mesh._vertices.resize(vertices.size());
		_mesh._faceIndices.resize(indices.size());

		// TODO: Decide how you want to initialize your physics mesh.
		for (int i = 0; i < vertices.size(); ++i) {
			_mesh._vertices[i]._position = vertices[i]._position;
		}

		memcpy(_mesh._faceIndices.data(), indices.data(), GetVectorSizeInBytes(indices));

		_isInitialized = true;
	}

	void RigidBody::PhysicsUpdate() {
		if (_isCollidable) {

		}

		if (_updateImplementation) {
			_updateImplementation(*_mesh._pMesh->_pGameObject);
		}

		float deltaTimeSeconds = (float)Time::Instance()._physicsDeltaTime * 0.001f;
		_mesh._pMesh->_pGameObject->_localTransform.RotateAroundPosition(GetCenterOfMass(), glm::normalize(_angularVelocity), glm::length(_angularVelocity) * deltaTimeSeconds);
		_mesh._pMesh->_pGameObject->_localTransform.Translate(_velocity * deltaTimeSeconds);
	}

	/**
	 * @brief Represents a three-dimensional bounding box.
	 */
	class BoundingBox {
	public:

		/**
		 * @brief Low bound, or more accurately, the position whose components are all the lowest number calculated from a collection of positions.
		 */
		glm::vec3 _min;

		/**
		 * @brief High bound, or more accurately, the position whose components are all the highest number calculated from a collection of positions.
		 */
		glm::vec3 _max;

		glm::vec3 GetCenter() {
			return glm::vec3((_min.x + _max.x) * 0.5f, (_min.y + _max.y) * 0.5f, (_min.z + _max.z) * 0.5f);
		}

		static BoundingBox Create(const Mesh& mesh) {
			auto& vertices = mesh._vertices._vertexData;
			BoundingBox boundingBox;

			if (vertices.size() <= 0) {
				return boundingBox;
			}

			float minimumX = vertices[0]._position.x;
			float minimumY = vertices[0]._position.y;
			float minimumZ = vertices[0]._position.z;

			float maximumX = minimumX;
			float maximumY = minimumY;
			float maximumZ = minimumZ;

			for (int i = 0; i < vertices.size(); ++i) {
				minimumX = std::min(minimumX, vertices[i]._position.x);
				minimumY = std::min(minimumY, vertices[i]._position.y);
				minimumZ = std::min(minimumZ, vertices[i]._position.z);

				maximumX = std::max(maximumX, vertices[i]._position.x);
				maximumY = std::max(maximumY, vertices[i]._position.y);
				maximumZ = std::max(maximumZ, vertices[i]._position.z);
			}

			boundingBox._min = glm::vec3(minimumX, minimumY, minimumZ);
			boundingBox._max = glm::vec3(maximumX, maximumY, maximumZ);

			return boundingBox;
		}
	};

	/**
	 * @brief Represents a general-purpose camera.
	 */
	class Camera : public GameObject {
	public:

		/**
		 * @brief Horizontal FOV in degrees.
		 */
		float _horizontalFov;

		/**
		 * @brief Lower bound that maps to normalizedDeviceCoordinates.z = 0 in the vertex shader, in meters.
		 * Anything closer than this will not be rendered by the graphics pipeline.
		 */
		float _nearClippingDistance;

		/**
		 * @brief Upper bound that maps to normalizedDeviceCoordinates.z = 1 in the vertex shader, in meters.
		 * Anything farther than this will not be rendered by the graphics pipeline.
		 */
		float _farClippingDistance;

		/**
		 * @brief This is a transform passed to the vertex shader that translates vertices from world space into camera space.
		 */
		Transform _view;

		/**
		 * @brief Up direction of the camera.
		 */
		glm::vec3 _up;

		// Temp
		float _lastYaw;
		float _lastPitch;
		float _lastRoll;

		float _yaw;
		float _pitch;
		float _roll;

		float _lastScrollY;

		/**
		 * @brief Camera related data directed to the vertex shader. Knowing this, the vertex shader is able to calculate the correct
		 * Vulkan view volume coordinates.
		 */
		struct {
			float tanHalfHorizontalFov;
			float aspectRatio;
			float nearClipDistance;
			float farClipDistance;
			glm::mat4 worldToCamera;
			glm::vec3 transform;
		} _cameraData;

		Camera() {
			_horizontalFov = 55.0f;
			_nearClippingDistance = 0.1f;
			_farClippingDistance = 200.0f;
			_up = glm::vec3(0.0f, 1.0f, 0.0f);
		}

		Camera(float horizontalFov, float nearClippingDistance, float farClippingDistance) {
			_horizontalFov = horizontalFov;
			_nearClippingDistance = nearClippingDistance;
			_farClippingDistance = farClippingDistance;
			_up = glm::vec3(0.0f, 1.0f, 0.0f);
		}

		ShaderResources CreateDescriptorSets(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, std::vector<DescriptorSetLayout>& layouts) {
			auto descriptorSetID = 0;

			// Create a temporary buffer.
			Buffer buffer{};
			auto bufferSizeBytes = sizeof(_cameraData);
			buffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer._createInfo.size = bufferSizeBytes;
			buffer._createInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			vkCreateBuffer(logicalDevice, &buffer._createInfo, nullptr, &buffer._buffer);

			// Allocate memory for the buffer.
			VkMemoryRequirements requirements{};
			vkGetBufferMemoryRequirements(logicalDevice, buffer._buffer, &requirements);
			buffer._gpuMemory = PhysicalDevice::AllocateMemory(physicalDevice, logicalDevice, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

			// Map memory to the correct GPU and CPU ranges for the buffer.
			vkBindBufferMemory(logicalDevice, buffer._buffer, buffer._gpuMemory, 0);
			vkMapMemory(logicalDevice, buffer._gpuMemory, 0, bufferSizeBytes, 0, &buffer._cpuMemory);
			memcpy(buffer._cpuMemory, &_cameraData, bufferSizeBytes);

			_buffers.push_back(buffer);

			VkDescriptorPool descriptorPool{};
			VkDescriptorPoolSize poolSizes[1] = { VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 } };
			VkDescriptorPoolCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			createInfo.maxSets = (uint32_t)1;
			createInfo.poolSizeCount = (uint32_t)1;
			createInfo.pPoolSizes = poolSizes;
			vkCreateDescriptorPool(logicalDevice, &createInfo, nullptr, &descriptorPool);

			VkDescriptorSet descriptorSet = VkHelper::AllocateDescriptorSet(logicalDevice, descriptorPool, layouts[descriptorSetID]._layout);

			// Update the descriptor set's data with the environment map's image.
			VkDescriptorBufferInfo bufferInfo{ buffer._buffer, 0, buffer._createInfo.size };
			VkWriteDescriptorSet writeInfo = {};
			writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeInfo.dstSet = descriptorSet;
			writeInfo.descriptorCount = 1;
			writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writeInfo.pBufferInfo = &bufferInfo;
			writeInfo.dstBinding = 0;
			vkUpdateDescriptorSets(logicalDevice, 1, &writeInfo, 0, nullptr);

			auto descriptorSets = std::vector<VkDescriptorSet>{ descriptorSet };
			_shaderResources._data.try_emplace(layouts[descriptorSetID], descriptorSets);
			return _shaderResources;
		}

		void UpdateShaderResources() {
			auto& globalSettings = GlobalSettings::Instance();

			_cameraData.worldToCamera = _view._matrix;
			_cameraData.tanHalfHorizontalFov = tan(glm::radians(_horizontalFov / 2.0f));
			_cameraData.aspectRatio = Helper::Convert<uint32_t, float>(globalSettings._windowWidth) / Helper::Convert<uint32_t, float>(globalSettings._windowHeight);
			_cameraData.nearClipDistance = _nearClippingDistance;
			_cameraData.farClipDistance = _farClippingDistance;
			_cameraData.transform = _localTransform.Position();

			memcpy(_buffers[0]._cpuMemory, &_cameraData, sizeof(_cameraData));
		}

		void Update(VulkanContext& vkContext) {
			auto& input = KeyboardMouse::Instance();
			auto& time = Time::Instance();
			auto mouseSens = GlobalSettings::Instance()._mouseSensitivity;
			_yaw += (float)input._deltaMouseX * mouseSens;

			if ((_pitch + (input._deltaMouseY * mouseSens)) > -90 && (_pitch + (input._deltaMouseY * mouseSens)) < 90) {
				_pitch += (float)input._deltaMouseY * mouseSens;
			}

			if (input.IsKeyHeldDown(GLFW_KEY_Q)) {
				_roll += 0.1f * (float)Time::Instance()._deltaTime;
			}

			if (input.IsKeyHeldDown(GLFW_KEY_E)) {
				_roll -= 0.1f * (float)Time::Instance()._deltaTime;
			}

			if (input.IsKeyHeldDown(GLFW_KEY_W)) {
				_localTransform.Translate(_localTransform.Forward() * 0.05f * (float)time._deltaTime);
			}

			if (input.IsKeyHeldDown(GLFW_KEY_A)) {
				_localTransform.Translate(-_localTransform.Right() * 0.05f * (float)time._deltaTime);
			}

			if (input.IsKeyHeldDown(GLFW_KEY_S)) {
				_localTransform.Translate(-_localTransform.Forward() * 0.05f * (float)time._deltaTime);
			}

			if (input.IsKeyHeldDown(GLFW_KEY_D)) {
				_localTransform.Translate(_localTransform.Right() * 0.05f * (float)time._deltaTime);
			}

			if (input.IsKeyHeldDown(GLFW_KEY_SPACE)) {
				_localTransform.Translate(_localTransform.Up() * 0.05f * (float)time._deltaTime);
			}

			if (input.IsKeyHeldDown(GLFW_KEY_LEFT_CONTROL)) {
				_localTransform.Translate(-_localTransform.Up() * 0.05f * (float)time._deltaTime);
			}

			float _deltaYaw = (_yaw - _lastYaw);
			float _deltaPitch = (_pitch - _lastPitch);
			float _deltaRoll = (_roll - _lastRoll);

			_lastYaw = _yaw;
			_lastPitch = _pitch;
			_lastRoll = _roll;

			// First apply roll rotation.
			_localTransform.Rotate(_localTransform.Forward(), _deltaRoll);

			// Transform the up vector according to delta roll but not delta pitch, so the
			// up vector is not affected by looking up and down, but it is by rolling the camera
			// left and right.
			auto axis = _localTransform.Forward(); // This gets the pitch-independent forward vector.
			auto angleRadians = glm::radians(_deltaRoll);
			auto cosine = cos(angleRadians / 2.0f);
			auto sine = sin(angleRadians / 2.0f);
			_up = glm::quat(cosine, axis.x * sine, axis.y * sine, axis.z * sine) * _up;

			// Then apply yaw rotation.
			_localTransform.Rotate(_up, _deltaYaw);

			// Then pitch.
			_localTransform.Rotate(_localTransform.Right(), _deltaPitch);

			// Vulkan's coordinate system is: X points to the right, Y points down, Z points into the screen.
			// Vulkan's viewport coordinate system is right handed and we are doing all our calculations assuming 
			// +Z is forward, +Y is up and +X is right, so using a left-handed coordinate system.

			// We use the inverse of the camera transformation matrix because this matrix will be applied to each vertex of each object in the scene in the vertex shader.
			// There is no such thing as a camera, because in the end we use the position of the vertices to draw stuff on screen. We can only modify the position of the
			// vertices as if they were viewed from the camera, because in the end the physical position of the vertex ends up in the vertex shader, which then calculates
			// its position on 2D screen using the projection matrix.
			// Suppose that our vertex V is at (0,10,0) (so above your head if you are looking down the z axis from the origin)
			// Also suppose that our camera is at (0,0,-10). This means that the camera is behind us by 10 units and can likely see the object
			// somewhat in the upper area of its screen. If we want to see vertex V as if we viewed it from the camera, we would have to move
			// vertex V 10 units forward from its current position. If instead of moving the camera BACK 10 units we move vertex V FORWARD 10 units, 
			// we get the exact same effect as if we actually had a camera and moved it backwards.
			_view._matrix = glm::inverse(_localTransform._matrix);

			float _deltaScrollY = ((float)input._scrollY - _lastScrollY);
			_horizontalFov -= _deltaScrollY;
			_lastScrollY = (float)input._scrollY;

			UpdateShaderResources();
		}
	};

	class SceneLoader {
	public:
		static std::vector<Material> LoadMaterials(VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice, tinygltf::Model& gltfScene) {
			std::vector<Material> outMaterials;

			for (int i = 0; i < gltfScene.materials.size(); ++i) {
				Material m;
				auto& baseColorTextureIndex = gltfScene.materials[i].pbrMetallicRoughness.baseColorTexture.index;
				m._name = gltfScene.materials[i].name;

				if (baseColorTextureIndex >= 0) {
					auto& baseColorImageIndex = gltfScene.textures[baseColorTextureIndex].source;
					auto& baseColorImageData = gltfScene.images[baseColorImageIndex].image;
					unsigned char* copiedImageData = new unsigned char[baseColorImageData.size()];
					memcpy(copiedImageData, baseColorImageData.data(), baseColorImageData.size());
					auto size = VkExtent2D{ (uint32_t)gltfScene.images[baseColorImageIndex].width, (uint32_t)gltfScene.images[baseColorImageIndex].height };

					auto& imageCreateInfo = m._albedo._createInfo;
					imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
					imageCreateInfo.extent = { (uint32_t)size.width, (uint32_t)size.height, 1 };
					imageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
					imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
					imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
					imageCreateInfo.arrayLayers = 1;
					imageCreateInfo.mipLevels = 1;
					imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
					imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
					imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
					CheckResult(vkCreateImage(logicalDevice, &imageCreateInfo, nullptr, &m._albedo._image));

					// Allocate memory on the GPU for the image.
					VkMemoryRequirements reqs;
					vkGetImageMemoryRequirements(logicalDevice, m._albedo._image, &reqs);
					VkMemoryAllocateInfo allocInfo{};
					allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
					allocInfo.allocationSize = reqs.size;
					allocInfo.memoryTypeIndex = PhysicalDevice::GetMemoryTypeIndex(physicalDevice, reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
					VkDeviceMemory mem;
					CheckResult(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &mem));
					CheckResult(vkBindImageMemory(logicalDevice, m._albedo._image, mem, 0));

					auto& imageViewCreateInfo = m._albedo._viewCreateInfo;
					imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
					imageViewCreateInfo.components = { {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY} };
					imageViewCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
					imageViewCreateInfo.image = m._albedo._image;
					imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
					imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
					imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
					imageViewCreateInfo.subresourceRange.layerCount = 1;
					imageViewCreateInfo.subresourceRange.levelCount = 1;
					CheckResult(vkCreateImageView(logicalDevice, &imageViewCreateInfo, nullptr, &m._albedo._view));

					auto& samplerCreateInfo = m._albedo._samplerCreateInfo;
					samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
					samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
					samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
					samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
					samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
					samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
					samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
					vkCreateSampler(logicalDevice, &samplerCreateInfo, nullptr, &m._albedo._sampler);

					m._albedo._pData = copiedImageData;
					m._albedo._sizeBytes = baseColorImageData.size();

					outMaterials.push_back(m);
				}
			}

			return outMaterials;
		}

		static Mesh* ProcessMesh(tinygltf::Mesh& gltfMesh, tinygltf::Model& gltfScene, Scene& scene, VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice, VkCommandPool& commandPool, VkQueue& queue) {
			for (auto& gltfPrimitive : gltfMesh.primitives) {

				auto faceIndicesAccessorIndex = gltfPrimitive.indices;
				auto vertexPositionsAccessorIndex = gltfPrimitive.attributes["POSITION"];
				auto vertexNormalsAccessorIndex = gltfPrimitive.attributes["NORMAL"];
				auto uvCoords0AccessorIndex = gltfPrimitive.attributes["TEXCOORD_0"];
				//auto uvCoords1AccessorIndex = gltfPrimitive.attributes["TEXCOORD_1"];
				//auto uvCoords2AccessorIndex = gltfPrimitive.attributes["TEXCOORD_2"];

				auto faceIndicesAccessor = gltfScene.accessors[faceIndicesAccessorIndex];
				auto positionsAccessor = gltfScene.accessors[vertexPositionsAccessorIndex];
				auto normalsAccessor = gltfScene.accessors[vertexNormalsAccessorIndex];
				auto uvCoords0Accessor = gltfScene.accessors[uvCoords0AccessorIndex];
				//auto uvCoords1Accessor = gltfScene.accessors[uvCoords1AccessorIndex];
				//auto uvCoords2Accessor = gltfScene.accessors[uvCoords2AccessorIndex];

				// Load face indices.
				auto faceIndicesCount = faceIndicesAccessor.count;
				auto faceIndicesBufferIndex = gltfScene.bufferViews[faceIndicesAccessor.bufferView].buffer;
				auto faceIndicesBufferOffset = gltfScene.bufferViews[faceIndicesAccessor.bufferView].byteOffset;
				auto faceIndicesBufferStride = faceIndicesAccessor.ByteStride(gltfScene.bufferViews[faceIndicesAccessor.bufferView]);
				auto faceIndicesBufferSizeBytes = faceIndicesCount * faceIndicesBufferStride;
				std::vector<unsigned int> faceIndices(faceIndicesCount);
				if (faceIndicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
					for (int i = 0; i < faceIndicesCount; ++i) {
						unsigned short index = 0;
						memcpy(&index, gltfScene.buffers[faceIndicesBufferIndex].data.data() + faceIndicesBufferOffset + i * faceIndicesBufferStride, faceIndicesBufferStride);
						faceIndices[i] = index;
					}
				}
				else if (faceIndicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
					memcpy(faceIndices.data(), gltfScene.buffers[faceIndicesBufferIndex].data.data() + faceIndicesBufferOffset, faceIndicesBufferSizeBytes);
				}

				// Load vertex Positions.
				auto vertexPositionsCount = positionsAccessor.count;
				auto vertexPositionsBufferIndex = gltfScene.bufferViews[positionsAccessor.bufferView].buffer;
				auto vertexPositionsBufferOffset = gltfScene.bufferViews[positionsAccessor.bufferView].byteOffset;
				auto vertexPositionsBufferStride = positionsAccessor.ByteStride(gltfScene.bufferViews[positionsAccessor.bufferView]);
				auto vertexPositionsBufferSizeBytes = vertexPositionsCount * vertexPositionsBufferStride;
				std::vector<glm::vec3> vertexPositions(vertexPositionsCount);
				memcpy(vertexPositions.data(), gltfScene.buffers[vertexPositionsBufferIndex].data.data() + vertexPositionsBufferOffset, vertexPositionsBufferSizeBytes);

				// Load vertex normals.
				auto vertexNormalsCount = normalsAccessor.count;
				auto vertexNormalsBufferIndex = gltfScene.bufferViews[normalsAccessor.bufferView].buffer;
				auto vertexNormalsBufferOffset = gltfScene.bufferViews[normalsAccessor.bufferView].byteOffset;
				auto vertexNormalsBufferStride = normalsAccessor.ByteStride(gltfScene.bufferViews[normalsAccessor.bufferView]);
				auto vertexNormalsBufferSizeBytes = vertexNormalsCount * vertexNormalsBufferStride;
				std::vector<glm::vec3> vertexNormals(vertexNormalsCount);
				memcpy(vertexNormals.data(), gltfScene.buffers[vertexNormalsBufferIndex].data.data() + vertexNormalsBufferOffset, vertexNormalsBufferSizeBytes);

				// Load UV coordinates for UV slot 0.
				auto uvCoords0Count = uvCoords0Accessor.count;
				auto uvCoords0BufferIndex = gltfScene.bufferViews[uvCoords0Accessor.bufferView].buffer;
				auto uvCoords0BufferOffset = gltfScene.bufferViews[uvCoords0Accessor.bufferView].byteOffset;
				auto uvCoords0BufferStride = uvCoords0Accessor.ByteStride(gltfScene.bufferViews[uvCoords0Accessor.bufferView]);
				auto uvCoords0BufferSize = uvCoords0Count * uvCoords0BufferStride;
				std::vector<glm::vec2> uvCoords0(uvCoords0Count);
				memcpy(uvCoords0.data(), gltfScene.buffers[uvCoords0BufferIndex].data.data() + uvCoords0BufferOffset, uvCoords0BufferSize);

				auto mesh = new Mesh();

				bool found = false;
				if (gltfPrimitive.material >= 0) {
					for (int i = 0; i < scene._materials.size() && !found; ++i) {
						if (scene._materials[i]._name == gltfScene.materials[gltfPrimitive.material].name) {
							mesh->_materialIndex = i;
							found = true;
						}
					}
					if (!found) {
						mesh->_materialIndex = 0;
					}
				}

				// Gather vertices and face indices.
				std::vector<Vertex> vertices;
				vertices.resize(vertexPositions.size());
				for (int i = 0; i < vertexPositions.size(); ++i) {
					Vertex v;

					// Transform all 3D space vectors into the engine's coordinate system (X Right, Y Up, Z forward).
					/*auto position = GltfToEngine()._matrix * glm::vec4(vertexPositions[i], 1.0f);
					auto normal = GltfToEngine()._matrix * glm::vec4(vertexNormals[i], 1.0f);;*/

					vertexPositions[i].x = -vertexPositions[i].x;
					vertexNormals[i].x = -vertexNormals[i].x;

					// Set the vertex attributes.
					/*v._position = glm::vec3(position);
					v._normal = glm::vec3(normal);*/
					v._position = vertexPositions[i];
					v._normal = vertexNormals[i];
					v._uvCoord = uvCoords0[i];
					vertices[i] = v;
				}

				// Copy vertices to the GPU.
				mesh->CreateVertexBuffer(physicalDevice, logicalDevice, commandPool, queue, vertices);

				// Copy face indices to the GPU.
				mesh->CreateIndexBuffer(physicalDevice, logicalDevice, commandPool, queue, faceIndices);

				return mesh;
			}

			return nullptr;
		}

		static Transform GetGltfNodeTransform(tinygltf::Node& gltfNode) {
			Transform outTransform;
			auto& tr = gltfNode.translation;
			auto& sc = gltfNode.scale;
			auto& rot = gltfNode.rotation;

			auto translation = glm::mat4x4{};
			if (tr.size() == 3) {
				auto translation = glm::mat4x4{
					glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), // Column 1
					glm::vec4(0.0f,  1.0f, 0.0f, 0.0f), // Column 2
					glm::vec4(0.0f,  0.0f, 1.0f, 0.0f), // Column 3
					glm::vec4(-tr[0],  tr[1], tr[2], 1.0f)	// Column 4
				};

				outTransform._matrix *= translation;
			}

			auto r = Transform();
			if (rot.size() == 4) {
				r.Rotate(glm::quat{ -(float)rot[3], (float)rot[0], (float)rot[1], (float)rot[2] });
				outTransform._matrix *= r._matrix;
			}

			/*if (sc.size() == 3) {
				gameObject._transform.SetScale(glm::vec3{ sc[0], sc[1], sc[2] });
			}*/

			return outTransform;
		}

		struct Node {
			int gltfSceneIndex;
			std::string name;
			Node* parent;
			std::vector<Node*> children;
		};

		static GameObject* ProcessNode(Node node, tinygltf::Model& gltfScene, Scene& scene, VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice, VkCommandPool& commandPool, VkQueue& queue) {
			auto& gltfNode = gltfScene.nodes[node.gltfSceneIndex];
			auto gameObject = new GameObject(gltfNode.name, &scene);
			auto gltfNodeTransform = GetGltfNodeTransform(gltfNode);
			gameObject->_localTransform = gltfNodeTransform._matrix;

			auto gltfMesh = gltfScene.meshes[gltfNode.mesh];
			gameObject->_pMesh = ProcessMesh(gltfMesh, gltfScene, scene, logicalDevice, physicalDevice, commandPool, queue);
			gameObject->_pMesh->_pGameObject = gameObject;
			return gameObject;
		}

		static GameObject* ProcessNodeHierarchy(Node root, tinygltf::Model& gltfScene, Scene& scene, VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice, VkCommandPool& commandPool, VkQueue& queue) {
			GameObject* outGameObject;
			if (root.gltfSceneIndex >= 0) {
				outGameObject = ProcessNode(root, gltfScene, scene, logicalDevice, physicalDevice, commandPool, queue);
			}
			else {
				outGameObject = new GameObject("Root", &scene);
			}

			for (int i = 0; i < root.children.size(); ++i) {
				auto child = ProcessNodeHierarchy(*root.children[i], gltfScene, scene, logicalDevice, physicalDevice, commandPool, queue);
				child->_pParent = outGameObject;
				outGameObject->_children.push_back(child);
			}
			return outGameObject;
		}

		static Node* FindExisting(Node* parent, int indexToFind) {
			if (parent->gltfSceneIndex == indexToFind) {
				return parent;
			}
			for (int i = 0; i < parent->children.size(); ++i) {
				Node* found = FindExisting(parent->children[i], indexToFind);
				if (found != nullptr) {
					return found;
				}
			}
			return nullptr;
		}

		static void RemoveExisting(Node* parent, Node* toRemove) {
			if (parent->parent != nullptr) {
				auto nodeToRemove = std::find_if(parent->parent->children.begin(), parent->parent->children.end(), [toRemove](Node* n) {return n->gltfSceneIndex == toRemove->gltfSceneIndex; });
				if (nodeToRemove != parent->parent->children.end()) {
					parent->parent->children.erase(nodeToRemove);
				}
			}

			for (int i = 0; i < parent->children.size(); ++i) {
				RemoveExisting(parent->children[i], toRemove);
			}
		}

		static Node* CreateNodeHierarchy(tinygltf::Model& gltfScene) {
			Node* root = new Node{ -1, "Root", nullptr, {} };

			for (int i = 0; i < gltfScene.nodes.size(); ++i) {
				auto existing = FindExisting(root, i);
				if (existing == nullptr) {
					existing = new Node{ i, gltfScene.nodes[i].name, root, {} };
				}

				for (int j = 0; j < gltfScene.nodes[i].children.size(); ++j) {
					auto childIndex = gltfScene.nodes[i].children[j];
					auto existingChild = FindExisting(root, childIndex);
					if (existingChild == nullptr) {
						existingChild = new Node{ childIndex, gltfScene.nodes[childIndex].name, existing, {} };
					}
					else {
						RemoveExisting(root, existingChild);
					}

					existing->children.push_back(existingChild);
				}
				root->children.push_back(existing);
			}

			return root;
		}

		static void DestroyNodeHierarchy(Node* root) {
			for (int i = 0; i < root->children.size(); ++i) {
				DestroyNodeHierarchy(root->children[i]);
			}
			free(root);
			root = nullptr;
		}

		static Scene LoadFile(std::filesystem::path filePath, VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice, VkCommandPool& commandPool, VkQueue& queue) {
			auto s = new Scene(logicalDevice, physicalDevice);
			auto& scene = *s;
			scene._pointLights.push_back(PointLight("DefaultLight"));

			tinygltf::Model gltfScene;
			tinygltf::TinyGLTF loader;
			std::string err;
			std::string warn;

			bool ret = loader.LoadBinaryFromFile(&gltfScene, &err, &warn, filePath.string());
			std::cout << warn << std::endl;
			std::cout << err << std::endl;

			auto materials = LoadMaterials(logicalDevice, physicalDevice, gltfScene);
			scene._materials.insert(scene._materials.end(), materials.begin(), materials.end());

			// Creates a hierarchy of nodes from the flat list of nodes that tinygltf's loader filled.
			// This is done because the transforms of each node are relative to the parent, and having a tree-like structure makes it much easier to apply transforms hierarchically.
			Node* rootNode = CreateNodeHierarchy(gltfScene);
			scene._pRootGameObject = ProcessNodeHierarchy(*rootNode, gltfScene, scene, logicalDevice, physicalDevice, commandPool, queue);
			DestroyNodeHierarchy(rootNode);
			rootNode = nullptr;

			return scene;
		}
	};

	VkShaderModule CreateShaderModule(VkDevice logicalDevice, const std::filesystem::path& absolutePath) {
		std::ifstream file(absolutePath.c_str(), std::ios::ate | std::ios::binary);
		VkShaderModule shaderModule = nullptr;
		if (file.is_open()) {
			std::vector<char> fileBytes(file.tellg());
			file.seekg(0, std::ios::beg);
			file.read(fileBytes.data(), fileBytes.size());
			file.close();

			VkShaderModuleCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.codeSize = fileBytes.size();
			createInfo.pCode = (uint32_t*)fileBytes.data();

			CheckResult(vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule));
		}
		else {
			auto msg = "failed to open file " + absolutePath.string();
			Exit(1, msg.c_str());
			exit(0);
		}

		return shaderModule;
	}

	/**
	 * @brief Encapsulates info for a swapchain.
	 * The swapchain is an image manager; it manages everything that involves presenting images to the screen,
	 * or more precisely passing the contents of the framebuffers on the GPU down to the window.
	 */
	class Swapchain {
	public:
		/**
		 * @brief Identifier for Vulkan.
		 */
		VkSwapchainKHR _handle;

		/**
		 * @brief These are the buffers that contain the final rendered images shown on screen.
		 * A framebuffer is stored on a different portion of memory with respect to the depth and
		 * color attachments used by a render pass. The depth and color images CONTRIBUTE to generating
		 * an image for a framebuffer.
		 */
		std::vector<VkFramebuffer> _frameBuffers;

		/**
		 * @brief Dimensions in pixels of the framebuffers.
		 */
		VkExtent2D _framebufferSize;

		/**
		 * @brief Image format that the window surface expects when it has to send images from a framebuffer to a monitor.
		 */
		VkSurfaceFormatKHR _surfaceFormat;

		/**
		 * @brief Used by Vulkan to know where and how to direct the contents of the framebuffers to the window on the screen.
		 */
		VkSurfaceKHR _windowSurface;

		/**
		 * @brief Old swapchain handle used when the swapchain is recreated.
		 */
		VkSwapchainKHR _oldSwapchainHandle;

		/**
		 * @brief The amount of images used by this swapchain to show render results in turn.
		 */
		uint32_t _imageCount;

		/**
		 * @brief Swapchain images used for presentation (showing the render results to the window).
		 */
		std::vector<Image> _images;
	};

	/**
	 * @brief Represents the Vulkan application.
	 */
	class VulkanApplication : public IVulkanUpdatable {
	public:

		/**
		 * @brief Wrapper for the window shown on screen.
		 */
		GLFWwindow* _pWindow;

		/**
		 * @brief Root for all Vulkan functionality.
		 */
		VkInstance _instance;

		/**
		 * @brief Connects the Vulkan API to the windowing system, so that Vulkan knows how to interact with the window on the screen.
		 */
		VkSurfaceKHR _windowSurface;

		/**
		 * @brief Represents the physical GPU. This is mostly used for querying the GPU about its hardware properties so that we know
		 * how to handle memory.
		 */
		VkPhysicalDevice _physicalDevice;

		/**
		 * @brief Represents the GPU and its inner workings at the logical level.
		 */
		VkDevice _logicalDevice;

		/**
		 * @brief Function pointer called by Vulkan each time it wants to report an error.
		 * Error reporting is set by enabling validation layers.
		 */
		VkDebugReportCallbackEXT _callback;

		/**
		 * @brief Semaphore that will be used by Vulkan to signal when an image has finished
		 * rendering and is available to be rendered to in one of the framebuffers.
		 */
		VkSemaphore _imageAvailableSemaphore;

		/**
		 * @brief Same as imageAvailableSemaphore.
		 */
		VkSemaphore	_renderingFinishedSemaphore;

		/**
		 * @brief .
		 */
		VkSemaphore	_uiRenderingFinishedSemaphore;

		/**
		 * @brief Encapsulates info for a render pass.
		 * A render pass represents an execution of an entire graphics pipeline to create an image.
		 * Render passes use what are called (in Vulkan gergo) attachments. Attachments are rendered images that contribute to rendering
		 * the final image that will go in the framebuffer. It is the renderpass's job to also do compositing, which
		 * is defining the logic according to which the attachments are merged to create the final image.
		 * See Swapchain to understand what framebuffers are.
		 */
		struct {
			/**
			 * @brief Identifier for Vulkan.
			 */
			VkRenderPass _handle;

			/**
			 * @brief Attachments used by the GPU to write color information to.
			 */
			std::vector<Image> _colorImages;

			/**
			 * @brief Attachment that stores per-pixel depth information for the hardwired depth testing stage.
			 * This makes sure that the pixels of each triangle are rendered or not, depending on which pixel
			 * is closer to the camera, which is the information stored in this image.
			 */
			Image _depthImage;

		} _renderPass;

		/**
		 * @brief The graphics pipeline represents, at the logical level, the entire process
		 * of inputting vertices, indices and textures into the GPU and getting a 2D image that represents the scene
		 * passed in out of it. In early GPUs, this process was hardwired into the graphics chip, but as technology improved and needs
		 * for better and more complex graphics increased, GPU producers have taken steps to make this a much more programmable
		 * and CPU-like process, so much so that technologies like CUDA (Compute Uniform Device Architecture) have come out.
		 *
		 * Nowadays the typical GPU consists of an array of clusters of microprocessors, where each micro
		 * processor is highly multithreaded, and its ALU and instruction set (thus its circuitry) optimized for operating on
		 * floating point numbers, vectors and matrices (as that is what is used to represent coordinates in space and space transformations).
		 *
		 * CUDA is the result of NVidia and Microsoft working together to improve the programmers' experience for programming
		 * shaders; the result is a product that maps C/C++ instructions to the GPU's ALUs instruction set, to take full advantage
		 * of the GPU's high amount of cores and threads. By doing this, things like crypto mining and machine learning have gotten
		 * a huge boost, as the data that requires manipulation is very loosely coupled (has very few dependecies on the data in
		 * the same set) and, as a result, can be processed by many independent threads simultaneously. This is called a compute pipeline.
		 * Khronos, the developers of the Vulkan API, have also done something similar called OpenCL, the compute side of OpenGL.
		 *
		 * The typical graphics (or render) pipeline consists of programmable, configurable and hardwired stages, where:
		 * a) The programmable stages are custom stages that will be run on the GPU's multi-purpose array of microprocessors using a program (a.k.a shader).
		 * b) The configurable stages are hardwired stages that can perform their task a different way based on user configuration via calls to the Vulkan API.
		 * c) The hardwired stages are immutable stages that cannot be changed unless manipulating the hardware.
		 * The graph of a typical graphics pipeline is shown under docs/GraphicsPipeline.jpg.
		 *
		 * More on the programmable stages:
		 * The programmable stages are the flexible stages that the programmer can fully customize by writing little programs called shaders.
		 * These shader programs will run:
		 * 1) once per vertex in the case of the vertex shader; this shader program's goal is to take vertex attrbutes in, and output
		 * a vertex color and 2D position (more precisely, a 3D position inside of the Vulkan's coordinate range, which is -1 to 1 for X and Y and 0 to 1 for Z).
		 * 2) once per pixel in the case of the fragment shader; this stage's goal is to take the rasterizer's output (which is a hardwired
		 * stage on the GPU chip as of 2022, whose goal is to color pixels based on vertex colors) and textures, and output a colored pixel based on
		 * the color of the textures and other variables such as direct and indirect lighting.
		 * There are other shader stages, but the 2 above are the strictly needed shaders in order to be able to render something.

		 * This type of execution flow has a name and is called SIMD (Single Instruction Multiple Data), as the same program (single instruction)
		 * is run independently on different cores/threads for multiple vertices or pixels (multiple data).
		 *
		 * Examples of the configurable stages are anti-aliasing and tessellation.
		 * Examples of hardwired stages are backface-culling, depth testing and alpha blending.
		 *
		 * For a good overall hardware and software explanation of a typical NVidia GPU, see
		 * https://developer.nvidia.com/gpugems/gpugems2/part-iv-general-purpose-computation-gpus-primer/chapter-30-geforce-6-series-gpu
		 */
		struct {

			/**
			 * @brief Identifier for Vulkan.
			 */
			VkPipeline _handle;

			/**
			 * This variable contains:
			 * 1) binding: the binding number of the vertex buffer defined when calling vkCmdBindVertexBuffers;
			 * 2) stride: the offset in bytes between each set of vertex attributes in the vertex buffer identified by the binding number above;
			 * 3) inputRate: unknown (info hard to find on this)
			 *
			 * Useful information:
			 * Vertex shaders can define input variables, which receive vertex attribute data transferred from
			 * one or more VkBuffer(s) by drawing commands. Vertex attribute data can be anything, but it's usually
			 * things like its position, normal and uv coordinates. Vertex shader input variables are bound to vertex buffers
			 * via an indirect binding, where the vertex shader associates a vertex input attribute number with
			 * each variable: the "location" decorator. Vertex input attributes (location) are associated to vertex input
			 * bindings (binding) on a per-pipeline basis, and vertex input bindings (binding) are associated with specific buffers
			 * (VkBuffer) on a per-draw basis via the vkCmdBindVertexBuffers command. Vertex input attributes and vertex input
			 * binding descriptions also contain format information controlling how data is extracted from buffer
			 * memory and converted to the format expected by the vertex shader.
			 *
			 * In short:
			 * Each vertex buffer is identified by a binding number, defined when calling vkCmdBindVertexBuffers.
			 * Each attribute inside a vertex buffer is identified by a location number, defined when creating a pipeline in a VkVertexInputBindingDescription struct.
			 */
			VkVertexInputBindingDescription	_vertexBindingDescription;

			/**
			 * Each VkVertexInputAttributeDescription contains:
			 * 1) location: identifier for the vertex attribute; also defined in the vertex shader definition of the attribute;
			 * 2) binding: the binding number of the vertex buffer defined when calling vkCmdBindVertexBuffers;
			 * 3) format: the format of this attribute/variable, VkFormat;
			 * 4) offset: the offset of the attribute in bytes within the set of vertex attributes.
			 *
			 * Useful information:
			 * Vertex shaders can define input variables, which receive vertex attribute data transferred from
			 * one or more VkBuffer(s) by drawing commands. Vertex attribute data can be anything, but it's usually
			 * things like its position, normal and uv coordinates. Vertex shader input variables are bound to vertex buffers
			 * via an indirect binding, where the vertex shader associates a vertex input attribute number with
			 * each variable: the "location" decorator. Vertex input attributes (location) are associated to vertex input
			 * bindings (binding) on a per-pipeline basis, and vertex input bindings (binding) are associated with specific buffers
			 * (VkBuffer) on a per-draw basis via the vkCmdBindVertexBuffers command. Vertex input attribute and vertex input
			 * binding descriptions also contain format information controlling how data is extracted from buffer
			 * memory and converted to the format expected by the vertex shader.
			 *
			 * In short:
			 * Each vertex buffer is identified by a binding number, defined every time we draw something by calling vkCmdBindVertexBuffers.
			 * Each attribute inside a vertex buffer is identified by a location number, defined here. The location number is defined when creating a pipeline.
			 */
			std::vector<VkVertexInputAttributeDescription> _vertexAttributeDescriptions;

			/**
			* @brief Access to descriptor sets from a pipeline is accomplished through a pipeline layout. Zero or more descriptor set layouts and zero or more
			* push constant ranges are combined to form a pipeline layout object describing the complete set of resources that can be accessed by a pipeline.
			* The pipeline layout represents a sequence of descriptor sets with each having a specific layout. This sequence of layouts is used to determine
			* the interface between shader stages and shader resources. Each pipeline is created using a pipeline layout.
			*/
			VkPipelineLayout _layout;

			/**
			 * @brief See ShaderResources definition.
			 */
			ShaderResources _shaderResources;

		} _graphicsPipeline;

		/**
		 * @brief Context that contains all the info that the Nuklear library uses to render UI.
		 */
		class NuklearUiContext {
		public:
			nk_context* _ctx;
			VkDevice _logicalDevice;
			VkPhysicalDevice _physicalDevice;
			GLFWwindow* _pWindow;
			uint32_t* _pQueueFamilyIndex;

			/**
			 * @brief The images to which Nuklear renders its UI.
			 */
			std::vector<Image> _overlayImages;
			std::vector<Image> _sceneImages;
			VkRenderPass _renderPass;
			VkDescriptorPool _descriptorPool;
			std::vector<VkDescriptorSet> _descriptorSets;
			std::vector<VkDescriptorSetLayout> _descriptorSetLayouts;
			VkPipelineLayout _pipelineLayout;
			VkPipeline _pipeline;
			VkCommandPool _commandPool;
			std::vector<VkCommandBuffer> _commandBuffers;
			Swapchain _swapchain;

			void Initialize(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkSurfaceKHR windowSurface, GLFWwindow* pWindow, VkQueue queue, uint32_t* pQueueFamilyIndex, Swapchain swapchain, VkRenderPass renderPass, std::vector<Image> sceneImages) {
				_logicalDevice = logicalDevice;
				_physicalDevice = physicalDevice;
				_pWindow = pWindow;
				_pQueueFamilyIndex = pQueueFamilyIndex;
				_swapchain = swapchain;
				_renderPass = renderPass;
				_sceneImages = sceneImages;

				CreateOverlayImages();
				CreateDescriptorSets();
				CreatePipelineLayout();

				uint32_t image_index;
				VkSemaphore nk_semaphore;
				std::vector<VkImageView> views; views.resize(_overlayImages.size());
				for (int i = 0; i < views.size(); ++i) views[i] = _overlayImages[i]._view;
				_ctx = nk_glfw3_init(_pWindow, _logicalDevice, _physicalDevice, *pQueueFamilyIndex, views.data(), views.size(), _swapchain._surfaceFormat.format, NK_GLFW3_INSTALL_CALLBACKS, 512 * 1024, 128 * 1024);
				/* Load Fonts: if none of these are loaded a default font will be used  */
				/* Load Cursor: if you uncomment cursor loading please hide the cursor */
				{
					struct nk_font_atlas* atlas;
					nk_glfw3_font_stash_begin(&atlas);
					/*struct nk_font *droid = nk_font_atlas_add_from_file(atlas,
					 * "../../../extra_font/DroidSans.ttf", 14, 0);*/
					 /*struct nk_font *roboto = nk_font_atlas_add_from_file(atlas,
					  * "../../../extra_font/Roboto-Regular.ttf", 14, 0);*/
					  /*struct nk_font *future = nk_font_atlas_add_from_file(atlas,
					   * "../../../extra_font/kenvector_future_thin.ttf", 13, 0);*/
					   /*struct nk_font *clean = nk_font_atlas_add_from_file(atlas,
						* "../../../extra_font/ProggyClean.ttf", 12, 0);*/
						/*struct nk_font *tiny = nk_font_atlas_add_from_file(atlas,
						 * "../../../extra_font/ProggyTiny.ttf", 10, 0);*/
						 /*struct nk_font *cousine = nk_font_atlas_add_from_file(atlas,
						  * "../../../extra_font/Cousine-Regular.ttf", 13, 0);*/
					nk_glfw3_font_stash_end(queue);
					/*nk_style_load_all_cursors(ctx, atlas->cursors);*/
				/*nk_style_set_font(ctx, &droid->handle);*/
				}

				/* GUI */
				if (nk_begin(_ctx, "Demo", nk_rect(50, 50, 230, 250),
					NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
					NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE)) {
					enum { EASY, HARD };
					static int op = EASY;
					static int property = 20;
					nk_layout_row_static(_ctx, 30, 80, 1);
					if (nk_button_label(_ctx, "button"))
						fprintf(stdout, "button pressed\n");

					nk_layout_row_dynamic(_ctx, 30, 2);
					if (nk_option_label(_ctx, "easy", op == EASY))
						op = EASY;
					if (nk_option_label(_ctx, "hard", op == HARD))
						op = HARD;

					nk_layout_row_dynamic(_ctx, 25, 1);
					nk_property_int(_ctx, "Compression:", 0, &property, 100, 10, 1);

					nk_layout_row_dynamic(_ctx, 20, 1);
					nk_label(_ctx, "background:", NK_TEXT_LEFT);
					nk_layout_row_dynamic(_ctx, 25, 1);
				}
				nk_end(_ctx);
			}

			void CreateOverlayImages() {
				_overlayImages.resize(_swapchain._imageCount);
				for (int i = 0; i < _swapchain._imageCount; ++i) {
					VkImageCreateInfo image_info{};
					image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
					image_info.imageType = VK_IMAGE_TYPE_2D;
					image_info.extent.width = _swapchain._framebufferSize.width;
					image_info.extent.height = _swapchain._framebufferSize.height;
					image_info.extent.depth = 1;
					image_info.mipLevels = 1;
					image_info.arrayLayers = 1;
					image_info.format = _swapchain._surfaceFormat.format;
					image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
					image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
					image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
					image_info.samples = VK_SAMPLE_COUNT_1_BIT;
					image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
					CheckResult(vkCreateImage(_logicalDevice, &image_info, NULL, &_overlayImages[i]._image));

					_overlayImages[i]._gpuMemory = VkHelper::AllocateGpuMemoryForImage(_logicalDevice, _physicalDevice, _overlayImages[i]._image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
					CheckResult(vkBindImageMemory(_logicalDevice, _overlayImages[i]._image, _overlayImages[i]._gpuMemory, 0));

					VkImageViewCreateInfo image_view_info{};
					image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
					image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
					image_view_info.format = _swapchain._surfaceFormat.format;
					image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
					image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
					image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
					image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
					image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					image_view_info.subresourceRange.baseMipLevel = 0;
					image_view_info.subresourceRange.levelCount = 1;
					image_view_info.subresourceRange.baseArrayLayer = 0;
					image_view_info.subresourceRange.layerCount = 1;
					image_view_info.image = _overlayImages[i]._image;
					CheckResult(vkCreateImageView(_logicalDevice, &image_view_info, NULL, &_overlayImages[i]._view));

					VkSamplerCreateInfo samplerCreateInfo{};
					samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
					samplerCreateInfo.pNext = NULL;
					samplerCreateInfo.maxAnisotropy = 1.0;
					samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
					samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
					samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
					samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
					samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
					samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
					samplerCreateInfo.mipLodBias = 0.0f;
					samplerCreateInfo.compareEnable = VK_FALSE;
					samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
					samplerCreateInfo.minLod = 0.0f;
					samplerCreateInfo.maxLod = 0.0f;
					samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
					CheckResult(vkCreateSampler(_logicalDevice, &samplerCreateInfo, NULL, &_overlayImages[i]._sampler));
				}
			}

			void CreateDescriptorSets() {
				_descriptorSets.resize(_swapchain._imageCount);
				_descriptorSetLayouts.resize(_swapchain._imageCount);

				for (int i = 0; i < _descriptorSets.size(); ++i) {
					VkDescriptorPoolSize poolSizes[2];
					poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					poolSizes[0].descriptorCount = 1;

					VkDescriptorPoolSize poolSize1{};
					poolSizes[1].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
					poolSizes[1].descriptorCount = 1;

					VkDescriptorPoolCreateInfo pool_info{};
					pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
					pool_info.poolSizeCount = 2;
					pool_info.pPoolSizes = poolSizes;
					pool_info.maxSets = 1;

					CheckResult(vkCreateDescriptorPool(_logicalDevice, &pool_info, NULL, &_descriptorPool));

					VkDescriptorSetLayoutBinding setLayoutBindings[2];
					memset(setLayoutBindings, 0, sizeof(VkDescriptorSetLayoutBinding) * 2);

					// Input attachment from the scene subpass
					setLayoutBindings[0].binding = 1;
					setLayoutBindings[0].descriptorCount = 1;
					setLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
					setLayoutBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

					// Overlay image
					setLayoutBindings[1].binding = 0;
					setLayoutBindings[1].descriptorCount = 1;
					setLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					setLayoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

					VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
					layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
					layoutCreateInfo.bindingCount = 2;
					layoutCreateInfo.pBindings = setLayoutBindings;
					CheckResult(vkCreateDescriptorSetLayout(_logicalDevice, &layoutCreateInfo, NULL, &_descriptorSetLayouts[i]));

					_descriptorSets[i] = VkHelper::AllocateDescriptorSet(_logicalDevice, _descriptorPool, _descriptorSetLayouts[i]);

					std::array<VkDescriptorImageInfo, 2> descriptors{};
					descriptors[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					descriptors[0].imageView = _overlayImages[i]._view;
					descriptors[0].sampler = _overlayImages[i]._sampler;

					descriptors[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					descriptors[1].imageView = _sceneImages[i]._view;
					descriptors[1].sampler = VK_NULL_HANDLE;

					VkWriteDescriptorSet write0{};
					write0.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					write0.dstSet = _descriptorSets[i];
					write0.dstBinding = 0;
					write0.dstArrayElement = 0;
					write0.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					write0.descriptorCount = 1;
					write0.pImageInfo = &descriptors[0];

					VkWriteDescriptorSet write1{};
					write1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					write1.dstSet = _descriptorSets[i];
					write1.dstBinding = 1;
					write1.dstArrayElement = 0;
					write1.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
					write1.descriptorCount = 1;
					write1.pImageInfo = &descriptors[1];

					VkWriteDescriptorSet descriptorWrites[2] = { write0, write1 };
					vkUpdateDescriptorSets(_logicalDevice, 2, descriptorWrites, 0, nullptr);
				}
			}

			void CreatePipelineLayout() {
				// Create pipeline layout info
				VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
				pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
				pipelineLayoutCreateInfo.setLayoutCount = _descriptorSetLayouts.size(); // Using one descriptor set layout
				pipelineLayoutCreateInfo.pSetLayouts = _descriptorSetLayouts.data();
				pipelineLayoutCreateInfo.pushConstantRangeCount = 0; // No push constants in this example
				pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

				// Create the pipeline layout
				VkPipelineLayout pipelineLayout;
				CheckResult(vkCreatePipelineLayout(_logicalDevice, &pipelineLayoutCreateInfo, nullptr, &_pipelineLayout));
			}
		};

		NuklearUiContext _uiCtx;
		Swapchain _swapchain;

		// Vulkan commands.
		VkCommandPool _commandPool;
		VkQueue _queue;
		VkFence _queueFence;
		uint32_t _queueFamilyIndex;
		std::vector<VkCommandBuffer> _drawCommandBuffers;

		// Game.
		KeyboardMouse& _input = KeyboardMouse::Instance();
		GlobalSettings& _settings = GlobalSettings::Instance();
		Scene _scene;
		Camera _mainCamera;

		void Run() {
			InitializeWindow();
			_input = KeyboardMouse(_pWindow);
			SetupVulkan();
			MainLoop();
			Cleanup(true);
		}

		static VkBool32 DebugCallback(VkDebugReportFlagsEXT flags,
			VkDebugReportObjectTypeEXT objType,
			uint64_t srcObject,
			size_t location,
			int32_t msgCode,
			const char* pLayerPrefix,
			const char* pMsg,
			void* pUserData) {
			if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
				Logger::Log("ERROR: [" + std::string(pLayerPrefix) + "] Code " + std::to_string(msgCode) + " : " + pMsg);
			}
			else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
				Logger::Log("WARNING: [" + std::string(pLayerPrefix) + "] Code " + std::to_string(msgCode) + " : " + pMsg);
			}

			return VK_FALSE;
		}

		void InitializeWindow() {
			glfwInit();
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			_pWindow = glfwCreateWindow(_settings._windowWidth, _settings._windowHeight, "Hold The Line!", nullptr, nullptr);
			glfwSetWindowSizeCallback(_pWindow, OnWindowResized);
		}

		void SetupVulkan() {
			_instance = CreateInstance();
			CreateDebugCallback(_instance);
			_windowSurface = CreateWindowSurface(_instance);
			_physicalDevice = CreatePhysicalDevice(_instance);

			auto flags = (VkQueueFlagBits)(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT);
			_queueFamilyIndex = VkHelper::FindQueueFamilyIndex(_physicalDevice, flags);
			if (!PhysicalDevice::SupportsSurface(_physicalDevice, _queueFamilyIndex, _windowSurface)) std::exit(1);

			VkDeviceQueueCreateInfo graphicsQueueInfo{};
			float queuePriority = 1.0f;
			graphicsQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			graphicsQueueInfo.queueFamilyIndex = _queueFamilyIndex;
			graphicsQueueInfo.queueCount = 1;
			graphicsQueueInfo.pQueuePriorities = &queuePriority;

			_logicalDevice = CreateLogicalDevice(_physicalDevice, { graphicsQueueInfo });
			vkGetDeviceQueue(_logicalDevice, _queueFamilyIndex, 0, &_queue);
			VkFenceCreateInfo fci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, 0 };
			vkCreateFence(_logicalDevice, &fci, NULL, &_queueFence);
			_commandPool = VkHelper::CreateCommandPool(_logicalDevice, _queueFamilyIndex);

			LoadScene();
			LoadEnvironmentMap();

			auto descriptorSetLayouts = CreateDescriptorSetLayouts();
			_graphicsPipeline._layout = CreatePipelineLayout(descriptorSetLayouts);
			CreateShaderResources(descriptorSetLayouts);

			CreateSwapchain();

			auto imageFormat = _swapchain._surfaceFormat.format;

			// Store the images used by the swap chain.
			// Note: these are the images that swap chain image indices refer to.
			// Note: actual number of images may differ from requested number, since it's a lower bound.
			uint32_t actualImageCount = 0;
			CheckResult(vkGetSwapchainImagesKHR(_logicalDevice, _swapchain._handle, &actualImageCount, nullptr));

			_renderPass._colorImages.resize(actualImageCount);

			std::vector<VkImage> swapchainImages;
			swapchainImages.resize(actualImageCount);
			CheckResult(vkGetSwapchainImagesKHR(_logicalDevice, _swapchain._handle, &actualImageCount, swapchainImages.data()));
			_swapchain._imageCount = actualImageCount;
			_swapchain._images.resize(actualImageCount);

			// Create the color attachments.
			for (uint32_t i = 0; i < actualImageCount; ++i) {

				// Image view used by the UI shader as output attachment, after it has combined the UI image with the scene image output by the first subpass.
				_swapchain._images[i]._image = swapchainImages[i];
				auto& createInfo = _swapchain._images[i]._viewCreateInfo;
				createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				createInfo.image = swapchainImages[i];
				createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				createInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
				createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				createInfo.subresourceRange.baseMipLevel = 0;
				createInfo.subresourceRange.levelCount = 1;
				createInfo.subresourceRange.baseArrayLayer = 0;
				createInfo.subresourceRange.layerCount = 1;
				vkCreateImageView(_logicalDevice, &createInfo, nullptr, &_swapchain._images[i]._view);

				// Scene image.
				auto& rpColorImg = _renderPass._colorImages[i];
				auto& imageCreateInfo = rpColorImg._createInfo;
				imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
				imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
				imageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
				imageCreateInfo.extent = { _swapchain._framebufferSize.width, _swapchain._framebufferSize.height, 1 };
				imageCreateInfo.mipLevels = 1;
				imageCreateInfo.arrayLayers = 1;
				imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
				imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
				imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
				vkCreateImage(_logicalDevice, &imageCreateInfo, nullptr, &rpColorImg._image);

				// Allocate memory on the GPU for the image.
				rpColorImg._gpuMemory = VkHelper::AllocateGpuMemoryForImage(_logicalDevice, _physicalDevice, rpColorImg._image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				vkBindImageMemory(_logicalDevice, rpColorImg._image, rpColorImg._gpuMemory, 0);

				auto& imageViewCreateInfo = rpColorImg._viewCreateInfo;
				imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				imageViewCreateInfo.image = rpColorImg._image;
				imageViewCreateInfo.format = imageCreateInfo.format;
				imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
				imageViewCreateInfo.subresourceRange.levelCount = 1;
				imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
				imageViewCreateInfo.subresourceRange.layerCount = 1;
				imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				vkCreateImageView(_logicalDevice, &imageViewCreateInfo, nullptr, &rpColorImg._view);

				auto& samplerCreateInfo = rpColorImg._samplerCreateInfo;
				samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
				samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
				samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
				samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
				samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
				samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
				samplerCreateInfo.anisotropyEnable = VK_FALSE;
				samplerCreateInfo.maxAnisotropy = 1.0f;
				samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
				samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
				samplerCreateInfo.compareEnable = VK_FALSE;
				samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
				samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
				samplerCreateInfo.mipLodBias = 0.0f;
				samplerCreateInfo.minLod = 0.0f;
				samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
				vkCreateSampler(_logicalDevice, &samplerCreateInfo, nullptr, &rpColorImg._sampler);
			}

			// Create the depth image.
			auto& imageCreateInfo = _renderPass._depthImage._createInfo;
			imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageCreateInfo.arrayLayers = 1;
			imageCreateInfo.extent = { _swapchain._framebufferSize.width, _swapchain._framebufferSize.height, 1 };
			imageCreateInfo.format = VK_FORMAT_D32_SFLOAT;
			imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
			imageCreateInfo.mipLevels = 1;
			imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			vkCreateImage(_logicalDevice, &imageCreateInfo, nullptr, &_renderPass._depthImage._image);

			// Allocate memory on the GPU for the image.
			_renderPass._depthImage._gpuMemory = VkHelper::AllocateGpuMemoryForImage(_logicalDevice, _physicalDevice, _renderPass._depthImage._image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			vkBindImageMemory(_logicalDevice, _renderPass._depthImage._image, _renderPass._depthImage._gpuMemory, 0);

			auto& imageViewCreateInfo = _renderPass._depthImage._viewCreateInfo;
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.components = { {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY} };
			imageViewCreateInfo.format = VK_FORMAT_D32_SFLOAT;
			imageViewCreateInfo.image = _renderPass._depthImage._image;
			imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
			imageViewCreateInfo.subresourceRange.layerCount = 1;
			imageViewCreateInfo.subresourceRange.levelCount = 1;
			vkCreateImageView(_logicalDevice, &imageViewCreateInfo, nullptr, &_renderPass._depthImage._view);

			auto& samplerCreateInfo = _renderPass._depthImage._samplerCreateInfo;
			samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCreateInfo.anisotropyEnable = false;
			samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
			samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
			samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
			vkCreateSampler(_logicalDevice, &samplerCreateInfo, nullptr, &_renderPass._depthImage._sampler);

			// Note: hardware will automatically transition attachment to the specified layout
			// Note: index refers to attachment descriptions array.
			VkAttachmentReference swapchainImageAttachmentReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			VkAttachmentReference colorAttachmentRefSubpass0 = { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			VkAttachmentReference inputAttachmentRefSubpass1 = { 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			VkAttachmentReference depthAttachmentReference = { 2, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

			// Note: this is a description of how the attachments of the render pass will be used in this sub pass
			// e.g. if they will be read in shaders and/or drawn to.
			VkSubpassDescription sceneRenderingSubpass = {};
			sceneRenderingSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			sceneRenderingSubpass.colorAttachmentCount = 1;
			sceneRenderingSubpass.pColorAttachments = &colorAttachmentRefSubpass0;
			sceneRenderingSubpass.pDepthStencilAttachment = &depthAttachmentReference;

			VkSubpassDescription uiRenderingSubpass = {};
			uiRenderingSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			uiRenderingSubpass.colorAttachmentCount = 1;
			uiRenderingSubpass.inputAttachmentCount = 1;
			uiRenderingSubpass.pColorAttachments = &swapchainImageAttachmentReference;
			uiRenderingSubpass.pInputAttachments = &inputAttachmentRefSubpass1;

			// Now we have to adjust the renderpass synchronization. Previously, it was possible that multiple frames 
			// were rendered simultaneously by the GPU. This is a problem when using depth buffers, because one frame 
			// could overwrite the depth buffer while a previous frame is still rendering to it.
			// We keep the subpass dependency for the color attachment we were already using.
			VkSubpassDependency colorDependency = {};
			colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			colorDependency.dstSubpass = 0;
			colorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			colorDependency.srcAccessMask = 0;
			colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			// This dependency tells Vulkan that the depth attachment in a renderpass cannot be used before 
			// previous subpasses have finished using it.
			VkSubpassDependency depthDependency = {};
			depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			depthDependency.dstSubpass = 0;
			depthDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			depthDependency.srcAccessMask = 0;
			depthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			VkSubpassDependency uiDependency = {};
			uiDependency.srcSubpass = 0;
			uiDependency.dstSubpass = 1;
			uiDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			uiDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			uiDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			uiDependency.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

			VkAttachmentDescription swapchainImageAttachmentDescription = {};
			swapchainImageAttachmentDescription.format = VK_FORMAT_R8G8B8A8_SRGB;
			swapchainImageAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
			swapchainImageAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			swapchainImageAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			swapchainImageAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			swapchainImageAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			swapchainImageAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			swapchainImageAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			// Describes how the render pass is going to use the main color attachment. An attachment is a fancy word for "image used for a render pass".
			VkAttachmentDescription colorAttachmentDescription = {};
			colorAttachmentDescription.format = VK_FORMAT_R8G8B8A8_SRGB;
			colorAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			// Describes how the render pass is going to use the depth attachment.
			VkAttachmentDescription depthAttachmentDescription = {};
			depthAttachmentDescription.flags = 0;
			depthAttachmentDescription.format = VK_FORMAT_D32_SFLOAT;
			depthAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
			depthAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			depthAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			depthAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			// Create the render pass. We pass in the main image attachment (color) and the depth image attachment, so the GPU knows how to treat
			// the images.
			VkAttachmentDescription attachmentDescriptions[] = { swapchainImageAttachmentDescription, colorAttachmentDescription, depthAttachmentDescription };
			VkSubpassDependency subpassDependencies[] = { colorDependency, depthDependency, uiDependency };
			VkSubpassDescription subpassDescriptions[] = { sceneRenderingSubpass, uiRenderingSubpass };
			VkRenderPassCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			createInfo.attachmentCount = 3;
			createInfo.pAttachments = attachmentDescriptions;
			createInfo.subpassCount = 2;
			createInfo.pSubpasses = subpassDescriptions;
			createInfo.dependencyCount = 3;
			createInfo.pDependencies = subpassDependencies;
			CheckResult(vkCreateRenderPass(_logicalDevice, &createInfo, nullptr, &_renderPass._handle));

			_swapchain._frameBuffers.resize(actualImageCount);
			_drawCommandBuffers.resize(actualImageCount);
			for (int i = 0; i < actualImageCount; ++i) _drawCommandBuffers[i] = VkHelper::CreateCommandBuffer(_logicalDevice, _commandPool);

			// Here we record the commands that will be executed in the render loop.
			for (size_t i = 0; i < actualImageCount; i++) {
				auto& currentFrameBuffer = _swapchain._frameBuffers[i];
				auto& currentCmdBuffer = _drawCommandBuffers[i];

				// We will render to the same depth image for each frame. 
				// We can just keep clearing and reusing the same depth image for every frame.
				VkImageView renderPassImages[3] = { _swapchain._images[i]._view, _renderPass._colorImages[i]._view, _renderPass._depthImage._view };
				VkFramebufferCreateInfo createInfo = {};
				createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				createInfo.renderPass = _renderPass._handle;
				createInfo.attachmentCount = 3;
				createInfo.pAttachments = renderPassImages;
				createInfo.width = _swapchain._framebufferSize.width;
				createInfo.height = _swapchain._framebufferSize.height;
				createInfo.layers = 1;
				CheckResult(vkCreateFramebuffer(_logicalDevice, &createInfo, nullptr, &currentFrameBuffer));

				_uiCtx.Initialize(_logicalDevice, _physicalDevice, _windowSurface, _pWindow, _queue, &_queueFamilyIndex, _swapchain, _renderPass._handle, _renderPass._colorImages);
				CreateGraphicsPipelines(_renderPass._handle, _graphicsPipeline._layout, _uiCtx._pipelineLayout);

				VkCommandBufferBeginInfo beginInfo = {};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
				vkBeginCommandBuffer(_drawCommandBuffers[i], &beginInfo);

				VkHelper::TransitionImageLayout(_logicalDevice, _commandPool, _queue, _swapchain._images[i]._image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
				VkHelper::TransitionImageLayout(_logicalDevice, _commandPool, _queue, _renderPass._colorImages[i]._image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
				VkHelper::TransitionImageLayout(_logicalDevice, _commandPool, _queue, _uiCtx._overlayImages[i]._image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

				VkClearValue swapchainImageClear{ { 0.0f, 0.0f, 0.0f, 1.0f } }; // R, G, B, A.
				VkClearValue sceneImageClear = { { 0.1f, 0.1f, 0.1f, 1.0f } };
				VkClearValue depthImageClear{ { 0.0f, 0.0f, 0.0f, 0.0f } }; depthImageClear.depthStencil.depth = 1.0f;
				VkClearValue clearValues[] = { swapchainImageClear, sceneImageClear, depthImageClear };

				VkRenderPassBeginInfo renderPassBeginInfo{};
				renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				renderPassBeginInfo.renderPass = _renderPass._handle;
				renderPassBeginInfo.framebuffer = currentFrameBuffer;
				renderPassBeginInfo.renderArea.offset.x = 0;
				renderPassBeginInfo.renderArea.offset.y = 0;
				renderPassBeginInfo.renderArea.extent = _swapchain._framebufferSize;
				renderPassBeginInfo.clearValueCount = 3;
				renderPassBeginInfo.pClearValues = clearValues;

				vkCmdBeginRenderPass(currentCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
				vkCmdBindPipeline(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline._handle);

				auto& shaderResources = _graphicsPipeline._shaderResources;
				VkDescriptorSet sets[4] = { shaderResources[0][0], shaderResources[1][0], shaderResources[2][0], shaderResources[4][0] };
				vkCmdBindDescriptorSets(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline._layout, 0, 1, &shaderResources[0][0], 0, nullptr);
				vkCmdBindDescriptorSets(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline._layout, 2, 1, &shaderResources[2][0], 0, nullptr);
				vkCmdBindDescriptorSets(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline._layout, 4, 1, &shaderResources[4][0], 0, nullptr);

				for (auto& gameObject : _scene._pRootGameObject->_children)
					gameObject->Draw(_graphicsPipeline._layout, currentCmdBuffer);

				//VkHelper::TransitionImageLayout(_logicalDevice, _commandPool, _queue, _renderPass._colorImages[i]._image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

				vkCmdNextSubpass(currentCmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
				vkCmdBindPipeline(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _uiCtx._pipeline);
				vkCmdBindDescriptorSets(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _uiCtx._pipelineLayout, 0, 1, _uiCtx._descriptorSets.data(), 0, nullptr);
				vkCmdDraw(currentCmdBuffer, 3, 1, 0, 0);
				vkCmdEndRenderPass(currentCmdBuffer);
				CheckResult(vkEndCommandBuffer(currentCmdBuffer));
			}

			//CreateGraphicsPipeline();
			//RecordDrawCommands();
			CreateSemaphores();
		}

		void PhysicsUpdate() {
			while (!glfwWindowShouldClose(_pWindow)) {
				auto& time = Time::Instance();
				time.PhysicsUpdate();

				/*for (auto& gameObject : _scene._pRootGameObject->_children) {
					if (gameObject->_pMesh == nullptr) {
						continue;
					}

					if (gameObject->_name != "falling" && gameObject->_name != "ground") {
						continue;
					}

					auto& body = gameObject->_body;
					if (gameObject->_name == "falling" && !gameObject->_body._isInitialized) {
						body._updateImplementation = Falling;
					}

					if (gameObject->_name == "ground" && !gameObject->_body._isInitialized) {
						body._updateImplementation = Ground;
					}

					body.Initialize(gameObject->_pMesh);
					body.PhysicsUpdate();
				}*/
			}
		}

		void MainLoop() {
			//std::thread physicsThread(&PhysicsUpdate, this);
			VulkanContext vkContext;
			vkContext._instance = _instance;
			vkContext._logicalDevice = _logicalDevice;
			vkContext._physicalDevice = _physicalDevice;
			vkContext._commandPool = _commandPool;
			vkContext._queue = _queue;
			vkContext._queueFamilyIndex = _queueFamilyIndex;

			while (!glfwWindowShouldClose(_pWindow)) {
				Update(vkContext);
				Draw();
				glfwPollEvents();
			}

			//physicsThread.join();
		}

		void Update(VulkanContext& vkContext) {
			Time::Instance().Update();
			_input.Update();

			_mainCamera.Update(vkContext);
			_scene.Update(vkContext);
		}

		static void OnWindowResized(GLFWwindow* window, int width, int height) {
			windowResized = true;

			if (width == 0 && height == 0) {
				windowMinimized = true;
				return;
			}

			windowMinimized = false;
			GlobalSettings::Instance()._windowWidth = width;
			GlobalSettings::Instance()._windowHeight = height;
		}

		void WindowSizeChanged() {
			windowResized = false;

			// Only recreate objects that are affected by framebuffer size changes.
			Cleanup(false);
			CreateSwapchain();
			//CreateRenderPass();
			//CreateGraphicsPipeline();
			_drawCommandBuffers.resize(_swapchain._imageCount);
			for (int i = 0; i < _drawCommandBuffers.size(); ++i) _drawCommandBuffers[i] = VkHelper::CreateCommandBuffer(_logicalDevice, _commandPool);
			//RecordDrawCommands();
			//vkDestroyPipelineLayout(_logicalDevice, _graphicsPipeline._shaderResources._pipelineLayout, nullptr);
		}

		void DestroyRenderPass() {
			//_renderPass._depthImage.Destroy();
			/*for (auto image : _renderPass.colorImages) {
				image.Destroy();
			}*/

			vkDestroyRenderPass(_logicalDevice, _renderPass._handle, nullptr);
		}

		void DestroySwapchain() {
			for (size_t i = 0; i < _swapchain._frameBuffers.size(); i++) {
				vkDestroyFramebuffer(_logicalDevice, _swapchain._frameBuffers[i], nullptr);
			}
		}

		void Cleanup(bool fullClean) {
			//vkDeviceWaitIdle(_logicalDevice);
			//vkFreeCommandBuffers(_logicalDevice, _commandPool, (uint32_t)_drawCommandBuffers.size(), _drawCommandBuffers.data());
			//vkDestroyPipeline(_logicalDevice, _graphicsPipeline._handle, nullptr);
			//DestroyRenderPass();
			//DestroySwapchain();

			//vkDestroyDescriptorSetLayout(_logicalDevice, _graphicsPipeline._shaderResources._cameraDataSet._layout, nullptr);
			////vkDestroyDescriptorSetLayout(_logicalDevice, _graphicsPipeline._shaderResources._samplerSet._layout, nullptr);

			//if (fullClean) {
			//	vkDestroySemaphore(_logicalDevice, _imageAvailableSemaphore, nullptr);
			//	vkDestroySemaphore(_logicalDevice, _renderingFinishedSemaphore, nullptr);
			//	DestroyRenderPass();
			//	vkDestroyCommandPool(_logicalDevice, _commandPool, nullptr);

			//	// Clean up uniform buffer related objects.
			//	vkDestroyDescriptorPool(_logicalDevice, _graphicsPipeline._shaderResources._cameraDataPool._handle, nullptr);
			//	_graphicsPipeline._shaderResources._cameraDataBuffer.Destroy();

			//	// Buffers must be destroyed after no command buffers are referring to them anymore.
			//	/*_graphicsPipeline._vertexBuffer.Destroy();
			//	_graphicsPipeline._indexBuffer.Destroy();*/

			//	// Note: implicitly destroys images (in fact, we're not allowed to do that explicitly).
			//	DestroySwapchain();

			//	vkDestroyDevice(_logicalDevice, nullptr);

			//	vkDestroySurfaceKHR(_instance, _windowSurface, nullptr);

			//	if (_settings._enableValidationLayers) {
			//		PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallback = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(_instance, "vkDestroyDebugReportCallbackEXT");
			//		DestroyDebugReportCallback(_instance, _callback, nullptr);
			//	}

			//	vkDestroyInstance(_instance, nullptr);
			//}
		}

		bool ValidationLayersSupported(const std::vector<const char*>& validationLayers) {
			uint32_t layerCount;
			vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

			std::vector<VkLayerProperties> availableLayers(layerCount);
			vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

			for (const char* layerName : validationLayers) {
				bool layerFound = false;

				for (const auto& layerProperties : availableLayers) {
					if (strcmp(layerName, layerProperties.layerName) == 0) {
						layerFound = true;
						break;
					}
				}

				if (!layerFound) {
					return false;
				}
			}
			return true;
		}

		VkInstance CreateInstance() {
			VkApplicationInfo appInfo = {};
			appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			appInfo.pApplicationName = "Hold The Line!";
			appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.pEngineName = "Celeritas Engine";
			appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.apiVersion = VK_API_VERSION_1_0;

			// Get instance extensions required by GLFW to draw to the window.
			unsigned int glfwExtensionCount;
			const char** glfwExtensions;
			glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

			std::vector<const char*> extensions;
			for (size_t i = 0; i < glfwExtensionCount; i++) {
				extensions.push_back(glfwExtensions[i]);
			}

			if (_settings._enableValidationLayers) {
				extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
			}

			uint32_t extensionCount = 0;
			CheckResult(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr));

			if (extensionCount == 0) {
				Exit(1, "no extensions supported");
			}

			std::vector<VkExtensionProperties> availableExtensions(extensionCount);
			CheckResult(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data()));

			VkInstanceCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			createInfo.pApplicationInfo = &appInfo;
			createInfo.enabledExtensionCount = (uint32_t)extensions.size();
			createInfo.ppEnabledExtensionNames = extensions.data();

			if (_settings._enableValidationLayers) {
				if (ValidationLayersSupported(_settings._pValidationLayers)) {
					createInfo.enabledLayerCount = 1;
					createInfo.ppEnabledLayerNames = _settings._pValidationLayers.data();
				}
			}

			VkInstance outInstance;
			CheckResult(vkCreateInstance(&createInfo, nullptr, &outInstance));
			return outInstance;
		}

		VkSurfaceKHR CreateWindowSurface(VkInstance& instance) {
			VkSurfaceKHR outSurface;
			CheckResult(glfwCreateWindowSurface(instance, _pWindow, NULL, &outSurface));
			return outSurface;
		}

		VkPhysicalDevice CreatePhysicalDevice(VkInstance instance) {
			// Try to find 1 Vulkan supported device.
			// Note: perhaps refactor to loop through devices and find first one that supports all required features and extensions.
			uint32_t deviceCount = 0;
			CheckResult(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));

			if (deviceCount <= 0) {
				Exit(1, "device count was zero");
			}

			deviceCount = 1;
			std::vector<VkPhysicalDevice> outDevice(deviceCount);
			vkEnumeratePhysicalDevices(instance, &deviceCount, outDevice.data());

			if (deviceCount <= 0) {
				Exit(1, "device count was zero");
			}

			// Check device features
			// Note: will apiVersion >= appInfo.apiVersion? Probably yes, but spec is unclear.
			/*VkPhysicalDeviceProperties deviceProperties;
			VkPhysicalDeviceFeatures deviceFeatures;
			vkGetPhysicalDeviceProperties(_handle, &deviceProperties);
			vkGetPhysicalDeviceFeatures(_handle, &deviceFeatures);

			uint32_t supportedVersion[] = {
				VK_VERSION_MAJOR(deviceProperties.apiVersion),
				VK_VERSION_MINOR(deviceProperties.apiVersion),
				VK_VERSION_PATCH(deviceProperties.apiVersion)
			};

			std::cout << "physical device supports version " << supportedVersion[0] << "." << supportedVersion[1] << "." << supportedVersion[2] << std::endl;*/

			return outDevice[0];
		}

		VkDevice CreateLogicalDevice(VkPhysicalDevice& physicalDevice, const std::vector<VkDeviceQueueCreateInfo>& queueCreateInfos) {
			VkDeviceCreateInfo deviceCreateInfo = {};
			deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
			deviceCreateInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();

			// Devices features to enable.
			VkPhysicalDeviceFeatures enabledFeatures = {};
			enabledFeatures.samplerAnisotropy = VK_TRUE;
			enabledFeatures.shaderClipDistance = VK_TRUE;
			enabledFeatures.shaderCullDistance = VK_TRUE;

			const char* deviceExtensions = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
			deviceCreateInfo.enabledExtensionCount = 1;
			deviceCreateInfo.ppEnabledExtensionNames = &deviceExtensions;
			deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

			if (_settings._enableValidationLayers) {
				deviceCreateInfo.enabledLayerCount = 1;
				deviceCreateInfo.ppEnabledLayerNames = _settings._pValidationLayers.data();
			}

			VkDevice outDevice;
			vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &outDevice);
			return outDevice;
		}

		void CreateDebugCallback(VkInstance& instance) {
			if (_settings._enableValidationLayers) {
				VkDebugReportCallbackCreateInfoEXT createInfo = {};
				createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
				createInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)DebugCallback;
				createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;

				PFN_vkCreateDebugReportCallbackEXT createDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");

				CheckResult(createDebugReportCallback(instance, &createInfo, nullptr, &_callback));
			}
		}

		void CreateSemaphores() {
			VkSemaphoreCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			CheckResult(vkCreateSemaphore(_logicalDevice, &createInfo, nullptr, &_imageAvailableSemaphore));
			CheckResult(vkCreateSemaphore(_logicalDevice, &createInfo, nullptr, &_renderingFinishedSemaphore));
		}

		void LoadScene() {
			//auto scenePath = Paths::ModelsPath() /= "MaterialSphere.glb";
			//auto scenePath = Paths::ModelsPath() /= "cubes.glb";
			//auto scenePath = Paths::ModelsPath() /= "directions.glb";
			//auto scenePath = Paths::ModelsPath() /= "f.glb";
			//auto scenePath = Paths::ModelsPath() /= "fr.glb";
			auto scenePath = Paths::ModelsPath() /= "mp5k.glb";
			//auto scenePath = Paths::ModelsPath() /= "collision.glb";
			//auto scenePath = Paths::ModelsPath() /= "forces.glb";
			//auto scenePath = Paths::ModelsPath() /= "hierarchy.glb";
			//auto scenePath = Paths::ModelsPath() /= "primitives.glb";
			//auto scenePath = Paths::ModelsPath() /= "translation.glb";
			//auto scenePath = Paths::ModelsPath() /= "mp5ktest.glb";
			//auto scenePath = Paths::ModelsPath() /= "rotation.glb";
			//auto scenePath = Paths::ModelsPath() /= "clipping.glb";
			//auto scenePath = Paths::ModelsPath() /= "Cube.glb";
			//auto scenePath = Paths::ModelsPath() /= "stanford_dragon_pbr.glb";
			//auto scenePath = Paths::ModelsPath() /= "SampleMap.glb";
			//auto scenePath = Paths::ModelsPath() /= "monster.glb";
			//auto scenePath = Paths::ModelsPath() /= "free_1972_datsun_4k_textures.glb";
			_scene = SceneLoader::LoadFile(scenePath, _logicalDevice, _physicalDevice, _commandPool, _queue);
		}

		void LoadEnvironmentMap() {
			_scene._environmentMap = CubicalEnvironmentMap(_physicalDevice, _logicalDevice);
			_scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "Waterfall.hdr");
			//_scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "Debug.png");
			//_scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "ModernBuilding.hdr");
			//_scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "Workshop.png");
			//_scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "Workshop.hdr");
			//_scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "garden.hdr");
			//_scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "ItalianFlag.png");
			//_scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "TestPng.png");
			//_scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "EnvMap.png");
			//_scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "texture.jpg");
			//_scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "Test1.png");

			_scene._environmentMap.CreateImage(_logicalDevice, _physicalDevice, _commandPool, _queue);
		}

		VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR> presentModes) {
			for (const auto& presentMode : presentModes) {
				if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
					return presentMode;
				}
			}

			// If mailbox is unavailable, fall back to FIFO (guaranteed to be available)
			return VK_PRESENT_MODE_FIFO_KHR;
		}

		VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
			for (const auto& availableSurfaceFormat : availableFormats) {
				if (availableSurfaceFormat.format == VK_FORMAT_R8G8B8A8_SRGB) return availableSurfaceFormat;
			}

			return availableFormats[0];
		}

		VkExtent2D ChooseFramebufferSize(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) {
			auto& settings = GlobalSettings::Instance();

			if (surfaceCapabilities.currentExtent.width == -1) {
				VkExtent2D swapChainExtent = {};

				swapChainExtent.width = std::min(std::max(settings._windowWidth, surfaceCapabilities.minImageExtent.width), surfaceCapabilities.maxImageExtent.width);
				swapChainExtent.height = std::min(std::max(settings._windowHeight, surfaceCapabilities.minImageExtent.height), surfaceCapabilities.maxImageExtent.height);

				return swapChainExtent;
			}
			else {
				return surfaceCapabilities.currentExtent;
			}
		}

		void CreateSwapchain() {
			// Get physical device capabilities for the window surface.
			VkSurfaceCapabilitiesKHR surfaceCapabilities = PhysicalDevice::GetSurfaceCapabilities(_physicalDevice, _windowSurface);
			std::vector<VkSurfaceFormatKHR> surfaceFormats = PhysicalDevice::GetSupportedFormatsForSurface(_physicalDevice, _windowSurface);
			std::vector<VkPresentModeKHR> presentModes = PhysicalDevice::GetSupportedPresentModesForSurface(_physicalDevice, _windowSurface);

			// Determine number of images for swapchain.
			uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
			if (surfaceCapabilities.maxImageCount != 0 && imageCount > surfaceCapabilities.maxImageCount) {
				imageCount = surfaceCapabilities.maxImageCount;
			}

			VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(surfaceFormats);
			_swapchain._framebufferSize = ChooseFramebufferSize(surfaceCapabilities);

			// Determine transformation to use (preferring no transform).
			VkSurfaceTransformFlagBitsKHR surfaceTransform;
			if (surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
				surfaceTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
			}
			else {
				surfaceTransform = surfaceCapabilities.currentTransform;
			}

			// Choose presentation mode (preferring MAILBOX ~= triple buffering).
			VkPresentModeKHR presentMode = ChoosePresentMode(presentModes);

			// Finally, create the swap chain.
			VkSwapchainCreateInfoKHR createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
			createInfo.surface = _windowSurface;
			createInfo.minImageCount = imageCount;
			createInfo.imageFormat = surfaceFormat.format;
			createInfo.imageColorSpace = surfaceFormat.colorSpace;
			createInfo.imageExtent = _swapchain._framebufferSize;
			createInfo.imageArrayLayers = 1;
			createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
			createInfo.preTransform = surfaceTransform;
			createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
			createInfo.presentMode = presentMode;
			createInfo.clipped = VK_TRUE;
			createInfo.oldSwapchain = _swapchain._oldSwapchainHandle;

			CheckResult(vkCreateSwapchainKHR(_logicalDevice, &createInfo, nullptr, &_swapchain._handle));

			if (_swapchain._oldSwapchainHandle != VK_NULL_HANDLE) {
				vkDestroySwapchainKHR(_logicalDevice, _swapchain._oldSwapchainHandle, nullptr);
			}

			_swapchain._oldSwapchainHandle = _swapchain._handle;
			_swapchain._surfaceFormat = surfaceFormat;
		}

		void CreateGraphicsPipelines(VkRenderPass renderPass, VkPipelineLayout scenePipelineLayout, VkPipelineLayout uiPipelineLayout) {
			// 3D scene pipeline
			{
				VkShaderModule vertexShaderModule = CreateShaderModule(_logicalDevice, Paths::VertexShaderPath());
				VkShaderModule fragmentShaderModule = CreateShaderModule(_logicalDevice, Paths::FragmentShaderPath());

				// Set up shader stage info.
				VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
				vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
				vertexShaderCreateInfo.module = vertexShaderModule;
				vertexShaderCreateInfo.pName = "main";

				VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = {};
				fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
				fragmentShaderCreateInfo.module = fragmentShaderModule;
				fragmentShaderCreateInfo.pName = "main";
				VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };

				// Vertex attribute binding - gives the vertex shader more info about a particular vertex buffer, denoted by the binding number. See binding for more info.
				_graphicsPipeline._vertexBindingDescription.binding = 0;
				_graphicsPipeline._vertexBindingDescription.stride = sizeof(Vertex);
				_graphicsPipeline._vertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

				// Describe how the shader should read vertex attributes when getting a vertex from the vertex buffer.
				// Object-space positions.
				_graphicsPipeline._vertexAttributeDescriptions.resize(3);
				_graphicsPipeline._vertexAttributeDescriptions[0].location = 0;
				_graphicsPipeline._vertexAttributeDescriptions[0].binding = 0;
				_graphicsPipeline._vertexAttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
				_graphicsPipeline._vertexAttributeDescriptions[0].offset = (uint32_t)Vertex::OffsetOf(Vertex::AttributeType::Position);

				// Normals.
				_graphicsPipeline._vertexAttributeDescriptions[1].location = 1;
				_graphicsPipeline._vertexAttributeDescriptions[1].binding = 0;
				_graphicsPipeline._vertexAttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
				_graphicsPipeline._vertexAttributeDescriptions[1].offset = (uint32_t)Vertex::OffsetOf(Vertex::AttributeType::Normal);

				// UV coordinates.
				_graphicsPipeline._vertexAttributeDescriptions[2].location = 2;
				_graphicsPipeline._vertexAttributeDescriptions[2].binding = 0;
				_graphicsPipeline._vertexAttributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
				_graphicsPipeline._vertexAttributeDescriptions[2].offset = (uint32_t)Vertex::OffsetOf(Vertex::AttributeType::UV);

				// Describe vertex input.
				VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
				vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
				vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
				vertexInputCreateInfo.pVertexBindingDescriptions = &_graphicsPipeline._vertexBindingDescription;
				vertexInputCreateInfo.vertexAttributeDescriptionCount = (uint32_t)_graphicsPipeline._vertexAttributeDescriptions.size();
				vertexInputCreateInfo.pVertexAttributeDescriptions = _graphicsPipeline._vertexAttributeDescriptions.data();

				// Describe input assembly - this allows Vulkan to know how many indices make up a face for the vkCmdDrawIndexed function.
				// The input assembly is the very first stage of the graphics pipeline, where vertices and indices are loaded from VRAM and assembled,
				// to then be passed to the shaders.
				VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
				inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
				inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
				inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

				// Describe viewport and scissor
				VkViewport viewport = {};
				viewport.x = 0.0f;
				viewport.y = 0.0f;
				viewport.width = Helper::Convert<uint32_t, float>(_swapchain._framebufferSize.width);
				viewport.height = Helper::Convert<uint32_t, float>(_swapchain._framebufferSize.height);
				viewport.minDepth = 0.0f;
				viewport.maxDepth = 1.0f;

				VkRect2D scissor = {};
				scissor.offset.x = 0;
				scissor.offset.y = 0;
				scissor.extent.width = _swapchain._framebufferSize.width;
				scissor.extent.height = _swapchain._framebufferSize.height;

				// Note: scissor test is always enabled (although dynamic scissor is possible).
				// Number of viewports must match number of scissors.
				VkPipelineViewportStateCreateInfo viewportCreateInfo = {};
				viewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
				viewportCreateInfo.viewportCount = 1;
				viewportCreateInfo.pViewports = &viewport;
				viewportCreateInfo.scissorCount = 1;
				viewportCreateInfo.pScissors = &scissor;

				// Describe rasterization - this tells Vulkan what settings to use when at the fragment shader stage of the pipeline, a.k.a. when
				// rendering pixels.
				// Note: depth bias and using polygon modes other than fill require changes to logical device creation (device features).
				VkPipelineRasterizationStateCreateInfo rasterizationCreateInfo = {};
				rasterizationCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
				rasterizationCreateInfo.depthClampEnable = VK_FALSE;
				rasterizationCreateInfo.rasterizerDiscardEnable = VK_FALSE;
				rasterizationCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
				rasterizationCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
				rasterizationCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
				rasterizationCreateInfo.depthBiasEnable = VK_FALSE;
				rasterizationCreateInfo.depthBiasConstantFactor = 0.0f;
				rasterizationCreateInfo.depthBiasClamp = 0.0f;
				rasterizationCreateInfo.depthBiasSlopeFactor = 0.0f;
				rasterizationCreateInfo.lineWidth = 1.0f;

				// Configure depth testing.
				VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo{};
				depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
				depthStencilCreateInfo.pNext = nullptr;
				depthStencilCreateInfo.depthTestEnable = VK_TRUE;
				depthStencilCreateInfo.depthWriteEnable = VK_TRUE;
				depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
				depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
				depthStencilCreateInfo.stencilTestEnable = VK_FALSE;
				depthStencilCreateInfo.minDepthBounds = 0.0f;
				depthStencilCreateInfo.maxDepthBounds = 1.0f;

				// Describe multisampling
				// Note: using multisampling also requires turning on device features.
				VkPipelineMultisampleStateCreateInfo multisampleCreateInfo = {};
				multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
				multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
				multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
				multisampleCreateInfo.minSampleShading = 1.0f;
				multisampleCreateInfo.alphaToCoverageEnable = VK_FALSE;
				multisampleCreateInfo.alphaToOneEnable = VK_FALSE;

				// Describing color blending.
				// Note: all paramaters except blendEnable and colorWriteMask are irrelevant here.
				VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
				colorBlendAttachmentState.blendEnable = VK_FALSE;
				colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
				colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
				colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
				colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
				colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
				colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
				colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

				// Note: all attachments must have the same values unless a device feature is enabled.
				VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo = {};
				colorBlendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
				colorBlendCreateInfo.logicOpEnable = VK_FALSE;
				colorBlendCreateInfo.logicOp = VK_LOGIC_OP_COPY;
				colorBlendCreateInfo.attachmentCount = 1;
				colorBlendCreateInfo.pAttachments = &colorBlendAttachmentState;
				colorBlendCreateInfo.blendConstants[0] = 0.0f;
				colorBlendCreateInfo.blendConstants[1] = 0.0f;
				colorBlendCreateInfo.blendConstants[2] = 0.0f;
				colorBlendCreateInfo.blendConstants[3] = 0.0f;

				// Create the graphics pipeline.
				VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
				pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
				pipelineCreateInfo.stageCount = 2;
				pipelineCreateInfo.pStages = shaderStages;
				pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
				pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
				pipelineCreateInfo.pViewportState = &viewportCreateInfo;
				pipelineCreateInfo.pRasterizationState = &rasterizationCreateInfo;
				pipelineCreateInfo.pDepthStencilState = &depthStencilCreateInfo;
				pipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
				pipelineCreateInfo.pColorBlendState = &colorBlendCreateInfo;
				pipelineCreateInfo.layout = scenePipelineLayout;
				pipelineCreateInfo.renderPass = renderPass;
				pipelineCreateInfo.subpass = 0;
				pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
				pipelineCreateInfo.basePipelineIndex = -1;

				CheckResult(vkCreateGraphicsPipelines(_logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &_graphicsPipeline._handle));
				vkDestroyShaderModule(_logicalDevice, vertexShaderModule, nullptr);
				vkDestroyShaderModule(_logicalDevice, fragmentShaderModule, nullptr);
			}

			// UI pipeline.
			{
				auto vertPath = Paths::ShadersPath() / std::filesystem::path("graphics\\NuklearUIVertexShader.spv");
				auto fragPath = Paths::ShadersPath() / std::filesystem::path("graphics\\NuklearUIFragmentShader.spv");
				VkShaderModule vertShaderModule = CreateShaderModule(_logicalDevice, vertPath);
				VkShaderModule fragShaderModule = CreateShaderModule(_logicalDevice, fragPath);

				VkPipelineShaderStageCreateInfo vert_shader_stage_info{};
				vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
				vert_shader_stage_info.module = vertShaderModule;
				vert_shader_stage_info.pName = "main";

				VkPipelineShaderStageCreateInfo frag_shader_stage_info{};
				frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
				frag_shader_stage_info.module = fragShaderModule;
				frag_shader_stage_info.pName = "main";

				VkPipelineShaderStageCreateInfo shader_stages[2]{};
				shader_stages[0] = vert_shader_stage_info;
				shader_stages[1] = frag_shader_stage_info;

				FILE* fp{};
				size_t file_len{};
				VkViewport viewport{};
				VkRect2D scissor{};
				VkResult result{};

				VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
				vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

				VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
				inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
				inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
				inputAssembly.primitiveRestartEnable = VK_FALSE;

				viewport.x = 0.0f;
				viewport.y = 0.0f;
				viewport.width = (float)_swapchain._framebufferSize.width;
				viewport.height = (float)_swapchain._framebufferSize.height;
				viewport.minDepth = 0.0f;
				viewport.maxDepth = 1.0f;

				scissor.extent.width = _swapchain._framebufferSize.width;
				scissor.extent.height = _swapchain._framebufferSize.height;

				VkPipelineViewportStateCreateInfo viewportState{};
				viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
				viewportState.viewportCount = 1;
				viewportState.pViewports = &viewport;
				viewportState.scissorCount = 1;
				viewportState.pScissors = &scissor;

				VkPipelineRasterizationStateCreateInfo rasterizer{};
				rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
				rasterizer.depthClampEnable = VK_FALSE;
				rasterizer.rasterizerDiscardEnable = VK_FALSE;
				rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
				rasterizer.lineWidth = 1.0f;
				rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
				rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
				rasterizer.depthBiasEnable = VK_FALSE;

				VkPipelineMultisampleStateCreateInfo multisampling{};
				multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
				multisampling.sampleShadingEnable = VK_FALSE;
				multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

				VkPipelineColorBlendAttachmentState colorBlendAttachment{};
				colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
				colorBlendAttachment.blendEnable = VK_TRUE;
				colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
				colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
				colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
				colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
				colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

				VkPipelineColorBlendStateCreateInfo colorBlending{};
				colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
				colorBlending.logicOpEnable = VK_FALSE;
				colorBlending.logicOp = VK_LOGIC_OP_COPY;
				colorBlending.attachmentCount = 1;
				colorBlending.pAttachments = &colorBlendAttachment;
				colorBlending.blendConstants[0] = 1.0f;
				colorBlending.blendConstants[1] = 1.0f;
				colorBlending.blendConstants[2] = 1.0f;
				colorBlending.blendConstants[3] = 1.0f;

				VkGraphicsPipelineCreateInfo pipelineInfo{};
				pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
				pipelineInfo.stageCount = 2;
				pipelineInfo.pStages = shader_stages;
				pipelineInfo.pVertexInputState = &vertexInputInfo;
				pipelineInfo.pInputAssemblyState = &inputAssembly;
				pipelineInfo.pViewportState = &viewportState;
				pipelineInfo.pRasterizationState = &rasterizer;
				pipelineInfo.pMultisampleState = &multisampling;
				pipelineInfo.pColorBlendState = &colorBlending;
				pipelineInfo.layout = uiPipelineLayout;
				pipelineInfo.renderPass = renderPass;
				pipelineInfo.subpass = 1;
				pipelineInfo.basePipelineHandle = NULL;

				CheckResult(vkCreateGraphicsPipelines(_logicalDevice, NULL, 1, &pipelineInfo, NULL, &_uiCtx._pipeline));

				if (fragShaderModule) vkDestroyShaderModule(_logicalDevice, fragShaderModule, NULL);
				if (vertShaderModule) vkDestroyShaderModule(_logicalDevice, vertShaderModule, NULL);
			}
		}

		std::vector<DescriptorSetLayout> CreateDescriptorSetLayouts() {
			VkDescriptorSetLayout cameraLayout;
			VkDescriptorSetLayout gameObjectLayout;
			VkDescriptorSetLayout meshLayout;
			VkDescriptorSetLayout lightLayout;
			VkDescriptorSetLayout miscLayout;

			{
				VkDescriptorSetLayoutBinding bindings[1] = { VkDescriptorSetLayoutBinding { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr } };
				VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
				layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				layoutCreateInfo.bindingCount = 1;
				layoutCreateInfo.pBindings = bindings;
				vkCreateDescriptorSetLayout(_logicalDevice, &layoutCreateInfo, nullptr, &cameraLayout);
			}

			{
				VkDescriptorSetLayoutBinding bindings[1] = { VkDescriptorSetLayoutBinding { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr } };
				VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
				layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				layoutCreateInfo.bindingCount = 1;
				layoutCreateInfo.pBindings = bindings;
				vkCreateDescriptorSetLayout(_logicalDevice, &layoutCreateInfo, nullptr, &gameObjectLayout);
			}

			{
				VkDescriptorSetLayoutBinding bindings[1] = { VkDescriptorSetLayoutBinding { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr } };
				VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
				layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				layoutCreateInfo.bindingCount = 1;
				layoutCreateInfo.pBindings = bindings;
				vkCreateDescriptorSetLayout(_logicalDevice, &layoutCreateInfo, nullptr, &lightLayout);
			}

			{
				VkDescriptorSetLayoutBinding bindings[3];
				bindings[0] = { VkDescriptorSetLayoutBinding {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &_scene._materials[0]._albedo._sampler} };
				bindings[1] = { VkDescriptorSetLayoutBinding {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &_scene._materials[0]._roughness._sampler} };
				bindings[2] = { VkDescriptorSetLayoutBinding {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &_scene._materials[0]._metalness._sampler} };
				VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
				layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				layoutCreateInfo.bindingCount = 3;
				layoutCreateInfo.pBindings = bindings;
				vkCreateDescriptorSetLayout(_logicalDevice, &layoutCreateInfo, nullptr, &meshLayout);
			}

			{
				VkDescriptorSetLayoutBinding bindings[1];
				bindings[0] = { VkDescriptorSetLayoutBinding { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &_scene._environmentMap._cubeMapImage._sampler } };
				VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
				layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				layoutCreateInfo.bindingCount = 1;
				layoutCreateInfo.pBindings = bindings;
				vkCreateDescriptorSetLayout(_logicalDevice, &layoutCreateInfo, nullptr, &miscLayout);
			}

			return {
				DescriptorSetLayout{ "cameraLayout", 0, cameraLayout },
				DescriptorSetLayout{ "gameObjectLayout", 1, gameObjectLayout },
				DescriptorSetLayout{ "lightLayout", 2, lightLayout },
				DescriptorSetLayout{ "meshLayout", 3, meshLayout },
				DescriptorSetLayout{ "miscLayout", 4, miscLayout }
			};
		}

		VkPipelineLayout CreatePipelineLayout(std::vector<DescriptorSetLayout>& descriptorSetLayouts) {
			// Select the layout from each descriptor set to create a layout-only vector.
			std::vector<VkDescriptorSetLayout> layouts;
			std::transform(descriptorSetLayouts.begin(), descriptorSetLayouts.end(), std::back_inserter(layouts),
				[](const DescriptorSetLayout& l) {
					return l._layout;
				});

			VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
			pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)layouts.size();
			pipelineLayoutCreateInfo.pSetLayouts = layouts.data();
			VkPipelineLayout outLayout;
			CheckResult(vkCreatePipelineLayout(_logicalDevice, &pipelineLayoutCreateInfo, nullptr, &outLayout));
			return outLayout;
		}

		void CreateShaderResources(std::vector<DescriptorSetLayout>& descriptorSetLayouts) {
			auto& shaderResources = _graphicsPipeline._shaderResources;
			auto cameraResources = _mainCamera.CreateDescriptorSets(_physicalDevice, _logicalDevice, _commandPool, _queue, descriptorSetLayouts);
			shaderResources.MergeResources(cameraResources);
			_mainCamera.UpdateShaderResources();

			auto sceneResources = _scene.CreateDescriptorSets(_physicalDevice, _logicalDevice, _commandPool, _queue, descriptorSetLayouts);
			shaderResources.MergeResources(sceneResources);
			_scene.UpdateShaderResources();
		}

		//void CreateUIDescriptorSet() {
		//	// Step 1: Create a Descriptor Set Layout
		//	VkDescriptorSetLayout descriptorSetLayout;
		//	VkDescriptorSetLayoutBinding layoutBinding = {};
		//	layoutBinding.binding = 0; // Binding 0 in the shader
		//	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; // For sampled images
		//	layoutBinding.descriptorCount = 1; // One image
		//	layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // Accessible in fragment shader
		//	layoutBinding.pImmutableSamplers = nullptr; // No immutable samplers

		//	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		//	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		//	layoutInfo.bindingCount = 1;
		//	layoutInfo.pBindings = &layoutBinding;

		//	CheckResult(vkCreateDescriptorSetLayout(_logicalDevice, &layoutInfo, nullptr, &_uiCtx._descriptorSetLayout));

		//	// Step 2: Create a Descriptor Pool
		//	VkDescriptorPool descriptorPool;
		//	VkDescriptorPoolSize poolSize = {};
		//	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		//	poolSize.descriptorCount = 1; // Only one descriptor

		//	VkDescriptorPoolCreateInfo poolInfo = {};
		//	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		//	poolInfo.poolSizeCount = 1;
		//	poolInfo.pPoolSizes = &poolSize;
		//	poolInfo.maxSets = 1; // Only one descriptor set

		//	CheckResult(vkCreateDescriptorPool(_logicalDevice, &poolInfo, nullptr, &descriptorPool));

		//	// Step 3: Allocate the Descriptor Set
		//	VkDescriptorSet descriptorSet;
		//	VkDescriptorSetAllocateInfo allocInfo = {};
		//	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		//	allocInfo.descriptorPool = descriptorPool;
		//	allocInfo.descriptorSetCount = 1;
		//	allocInfo.pSetLayouts = &descriptorSetLayout;

		//	CheckResult(vkAllocateDescriptorSets(_logicalDevice, &allocInfo, &_uiCtx._descriptorSet));
		//}

		//void RecordDrawCommands() {
		//	// Prepare data for recording command buffers.
		//	VkCommandBufferBeginInfo beginInfo = {};
		//	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		//	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		//	VkImageSubresourceRange subResourceRange = {};
		//	subResourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		//	subResourceRange.baseMipLevel = 0;
		//	subResourceRange.levelCount = 1;
		//	subResourceRange.baseArrayLayer = 0;
		//	subResourceRange.layerCount = 1;

		//	// Record command buffer for each swapchain image.
		//	for (size_t i = 0; i < _renderPass._colorImages.size(); i++) {
		//		vkBeginCommandBuffer(_drawCommandBuffers[i], &beginInfo);

		//		VkHelper::TransitionImageLayout(_logicalDevice, _commandPool, _queue, _renderPass._colorImages[i]._image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

		//		VkClearValue clearColor = {
		//		{ 0.1f, 0.1f, 0.1f, 1.0f } // R, G, B, A.
		//		};

		//		VkClearValue depthClear;
		//		depthClear.depthStencil.depth = 1.0f;
		//		VkClearValue clearValues[] = { clearColor, depthClear };

		//		VkRenderPassBeginInfo renderPassBeginInfo = {};
		//		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		//		renderPassBeginInfo.renderPass = _renderPass._handle;
		//		renderPassBeginInfo.framebuffer = _swapchain._frameBuffers[i];
		//		renderPassBeginInfo.renderArea.offset.x = 0;
		//		renderPassBeginInfo.renderArea.offset.y = 0;
		//		renderPassBeginInfo.renderArea.extent = _swapchain._framebufferSize;
		//		renderPassBeginInfo.clearValueCount = 2;
		//		renderPassBeginInfo.pClearValues = &clearValues[0];

		//		vkCmdBeginRenderPass(_drawCommandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		//		vkCmdBindPipeline(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline._handle);

		//		auto& shaderResources = _graphicsPipeline._shaderResources;
		//		VkDescriptorSet sets[4] = { shaderResources[0][0], shaderResources[1][0], shaderResources[2][0], shaderResources[4][0] };
		//		vkCmdBindDescriptorSets(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline._layout, 0, 1, &shaderResources[0][0], 0, nullptr);
		//		vkCmdBindDescriptorSets(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline._layout, 2, 1, &shaderResources[2][0], 0, nullptr);
		//		vkCmdBindDescriptorSets(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline._layout, 4, 1, &shaderResources[4][0], 0, nullptr);

		//		for (auto& gameObject : _scene._pRootGameObject->_children) {
		//			gameObject->Draw(_graphicsPipeline._layout, _drawCommandBuffers[i]);
		//		}

		//		vkCmdEndRenderPass(_drawCommandBuffers[i]);

		//		CheckResult(vkEndCommandBuffer(_drawCommandBuffers[i]));
		//	}
		//}

		void Draw() {
			if (windowMinimized) return;
			vkResetFences(_logicalDevice, 1, &_queueFence);

			// Acquire image.
			uint32_t imageIndex;
			VkResult res = vkAcquireNextImageKHR(_logicalDevice, _swapchain._handle, UINT64_MAX, _imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

			// Unless surface is out of date right now, defer swap chain recreation until end of this frame.
			if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || windowResized) { WindowSizeChanged(); return; }
			else if (res != VK_SUCCESS) { std::cerr << "failed to acquire image\n"; exit(1); }

			auto nk_semaphore = nk_glfw3_render(_queue, imageIndex, _imageAvailableSemaphore, NK_ANTI_ALIASING_ON);

			//VkHelper::TransitionImageLayout(_logicalDevice, _commandPool, _queue, _uiCtx._overlayImages[imageIndex]._image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			// Wait for image to be available and draw.
			// This is the stage where the queue should wait on the semaphore.
			VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = &nk_semaphore;
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = &_renderingFinishedSemaphore;
			submitInfo.pWaitDstStageMask = &waitDstStageMask;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &_drawCommandBuffers[imageIndex];

			vkQueueSubmit(_queue, 1, &submitInfo, _queueFence);
			vkWaitForFences(_logicalDevice, 1, &_queueFence, VK_TRUE, UINT64_MAX);

			// Present drawn image.
			// Note: semaphore here is not strictly necessary, because commands are processed in submission order within a single queue.
			VkPresentInfoKHR presentInfo = {};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = &_renderingFinishedSemaphore;
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = &_swapchain._handle;
			presentInfo.pImageIndices = &imageIndex;

			res = vkQueuePresentKHR(_queue, &presentInfo);

			if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR || windowResized) {
				WindowSizeChanged();
			}
			else if (res != VK_SUCCESS) {
				std::cerr << "failed to submit present command buffer" << std::endl;
				exit(1);
			}
		}
	};
}

int main() {
	Engine::GlobalSettings::Instance().Load(Engine::Paths::Settings());
	Engine::VulkanApplication app;
	app.Run();

	return 0;
}
