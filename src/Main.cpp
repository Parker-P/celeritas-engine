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

using namespace std::chrono_literals;

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
	 * @param rayOrigin The origin point of the ray, in world space.
	 * @param rayVector The ray vector. Can be normalized or not.
	 * @param v1 Coordinates of vertex 1 in world space.
	 * @param v2 Coordinates of vertex 2 in world space.
	 * @param v3 Coordinates of vertex 3 in world space.
	 * @param outIntersectionPoint Intersection point in world space. This will remain unchanged if the function returns false.
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

		if (determinant > -epsilon && determinant < epsilon) return false;    // This ray is parallel to this triangle.

		float inverseDeterminant = 1.0f / determinant;
		glm::vec3 s = rayOrigin - v1;
		float u = inverseDeterminant * dot(s, rayCrossE2);

		if (u < 0 || u > 1) return false;

		glm::vec3 sCrossE1 = cross(s, edge1);
		float v = inverseDeterminant * glm::dot(rayVector, sCrossE1);

		if (v < 0 || u + v > 1) return false;

		// At this stage we can compute t to find out where the intersection point is along the ray.
		float t = inverseDeterminant * dot(edge2, sCrossE1);

		if (t > epsilon) {
			outIntersectionPoint = rayOrigin + rayVector * t;
			return true;
		}
		else return false; // This means that the origin of the ray is inside the triangle.
	}

	bool IsSegmentIntersectingTriangle(glm::vec3 rayOrigin, glm::vec3 rayVector, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, glm::vec3& outIntersectionPoint) {
		if (IsRayIntersectingTriangle(rayOrigin, rayVector, v1, v2, v3, outIntersectionPoint))
			return glm::length(outIntersectionPoint - rayOrigin) < glm::length(rayVector);
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
	 * @brief Used by implementing classes to mark themselves as a class that is meant to do work on each iteration of the main loop.
	 */
	class IUpdatable {
	public:

		/**
		 * @brief Function called on implementing classes in each iteration of the main loop.
		 */
		virtual void Update() = 0;
	};

	struct Helpers {

		/**
		 * @brief Convert values. Returns the converted value.
		 * @param value
		 * @return
		 */
		template <typename FromType, typename ToType>
		static ToType Convert(FromType value) { return Convert<ToType>(value); }

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
			if (value == "true" || value == "1") { return true; }
			return false;
		}

		/**
		 * @brief Converts string to int.
		 * @param value
		 * @return
		 */
		template<>
		static int Convert(std::string value) { return std::stoi(value); }

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

		/**
		 * @brief Mirrors a glm::vec3 vector across a given axis vector.
		 *
		 * @param vector The vector to mirror.
		 * @param axis The axis vector (not necessarily normalized).
		 * @return glm::vec3 The mirrored vector.
		 */
		static glm::vec3 MirrorVectorAcrossAxis(const glm::vec3& vector, const glm::vec3& axis) {
			glm::vec3 normalizedAxis = glm::normalize(axis); // Normalize the axis vector
			glm::vec3 projection = glm::dot(vector, normalizedAxis) * normalizedAxis; // Project the vector onto the axis
			glm::vec3 perpendicular = vector - projection; // Compute the perpendicular component
			glm::vec3 mirroredVector = projection - perpendicular; // Compute the mirrored vector
			return mirroredVector;
		}

		static bool IsVectorZero(const glm::vec3& vector, float tolerance = std::numeric_limits<float>::epsilon()) {
			return (vector.x >= -tolerance && vector.x <= tolerance &&
				vector.y >= -tolerance && vector.y <= tolerance &&
				vector.z >= -tolerance && vector.z <= tolerance);
		}

		static bool IsPositionValid(const glm::vec3& position) {
			return glm::isinf(position.x) && glm::isnan(position.x) &&
				glm::isinf(position.y) && glm::isnan(position.y) &&
				glm::isinf(position.z) && glm::isnan(position.z);
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
		 * @brief Gamma correction to make images look more or less bright.
		 */
		float _gammaCorrection;

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
			auto text = Helpers::GetFileText(absolutePathToJson);

			sjson::parsing::parse_results json = sjson::parsing::parse(text.data());
			sjson::jobject rootObj = sjson::jobject::parse(json.value);

			// Get the values from the parsed result.
			auto evlJson = rootObj.get("EnableValidationLayers");
			_enableValidationLayers = Helpers::Convert<std::string, bool>(TrimEnds(evlJson));

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
			_windowWidth = Helpers::Convert<std::string, int>(width);
			_windowHeight = Helpers::Convert<std::string, int>(height);

			auto input = sjson::jobject::parse(rootObj.get("Input"));
			auto sens = input.get("MouseSensitivity");
			_mouseSensitivity = Helpers::Convert<std::string, float>(sens);

			auto graphics = sjson::jobject::parse(rootObj.get("Graphics"));
			auto gc = graphics.get("GammaCorrection");
			_gammaCorrection = Helpers::Convert<std::string, float>(gc);
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
		GLFWwindow* _pWindow;

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
			if (Instance()._cursorEnabled) return;
			Instance()._mouseX = xPos;
			Instance()._mouseY = yPos;
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

		void Initialize(GLFWwindow* pWindow) {
			if (nullptr == pWindow) return;
			_pWindow = pWindow;

			// Keyboard init
			glfwSetKeyCallback(pWindow, KeyCallback);

			// Mouse init
			_cursorEnabled = true;
			glfwSetInputMode(_pWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			if (glfwRawMouseMotionSupported()) glfwSetInputMode(pWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
			glfwSetCursorPosCallback(pWindow, CursorPositionCallback);
			glfwSetScrollCallback(pWindow, ScrollWheelCallback);

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

		void ToggleCursor() {
			_cursorEnabled = !_cursorEnabled;
			glfwSetInputMode(_pWindow, GLFW_CURSOR, _cursorEnabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
		}

		void Update() {
			if (WasKeyPressed(GLFW_KEY_ESCAPE)) {
				ToggleCursor();
				if (!_cursorEnabled) glfwSetCursorPos(_pWindow, _lastMouseX, _lastMouseY);
			};

			_deltaMouseX = (_mouseX - _lastMouseX);
			_deltaMouseY = (_mouseY - _lastMouseY);

			_lastMouseX = _mouseX;
			_lastMouseY = _mouseY;
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

		static void* DownloadImage(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue, VkImage image, uint32_t width, uint32_t height, VkDeviceMemory& outStagingMemory, VkBuffer& outStagingBuffer) {
			CreateBuffer(logicalDevice, physicalDevice, 4 * width * height, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT /*| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT*/,
				&outStagingBuffer, &outStagingMemory);

			TransitionImageLayout(logicalDevice, commandPool, queue, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			CopyImageToBuffer(logicalDevice, commandPool, queue, image, outStagingBuffer, width, height);

			void* data;
			vkMapMemory(logicalDevice, outStagingMemory, 0, 4 * width * height, 0, &data);
			return data;
		}

		static void CreateBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* bufferMemory) {
			VkBufferCreateInfo bufferInfo{};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = size;
			bufferInfo.usage = usage;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
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
			if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) { barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; }
			if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) { barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT; sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; }

			if (newLayout == VK_IMAGE_LAYOUT_UNDEFINED) { barrier.dstAccessMask = 0; destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; }
			if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) { barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT; destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT; }
			if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) { barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT; }
			if (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) { barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT; destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; }
			if (newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) { barrier.dstAccessMask = 0; destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; }
			if (newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) { barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; }

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

		static void DestroyImage(VkDevice logicalDevice, VkImage image, VkImageView imageView = nullptr, VkSampler sampler = nullptr) {
			vkDestroyImage(logicalDevice, image, nullptr);
			if (imageView != nullptr) vkDestroyImageView(logicalDevice, imageView, nullptr);
			if (imageView != nullptr) vkDestroySampler(logicalDevice, sampler, nullptr);
		}

		static void CopyBufferDataToDeviceMemory(VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice, VkCommandPool commandPool, VkQueue queue, VkBuffer buffer, void* pData, size_t sizeBytes) {
			// Create a temporary buffer.
			VkBufferCreateInfo createInfo{};
			VkBuffer stagingBuffer;
			createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			createInfo.size = sizeBytes;
			createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			vkCreateBuffer(logicalDevice, &createInfo, nullptr, &stagingBuffer);

			// Allocate memory for the buffer.
			VkDeviceMemory bufferGpuMemory;
			VkMemoryRequirements requirements{};
			vkGetBufferMemoryRequirements(logicalDevice, stagingBuffer, &requirements);
			bufferGpuMemory = PhysicalDevice::AllocateMemory(physicalDevice, logicalDevice, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

			// Map memory to the correct GPU and CPU ranges for the buffer.
			void* cpuMemory;
			vkBindBufferMemory(logicalDevice, stagingBuffer, bufferGpuMemory, 0);
			vkMapMemory(logicalDevice, bufferGpuMemory, 0, sizeBytes, 0, &cpuMemory);
			memcpy(cpuMemory, pData, sizeBytes);

			auto copyCommandBuffer = VkHelper::CreateCommandBuffer(logicalDevice, commandPool);
			VkHelper::StartRecording(copyCommandBuffer);

			VkBufferCopy copyRegion = {};
			copyRegion.size = sizeBytes;
			vkCmdCopyBuffer(copyCommandBuffer, stagingBuffer, buffer, 1, &copyRegion);

			VkHelper::StopRecording(copyCommandBuffer);
			VkHelper::ExecuteCommands(copyCommandBuffer, queue);

			vkFreeCommandBuffers(logicalDevice, commandPool, 1, &copyCommandBuffer);
			vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
		}

		static VkShaderModule CreateShaderModule(VkDevice logicalDevice, const char* absolutePath) {
			std::ifstream file(absolutePath, std::ios::ate | std::ios::binary);
			if (!file.is_open()) { std::cout << "Failed opening file " << absolutePath << std::endl; exit(0); }

			std::vector<char> fileBytes(file.tellg());
			file.seekg(0, std::ios::beg);
			file.read(fileBytes.data(), fileBytes.size());
			file.close();

			VkShaderModuleCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.codeSize = fileBytes.size();
			createInfo.pCode = (uint32_t*)fileBytes.data();
			VkShaderModule shaderModule = nullptr;
			CheckResult(vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule));

			return shaderModule;
		}
	};

	struct Buffer {
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

		void Translate(const glm::vec3& offsetLocalSpace) {
			_matrix[3][0] += offsetLocalSpace.x;
			_matrix[3][1] += offsetLocalSpace.y;
			_matrix[3][2] += offsetLocalSpace.z;
		}

		void RotateAroundPosition(const glm::vec3& positionWorldSpace, const glm::vec3& axisWorldSpace, const float& angleRadians) {
			if (angleRadians == 0) return;
			auto rotation = MakeQuaternionRotation(axisWorldSpace, angleRadians);
			auto currentPosition = Position();
			SetPosition(positionWorldSpace + (rotation * (currentPosition - positionWorldSpace)));
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
	 * @brief Encapsulates info for a swapchain.
	 * The swapchain is an image manager; it manages everything that involves presenting images to the screen,
	 * or more precisely passing the contents of the framebuffers on the GPU down to the window.
	 */
	struct Swapchain {
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
	 * @brief Encapsulates info for a render pass.
	 * A render pass represents a specific set of commands that generate images, called attachments.
	 * Attachments are rendered images that contribute to rendering the final image that will go in the framebuffer.
	 * It is the renderpass's job to also do compositing, which is defining the logic according to which the attachments are merged to create the final image.
	 * See Swapchain to understand what framebuffers are.
	 */
	struct RenderPass {
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

	};

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
	struct Pipeline {

		/**
		 * @brief Identifier for Vulkan.
		 */
		VkPipeline _handle;

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
	};

	/**
	 * @brief Represents all the needed, or neeeded in order to obtain, information to run any call in the Vulkan API.
	 */
	struct VkContext {
		VkInstance _instance;
		VkDevice _logicalDevice;
		VkPhysicalDevice _physicalDevice;
		VkCommandPool _commandPool;
		VkSurfaceKHR _windowSurface;
		VkQueue _queue;
		uint32_t _queueFamilyIndex;
		VkFence _queueFence;
		/**
		 * @brief Function pointer called by Vulkan each time it wants to report an error.
		 * Error reporting is set by enabling validation layers.
		 */
		VkDebugReportCallbackEXT _callback;
	};

	/**
	 * @brief All the information the engine needs to render the images sent to the window.
	 */
	struct VkRenderContext {
		/**
		 * @brief The images to which Nuklear renders its UI.
		 */
		std::vector<Image> _overlayImages;
		nk_context* _uiCtx;
		std::vector<VkCommandBuffer> _drawCommandBuffers;
		Swapchain _swapchain{};
		Pipeline _envMapPipeline{};
		Pipeline _scenePipeline{};
		Pipeline _uiPipeline{};
		RenderPass _renderPass{};
		GLFWwindow* _pWindow;
		/**
		 * @brief Semaphore that will be used by Vulkan to signal when an image has finished
		 * rendering and is available to be rendered to in one of the framebuffers.
		 */
		VkSemaphore _imageAvailableSemaphore;
		VkSemaphore	_renderingFinishedSemaphore;
	};

	/**
	 * @brief Used by implementing classes to mark themselves as a class that is meant to do work on each iteration of the main loop.
	 */
	class IVulkanUpdatable {
	public:

		/**
		 * @brief Function called on implementing classes in each iteration of the main loop. The main loop is defined in VulkanApplication.cpp.
		 */
		virtual void Update(VkContext& context) = 0;
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
		virtual ShaderResources CreateDescriptorSets(VkContext& ctx, std::vector<DescriptorSetLayout>& layouts) = 0;

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
			 * @brief List of vertices that make up the drawable object.
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

		void CreateVertexBuffer(VkContext& ctx, const std::vector<Vertex>& vertices) {
			_vertices._vertexData = vertices;

			// Create a temporary buffer.
			auto& buffer = _vertices._vertexBuffer;
			auto bufferSizeBytes = GetVectorSizeInBytes(vertices);
			buffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer._createInfo.size = bufferSizeBytes;
			buffer._createInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			vkCreateBuffer(ctx._logicalDevice, &buffer._createInfo, nullptr, &buffer._buffer);

			// Allocate memory for the buffer.
			VkMemoryRequirements requirements{};
			vkGetBufferMemoryRequirements(ctx._logicalDevice, buffer._buffer, &requirements);
			buffer._gpuMemory = PhysicalDevice::AllocateMemory(ctx._physicalDevice, ctx._logicalDevice, requirements, (VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

			vkMapMemory(ctx._logicalDevice, buffer._gpuMemory, 0, GetVectorSizeInBytes(vertices), 0, &buffer._cpuMemory);

			// Map memory to the correct GPU and CPU ranges for the buffer.
			vkBindBufferMemory(ctx._logicalDevice, buffer._buffer, buffer._gpuMemory, 0);

			// Send the buffer to GPU.
			buffer._pData = (void*)vertices.data();
			buffer._sizeBytes = bufferSizeBytes;
			Buffer::CopyToDeviceMemory(ctx._logicalDevice, ctx._physicalDevice, ctx._commandPool, ctx._queue, buffer._buffer, buffer._pData, buffer._sizeBytes);
		}

		void CreateIndexBuffer(VkContext& ctx, const std::vector<unsigned int>& indices) {
			_faceIndices._indexData = indices;

			// Create a temporary buffer.
			auto& buffer = _faceIndices._indexBuffer;
			auto bufferSizeBytes = GetVectorSizeInBytes(indices);
			buffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer._createInfo.size = bufferSizeBytes;
			buffer._createInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			vkCreateBuffer(ctx._logicalDevice, &buffer._createInfo, nullptr, &buffer._buffer);

			// Allocate memory for the buffer.
			VkMemoryRequirements requirements{};
			vkGetBufferMemoryRequirements(ctx._logicalDevice, buffer._buffer, &requirements);
			buffer._gpuMemory = PhysicalDevice::AllocateMemory(ctx._physicalDevice, ctx._logicalDevice, requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			// Map memory to the correct GPU and CPU ranges for the buffer.
			vkBindBufferMemory(ctx._logicalDevice, buffer._buffer, buffer._gpuMemory, 0);

			// TODO: send the buffer to GPU.
			buffer._pData = (void*)indices.data();
			buffer._sizeBytes = bufferSizeBytes;
			Buffer::CopyToDeviceMemory(ctx._logicalDevice, ctx._physicalDevice, ctx._commandPool, ctx._queue, buffer._buffer, buffer._pData, buffer._sizeBytes);
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

		ShaderResources CreateDescriptorSets(VkContext& ctx, std::vector<DescriptorSetLayout>& layouts) {
			auto descriptorSetID = 2;

			// Create a temporary buffer.
			Buffer buffer{};
			auto bufferSizeBytes = sizeof(_lightData);
			buffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer._createInfo.size = bufferSizeBytes;
			buffer._createInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			vkCreateBuffer(ctx._logicalDevice, &buffer._createInfo, nullptr, &buffer._buffer);

			// Allocate memory for the buffer.
			VkMemoryRequirements requirements{};
			vkGetBufferMemoryRequirements(ctx._logicalDevice, buffer._buffer, &requirements);
			buffer._gpuMemory = PhysicalDevice::AllocateMemory(ctx._physicalDevice, ctx._logicalDevice, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

			// Map memory to the correct GPU and CPU ranges for the buffer.
			vkBindBufferMemory(ctx._logicalDevice, buffer._buffer, buffer._gpuMemory, 0);
			vkMapMemory(ctx._logicalDevice, buffer._gpuMemory, 0, bufferSizeBytes, 0, &buffer._cpuMemory);
			memcpy(buffer._cpuMemory, &_lightData, bufferSizeBytes);

			_buffers.push_back(buffer);

			VkDescriptorPool descriptorPool{};
			VkDescriptorPoolSize poolSizes[1] = { VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 } };
			VkDescriptorPoolCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			createInfo.maxSets = (uint32_t)1;
			createInfo.poolSizeCount = (uint32_t)1;
			createInfo.pPoolSizes = poolSizes;
			vkCreateDescriptorPool(ctx._logicalDevice, &createInfo, nullptr, &descriptorPool);

			// Create the descriptor set.
			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = (uint32_t)1;
			allocInfo.pSetLayouts = &layouts[descriptorSetID]._layout;
			VkDescriptorSet descriptorSet;
			vkAllocateDescriptorSets(ctx._logicalDevice, &allocInfo, &descriptorSet);

			// Update the descriptor set's data.
			VkDescriptorBufferInfo bufferInfo{ buffer._buffer, 0, buffer._createInfo.size };
			VkWriteDescriptorSet writeInfo = {};
			writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeInfo.dstSet = descriptorSet;
			writeInfo.descriptorCount = 1;
			writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writeInfo.pBufferInfo = &bufferInfo;
			writeInfo.dstBinding = 0;
			vkUpdateDescriptorSets(ctx._logicalDevice, 1, &writeInfo, 0, nullptr);

			auto descriptorSets = std::vector<VkDescriptorSet>{ descriptorSet };
			_shaderResources._data.try_emplace(layouts[descriptorSetID], descriptorSets);
			return _shaderResources;
		}

		void UpdateShaderResources() {
			_lightData.position = _transform.Position();
			_lightData.colorIntensity = glm::vec4(1.0f, 1.0f, 1.0f, 15000.0f);
			memcpy(_buffers[0]._cpuMemory, &_lightData, sizeof(_lightData));
		}

		void Update(VkContext& vkContext) {
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
					0,
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
											   0,
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
													0,
													(VkDescriptorSetLayoutCreateFlags)0,
													(uint32_t)descriptorCount,
													(const VkDescriptorSetLayoutBinding*)descriptorSetLayoutBindings };
			//create layout
			res = vkCreateDescriptorSetLayout(_device, &descriptorSetLayoutCreateInfo, NULL, &_descriptorSetLayout);
			if (res != VK_SUCCESS) return res;
			free(descriptorSetLayoutBindings);

			//provide the layout with actual buffers and their sizes
			VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
												0,
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
												 0,
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
											   0,
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
													0,
													(VkPipelineShaderStageCreateFlags)0,
													(VkShaderStageFlagBits)VK_SHADER_STAGE_COMPUTE_BIT,
													(VkShaderModule)shaderModule,
													(const char*)"main",
													(VkSpecializationInfo*)&specializationInfo };

			VkComputePipelineCreateInfo computePipelineCreateInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
												0,
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
			commandBufferAllocateInfo.pNext = 0;
			commandBufferAllocateInfo.commandPool = _commandPool;
			commandBufferAllocateInfo.level = (VkCommandBufferLevel)VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			commandBufferAllocateInfo.commandBufferCount = (uint32_t)1;
			res = vkAllocateCommandBuffers(_device, &commandBufferAllocateInfo, &commandBuffer);

			// Begin command buffer recording.
			VkCommandBufferBeginInfo commandBufferBeginInfo;
			commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			commandBufferBeginInfo.pNext = 0;
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
			submitInfo.pNext = 0;
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
			bufferCreateInfo.pNext = 0;
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
										 0,
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
												0,
												(VkCommandPool)_commandPool,
												(VkCommandBufferLevel)VK_COMMAND_BUFFER_LEVEL_PRIMARY,
												(uint32_t)1 };
			VkCommandBuffer commandBuffer = { 0 };
			res = vkAllocateCommandBuffers(_device, &commandBufferAllocateInfo, &commandBuffer);
			if (res != VK_SUCCESS) return res;



			VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
											 0,
											 (VkCommandBufferUsageFlags)VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
											 (const VkCommandBufferInheritanceInfo*)NULL };
			res = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
			if (res != VK_SUCCESS) return res;
			VkBufferCopy copyRegion = { 0, 0, stagingBufferSize };
			vkCmdCopyBuffer(commandBuffer, stagingBuffer, *outBuffer, 1, &copyRegion);
			res = vkEndCommandBuffer(commandBuffer);
			if (res != VK_SUCCESS) return res;

			VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO,
								 0,
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
												0,
												(VkCommandPool)_commandPool,
												(VkCommandBufferLevel)VK_COMMAND_BUFFER_LEVEL_PRIMARY,
												(uint32_t)1 };
			res = vkAllocateCommandBuffers(_device, &commandBufferAllocateInfo, &commandBuffer);
			if (res != VK_SUCCESS) return res;

			VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
											 0,
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
								 0,
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
					0,
					(VkDeviceQueueCreateFlags)0,
					(uint32_t)_queueFamilyIndex,
					(uint32_t)1,
					(const float*)&queuePriorities };

			VkPhysicalDeviceFeatures physicalDeviceFeatures = { 0 };
			physicalDeviceFeatures.shaderFloat64 = VK_TRUE;//this enables double precision support in shaders 

			VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
					0,
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
	class CubicalEnvironmentMap : public IPipelineable, public IDrawable {

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

		ShaderResources CreateDescriptorSets(VkContext& ctx, std::vector<DescriptorSetLayout>& layouts) {
			auto descriptorSetID = 4;

			// Map the cubemap image to the fragment shader.
			VkDescriptorPool descriptorPool{};
			VkDescriptorPoolSize poolSizes[1] = { VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 } };
			VkDescriptorPoolCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			createInfo.maxSets = (uint32_t)1;
			createInfo.poolSizeCount = (uint32_t)1;
			createInfo.pPoolSizes = poolSizes;
			vkCreateDescriptorPool(ctx._logicalDevice, &createInfo, nullptr, &descriptorPool);

			// Create the descriptor set.
			VkDescriptorSet set{};
			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = (uint32_t)1;
			allocInfo.pSetLayouts = &layouts[descriptorSetID]._layout;
			vkAllocateDescriptorSets(ctx._logicalDevice, &allocInfo, &set);

			// Update the descriptor set's data with the environment map's image.
			VkDescriptorImageInfo imageInfo{ _cubeMapImage._sampler, _cubeMapImage._view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			VkWriteDescriptorSet writeInfo = {};
			writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeInfo.dstSet = set;
			writeInfo.descriptorCount = 1;
			writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeInfo.pImageInfo = &imageInfo;
			writeInfo.dstBinding = 0;
			vkUpdateDescriptorSets(ctx._logicalDevice, 1, &writeInfo, 0, nullptr);

			auto descriptorSets = std::vector<VkDescriptorSet>{ set };
			_shaderResources._data.try_emplace(layouts[descriptorSetID], descriptorSets);
			return _shaderResources;
		}

		void UpdateShaderResources() {
		}

		void CreateVertexBuffer(VkContext& ctx) {
			//  _right, _left, _upper, _lower, _front, _back 
			const unsigned int coordinateCount = 72;
			float skyBoxVertices[coordinateCount] = {
				// Back face (-Z)
				-1.0f, -1.0f, -1.0f, // Vertex 0
				1.0f, -1.0f, -1.0f, // Vertex 1
				1.0f,  1.0f, -1.0f, // Vertex 2
				-1.0f,  1.0f, -1.0f, // Vertex 3

				// Front face (+Z)
				-1.0f, -1.0f,  1.0f, // Vertex 4
				1.0f, -1.0f,  1.0f, // Vertex 5
				1.0f,  1.0f,  1.0f, // Vertex 6
				-1.0f,  1.0f,  1.0f, // Vertex 7

				// Left face (-X)
				-1.0f, -1.0f,  1.0f, // Vertex 4
				-1.0f, -1.0f, -1.0f, // Vertex 0
				-1.0f,  1.0f, -1.0f, // Vertex 3
				-1.0f,  1.0f,  1.0f, // Vertex 7

				// Right face (+X)
				1.0f, -1.0f, -1.0f, // Vertex 1
				1.0f, -1.0f,  1.0f, // Vertex 5
				1.0f,  1.0f,  1.0f, // Vertex 6
				1.0f,  1.0f, -1.0f, // Vertex 2

				// Bottom face (-Y)
				-1.0f, -1.0f, -1.0f, // Vertex 0
				-1.0f, -1.0f,  1.0f, // Vertex 4
				1.0f, -1.0f,  1.0f, // Vertex 5
				1.0f, -1.0f, -1.0f, // Vertex 1

				// Top face (+Y)
				-1.0f,  1.0f,  1.0f, // Vertex 7
				-1.0f,  1.0f, -1.0f, // Vertex 3
				1.0f,  1.0f, -1.0f, // Vertex 2
				1.0f,  1.0f,  1.0f  // Vertex 6
			};

			// Create a temporary buffer.
			auto& buffer = _vertices._vertexBuffer;
			auto bufferSizeBytes = coordinateCount * sizeof(float);
			buffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer._createInfo.size = bufferSizeBytes;
			buffer._createInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			vkCreateBuffer(ctx._logicalDevice, &buffer._createInfo, nullptr, &buffer._buffer);

			// Allocate memory for the buffer.
			VkMemoryRequirements requirements{};
			vkGetBufferMemoryRequirements(ctx._logicalDevice, buffer._buffer, &requirements);
			buffer._gpuMemory = PhysicalDevice::AllocateMemory(ctx._physicalDevice, ctx._logicalDevice, requirements, (VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

			vkMapMemory(ctx._logicalDevice, buffer._gpuMemory, 0, bufferSizeBytes, 0, &buffer._cpuMemory);

			// Map memory to the correct GPU and CPU ranges for the buffer.
			vkBindBufferMemory(ctx._logicalDevice, buffer._buffer, buffer._gpuMemory, 0);

			// Send the buffer to GPU.
			buffer._pData = skyBoxVertices;
			buffer._sizeBytes = bufferSizeBytes;
			Buffer::CopyToDeviceMemory(ctx._logicalDevice, ctx._physicalDevice, ctx._commandPool, ctx._queue, buffer._buffer, buffer._pData, buffer._sizeBytes);
		}

		void CreateIndexBuffer(VkContext& ctx) {
			//  _right, _left, _upper, _lower, _front, _back 
			const unsigned int indexCount = 36;
			uint32_t skyBoxFaceIndices[indexCount] = {

				// Right face (+X)
				1, 5, 6, // First triangle
				6, 2, 1, // Second triangle

				// Left face (-X)
				4, 0, 3, // First triangle
				3, 7, 4, // Second triangle

				// Top face (+Y)
				7, 3, 2, // First triangle
				2, 6, 7,  // Second triangle

				// Bottom face (-Y)
				0, 4, 5, // First triangle
				5, 1, 0, // Second triangle

				// Front face (+Z)
				6, 5, 4, // First triangle
				6, 4, 7, // Second triangle

				// Back face (-Z)
				0, 1, 2, // First triangle
				2, 3, 0 // Second triangle
			};

			// Create a temporary buffer.
			auto& buffer = _faceIndices._indexBuffer;
			auto bufferSizeBytes = indexCount * sizeof(uint32_t);
			buffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer._createInfo.size = bufferSizeBytes;
			buffer._createInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			vkCreateBuffer(ctx._logicalDevice, &buffer._createInfo, nullptr, &buffer._buffer);

			// Allocate memory for the buffer.
			VkMemoryRequirements requirements{};
			vkGetBufferMemoryRequirements(ctx._logicalDevice, buffer._buffer, &requirements);
			buffer._gpuMemory = PhysicalDevice::AllocateMemory(ctx._physicalDevice, ctx._logicalDevice, requirements, (VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

			vkMapMemory(ctx._logicalDevice, buffer._gpuMemory, 0, bufferSizeBytes, 0, &buffer._cpuMemory);

			// Map memory to the correct GPU and CPU ranges for the buffer.
			vkBindBufferMemory(ctx._logicalDevice, buffer._buffer, buffer._gpuMemory, 0);

			// Send the buffer to GPU.
			buffer._pData = skyBoxFaceIndices;
			buffer._sizeBytes = bufferSizeBytes;
			Buffer::CopyToDeviceMemory(ctx._logicalDevice, ctx._physicalDevice, ctx._commandPool, ctx._queue, buffer._buffer, buffer._pData, buffer._sizeBytes);
		}

		void Draw(VkPipelineLayout& pipelineLayout, VkCommandBuffer& drawCommandBuffer) {
			VkDescriptorSet sets[2] = { _shaderResources[0][0], _shaderResources[4][0] };
			vkCmdBindDescriptorSets(drawCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, sets, 0, nullptr);
			vkCmdBindDescriptorSets(drawCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &sets[1], 0, nullptr);

			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(drawCommandBuffer, 0, 1, &_vertices._vertexBuffer._buffer, &offset);
			vkCmdBindIndexBuffer(drawCommandBuffer, _faceIndices._indexBuffer._buffer, 0, VkIndexType::VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(drawCommandBuffer, 36, 1, 0, 0, 0);
		}
	};

	// Forward declarations for the compiler.
	class GameObject;
	class Scene;
	class RigidBody;
	class Mesh;

	struct CollisionContext {
		RigidBody* _collidee; // Collision receiver.
		std::vector<glm::vec4> _collisionPositions; // List of points where a collision was detected in world space. W component discarded, only there for efficiency reasons, only there for efficiency reasons with CPU-GPU interop.
		std::vector<glm::vec4> _collisionNormals; // List of normals for each collision position. W component discarded, only there for efficiency reasons with CPU-GPU interop.
		std::vector<RigidBody*> _collisionObjects; // List of objects for which the collision was detected.
		glm::vec4 _averagePosition;
		glm::vec4 _averageNormal;

		void CalculateAverages() {
			auto count = _collisionPositions.size();

			for (int j = 0; j < count; ++j) {
				_averageNormal += _collisionNormals[j];
				_averagePosition += _collisionPositions[j];
			}

			_averagePosition /= count; _averageNormal /= count;
		}
	};

	struct EngineContext;

	/**
		 * @brief Used by implementing classes to mark themselves as a class that is meant to do work on the main loop of the physics thread.
		 */
	class IPhysicsUpdatable {
	public:

		/**
		 * @brief Function called on implementing classes in each iteration of the main loop.
		 */
		virtual void PhysicsUpdate(VkContext& ctx, VkContext& collisionCtx, EngineContext& eCtx) = 0;
	};

	class Time : public Singleton<Time>, public IUpdatable, public IPhysicsUpdatable {
	public:

		/**
		 * @brief The time this instance was created; assigned to in the constructor.
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
		 * @brief Fixed physics update time in milliseconds.
		 * The engine will wait for (_fixedPhysicsDeltaTime - _physicsDeltaTime) milliseconds if _physicsDeltaTime is lower than this number.
		 */
		double _fixedPhysicsDeltaTime = 16;

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
		void PhysicsUpdate(VkContext& ctx, VkContext& collisionCtx, EngineContext& eCtx) {
			auto now = std::chrono::high_resolution_clock::now();
			_physicsDeltaTime = (now - _lastPhysicsUpdateTime).count() * 0.000001;
			_lastPhysicsUpdateTime = now;
		}
	};

	/*struct ForceCtx {
		glm::vec3 _force;
		glm::vec3 _pointOfApplication;
		float _deltaTimeSeconds;
		bool _isTorque = false;
		bool _ignoreMass = false;
		bool _isApplicationPointWorldSpace = true;
		bool _isForceWorldSpace = true;

		ForceCtx(glm::vec3 force = glm::vec3(0.0f), glm::vec3 pointOfApplication = glm::vec3(0.0f), float deltaTimeSeconds = 0.0f, bool isTorque = false, bool ignoreMass = false, bool isApplicationPointWorldSpace = true, bool isForceWorldSpace = true) :
			_force(force),
			_pointOfApplication(pointOfApplication),
			_deltaTimeSeconds(deltaTimeSeconds),
			_isTorque(isTorque),
			_ignoreMass(ignoreMass),
			_isApplicationPointWorldSpace(isApplicationPointWorldSpace),
			_isForceWorldSpace(isForceWorldSpace)
		{
		}
	};*/

	glm::vec3 gGravity = glm::vec3(0.0f, -10.0f, 0.0f);

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
		 * @brief The velocity vector of this physics body in units per second in world space.
		 */
		glm::vec3 _velocity = glm::vec3(0.0f, 0.0f, 0.0f);

		/**
		 * @brief The angular velocity in radians per second. The direction of the vector represents the axis of rotation in world space, whereas the
		 * length of the vector is the actual value in radians per second.
		 */
		glm::vec3 _angularVelocity = glm::vec3(0.0f, 0.0f, 0.0f);

		/**
		 * @brief Mass in kg.
		 */
		float _mass;

		/**
		 * @brief Friction coefficient. Adjust depending on in-engine behaviour.
		 */
		float _friction;

		/**
		 * @brief Bounciness coefficient. Adjust depending on in-engine behaviour.
		 */
		float _bounciness;

		/**
		 * @brief If this is set to true, _overriddenCenterOfMass will be used as center of mass.
		 */
		bool _isCenterOfMassOverridden;

		/**
		 * @brief Overridden center of mass in local space.
		 */
		glm::vec3 _overriddenCenterOfMassLocalSpace;

		/**
		 * @brief Lock rotation along the X axis.
		 */
		bool _lockRotationX;

		/**
		 * @brief Lock rotation along the Y axis.
		 */
		bool _lockRotationY;

		/**
		 * @brief Lock rotation along the Z axis.
		 */
		bool _lockRotationZ;

		/**
		 * @brief Lock translation along the X axis.
		 */
		bool _lockTranslationX;

		/**
		 * @brief Lock translation along the Y axis.
		 */
		bool _lockTranslationY;

		/**
		 * @brief Lock translation along the Z axis.
		 */
		bool _lockTranslationZ;

		bool _isAffectedByGravity;

		/**
		 * @brief The cutoff time after which a collision becomes continuous or stops being continuous.
		 */
		int _continuousCollisionThresholdMilliseconds = 100;

		std::chrono::high_resolution_clock::time_point _lastTimeCollided;

		float _clampAngularVelocity = 2.0f;

		/**
		 * @brief True if the body has been colliding for longer than the continuous collision threshold, and false if it has not been colliding for longer than said threshold.
		 */
		bool _isColliding = false;

		/**
		 * @brief Physics update implementation for this specific rigidbody.
		 */
		void(*_updateImplementation)(GameObject&);

		/**
		 * @brief GameObject tied to this RigidBody.
		 */
		GameObject* _pGameObject;

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

		static glm::vec3 CalculateTransmittedForce(const glm::vec3& transmitterPosition, const glm::vec3& force, const glm::vec3& receiverPosition);

		/**
		 * @brief Returns the center of mass in local space.
		 * @return
		 */
		glm::vec3 GetCenterOfMass(bool worldSpace = false);

		glm::vec3 GetVelocityAtPosition(const glm::vec3& position);

		void AddForceAtPosition(const glm::vec3& force, const glm::vec3& pointOfApplication, const float& deltaTimeSeconds, bool isApplicationPointWorldSpace = true, bool isForceWorldSpace = true, const bool& ignoreMass = false);

		void AddForce(const glm::vec3& force, const float& deltaTimeSeconds, const bool& ignoreMass = false);

		void AddTorque(const glm::vec3& torqueWorldSpaceAxis, const float& deltaTimeSeconds, const bool& ignoreMass = false);

		CollisionContext DetectCollision(RigidBody& other);

		std::vector< GameObject*> GetGameObjects(GameObject* pRoot, std::vector<GameObject*> excludedObjects = {});

		std::vector<CollisionContext> DetectCollisions(VkContext& ctx, VkContext& collisionCtx, std::vector<RigidBody*> bodiesToExclude = {});

		/**
		 * @brief Basic init that guarantees the body's simulation.
		 */
		void Initialize(GameObject* pGameObject, const float& mass = 1.0f, const bool& overrideCenterOfMass = false, const glm::vec3& overriddenCenterOfMass = glm::vec3{});

		void PhysicsUpdate(VkContext& ctx, VkContext& collisionCtx, EngineContext& eCtx);

		bool IsRotationLocked();

		bool IsTranslationLocked();

		void LockRotation();

		void LockTranslation();

		void UnlockRotation();

		void UnlockTranslation();
	};

	class Mesh : public IVulkanUpdatable, public IDrawable, public IPipelineable {

	public:

		Mesh() = default;
		~Mesh();
		int _materialIndex = 0;
		GameObject* _pGameObject = nullptr;

		ShaderResources CreateDescriptorSets(VkContext& ctx, std::vector<DescriptorSetLayout>& layouts);
		void UpdateShaderResources();
		void Update(VkContext& vkContext);
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
		ShaderResources CreateDescriptorSets(VkContext& ctx, std::vector<DescriptorSetLayout>& layouts);
		Transform GetWorldSpaceTransform();
		void UpdateShaderResources();
		void PhysicsUpdate(VkContext& ctx, VkContext& collisionCtx, EngineContext& eCtx);
		void Update(VkContext& vkContext);
		void Draw(VkPipelineLayout& pipelineLayout, VkCommandBuffer& drawCommandBuffer);
	};

	struct GpuCollisionDetector {
		static int GetComputeQueueFamilyIndex(const VkPhysicalDevice& physicalDevice) {
			//find a queue family for a selected GPU, select the first available for use
			uint32_t queueFamilyCount;
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);

			VkQueueFamilyProperties* queueFamilies = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);

			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies);
			uint32_t i = 0;
			for (; i < queueFamilyCount; i++) {
				VkQueueFamilyProperties props = queueFamilies[i];
				if (props.queueCount > 0 && (props.queueFlags & VK_QUEUE_COMPUTE_BIT)) break;
			}
			free(queueFamilies);
			if (i == queueFamilyCount) return -1;
			return i;
		}

		static void CreateNewComputeDevice(VkDevice& device, VkPhysicalDevice& physicalDevice, VkDevice& outComputeDevice, VkQueue& outComputeQueue, uint32_t& outComputeQueueFamilyIndex) {
			int computeFamilyIndex = GetComputeQueueFamilyIndex(physicalDevice);
			if (computeFamilyIndex < 0) return;

			float queuePriorities = 1.0;
			VkDeviceQueueCreateInfo deviceQueueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, 0, (VkDeviceQueueCreateFlags)0, (uint32_t)computeFamilyIndex,(uint32_t)1, (const float*)&queuePriorities };

			VkPhysicalDeviceFeatures physicalDeviceFeatures = { 0 };
			//physicalDeviceFeatures.shaderFloat64 = VK_TRUE;//this enables double precision support in shaders 

			VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
					0,
					(VkDeviceCreateFlags)0,
					(uint32_t)1,
					(const VkDeviceQueueCreateInfo*)&deviceQueueCreateInfo,
					(uint32_t)0,
					(const char* const*)NULL,
					(uint32_t)0,
					(const char* const*)NULL,
					(const VkPhysicalDeviceFeatures*)&physicalDeviceFeatures };

			CheckResult(vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &outComputeDevice));
			outComputeQueueFamilyIndex = computeFamilyIndex;

			vkGetDeviceQueue(outComputeDevice, computeFamilyIndex, 0, &outComputeQueue);
			if (!outComputeQueue) { std::cout << "Failed to get compute queue" << std::endl; return; }
		}

		static VkContext InitializeVulkan(VkDevice& device, VkPhysicalDevice physicalDevice) {
			// Create logical device representation.
			VkContext ctx;
			ctx._physicalDevice = physicalDevice;
			CreateNewComputeDevice(device, physicalDevice, ctx._logicalDevice, ctx._queue, ctx._queueFamilyIndex);
			if (!ctx._logicalDevice) { std::cout << "Failed to create compute device" << std::endl; return {}; }

			// Create a fence for synchronization.
			VkFenceCreateInfo fenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, 0 };
			if (vkCreateFence(ctx._logicalDevice, &fenceCreateInfo, NULL, &ctx._queueFence)) { std::cout << "Fence creation failed." << std::endl; }

			// Create a structure from which command buffer memory is allocated from.
			VkCommandPoolCreateInfo commandPoolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, ctx._queueFamilyIndex };
			if (vkCreateCommandPool(ctx._logicalDevice, &commandPoolCreateInfo, NULL, &ctx._commandPool)) { std::cout << "Command Pool Creation failed." << std::endl; }
			return ctx;
		}

		static std::vector<uint32_t> CalculateWorkGroupCount(VkPhysicalDeviceProperties& gpuProperties, uint32_t minimumThreadCount, const std::vector<uint32_t>& workGroupSize) {
			// Prepare variables.
			uint32_t maxWorkGroupInvocations = gpuProperties.limits.maxComputeWorkGroupInvocations;
			uint32_t* maxWorkGroupSize = gpuProperties.limits.maxComputeWorkGroupSize;
			uint32_t* maxWorkGroupCount = gpuProperties.limits.maxComputeWorkGroupCount;
			std::vector<uint32_t> outWorkGroupCount = { 1, 1, 1 };

			// Use the work group size first, as that directly controls the amount of threads 1 to 1.
			uint32_t totalWorkGroupSize = workGroupSize[0] * workGroupSize[1] * workGroupSize[2];
			if (totalWorkGroupSize >= minimumThreadCount) return outWorkGroupCount;

			// If one workgroup still doesn't do it, use multiple workgroups with the size of each one calculated earlier.
			if (totalWorkGroupSize < minimumThreadCount) {
				for (int i = 0; i < 3; ++i) {
					for (; outWorkGroupCount[i] < maxWorkGroupCount[i]; ++outWorkGroupCount[i]) {
						if (((outWorkGroupCount[0] * outWorkGroupCount[1] * outWorkGroupCount[2]) * totalWorkGroupSize) >= minimumThreadCount) { break; }
					}
				}
			}
		}

		static VkResult AllocateGPUOnlyBuffer(VkBufferUsageFlags bufferUsageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize bufferSizeBytes, VkContext& ctx, VkBuffer* outBuffer, VkDeviceMemory* outDeviceMemory) {
			//allocate the buffer used by the GPU with specified properties
			VkResult res = VK_SUCCESS;
			uint32_t queueFamilyIndices;
			VkBufferCreateInfo bufferCreateInfo;
			bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferCreateInfo.pNext = 0;
			bufferCreateInfo.flags = (VkBufferCreateFlags)0;
			bufferCreateInfo.size = (VkDeviceSize)bufferSizeBytes;
			bufferCreateInfo.usage = (VkBufferUsageFlags)bufferUsageFlags;
			bufferCreateInfo.sharingMode = (VkSharingMode)VK_SHARING_MODE_EXCLUSIVE;
			bufferCreateInfo.queueFamilyIndexCount = (uint32_t)1;
			bufferCreateInfo.pQueueFamilyIndices = (const uint32_t*)&queueFamilyIndices;

			res = vkCreateBuffer(ctx._logicalDevice, &bufferCreateInfo, NULL, outBuffer);
			if (res != VK_SUCCESS) return res;

			VkMemoryRequirements memoryRequirements = { 0 };
			vkGetBufferMemoryRequirements(ctx._logicalDevice, *outBuffer, &memoryRequirements);

			//find memory with specified properties
			uint32_t memoryTypeIndex = 0xFFFFFFFF;
			VkPhysicalDeviceMemoryProperties gpuMemoryProps;
			vkGetPhysicalDeviceMemoryProperties(ctx._physicalDevice, &gpuMemoryProps);

			for (uint32_t i = 0; i < gpuMemoryProps.memoryTypeCount; ++i) {
				if ((memoryRequirements.memoryTypeBits & (1 << i)) && ((gpuMemoryProps.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags)) {
					memoryTypeIndex = i;
					break;
				}
			}
			if (0xFFFFFFFF == memoryTypeIndex) return VK_ERROR_INITIALIZATION_FAILED;

			VkMemoryAllocateInfo memoryAllocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
										 0,
										 (VkDeviceSize)memoryRequirements.size,
										 (uint32_t)memoryTypeIndex };

			res = vkAllocateMemory(ctx._logicalDevice, &memoryAllocateInfo, NULL, outDeviceMemory);
			if (res != VK_SUCCESS) return res;

			res = vkBindBufferMemory(ctx._logicalDevice, *outBuffer, *outDeviceMemory, 0);
			return res;
		}

		static VkResult UploadDataToGPU(void* data, VkBuffer* outBuffer, VkDeviceSize bufferSizeBytes, VkContext& ctx) {
			VkResult res = VK_SUCCESS;

			// a function that transfers data from the CPU to the GPU using staging buffer,
			// because the GPU memory is not host-coherent
			VkDeviceSize stagingBufferSize = bufferSizeBytes;
			VkBuffer stagingBuffer = { 0 };
			VkDeviceMemory stagingBufferMemory = { 0 };
			res = AllocateGPUOnlyBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				stagingBufferSize,
				ctx,
				&stagingBuffer,
				&stagingBufferMemory);
			if (res != VK_SUCCESS) return res;

			void* stagingData;
			res = vkMapMemory(ctx._logicalDevice, stagingBufferMemory, 0, stagingBufferSize, 0, &stagingData);
			if (res != VK_SUCCESS) return res;
			memcpy(stagingData, data, stagingBufferSize);
			vkUnmapMemory(ctx._logicalDevice, stagingBufferMemory);

			VkCommandBufferAllocateInfo commandBufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
												0,
												(VkCommandPool)ctx._commandPool,
												(VkCommandBufferLevel)VK_COMMAND_BUFFER_LEVEL_PRIMARY,
												(uint32_t)1 };
			VkCommandBuffer commandBuffer = { 0 };
			res = vkAllocateCommandBuffers(ctx._logicalDevice, &commandBufferAllocateInfo, &commandBuffer);
			if (res != VK_SUCCESS) return res;



			VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
											 0,
											 (VkCommandBufferUsageFlags)VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
											 (const VkCommandBufferInheritanceInfo*)NULL };
			res = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
			if (res != VK_SUCCESS) return res;
			VkBufferCopy copyRegion = { 0, 0, stagingBufferSize };
			vkCmdCopyBuffer(commandBuffer, stagingBuffer, *outBuffer, 1, &copyRegion);
			res = vkEndCommandBuffer(commandBuffer);
			if (res != VK_SUCCESS) return res;

			VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO,
								 0,
								 (uint32_t)0,
								 (const VkSemaphore*)NULL,
								 (const VkPipelineStageFlags*)NULL,
								 (uint32_t)1,
								 (const VkCommandBuffer*)&commandBuffer,
								 (uint32_t)0,
								 (const VkSemaphore*)NULL };

			res = vkQueueSubmit(ctx._queue, 1, &submitInfo, ctx._queueFence);
			if (res != VK_SUCCESS) return res;


			res = vkWaitForFences(ctx._logicalDevice, 1, &ctx._queueFence, VK_TRUE, 100000000000);
			if (res != VK_SUCCESS) return res;

			res = vkResetFences(ctx._logicalDevice, 1, &ctx._queueFence);
			if (res != VK_SUCCESS) return res;

			vkFreeCommandBuffers(ctx._logicalDevice, ctx._commandPool, 1, &commandBuffer);
			vkDestroyBuffer(ctx._logicalDevice, stagingBuffer, NULL);
			vkFreeMemory(ctx._logicalDevice, stagingBufferMemory, NULL);
			return res;
		}

		static VkResult DownloadDataFromGPU(void* data, VkDeviceSize bufferSize, VkContext& ctx, VkBuffer* outBuffer) {
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
				ctx,
				&stagingBuffer,
				&stagingBufferMemory);
			if (res != VK_SUCCESS) return res;

			VkCommandBufferAllocateInfo commandBufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, 0, ctx._commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
			res = vkAllocateCommandBuffers(ctx._logicalDevice, &commandBufferAllocateInfo, &commandBuffer);
			if (res != VK_SUCCESS) return res;

			VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, NULL };
			res = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
			if (res != VK_SUCCESS) return res;

			VkBufferCopy copyRegion = { (VkDeviceSize)0, (VkDeviceSize)0, (VkDeviceSize)stagingBufferSize };

			vkCmdCopyBuffer(commandBuffer, outBuffer[0], stagingBuffer, 1, &copyRegion);
			vkEndCommandBuffer(commandBuffer);

			VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO, 0, 0, NULL, NULL, 1, &commandBuffer, 0, NULL };

			res = vkQueueSubmit(ctx._queue, 1, &submitInfo, ctx._queueFence);
			if (res != VK_SUCCESS) return res;

			res = vkWaitForFences(ctx._logicalDevice, 1, &ctx._queueFence, VK_TRUE, 100000000000);
			if (res != VK_SUCCESS) return res;

			res = vkResetFences(ctx._logicalDevice, 1, &ctx._queueFence);
			if (res != VK_SUCCESS) return res;

			vkFreeCommandBuffers(ctx._logicalDevice, ctx._commandPool, 1, &commandBuffer);

			void* stagingData;
			res = vkMapMemory(ctx._logicalDevice, stagingBufferMemory, 0, stagingBufferSize, 0, &stagingData);
			if (res != VK_SUCCESS) return res;

			memcpy(data, stagingData, stagingBufferSize);
			vkUnmapMemory(ctx._logicalDevice, stagingBufferMemory);

			vkDestroyBuffer(ctx._logicalDevice, stagingBuffer, NULL);
			vkFreeMemory(ctx._logicalDevice, stagingBufferMemory, NULL);
			return res;
		}

		static VkResult CreateComputePipeline(VkBuffer* shaderBuffersArray, VkDeviceSize* arrayOfSizesOfEachBuffer, const char* shaderFilePath, VkContext& ctx, VkPipeline& outPipeline, VkPipelineLayout& outLayout, VkDescriptorSet& outDescriptorSet) {
			VkDescriptorPool descriptorPool;
			VkResult res = VK_SUCCESS;

			// We now have 5 buffers: vertexA, indexA, vertexB, indexB, result
			uint32_t descriptorCount = 5;

			VkDescriptorPoolSize descriptorPoolSize = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorCount };

			const VkDescriptorType descriptorTypes[5] = {
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // vertex A
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // index A
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // vertex B
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // index B
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER  // output
			};

			VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
				VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr, 0, 1, 1, &descriptorPoolSize
			};
			CheckResult(vkCreateDescriptorPool(ctx._logicalDevice, &descriptorPoolCreateInfo, nullptr, &descriptorPool));

			VkDescriptorSetLayoutBinding* descriptorSetLayoutBindings = (VkDescriptorSetLayoutBinding*)malloc(descriptorCount * sizeof(VkDescriptorSetLayoutBinding));
			for (uint32_t i = 0; i < descriptorCount; ++i) {
				descriptorSetLayoutBindings[i].binding = i;
				descriptorSetLayoutBindings[i].descriptorType = descriptorTypes[i];
				descriptorSetLayoutBindings[i].descriptorCount = 1;
				descriptorSetLayoutBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
				descriptorSetLayoutBindings[i].pImmutableSamplers = nullptr;
			}

			VkDescriptorSetLayout descriptorSetLayout;
			VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, descriptorCount, descriptorSetLayoutBindings
			};
			CheckResult(vkCreateDescriptorSetLayout(ctx._logicalDevice, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout));
			free(descriptorSetLayoutBindings);

			VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, descriptorPool, 1, &descriptorSetLayout
			};

			CheckResult(vkAllocateDescriptorSets(ctx._logicalDevice, &descriptorSetAllocateInfo, &outDescriptorSet));

			for (uint32_t i = 0; i < descriptorCount; ++i) {
				VkDescriptorBufferInfo descriptorBufferInfo = { shaderBuffersArray[i], 0, arrayOfSizesOfEachBuffer[i] };
				VkWriteDescriptorSet writeDescriptorSet = {
					VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, outDescriptorSet, i, 0, 1,
					descriptorTypes[i], nullptr, &descriptorBufferInfo, nullptr
				};
				vkUpdateDescriptorSets(ctx._logicalDevice, 1, &writeDescriptorSet, 0, nullptr);
			}

			VkPushConstantRange range = {};
			range.offset = 0;
			range.size = sizeof(glm::mat4) * 2;
			range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
				VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1, &descriptorSetLayout, 1, &range
			};
			CheckResult(vkCreatePipelineLayout(ctx._logicalDevice, &pipelineLayoutCreateInfo, nullptr, &outLayout));

			VkShaderModule shaderModule = VkHelper::CreateShaderModule(ctx._logicalDevice, shaderFilePath);

			VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo = {
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT,
				shaderModule, "main", nullptr
			};

			VkComputePipelineCreateInfo computePipelineCreateInfo = {
				VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, nullptr, 0,
				pipelineShaderStageCreateInfo, outLayout, VK_NULL_HANDLE, 0
			};

			res = vkCreateComputePipelines(ctx._logicalDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &outPipeline);
			vkDestroyShaderModule(ctx._logicalDevice, shaderModule, nullptr);

			return res;
		}

		static VkResult Dispatch(VkContext& ctx, VkPipeline pipeline, VkPipelineLayout pipelineLayout,
			VkDescriptorSet descriptorSet, std::vector<uint32_t>& workGroupCount,
			const glm::mat4x4& objectToWorldA, const glm::mat4x4& objectToWorldB) {
			VkResult res = VK_SUCCESS;
			VkCommandBuffer commandBuffer = { 0 };

			// Create a command buffer to be executed on the GPU
			VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
			commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			commandBufferAllocateInfo.commandPool = ctx._commandPool;
			commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			commandBufferAllocateInfo.commandBufferCount = 1;
			res = vkAllocateCommandBuffers(ctx._logicalDevice, &commandBufferAllocateInfo, &commandBuffer);
			if (res != VK_SUCCESS) return res;

			// Begin command buffer recording
			VkCommandBufferBeginInfo commandBufferBeginInfo = {};
			commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			res = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
			if (res != VK_SUCCESS) return res;

			// Define push constants structure matching the shader
			struct PushConstants {
				glm::mat4x4 localToWorldA;
				glm::mat4x4 localToWorldB;
			} pushConstants;
			pushConstants.localToWorldA = objectToWorldA;
			pushConstants.localToWorldB = objectToWorldB;

			// Bind pipeline and push constants
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
			vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), &pushConstants);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);
			vkCmdDispatch(commandBuffer, workGroupCount[0], workGroupCount[1], workGroupCount[2]);

			// End command buffer recording
			res = vkEndCommandBuffer(commandBuffer);
			if (res != VK_SUCCESS) return res;

			// Submit the command buffer
			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffer;
			clock_t t = clock();
			res = vkQueueSubmit(ctx._queue, 1, &submitInfo, ctx._queueFence);
			if (res != VK_SUCCESS) return res;

			// Wait for the fence
			res = vkWaitForFences(ctx._logicalDevice, 1, &ctx._queueFence, VK_TRUE, 30000000000);
			if (res != VK_SUCCESS) return res;

			// Calculate execution time
			t = clock() - t;
			double time = ((double)t) / CLOCKS_PER_SEC * 1000;

			// Reset fence and free command buffer
			res = vkResetFences(ctx._logicalDevice, 1, &ctx._queueFence);
			if (res != VK_SUCCESS) return res;

			vkFreeCommandBuffers(ctx._logicalDevice, ctx._commandPool, 1, &commandBuffer);
			return res;
		}

		static CollisionContext Run(VkContext& collisionCtx, RigidBody& bodyA, RigidBody& bodyB, bool& outCollided) {
			// Get device properties and memory properties, if needed.
			VkPhysicalDeviceProperties gpuProperties;
			VkPhysicalDeviceMemoryProperties gpuMemoryProperties;
			vkGetPhysicalDeviceProperties(collisionCtx._physicalDevice, &gpuProperties);
			vkGetPhysicalDeviceMemoryProperties(collisionCtx._physicalDevice, &gpuMemoryProperties);

			// Calculate how many workgroups and the size of each workgroup we are going to use.
			// We want one GPU thread to operate on a single value from the input buffer, so the required thread size is the input buffer size.
			RigidBody* a = &bodyA;
			RigidBody* b = &bodyB;
			auto& aVertices = a->_pGameObject->_pMesh->_vertices._vertexData;
			auto& bVertices = b->_pGameObject->_pMesh->_vertices._vertexData;
			auto& aIndices = a->_pGameObject->_pMesh->_faceIndices._indexData;
			auto& bIndices = b->_pGameObject->_pMesh->_faceIndices._indexData;

			if (aVertices.size() < bVertices.size()) { auto tmp = b; b = a; a = tmp; }

			size_t sizeA_bytes = aVertices.size() * sizeof(glm::vec4);
			size_t sizeB_bytes = bVertices.size() * sizeof(glm::vec4);
			size_t sizeIndexA_bytes = aIndices.size() * sizeof(uint32_t);
			size_t sizeIndexB_bytes = bIndices.size() * sizeof(uint32_t);

			auto faceCount = aIndices.size() / 3;
			auto outputCount = faceCount * 2;
			size_t sizeOutputBytes = sizeof(glm::vec4) * outputCount;
			glm::vec4* dataInputBufferA = (glm::vec4*)malloc(sizeA_bytes);
			glm::vec4* dataInputBufferB = (glm::vec4*)malloc(sizeB_bytes);
			if (!dataInputBufferA || !dataInputBufferB) { std::cout << "Failed allocating buffers for input meshes" << std::endl; return {}; }

			for (int i = 0; i < aVertices.size(); ++i) {
				dataInputBufferA[i].x = aVertices[i]._position.x;
				dataInputBufferA[i].y = aVertices[i]._position.y;
				dataInputBufferA[i].z = aVertices[i]._position.z;
				dataInputBufferA[i].w = 1.0f;
			}
			for (int i = 0; i < bVertices.size(); ++i) {
				dataInputBufferB[i].x = bVertices[i]._position.x;
				dataInputBufferB[i].y = bVertices[i]._position.y;
				dataInputBufferB[i].z = bVertices[i]._position.z;
				dataInputBufferB[i].w = 1.0f;
			}

			std::vector <uint32_t> workGroupCount = CalculateWorkGroupCount(gpuProperties, (uint32_t)outputCount, { 256, 1, 1 });

			//use default values if coalescedMemory = 0
			//if (_coalescedMemory == 0) {
			//	switch (_physicalDeviceProperties.vendorID) {
			//	case 0x10DE://NVIDIA - change to 128 before Pascal
			//		_coalescedMemory = 32;
			//		break;
			//	case 0x8086://INTEL
			//		_coalescedMemory = 64;
			//		break;
			//	case 0x13B5://AMD
			//		_coalescedMemory = 64;
			//		break;
			//	default:
			//		_coalescedMemory = 64;
			//		break;
			//	}
			//}

			// Create the input buffer.
			VkResult res = VK_SUCCESS;
			Buffer vertexBufferA, indexBufferA, vertexBufferB, indexBufferB, outputBuffer;

			VkHelper::CreateBuffer(collisionCtx._logicalDevice,
				collisionCtx._physicalDevice,
				sizeA_bytes,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				&vertexBufferA._buffer,
				&vertexBufferA._gpuMemory);

			VkHelper::CreateBuffer(collisionCtx._logicalDevice, collisionCtx._physicalDevice, sizeIndexA_bytes,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				&indexBufferA._buffer, &indexBufferA._gpuMemory);

			VkHelper::CreateBuffer(collisionCtx._logicalDevice,
				collisionCtx._physicalDevice,
				sizeB_bytes,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				&vertexBufferB._buffer,
				&vertexBufferB._gpuMemory);

			VkHelper::CreateBuffer(collisionCtx._logicalDevice, collisionCtx._physicalDevice, sizeIndexB_bytes,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				&indexBufferB._buffer, &indexBufferB._gpuMemory);

			VkHelper::CreateBuffer(collisionCtx._logicalDevice,
				collisionCtx._physicalDevice,
				sizeOutputBytes,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				&outputBuffer._buffer,
				&outputBuffer._gpuMemory);

			// Transfer data to GPU staging buffer and thereafter sync the staging buffer with GPU local memory.
			VkHelper::CopyBufferDataToDeviceMemory(collisionCtx._logicalDevice, collisionCtx._physicalDevice, collisionCtx._commandPool, collisionCtx._queue, vertexBufferA._buffer, dataInputBufferA, sizeA_bytes);
			VkHelper::CopyBufferDataToDeviceMemory(collisionCtx._logicalDevice, collisionCtx._physicalDevice, collisionCtx._commandPool, collisionCtx._queue, indexBufferA._buffer, aIndices.data(), sizeIndexA_bytes);
			VkHelper::CopyBufferDataToDeviceMemory(collisionCtx._logicalDevice, collisionCtx._physicalDevice, collisionCtx._commandPool, collisionCtx._queue, vertexBufferB._buffer, dataInputBufferB, sizeB_bytes);
			VkHelper::CopyBufferDataToDeviceMemory(collisionCtx._logicalDevice, collisionCtx._physicalDevice, collisionCtx._commandPool, collisionCtx._queue, indexBufferB._buffer, bIndices.data(), sizeIndexB_bytes);

			VkBuffer buffers[5] = {
				vertexBufferA._buffer,
				indexBufferA._buffer,
				vertexBufferB._buffer,
				indexBufferB._buffer,
				outputBuffer._buffer
			};
			VkDeviceSize bufferSizes[5] = {
				sizeA_bytes,
				sizeIndexA_bytes,
				sizeB_bytes,
				sizeIndexB_bytes,
				sizeOutputBytes // same size as input for simplicity
			};

			// Create 
			//const char* shaderPath = "C:\\code\\vulkan-compute\\shaders\\Shader.spv";
			auto shaderPath = Paths::ShadersPath() /= L"compute\\CollisionDetection.spv";
			VkPipeline pipeline; VkPipelineLayout layout; VkDescriptorSet descriptorSet;
			if (CreateComputePipeline(buffers, bufferSizes, shaderPath.string().c_str(), collisionCtx, pipeline, layout, descriptorSet) != VK_SUCCESS) {
				std::cout << "Application creation failed." << std::endl;
			}

			auto aTransform = a->_pGameObject->GetWorldSpaceTransform()._matrix;
			auto bTransform = b->_pGameObject->GetWorldSpaceTransform()._matrix;

			if (Dispatch(collisionCtx, pipeline, layout, descriptorSet, workGroupCount, aTransform, bTransform) != VK_SUCCESS) {
				std::cout << "Application run failed." << std::endl;
			}

			auto shaderOutputBufferData = (glm::vec4*)malloc(sizeOutputBytes);

			res = vkGetFenceStatus(collisionCtx._logicalDevice, collisionCtx._queueFence);

			//Transfer data from GPU using staging buffer, if needed
			if (DownloadDataFromGPU(shaderOutputBufferData, sizeOutputBytes, collisionCtx, &outputBuffer._buffer) != VK_SUCCESS)
				std::cout << "Failed downloading data from GPU." << std::endl;

			// Free resources.
			//vkDestroyBuffer(ctx._logicalDevice, inputBuffer._buffer, nullptr);
			//vkFreeMemory(ctx._logicalDevice, inputBuffer._gpuMemory, nullptr);
			//vkDestroyBuffer(ctx._logicalDevice, outputBuffer._buffer, nullptr);
			//vkFreeMemory(ctx._logicalDevice, outputBuffer._gpuMemory, nullptr);

			// Convert GPU results into collision context info
			CollisionContext outCollisionCtx;
			for (int i = 0; i < faceCount; ++i) {
				if (shaderOutputBufferData[i].x == 3.402823466e+38f &&
					shaderOutputBufferData[i].y == 3.402823466e+38f &&
					shaderOutputBufferData[i].z == 3.402823466e+38f &&
					shaderOutputBufferData[i].w == 3.402823466e+38f &&
					shaderOutputBufferData[i + faceCount].x == 3.402823466e+38f &&
					shaderOutputBufferData[i + faceCount].y == 3.402823466e+38f &&
					shaderOutputBufferData[i + faceCount].z == 3.402823466e+38f &&
					shaderOutputBufferData[i + faceCount].w == 3.402823466e+38f)
					continue;
				outCollided = true;
				outCollisionCtx._collidee = shaderOutputBufferData[i].w == 0.0f ? a : b;
				outCollisionCtx._collisionPositions.push_back(shaderOutputBufferData[i]);
				outCollisionCtx._collisionNormals.push_back(shaderOutputBufferData[i + faceCount]);
				outCollisionCtx._collisionObjects.push_back(shaderOutputBufferData[i].w == 0.0f ? a : b);
			}

			vkDestroyBuffer(collisionCtx._logicalDevice, vertexBufferA._buffer, nullptr);
			vkDestroyBuffer(collisionCtx._logicalDevice, indexBufferA._buffer, nullptr);
			vkDestroyBuffer(collisionCtx._logicalDevice, vertexBufferB._buffer, nullptr);
			vkDestroyBuffer(collisionCtx._logicalDevice, indexBufferB._buffer, nullptr);
			vkDestroyBuffer(collisionCtx._logicalDevice, outputBuffer._buffer, nullptr);
			free(shaderOutputBufferData);
			free(dataInputBufferA);
			free(dataInputBufferB);

			if (!outCollided) return outCollisionCtx;

			return outCollisionCtx;
		}
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

		void PhysicsUpdate(VkContext& ctx, VkContext& collisionCtx, EngineContext& eCtx) {
			for (auto& gameObject : _pRootGameObject->_children)
				gameObject->PhysicsUpdate(ctx, collisionCtx, eCtx);
		}

		void Update(VkContext& vkContext) {
			for (auto& light : _pointLights)
				light.Update(vkContext);

			for (auto& gameObject : _pRootGameObject->_children)
				gameObject->Update(vkContext);
		}

		ShaderResources CreateDescriptorSets(VkContext& ctx, std::vector<DescriptorSetLayout>& layouts) {
			for (auto& gameObject : _pRootGameObject->_children) {
				auto gameObjectResources = gameObject->CreateDescriptorSets(ctx, layouts);
				_shaderResources.MergeResources(gameObjectResources);
			}

			for (auto& light : _pointLights) {
				auto lightResources = light.CreateDescriptorSets(ctx, layouts);
				_shaderResources.MergeResources(lightResources);
				light.UpdateShaderResources();
			}

			auto environmentMapResources = _environmentMap.CreateDescriptorSets(ctx, layouts);
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

	ShaderResources GameObject::CreateDescriptorSets(VkContext& ctx, std::vector<DescriptorSetLayout>& layouts) {
		auto descriptorSetID = 1;
		auto globalTransform = GetWorldSpaceTransform();

		// Create a temporary buffer.
		Buffer buffer{};
		auto bufferSizeBytes = sizeof(_localTransform._matrix);
		buffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer._createInfo.size = bufferSizeBytes;
		buffer._createInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		vkCreateBuffer(ctx._logicalDevice, &buffer._createInfo, nullptr, &buffer._buffer);

		// Allocate memory for the buffer.
		VkMemoryRequirements requirements{};
		vkGetBufferMemoryRequirements(ctx._logicalDevice, buffer._buffer, &requirements);
		buffer._gpuMemory = PhysicalDevice::AllocateMemory(ctx._physicalDevice, ctx._logicalDevice, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		// Map memory to the correct GPU and CPU ranges for the buffer.
		vkBindBufferMemory(ctx._logicalDevice, buffer._buffer, buffer._gpuMemory, 0);
		vkMapMemory(ctx._logicalDevice, buffer._gpuMemory, 0, bufferSizeBytes, 0, &buffer._cpuMemory);
		memcpy(buffer._cpuMemory, &globalTransform._matrix, bufferSizeBytes);

		_buffers.push_back(buffer);

		VkDescriptorPool descriptorPool{};
		VkDescriptorPoolSize poolSizes[1] = { VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 } };
		VkDescriptorPoolCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.maxSets = (uint32_t)1;
		createInfo.poolSizeCount = (uint32_t)1;
		createInfo.pPoolSizes = poolSizes;
		vkCreateDescriptorPool(ctx._logicalDevice, &createInfo, nullptr, &descriptorPool);

		// Create the descriptor set.
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = (uint32_t)1;
		allocInfo.pSetLayouts = &layouts[descriptorSetID]._layout;
		VkDescriptorSet descriptorSet;
		vkAllocateDescriptorSets(ctx._logicalDevice, &allocInfo, &descriptorSet);

		// Update the descriptor set's data.
		VkDescriptorBufferInfo bufferInfo{ buffer._buffer, 0, buffer._createInfo.size };
		VkWriteDescriptorSet writeInfo = {};
		writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeInfo.dstSet = descriptorSet;
		writeInfo.descriptorCount = 1;
		writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeInfo.pBufferInfo = &bufferInfo;
		writeInfo.dstBinding = 0;
		vkUpdateDescriptorSets(ctx._logicalDevice, 1, &writeInfo, 0, nullptr);

		auto descriptorSets = std::vector<VkDescriptorSet>{ descriptorSet };
		_shaderResources._data.try_emplace(layouts[descriptorSetID], descriptorSets);

		if (_pMesh) {
			auto meshResources = _pMesh->CreateDescriptorSets(ctx, layouts);
			_shaderResources.MergeResources(meshResources);
		}

		for (auto& child : _children) {
			auto childResources = child->CreateDescriptorSets(ctx, layouts);
		}

		return _shaderResources;
	}

	Transform GameObject::GetWorldSpaceTransform() {
		Transform outTransform;
		GameObject* current = this;
		do {
			outTransform._matrix *= current->_localTransform._matrix;
			current = current->_pParent;
		} while (current->_pParent != nullptr);
		outTransform.SetPosition(glm::vec3(_pParent->_localTransform._matrix * glm::vec4(_localTransform.Position(), 1.0f)));
		return outTransform;
	}

	void GameObject::UpdateShaderResources() {
		auto worldTransform = GetWorldSpaceTransform()._matrix;
		_gameObjectData.transform = worldTransform;
		memcpy(_buffers[0]._cpuMemory, &_gameObjectData, sizeof(_gameObjectData));
	}

	void GameObject::PhysicsUpdate(VkContext& ctx, VkContext& collisionCtx, EngineContext& eCtx) {
		_body.PhysicsUpdate(ctx, collisionCtx, eCtx);
	}

	void GameObject::Update(VkContext& vkContext) {
		if (_pMesh != nullptr) {
			_pMesh->Update(vkContext);
		}

		UpdateShaderResources();

		for (auto& child : _children) {
			child->Update(vkContext);
		}
	}

	void GameObject::Draw(VkPipelineLayout& pipelineLayout, VkCommandBuffer& drawCommandBuffer) {
		if (_pMesh != nullptr) {
			vkCmdBindDescriptorSets(drawCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &_shaderResources[1][0], 0, nullptr);
			_pMesh->Draw(pipelineLayout, drawCommandBuffer);
		}

		for (int i = 0; i < _children.size(); ++i) {
			_children[i]->Draw(pipelineLayout, drawCommandBuffer);
		}
	}

	Mesh::~Mesh()
	{
		// TODO
	}

	// Mesh
	ShaderResources Mesh::CreateDescriptorSets(VkContext& ctx, std::vector<DescriptorSetLayout>& layouts) {
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
		CopyImageToDeviceMemory(ctx._logicalDevice, ctx._physicalDevice, ctx._commandPool, ctx._queue, albedoMap._image, albedoMap._createInfo.extent.width, albedoMap._createInfo.extent.height, albedoMap._createInfo.extent.depth, albedoMap._pData, albedoMap._sizeBytes);
		CopyImageToDeviceMemory(ctx._logicalDevice, ctx._physicalDevice, ctx._commandPool, ctx._queue, albedoMap._image, roughnessMap._createInfo.extent.width, roughnessMap._createInfo.extent.height, roughnessMap._createInfo.extent.depth, roughnessMap._pData, roughnessMap._sizeBytes);
		CopyImageToDeviceMemory(ctx._logicalDevice, ctx._physicalDevice, ctx._commandPool, ctx._queue, albedoMap._image, metalnessMap._createInfo.extent.width, metalnessMap._createInfo.extent.height, metalnessMap._createInfo.extent.depth, metalnessMap._pData, metalnessMap._sizeBytes);

		auto commandBuffer = VkHelper::CreateCommandBuffer(ctx._logicalDevice, ctx._commandPool);
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
		VkHelper::ExecuteCommands(commandBuffer, ctx._queue);

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
		vkCreateDescriptorPool(ctx._logicalDevice, &createInfo, nullptr, &descriptorPool);

		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = (uint32_t)1;
		allocInfo.pSetLayouts = &layouts[descriptorSetID]._layout;
		VkDescriptorSet descriptorSet;
		vkAllocateDescriptorSets(ctx._logicalDevice, &allocInfo, &descriptorSet);

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
		vkUpdateDescriptorSets(ctx._logicalDevice, 1, &writeInfo, 0, nullptr);

		auto descriptorSets = std::vector<VkDescriptorSet>{ descriptorSet };
		_shaderResources._data.try_emplace(layouts[descriptorSetID], descriptorSets);
		return _shaderResources;
	}

	void Mesh::UpdateShaderResources() {
		// TODO
	}

	void Mesh::Update(VkContext& vkContext) {
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
	glm::vec3 RigidBody::CalculateTransmittedForce(const glm::vec3& transmitterPosition, const glm::vec3& force, const glm::vec3& receiverPosition) {
		if (Helpers::IsVectorZero(receiverPosition - transmitterPosition, 0.001f)) return force;
		auto effectiveForce = glm::normalize(receiverPosition - transmitterPosition);
		auto scaleFactor = glm::dot(effectiveForce, force);
		return effectiveForce * scaleFactor;
	}

	glm::vec3 RigidBody::GetCenterOfMass(bool worldSpace) {
		auto worldSpaceTransform = _pGameObject->GetWorldSpaceTransform()._matrix;
		if (_isCenterOfMassOverridden) return worldSpace ? glm::vec3(worldSpaceTransform * glm::vec4(_overriddenCenterOfMassLocalSpace, 1.0f)) : _overriddenCenterOfMassLocalSpace;

		auto& vertices = _pGameObject->_pMesh->_vertices._vertexData;
		int vertexCount = (int)vertices.size();
		float totalX = 0.0f;
		float totalY = 0.0f;
		float totalZ = 0.0f;

		for (int i = 0; i < vertexCount; ++i) {
			totalX += vertices[i]._position.x;
			totalY += vertices[i]._position.y;
			totalZ += vertices[i]._position.z;
		}

		auto averagePos = glm::vec3(totalX / vertexCount, totalY / vertexCount, totalZ / vertexCount);
		return worldSpace ? glm::vec3(worldSpaceTransform * glm::vec4(averagePos, 1.0f)) : averagePos;
	}

	glm::vec3 RigidBody::GetVelocityAtPosition(const glm::vec3& positionWorldSpace) {
		glm::vec3 linearVelocityContributionFromAngularVelocityAtPosition;
		if (!Helpers::IsVectorZero(_angularVelocity)) {
			auto rotationAxis = glm::normalize(_angularVelocity);
			auto worldSpaceCom = GetCenterOfMass(true);
			auto posToCom = worldSpaceCom - positionWorldSpace;
			glm::vec3 directionOfAngularVelocityContributionAtPosition = -glm::normalize(glm::cross(rotationAxis, posToCom));
			linearVelocityContributionFromAngularVelocityAtPosition = directionOfAngularVelocityContributionAtPosition * (glm::length(posToCom) * glm::length(_angularVelocity));
		}
		return linearVelocityContributionFromAngularVelocityAtPosition + _velocity;
	}

	void RigidBody::AddForceAtPosition(const glm::vec3& force, const glm::vec3& pointOfApplication, const float& deltaTimeSeconds, bool isApplicationPointWorldSpace, bool isForceWorldSpace, const bool& ignoreMass) {
		auto worldSpaceTransform = _pGameObject->GetWorldSpaceTransform()._matrix;
		auto worldSpaceForce = isForceWorldSpace ? force : glm::vec3(worldSpaceTransform * glm::vec4(force, 1.0f));

		// First calculate the translation component of the force to apply.
		auto worldSpaceCom = GetCenterOfMass(true);
		auto worldSpacePointOfApplication = isApplicationPointWorldSpace ? pointOfApplication : glm::vec3(worldSpaceTransform * glm::vec4(pointOfApplication, 1.0f));

		glm::vec3 translationForce = CalculateTransmittedForce(worldSpacePointOfApplication, worldSpaceForce, worldSpaceCom);
		//if (ignoreMass) glm::vec3 translationAcceleration = translationForce / _mass;
		glm::vec3 translationDelta = translationForce * deltaTimeSeconds;
		translationDelta.x = _lockTranslationX ? 0.0f : translationDelta.x;
		translationDelta.y = _lockTranslationY ? 0.0f : translationDelta.y;
		translationDelta.z = _lockTranslationZ ? 0.0f : translationDelta.z;
		_velocity += translationDelta;

		// Now calculate the rotation component.
		auto positionToCom = worldSpaceCom - worldSpacePointOfApplication;
		if (Helpers::IsVectorZero(positionToCom, 0.001f)) return;

		auto rotationAxis = -glm::normalize(glm::cross(positionToCom, worldSpaceForce));
		if (glm::isnan(rotationAxis.x) || glm::isnan(rotationAxis.y) || glm::isnan(rotationAxis.x)) return;

		auto comPerpendicularDirection = glm::normalize(glm::cross(positionToCom, rotationAxis));
		auto rotationalForce = comPerpendicularDirection * glm::dot(comPerpendicularDirection, worldSpaceForce);
		auto rotationalInertia = powf(glm::length(positionToCom), 2.0f) * _mass;
		// TODO: better approximation for rotational inertia.

		auto angularAcceleration = glm::cross(rotationalForce, positionToCom) / rotationalInertia;
		AddTorque(angularAcceleration, deltaTimeSeconds, true);
	}

	void RigidBody::AddForce(const glm::vec3& force, const float& deltaTimeSeconds, const bool& ignoreMass) {
		auto translationDelta = ignoreMass ? (force * deltaTimeSeconds) : ((force / _mass) * deltaTimeSeconds);
		translationDelta.x = _lockTranslationX ? 0.0f : translationDelta.x;
		translationDelta.y = _lockTranslationY ? 0.0f : translationDelta.y;
		translationDelta.z = _lockTranslationZ ? 0.0f : translationDelta.z;
		_velocity += translationDelta;
	}

	void RigidBody::AddTorque(const glm::vec3& torqueWorldSpaceAxis, const float& deltaTimeSeconds, const bool& ignoreMass) {
		auto rotationDelta = ignoreMass ? (torqueWorldSpaceAxis * deltaTimeSeconds) : ((torqueWorldSpaceAxis / _mass) * deltaTimeSeconds);
		rotationDelta.x = _lockRotationX ? 0.0f : rotationDelta.x;
		rotationDelta.y = _lockRotationY ? 0.0f : rotationDelta.y;
		rotationDelta.z = _lockRotationZ ? 0.0f : rotationDelta.z;
		_angularVelocity += rotationDelta;
	}

	CollisionContext RigidBody::DetectCollision(RigidBody& other) {
		CollisionContext outCtx;
		outCtx._collidee = &other;
		auto worldSpaceOther = other._pGameObject->GetWorldSpaceTransform();
		auto worldSpaceCurrent = _pGameObject->GetWorldSpaceTransform();
		auto& otherMesh = other._pGameObject->_pMesh;
		auto& mesh = _pGameObject->_pMesh;

		for (int i = 0; i < otherMesh->_faceIndices._indexData.size(); i += 3) {
			for (int j = 0; j < otherMesh->_faceIndices._indexData.size(); j += 3) {

				auto v1Other = glm::vec3(worldSpaceOther._matrix * glm::vec4(otherMesh->_vertices._vertexData[otherMesh->_faceIndices._indexData[i]]._position, 1.0f));
				auto v2Other = glm::vec3(worldSpaceOther._matrix * glm::vec4(otherMesh->_vertices._vertexData[otherMesh->_faceIndices._indexData[i + 1]]._position, 1.0f));
				auto v3Other = glm::vec3(worldSpaceOther._matrix * glm::vec4(otherMesh->_vertices._vertexData[otherMesh->_faceIndices._indexData[i + 2]]._position, 1.0f));

				auto v1 = glm::vec3(worldSpaceCurrent._matrix * glm::vec4(mesh->_vertices._vertexData[otherMesh->_faceIndices._indexData[j]]._position, 1.0f));
				auto v2 = glm::vec3(worldSpaceCurrent._matrix * glm::vec4(mesh->_vertices._vertexData[otherMesh->_faceIndices._indexData[j + 1]]._position, 1.0f));
				auto v3 = glm::vec3(worldSpaceCurrent._matrix * glm::vec4(mesh->_vertices._vertexData[otherMesh->_faceIndices._indexData[j + 2]]._position, 1.0f));

				glm::vec3 intersectionPoint1, intersectionPoint2, intersectionPoint3;
				auto edge1 = v2Other - v1Other;
				auto edge2 = v3Other - v1Other;
				auto edge3 = v2Other - v3Other;
				glm::vec3 normal = -glm::normalize(glm::cross(edge1, edge2));

				// Check collision by testing this body's edges as segments against the other body's face.
				if (IsSegmentIntersectingTriangle(v1, v2 - v1, v1Other, v2Other, v3Other, intersectionPoint1)) { outCtx._collisionPositions.push_back(glm::vec4(intersectionPoint1, 1.0f)); outCtx._collisionNormals.push_back(glm::vec4(normal, 1.0f)); }
				if (IsSegmentIntersectingTriangle(v1, v3 - v1, v1Other, v2Other, v3Other, intersectionPoint2)) { outCtx._collisionPositions.push_back(glm::vec4(intersectionPoint2, 1.0f)); outCtx._collisionNormals.push_back(glm::vec4(normal, 1.0f)); }
				if (IsSegmentIntersectingTriangle(v3, v2 - v3, v1Other, v2Other, v3Other, intersectionPoint3)) { outCtx._collisionPositions.push_back(glm::vec4(intersectionPoint3, 1.0f)); outCtx._collisionNormals.push_back(glm::vec4(normal, 1.0f)); }
				//if (!IsVectorZero(intersectionPoint1) || !IsVectorZero(intersectionPoint2) || !IsVectorZero(intersectionPoint3)) return outCtx;

				// Check collision by testing the other body's edges as segments against the current body's face.
				if (IsSegmentIntersectingTriangle(v1Other, edge1, v1, v2, v3, intersectionPoint1)) { outCtx._collisionPositions.push_back(glm::vec4(intersectionPoint1, 1.0f)); outCtx._collisionNormals.push_back(glm::vec4(normal, 1.0f)); }
				if (IsSegmentIntersectingTriangle(v1Other, edge2, v1, v2, v3, intersectionPoint2)) { outCtx._collisionPositions.push_back(glm::vec4(intersectionPoint2, 1.0f)); outCtx._collisionNormals.push_back(glm::vec4(normal, 1.0f)); }
				if (IsSegmentIntersectingTriangle(v3Other, edge3, v1, v2, v3, intersectionPoint3)) { outCtx._collisionPositions.push_back(glm::vec4(intersectionPoint3, 1.0f)); outCtx._collisionNormals.push_back(glm::vec4(normal, 1.0f)); }
			}
		}

		return outCtx;
	}

	std::vector<GameObject*> RigidBody::GetGameObjects(GameObject* pRoot, std::vector<GameObject*> excludedObjects) {
		std::vector<GameObject*> outGameObjects;
		if (!pRoot->_pMesh) goto searchChildren;
		if (pRoot->_pMesh->_vertices._vertexData.size() < 1) goto searchChildren;
		for (int i = 0; i < excludedObjects.size(); ++i) if (excludedObjects[i] == pRoot) goto searchChildren;
		outGameObjects.push_back(pRoot);

	searchChildren:
		for (int i = 0; i < pRoot->_children.size(); ++i) {
			auto objects = GetGameObjects(pRoot->_children[i], excludedObjects);
			outGameObjects.insert(outGameObjects.end(), objects.begin(), objects.end());
		}
		return outGameObjects;
	}

	std::vector<CollisionContext> RigidBody::DetectCollisions(VkContext& ctx, VkContext& collisionCtx, std::vector<RigidBody*> bodiesToExclude) {
		std::vector<GameObject*> gameObjectsToExclude;
		gameObjectsToExclude.push_back(_pGameObject);
		for (int i = 1; i < gameObjectsToExclude.size(); ++i) gameObjectsToExclude[i] = bodiesToExclude[i - 1]->_pGameObject;
		auto otherGameObjects = GetGameObjects(_pGameObject->_pScene->_pRootGameObject, gameObjectsToExclude);

		std::vector<CollisionContext> outCollisions;
		for (int i = 0; i < otherGameObjects.size(); ++i) {
			if (otherGameObjects[i]->_pMesh->_vertices._vertexData.size() < 1) continue;
			//auto collision = DetectCollision(otherGameObjects[i]->_body);
			//bool hasCollided = collision._collisionPositions.size() > 0;
			bool hasCollided = false;

			//auto start = std::chrono::high_resolution_clock::now();
			auto collision = GpuCollisionDetector::Run(collisionCtx, *this, otherGameObjects[i]->_body, hasCollided);
			//auto end = std::chrono::high_resolution_clock::now();
			//auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
			//std::cout << "Elapsed time: " << elapsed.count() << " seconds" << std::endl;
			if (!hasCollided) continue;
			outCollisions.push_back(collision);
			//if (collision._collisionPositions.size() > 0) outCollisions.push_back(collision);
		}
		return outCollisions;
	}

	void RigidBody::Initialize(GameObject* pGameObject, const float& mass, const bool& overrideCenterOfMass, const glm::vec3& overriddenCenterOfMass) {
		if (!pGameObject) return;
		if (!pGameObject->_pMesh) return;
		if (pGameObject->_pMesh->_vertices._vertexData.size() < 1) return;
		if (mass <= 0.001f) return;

		_pGameObject = pGameObject;
		_mass = mass;
		_isCenterOfMassOverridden = overrideCenterOfMass;
		_overriddenCenterOfMassLocalSpace = overriddenCenterOfMass;
		_isInitialized = true;
	}

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
		float _lastYaw = 0.0f;
		float _lastPitch = 0.0f;
		float _lastRoll = 0.0f;

		float _yaw = 0.0f;
		float _pitch = 0.0f;
		float _roll = 0.0f;

		float _lastScrollY = 0.0f;

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

		ShaderResources CreateDescriptorSets(VkContext& ctx, std::vector<DescriptorSetLayout>& layouts) {
			auto descriptorSetID = 0;

			// Create a temporary buffer.
			Buffer buffer{};
			auto bufferSizeBytes = sizeof(_cameraData);
			buffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer._createInfo.size = bufferSizeBytes;
			buffer._createInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			vkCreateBuffer(ctx._logicalDevice, &buffer._createInfo, nullptr, &buffer._buffer);

			// Allocate memory for the buffer.
			VkMemoryRequirements requirements{};
			vkGetBufferMemoryRequirements(ctx._logicalDevice, buffer._buffer, &requirements);
			buffer._gpuMemory = PhysicalDevice::AllocateMemory(ctx._physicalDevice, ctx._logicalDevice, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

			// Map memory to the correct GPU and CPU ranges for the buffer.
			vkBindBufferMemory(ctx._logicalDevice, buffer._buffer, buffer._gpuMemory, 0);
			vkMapMemory(ctx._logicalDevice, buffer._gpuMemory, 0, bufferSizeBytes, 0, &buffer._cpuMemory);
			memcpy(buffer._cpuMemory, &_cameraData, bufferSizeBytes);

			_buffers.push_back(buffer);

			VkDescriptorPool descriptorPool{};
			VkDescriptorPoolSize poolSizes[1] = { VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 } };
			VkDescriptorPoolCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			createInfo.maxSets = (uint32_t)1;
			createInfo.poolSizeCount = (uint32_t)1;
			createInfo.pPoolSizes = poolSizes;
			vkCreateDescriptorPool(ctx._logicalDevice, &createInfo, nullptr, &descriptorPool);

			VkDescriptorSet descriptorSet = VkHelper::AllocateDescriptorSet(ctx._logicalDevice, descriptorPool, layouts[descriptorSetID]._layout);

			// Update the descriptor set's data with the environment map's image.
			VkDescriptorBufferInfo bufferInfo{ buffer._buffer, 0, buffer._createInfo.size };
			VkWriteDescriptorSet writeInfo = {};
			writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeInfo.dstSet = descriptorSet;
			writeInfo.descriptorCount = 1;
			writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writeInfo.pBufferInfo = &bufferInfo;
			writeInfo.dstBinding = 0;
			vkUpdateDescriptorSets(ctx._logicalDevice, 1, &writeInfo, 0, nullptr);

			auto descriptorSets = std::vector<VkDescriptorSet>{ descriptorSet };
			_shaderResources._data.try_emplace(layouts[descriptorSetID], descriptorSets);
			return _shaderResources;
		}

		void UpdateShaderResources() {
			auto& globalSettings = GlobalSettings::Instance();

			_cameraData.worldToCamera = _view._matrix;
			_cameraData.tanHalfHorizontalFov = tan(glm::radians(_horizontalFov / 2.0f));
			_cameraData.aspectRatio = Helpers::Convert<uint32_t, float>(globalSettings._windowWidth) / Helpers::Convert<uint32_t, float>(globalSettings._windowHeight);
			_cameraData.nearClipDistance = _nearClippingDistance;
			_cameraData.farClipDistance = _farClippingDistance;
			_cameraData.transform = _localTransform.Position();

			memcpy(_buffers[0]._cpuMemory, &_cameraData, sizeof(_cameraData));
		}

		void Update(VkContext& vkContext, KeyboardMouse& input) {
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

	/**
	 * @brief All the high-level information needed by the engine to perform user-related tasks such as managing cameras and scenes.
	 */
	struct EngineContext {
		Scene _scene;
		Camera _mainCamera;
		Time& _time = Time::Instance();
		KeyboardMouse& _input = KeyboardMouse::Instance();
		GlobalSettings& _globalSettings = Engine::GlobalSettings::Instance();
	};

	//void CalculateTransmittedForceRecursively(RigidBody* transmitter, RigidBody* receiver, glm::vec3 transmitterVelocity, glm::vec3& collisionPosition, glm::vec3& collisionNormal, glm::vec3& outForce, std::vector<RigidBody*> consideredBodies) {
	//	//if (transmitter->_pGameObject->_name == "StationaryCube" && receiver->_pGameObject->_name == "FreeCube") DebugBreak();
	//	if (receiver->IsRotationLocked() && receiver->IsTranslationLocked()) { outForce = glm::vec3(0.0f, 0.0f, 0.0f); return; }
	//	auto normalizedForce = outForce; glm::normalize(normalizedForce);
	//	//outForce = RigidBody::CalculateTransmittedForce(collisionPosition, normalizedForce * glm::dot(transmitter->_mass * transmitterVelocity, collisionNormal), receiver->GetCenterOfMass(true));
	//	auto scaleFactor = abs(glm::dot(transmitter->_mass * transmitterVelocity, collisionNormal));
	//	if (scaleFactor > 0.0f) outForce *= scaleFactor;
	//	consideredBodies.push_back(receiver);
	//	auto collisions = receiver->DetectCollisions(consideredBodies);
	//	for (int i = 0; i < collisions.size(); ++i) {
	//		collisions[i].CalculateAverages();
	//		CalculateTransmittedForceRecursively(receiver, collisions[i]._collidee, receiver->GetVelocityAtPosition(collisions[i]._averagePosition), collisions[i]._averagePosition, collisions[i]._averageNormal, outForce, consideredBodies);
	//		consideredBodies.push_back(collisions[i]._collidee);
	//	}
	//}

	void RigidBody::PhysicsUpdate(VkContext& ctx, VkContext& collisionCtx, EngineContext& eCtx) {
		float deltaTimeSeconds = (float)eCtx._time._physicsDeltaTime * 0.001f;
		if (IsRotationLocked() && IsTranslationLocked()) return;
		if (_isColliding && glm::length(_velocity) < 0.1f) return;
		if (_isAffectedByGravity) AddForce(gGravity, deltaTimeSeconds, true);

		// Approximate air resistance/rotational friction.
		auto airFrictionCoefficient = 0.09f;
		auto frictionMultiplier = -airFrictionCoefficient / powf(_mass, 2.0f);
		AddForce(_velocity * frictionMultiplier, deltaTimeSeconds);
		AddTorque(_angularVelocity * frictionMultiplier, deltaTimeSeconds);
		auto wscom = GetCenterOfMass(true);

		// Resolve collisions.
		do {
			if (!_isCollidable) break;

			auto collisions = DetectCollisions(ctx, collisionCtx);

			// Detect continuous collision
			bool isOverCollisionThreshold = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - _lastTimeCollided).count() > _continuousCollisionThresholdMilliseconds;
			if (collisions.size() > 0) {
				if (isOverCollisionThreshold) _isColliding = true;
				_lastTimeCollided = std::chrono::high_resolution_clock::now();
			}
			else if (isOverCollisionThreshold) { _isColliding = false; break; }

			for (int i = 0; i < collisions.size(); ++i) {
				auto physicsDeltaTime = (float)eCtx._time._physicsDeltaTime;
				collisions[i].CalculateAverages();
				glm::vec3 averageCollisionPosition = glm::vec3(collisions[i]._averagePosition);
				glm::vec3 averageCollisionNormal = glm::vec3(collisions[i]._averageNormal);

				auto velocityAtPosition = GetVelocityAtPosition(averageCollisionPosition);
				auto velocityLength = glm::length(velocityAtPosition);
				auto velocityDirection = velocityAtPosition; glm::normalize(velocityDirection);

				// Clamp angular velocity
				if (glm::length(_angularVelocity) > 5.0f) {
					glm::vec3 nrm; nrm.x = _angularVelocity.x; nrm.y = _angularVelocity.y; nrm.z = _angularVelocity.z;
					nrm = glm::normalize(nrm);
					_angularVelocity = nrm * 5.0f;
				}

				if (velocityLength < glm::length(collisions[i]._collidee->GetVelocityAtPosition(averageCollisionPosition))) continue;
				//if () continue;

				CollisionContext collisionCtx = collisions[i];
				// Correct the body's position by moving the object backwards along the collision normal until there are no more collisions, to reduce object penetration
				// and make the collision detection seem more accurate.
				auto translationDelta = velocityLength * physicsDeltaTime * 0.001f;
				for (int j = 0; j < 20; ++j) {
					if (glm::dot(velocityAtPosition, averageCollisionNormal) > 0) break;
					//_pGameObject->_localTransform.Translate(averageCollisionNormal * translationDelta);
					//collisionCtx = DetectCollision(*collisionCtx._collidee);
					auto f = (averageCollisionNormal * glm::dot(-velocityAtPosition, averageCollisionNormal)) + (averageCollisionNormal * 0.2f);
					AddForceAtPosition(f, averageCollisionPosition, 1.0f, true, true, true);
					collisions[i]._collidee->AddForceAtPosition(-f, averageCollisionPosition, 1.0f);
					velocityAtPosition = GetVelocityAtPosition(averageCollisionPosition);
				}

				auto frictionForceDirection = glm::cross(glm::cross(velocityAtPosition, averageCollisionNormal), averageCollisionNormal);
				if (!Helpers::IsVectorZero(frictionForceDirection)) frictionForceDirection = glm::normalize(frictionForceDirection);

				auto frictionComponent = !Helpers::IsVectorZero(frictionForceDirection)
					? frictionForceDirection * (glm::dot(velocityAtPosition, frictionForceDirection) * _mass * _friction) * deltaTimeSeconds
					: glm::vec3(0.0f, 0.0f, 0.0f);
				AddForceAtPosition(-frictionComponent * 4.0f, averageCollisionPosition, 1.0f);
			}
		} while (false);

		_pGameObject->_localTransform.RotateAroundPosition(wscom, glm::normalize(_angularVelocity), glm::length(_angularVelocity) * deltaTimeSeconds);
		_pGameObject->_localTransform.Translate(_velocity * deltaTimeSeconds);

		std::cout << "Physics delta time: " << eCtx._time._physicsDeltaTime << "\n";
	}

	bool RigidBody::IsRotationLocked() { return _lockRotationX && _lockRotationY && _lockRotationZ; }

	bool RigidBody::IsTranslationLocked() { return _lockTranslationX && _lockTranslationY && _lockTranslationZ; }

	void RigidBody::LockRotation() { _lockRotationX = true; _lockRotationY = true; _lockRotationZ = true; }

	void RigidBody::LockTranslation() { _lockTranslationX = true; _lockTranslationY = true; _lockTranslationZ = true; }

	void RigidBody::UnlockRotation() { _lockRotationX = false; _lockRotationY = false; _lockRotationZ = false; }

	void RigidBody::UnlockTranslation() { _lockTranslationX = false; _lockTranslationY = false; _lockTranslationZ = false; }

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

		static Mesh* ProcessMesh(tinygltf::Mesh& gltfMesh, tinygltf::Model& gltfScene, Scene& scene, Transform localTransform, VkContext& ctx) {
			if (gltfMesh.primitives.size() < 1) return nullptr;

			auto& gltfPrimitive = gltfMesh.primitives[0];
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
				v._position = glm::vec3(localTransform._matrix * glm::vec4(vertexPositions[i], 1.0f));
				v._normal = vertexNormals[i];
				v._uvCoord = uvCoords0[i];
				vertices[i] = v;
			}

			// Copy vertices to the GPU.
			mesh->CreateVertexBuffer(ctx, vertices);

			// Copy face indices to the GPU.
			mesh->CreateIndexBuffer(ctx, faceIndices);

			return mesh;
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

			if (sc.size() == 3) {
				outTransform.SetScale(glm::vec3{ sc[0], sc[1], sc[2] });
			}

			return outTransform;
		}

		struct Node {
			int gltfSceneIndex;
			std::string name;
			Node* parent;
			std::vector<Node*> children;
			GameObject* pGameObject;
		};

		static GameObject* FindGameObject(Node* rootNode, std::string name) {
			if (rootNode->pGameObject->_name == name) return rootNode->pGameObject;
			for (int i = 0; i < rootNode->children.size(); ++i) {
				auto childSearchResult = FindGameObject(rootNode->children[i], name);
				if (childSearchResult) return childSearchResult;
			}
			return nullptr;
		}

		static bool GetBoolProperty(tinygltf::Node& node, const std::string& propertyName) {
			auto p = node.extras.Get(propertyName);
			if (p.IsBool()) return p.Get<bool>();
			return false;
		}

		static double GetNumberProperty(tinygltf::Node& node, const std::string& propertyName) {
			auto p = node.extras.Get(propertyName);
			if (p.IsReal() || p.IsNumber() || p.IsInt()) return (double)p.GetNumberAsDouble();
			return 0.0;
		}

		static void ProcessGameConfig(Node* rootNode, Node* currentNode, tinygltf::Model& gltfScene) {
			do {
				if (currentNode->gltfSceneIndex < 0) break;
				auto& gltfNode = gltfScene.nodes[currentNode->gltfSceneIndex];
				if (gltfNode.extras.Keys().size() < 1) break;

				auto collisionMeshProp = gltfNode.extras.Get("CollisionMeshName");
				std::string collisionMeshName = "";
				if (collisionMeshProp.IsString()) collisionMeshName = collisionMeshProp.Get<std::string>();
				if (collisionMeshName == "") break;

				auto gameObject = FindGameObject(rootNode, collisionMeshName);
				if (!gameObject) break;

				std::vector<glm::vec4> v;
				std::vector<unsigned int> fi;
				for (int i = 0; i < gameObject->_pMesh->_vertices._vertexData.size(); ++i) v.push_back(glm::vec4(gameObject->_pMesh->_vertices._vertexData[i]._position, 0.0f));
				for (int i = 0; i < gameObject->_pMesh->_faceIndices._indexData.size(); ++i) fi.push_back(gameObject->_pMesh->_faceIndices._indexData[i]);
				/*if (gameObject->_name != currentNode->name) {
					delete(gameObject->_pMesh);
					gameObject->_pMesh = nullptr;
				}*/
				currentNode->pGameObject->_body.Initialize(currentNode->pGameObject);

				// Additional init
				currentNode->pGameObject->_body._friction = (float)GetNumberProperty(gltfNode, "Friction");
				currentNode->pGameObject->_body._mass = (float)GetNumberProperty(gltfNode, "Mass");
				currentNode->pGameObject->_body._isAffectedByGravity = GetBoolProperty(gltfNode, "EnableGravity");
				currentNode->pGameObject->_body._isCollidable = GetBoolProperty(gltfNode, "IsCollidable");
				currentNode->pGameObject->_body._lockRotationX = GetBoolProperty(gltfNode, "LockRotationX");
				currentNode->pGameObject->_body._lockRotationY = GetBoolProperty(gltfNode, "LockRotationY");
				currentNode->pGameObject->_body._lockRotationZ = GetBoolProperty(gltfNode, "LockRotationZ");
				currentNode->pGameObject->_body._lockTranslationX = GetBoolProperty(gltfNode, "LockTranslationX");
				currentNode->pGameObject->_body._lockTranslationY = GetBoolProperty(gltfNode, "LockTranslationY");
				currentNode->pGameObject->_body._lockTranslationZ = GetBoolProperty(gltfNode, "LockTranslationZ");
			} while (false);

			for (int i = 0; i < currentNode->children.size(); ++i) {
				ProcessGameConfig(rootNode, currentNode->children[i], gltfScene);
			}
		}

		static GameObject* ProcessNode(Node* node, tinygltf::Model& gltfScene, Scene& scene, VkContext& ctx) {
			auto& gltfNode = gltfScene.nodes[node->gltfSceneIndex];
			auto gameObject = new GameObject(gltfNode.name, &scene);
			auto gltfNodeTransform = GetGltfNodeTransform(gltfNode);
			gameObject->_localTransform = glm::mat4(1.0f);

			if (gltfNode.mesh < 0) return gameObject;

			auto gltfMesh = gltfScene.meshes[gltfNode.mesh];
			gameObject->_pMesh = ProcessMesh(gltfMesh, gltfScene, scene, gltfNodeTransform, ctx);
			gameObject->_pMesh->_pGameObject = gameObject;

			return gameObject;
		}

		static GameObject* ProcessNodeHierarchy(Node* root, tinygltf::Model& gltfScene, Scene& scene, VkContext& ctx) {
			GameObject* outGameObject;
			if (root->gltfSceneIndex >= 0) outGameObject = ProcessNode(root, gltfScene, scene, ctx);
			else outGameObject = new GameObject("Root", &scene);
			root->pGameObject = outGameObject;

			for (int i = 0; i < root->children.size(); ++i) {
				auto child = ProcessNodeHierarchy(root->children[i], gltfScene, scene, ctx);
				child->_pParent = outGameObject;
				outGameObject->_children.push_back(child);
			}
			return outGameObject;
		}

		static Node* FindExisting(Node* parent, int indexToFind) {
			if (parent->gltfSceneIndex == indexToFind) return parent;
			for (int i = 0; i < parent->children.size(); ++i) {
				Node* found = FindExisting(parent->children[i], indexToFind);
				if (found != nullptr) return found;
			}
			return nullptr;
		}

		static void RemoveExisting(Node* parent, Node* toRemove) {
			if (parent->parent != nullptr) {
				auto nodeToRemove = std::find_if(parent->parent->children.begin(), parent->parent->children.end(), [toRemove](Node* n) {return n->gltfSceneIndex == toRemove->gltfSceneIndex; });
				if (nodeToRemove != parent->parent->children.end())
					parent->parent->children.erase(nodeToRemove);
			}

			for (int i = 0; i < parent->children.size(); ++i)
				RemoveExisting(parent->children[i], toRemove);
		}

		static Node* CreateNodeHierarchy(tinygltf::Model& gltfScene) {
			Node* root = new Node{ -1, "Root", nullptr, {} };

			for (int i = 0; i < gltfScene.nodes.size(); ++i) {
				auto existing = FindExisting(root, i);
				if (existing == nullptr) existing = new Node{ i, gltfScene.nodes[i].name, root, {} };

				for (int j = 0; j < gltfScene.nodes[i].children.size(); ++j) {
					auto childIndex = gltfScene.nodes[i].children[j];
					auto existingChild = FindExisting(root, childIndex);
					if (existingChild == nullptr)
						existingChild = new Node{ childIndex, gltfScene.nodes[childIndex].name, existing, {} };
					else RemoveExisting(root, existingChild);

					existing->children.push_back(existingChild);
				}
				root->children.push_back(existing);
			}

			return root;
		}

		static void DestroyNodeHierarchy(Node* root) {
			for (int i = 0; i < root->children.size(); ++i)
				DestroyNodeHierarchy(root->children[i]);
			delete(root);
			root = nullptr;
		}

		static Scene LoadFile(std::filesystem::path filePath, VkContext& ctx) {
			auto s = new Scene(ctx._logicalDevice, ctx._physicalDevice);
			auto& scene = *s;
			scene._pointLights.push_back(PointLight("DefaultLight"));

			tinygltf::Model gltfScene;
			tinygltf::TinyGLTF loader;
			std::string err;
			std::string warn;

			bool ret = loader.LoadBinaryFromFile(&gltfScene, &err, &warn, filePath.string());
			std::cout << warn << std::endl;
			std::cout << err << std::endl;

			auto materials = LoadMaterials(ctx._logicalDevice, ctx._physicalDevice, gltfScene);
			scene._materials.insert(scene._materials.end(), materials.begin(), materials.end());

			// Creates a hierarchy of nodes from the flat list of nodes that tinygltf's loader filled.
			// This is done because the transforms of each node are relative to the parent, and having a tree-like structure makes it much easier to apply transforms hierarchically.
			Node* rootNode = CreateNodeHierarchy(gltfScene);
			scene._pRootGameObject = ProcessNodeHierarchy(rootNode, gltfScene, scene, ctx);
			ProcessGameConfig(rootNode, rootNode, gltfScene);
			DestroyNodeHierarchy(rootNode);
			rootNode = nullptr;

			return scene;
		}
	};

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

	void Cleanup(bool fullClean) {
		nk_glfw3_shutdown();
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

	static void OnWindowResized(GLFWwindow* window, int width, int height) {
		windowResized = true;
		if (width == 0 && height == 0) { windowMinimized = true; return; }
		windowMinimized = false;
		GlobalSettings::Instance()._windowWidth = width;
		GlobalSettings::Instance()._windowHeight = height;
	}

	//void DestroyRenderPass() {
	//	//_renderPass._depthImage.Destroy();
	//	/*for (auto image : _renderPass.colorImages) {
	//		image.Destroy();
	//	}*/

	//	vkDestroyRenderPass(_logicalDevice, _renderPass._handle, nullptr);
	//}

	//void DestroySwapchain() {
	//	for (size_t i = 0; i < _swapchain._frameBuffers.size(); i++) {
	//		vkDestroyFramebuffer(_logicalDevice, _swapchain._frameBuffers[i], nullptr);
	//	}
	//}

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

			if (!layerFound) return false;
		}
		return true;
	}

	void CreateDebugCallback(VkContext& ctx, GlobalSettings& settings) {
		if (!settings._enableValidationLayers) return;
		VkDebugReportCallbackCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		createInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)DebugCallback;
		createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		PFN_vkCreateDebugReportCallbackEXT createDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(ctx._instance, "vkCreateDebugReportCallbackEXT");
		createDebugReportCallback(ctx._instance, &createInfo, nullptr, &ctx._callback);
	}

	Scene LoadScene(VkContext& ctx) {
		//auto scenePath = Paths::ModelsPath() /= "MaterialSphere.glb";
		auto scenePath = Paths::ModelsPath() /= "cubes.glb";
		//auto scenePath = Paths::ModelsPath() /= "directions.glb";
		//auto scenePath = Paths::ModelsPath() /= "f.glb";
		//auto scenePath = Paths::ModelsPath() /= "fr.glb";
		//auto scenePath = Paths::ModelsPath() /= "mp5k.glb";
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
		return SceneLoader::LoadFile(scenePath, ctx);
	}

	void LoadEnvironmentMap(VkContext& ctx, EngineContext& eCtx) {
		eCtx._scene._environmentMap = CubicalEnvironmentMap(ctx._physicalDevice, ctx._logicalDevice);
		//eCtx._scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "Waterfall.hdr");
		//eCtx._scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "MountainsClearSky.hdr");
		eCtx._scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "BlueSky.hdr");
		//_scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "Debug.png");
		//eCtx._scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "ModernBuilding.hdr");
		//_scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "Workshop.png");
		//eCtx._scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "Workshop.hdr");
		//_scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "garden.hdr");
		//_scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "ItalianFlag.png");
		//_scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "TestPng.png");
		//_scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "EnvMap.png");
		//_scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "texture.jpg");
		//_scene._environmentMap.LoadFromSphericalHDRI(Paths::TexturesPath() /= "Test1.png");

		eCtx._scene._environmentMap.CreateImage(ctx._logicalDevice, ctx._physicalDevice, ctx._commandPool, ctx._queue);
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

	void CreateGraphicsPipelines(VkContext& ctx, VkRenderContext& rCtx) {
		// Environment map pipeline to draw it as a skybox.
		{
			auto vertPath = Paths::ShadersPath() / std::filesystem::path("graphics\\EnvMapVertShader.spv");
			auto fragPath = Paths::ShadersPath() / std::filesystem::path("graphics\\EnvMapFragShader.spv");
			VkShaderModule vertexShaderModule = VkHelper::CreateShaderModule(ctx._logicalDevice, vertPath.string().c_str());
			VkShaderModule fragmentShaderModule = VkHelper::CreateShaderModule(ctx._logicalDevice, fragPath.string().c_str());

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
			VkVertexInputBindingDescription vertexBindingDescription;
			vertexBindingDescription.binding = 0;
			vertexBindingDescription.stride = sizeof(float) * 3;
			vertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			// Describe how the shader should read vertex attributes when getting a vertex from the vertex buffer.
			// Object-space positions.
			VkVertexInputAttributeDescription vertexAttributeDescriptions{};
			vertexAttributeDescriptions.location = 0;
			vertexAttributeDescriptions.binding = 0;
			vertexAttributeDescriptions.format = VK_FORMAT_R32G32B32_SFLOAT;
			vertexAttributeDescriptions.offset = 0;

			// Describe vertex input.
			VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
			vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
			vertexInputCreateInfo.pVertexBindingDescriptions = &vertexBindingDescription;
			vertexInputCreateInfo.vertexAttributeDescriptionCount = (uint32_t)1;
			vertexInputCreateInfo.pVertexAttributeDescriptions = &vertexAttributeDescriptions;

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
			viewport.width = Helpers::Convert<uint32_t, float>(rCtx._swapchain._framebufferSize.width);
			viewport.height = Helpers::Convert<uint32_t, float>(rCtx._swapchain._framebufferSize.height);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			VkRect2D scissor = {};
			scissor.offset.x = 0;
			scissor.offset.y = 0;
			scissor.extent.width = rCtx._swapchain._framebufferSize.width;
			scissor.extent.height = rCtx._swapchain._framebufferSize.height;

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
			rasterizationCreateInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
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
			depthStencilCreateInfo.depthWriteEnable = VK_FALSE;
			depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
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
			pipelineCreateInfo.layout = rCtx._envMapPipeline._layout;
			pipelineCreateInfo.renderPass = rCtx._renderPass._handle;
			pipelineCreateInfo.subpass = 0;
			pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
			pipelineCreateInfo.basePipelineIndex = -1;

			CheckResult(vkCreateGraphicsPipelines(ctx._logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &rCtx._envMapPipeline._handle));
			vkDestroyShaderModule(ctx._logicalDevice, vertexShaderModule, nullptr);
			vkDestroyShaderModule(ctx._logicalDevice, fragmentShaderModule, nullptr);
		}

		// 3D scene pipeline.
		{
			VkShaderModule vertexShaderModule = VkHelper::CreateShaderModule(ctx._logicalDevice, Paths::VertexShaderPath().string().c_str());
			VkShaderModule fragmentShaderModule = VkHelper::CreateShaderModule(ctx._logicalDevice, Paths::FragmentShaderPath().string().c_str());

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
			VkVertexInputBindingDescription vertexBindingDescription;
			vertexBindingDescription.binding = 0;
			vertexBindingDescription.stride = sizeof(Vertex);
			vertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			// Describe how the shader should read vertex attributes when getting a vertex from the vertex buffer.
			// Object-space positions.
			VkVertexInputAttributeDescription vertexAttributeDescriptions[3];
			vertexAttributeDescriptions[0].location = 0;
			vertexAttributeDescriptions[0].binding = 0;
			vertexAttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			vertexAttributeDescriptions[0].offset = (uint32_t)Vertex::OffsetOf(Vertex::AttributeType::Position);

			// Normals.
			vertexAttributeDescriptions[1].location = 1;
			vertexAttributeDescriptions[1].binding = 0;
			vertexAttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			vertexAttributeDescriptions[1].offset = (uint32_t)Vertex::OffsetOf(Vertex::AttributeType::Normal);

			// UV coordinates.
			vertexAttributeDescriptions[2].location = 2;
			vertexAttributeDescriptions[2].binding = 0;
			vertexAttributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
			vertexAttributeDescriptions[2].offset = (uint32_t)Vertex::OffsetOf(Vertex::AttributeType::UV);

			// Describe vertex input.
			VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
			vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
			vertexInputCreateInfo.pVertexBindingDescriptions = &vertexBindingDescription;
			vertexInputCreateInfo.vertexAttributeDescriptionCount = (uint32_t)3;
			vertexInputCreateInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions;

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
			viewport.width = Helpers::Convert<uint32_t, float>(rCtx._swapchain._framebufferSize.width);
			viewport.height = Helpers::Convert<uint32_t, float>(rCtx._swapchain._framebufferSize.height);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			VkRect2D scissor = {};
			scissor.offset.x = 0;
			scissor.offset.y = 0;
			scissor.extent.width = rCtx._swapchain._framebufferSize.width;
			scissor.extent.height = rCtx._swapchain._framebufferSize.height;

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
			pipelineCreateInfo.layout = rCtx._scenePipeline._layout;
			pipelineCreateInfo.renderPass = rCtx._renderPass._handle;
			pipelineCreateInfo.subpass = 0;
			pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
			pipelineCreateInfo.basePipelineIndex = -1;

			CheckResult(vkCreateGraphicsPipelines(ctx._logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &rCtx._scenePipeline._handle));
			vkDestroyShaderModule(ctx._logicalDevice, vertexShaderModule, nullptr);
			vkDestroyShaderModule(ctx._logicalDevice, fragmentShaderModule, nullptr);
		}

		// UI pipeline.
		{
			auto vertPath = Paths::ShadersPath() / std::filesystem::path("graphics\\NuklearUIVertexShader.spv");
			auto fragPath = Paths::ShadersPath() / std::filesystem::path("graphics\\NuklearUIFragmentShader.spv");
			VkShaderModule vertShaderModule = VkHelper::CreateShaderModule(ctx._logicalDevice, vertPath.string().c_str());
			VkShaderModule fragShaderModule = VkHelper::CreateShaderModule(ctx._logicalDevice, fragPath.string().c_str());

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

			VkPipelineShaderStageCreateInfo shader_stages[2]{ vert_shader_stage_info, frag_shader_stage_info };

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
			viewport.width = (float)rCtx._swapchain._framebufferSize.width;
			viewport.height = (float)rCtx._swapchain._framebufferSize.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			scissor.extent.width = rCtx._swapchain._framebufferSize.width;
			scissor.extent.height = rCtx._swapchain._framebufferSize.height;

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
			pipelineInfo.layout = rCtx._uiPipeline._layout;
			pipelineInfo.renderPass = rCtx._renderPass._handle;
			pipelineInfo.subpass = 1;
			pipelineInfo.basePipelineHandle = NULL;

			CheckResult(vkCreateGraphicsPipelines(ctx._logicalDevice, NULL, 1, &pipelineInfo, NULL, &rCtx._uiPipeline._handle));

			if (fragShaderModule) vkDestroyShaderModule(ctx._logicalDevice, fragShaderModule, NULL);
			if (vertShaderModule) vkDestroyShaderModule(ctx._logicalDevice, vertShaderModule, NULL);
		}
	}

	std::vector<DescriptorSetLayout> CreateSceneDescriptorSetLayouts(VkContext& ctx, Scene scene) {
		VkDescriptorSetLayout cameraLayout;
		VkDescriptorSetLayout gameObjectLayout;
		VkDescriptorSetLayout meshLayout;
		VkDescriptorSetLayout lightLayout;
		VkDescriptorSetLayout envMapLayout;

		{
			VkDescriptorSetLayoutBinding bindings[1] = { VkDescriptorSetLayoutBinding { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr } };
			VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
			layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutCreateInfo.bindingCount = 1;
			layoutCreateInfo.pBindings = bindings;
			vkCreateDescriptorSetLayout(ctx._logicalDevice, &layoutCreateInfo, nullptr, &cameraLayout);
		}

		{
			VkDescriptorSetLayoutBinding bindings[1] = { VkDescriptorSetLayoutBinding { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr } };
			VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
			layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutCreateInfo.bindingCount = 1;
			layoutCreateInfo.pBindings = bindings;
			vkCreateDescriptorSetLayout(ctx._logicalDevice, &layoutCreateInfo, nullptr, &gameObjectLayout);
		}

		{
			VkDescriptorSetLayoutBinding bindings[1] = { VkDescriptorSetLayoutBinding { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr } };
			VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
			layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutCreateInfo.bindingCount = 1;
			layoutCreateInfo.pBindings = bindings;
			vkCreateDescriptorSetLayout(ctx._logicalDevice, &layoutCreateInfo, nullptr, &lightLayout);
		}

		{
			VkDescriptorSetLayoutBinding bindings[3];
			bindings[0] = { VkDescriptorSetLayoutBinding {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &scene._materials[0]._albedo._sampler} };
			bindings[1] = { VkDescriptorSetLayoutBinding {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &scene._materials[0]._roughness._sampler} };
			bindings[2] = { VkDescriptorSetLayoutBinding {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &scene._materials[0]._metalness._sampler} };
			VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
			layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutCreateInfo.bindingCount = 3;
			layoutCreateInfo.pBindings = bindings;
			vkCreateDescriptorSetLayout(ctx._logicalDevice, &layoutCreateInfo, nullptr, &meshLayout);
		}

		{
			VkDescriptorSetLayoutBinding bindings[1];
			bindings[0] = { VkDescriptorSetLayoutBinding { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &scene._environmentMap._cubeMapImage._sampler } };
			VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
			layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutCreateInfo.bindingCount = 1;
			layoutCreateInfo.pBindings = bindings;
			vkCreateDescriptorSetLayout(ctx._logicalDevice, &layoutCreateInfo, nullptr, &envMapLayout);
		}

		return {
			DescriptorSetLayout{ "cameraLayout", 0, cameraLayout },
			DescriptorSetLayout{ "gameObjectLayout", 1, gameObjectLayout },
			DescriptorSetLayout{ "lightLayout", 2, lightLayout },
			DescriptorSetLayout{ "meshLayout", 3, meshLayout },
			DescriptorSetLayout{ "envMapLayout", 4, envMapLayout }
		};
	}

	VkPipelineLayout CreateScenePipelineLayout(VkContext& ctx, std::vector<DescriptorSetLayout>& descriptorSetLayouts) {
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
		CheckResult(vkCreatePipelineLayout(ctx._logicalDevice, &pipelineLayoutCreateInfo, nullptr, &outLayout));
		return outLayout;
	}

	void CreateSceneShaderResources(VkContext& ctx, VkRenderContext& rCtx, EngineContext& eCtx, std::vector<DescriptorSetLayout>& descriptorSetLayouts) {
		auto& shaderResources = rCtx._scenePipeline._shaderResources;
		auto cameraResources = eCtx._mainCamera.CreateDescriptorSets(ctx, descriptorSetLayouts);
		shaderResources.MergeResources(cameraResources);
		eCtx._mainCamera.UpdateShaderResources();

		auto sceneResources = eCtx._scene.CreateDescriptorSets(ctx, descriptorSetLayouts);

		VkDescriptorSetLayout envMapPipelineDescriptorSetLayouts[2]{ descriptorSetLayouts[0]._layout, descriptorSetLayouts[4]._layout };
		VkPipelineLayoutCreateInfo layoutCreateInfo{};
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutCreateInfo.setLayoutCount = 2;
		layoutCreateInfo.pSetLayouts = envMapPipelineDescriptorSetLayouts;
		vkCreatePipelineLayout(ctx._logicalDevice, &layoutCreateInfo, nullptr, &rCtx._envMapPipeline._layout);
		eCtx._scene._environmentMap.CreateVertexBuffer(ctx);
		eCtx._scene._environmentMap.CreateIndexBuffer(ctx);

		shaderResources.MergeResources(sceneResources);
		eCtx._scene.UpdateShaderResources();
	}

	void CreateRenderingResources(VkContext& ctx, EngineContext& eCtx, VkRenderContext* outRenderCtx) {
		// Create the swapchain.
		// Get physical device capabilities for the window surface.
		VkSurfaceCapabilitiesKHR surfaceCapabilities = PhysicalDevice::GetSurfaceCapabilities(ctx._physicalDevice, ctx._windowSurface);
		std::vector<VkSurfaceFormatKHR> surfaceFormats = PhysicalDevice::GetSupportedFormatsForSurface(ctx._physicalDevice, ctx._windowSurface);
		std::vector<VkPresentModeKHR> presentModes = PhysicalDevice::GetSupportedPresentModesForSurface(ctx._physicalDevice, ctx._windowSurface);

		// Determine number of images for swapchain.
		uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
		if (surfaceCapabilities.maxImageCount != 0 && imageCount > surfaceCapabilities.maxImageCount) {
			imageCount = surfaceCapabilities.maxImageCount;
		}

		VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(surfaceFormats);
		outRenderCtx->_swapchain._framebufferSize = ChooseFramebufferSize(surfaceCapabilities);

		// Determine transformation to use (preferring no transform).
		VkSurfaceTransformFlagBitsKHR surfaceTransform;
		if (surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) surfaceTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		else surfaceTransform = surfaceCapabilities.currentTransform;

		// Choose presentation mode (preferring MAILBOX ~= triple buffering).
		VkPresentModeKHR presentMode = ChoosePresentMode(presentModes);

		// Finally, create the swap chain.
		VkSwapchainCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = ctx._windowSurface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = outRenderCtx->_swapchain._framebufferSize;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
		createInfo.preTransform = surfaceTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = outRenderCtx->_swapchain._oldSwapchainHandle;

		CheckResult(vkCreateSwapchainKHR(ctx._logicalDevice, &createInfo, nullptr, &outRenderCtx->_swapchain._handle));

		if (outRenderCtx->_swapchain._oldSwapchainHandle != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(ctx._logicalDevice, outRenderCtx->_swapchain._oldSwapchainHandle, nullptr);
		}

		outRenderCtx->_swapchain._oldSwapchainHandle = outRenderCtx->_swapchain._handle;
		outRenderCtx->_swapchain._surfaceFormat = surfaceFormat;


		auto& imageFormat = outRenderCtx->_swapchain._surfaceFormat.format;
		// Store the images used by the swap chain.
		// Note: these are the images that swap chain image indices refer to.
		// Note: actual number of images may differ from requested number, since it's a lower bound.
		uint32_t actualImageCount = 0;
		CheckResult(vkGetSwapchainImagesKHR(ctx._logicalDevice, outRenderCtx->_swapchain._handle, &actualImageCount, nullptr));

		outRenderCtx->_renderPass._colorImages.resize(actualImageCount);
		outRenderCtx->_swapchain._images.resize(actualImageCount);
		outRenderCtx->_overlayImages.resize(actualImageCount);
		outRenderCtx->_swapchain._frameBuffers.resize(actualImageCount);
		outRenderCtx->_drawCommandBuffers.resize(actualImageCount);

		std::vector<VkImage> swapchainImages;
		swapchainImages.resize(actualImageCount);
		CheckResult(vkGetSwapchainImagesKHR(ctx._logicalDevice, outRenderCtx->_swapchain._handle, &actualImageCount, swapchainImages.data()));
		outRenderCtx->_swapchain._imageCount = actualImageCount;

		// Create the scene and UI color attachments.
		for (uint32_t i = 0; i < actualImageCount; ++i) {
			outRenderCtx->_drawCommandBuffers[i] = VkHelper::CreateCommandBuffer(ctx._logicalDevice, ctx._commandPool);

			// Image view used by the UI shader as output attachment, after it has combined the UI image with the scene color image, which is output by the first subpass.
			outRenderCtx->_swapchain._images[i]._image = swapchainImages[i];
			auto& createInfo = outRenderCtx->_swapchain._images[i]._viewCreateInfo;
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
			vkCreateImageView(ctx._logicalDevice, &createInfo, nullptr, &outRenderCtx->_swapchain._images[i]._view);

			// Scene color image.
			{
				auto& rpColorImg = outRenderCtx->_renderPass._colorImages[i];
				auto& imageCreateInfo = rpColorImg._createInfo;
				imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
				imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
				imageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
				imageCreateInfo.extent = { outRenderCtx->_swapchain._framebufferSize.width, outRenderCtx->_swapchain._framebufferSize.height, 1 };
				imageCreateInfo.mipLevels = 1;
				imageCreateInfo.arrayLayers = 1;
				imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
				imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
				imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
				vkCreateImage(ctx._logicalDevice, &imageCreateInfo, nullptr, &rpColorImg._image);

				// Allocate memory on the GPU for the image.
				rpColorImg._gpuMemory = VkHelper::AllocateGpuMemoryForImage(ctx._logicalDevice, ctx._physicalDevice, rpColorImg._image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				vkBindImageMemory(ctx._logicalDevice, rpColorImg._image, rpColorImg._gpuMemory, 0);

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
				vkCreateImageView(ctx._logicalDevice, &imageViewCreateInfo, nullptr, &rpColorImg._view);

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
				vkCreateSampler(ctx._logicalDevice, &samplerCreateInfo, nullptr, &rpColorImg._sampler);
			}

			// UI image.
			{
				VkImageCreateInfo image_info{};
				image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
				image_info.imageType = VK_IMAGE_TYPE_2D;
				image_info.extent.width = outRenderCtx->_swapchain._framebufferSize.width;
				image_info.extent.height = outRenderCtx->_swapchain._framebufferSize.height;
				image_info.extent.depth = 1;
				image_info.mipLevels = 1;
				image_info.arrayLayers = 1;
				image_info.format = outRenderCtx->_swapchain._surfaceFormat.format;
				image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
				image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
				image_info.samples = VK_SAMPLE_COUNT_1_BIT;
				image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				CheckResult(vkCreateImage(ctx._logicalDevice, &image_info, NULL, &outRenderCtx->_overlayImages[i]._image));

				outRenderCtx->_overlayImages[i]._gpuMemory = VkHelper::AllocateGpuMemoryForImage(ctx._logicalDevice, ctx._physicalDevice, outRenderCtx->_overlayImages[i]._image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				CheckResult(vkBindImageMemory(ctx._logicalDevice, outRenderCtx->_overlayImages[i]._image, outRenderCtx->_overlayImages[i]._gpuMemory, 0));

				VkImageViewCreateInfo image_view_info{};
				image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
				image_view_info.format = outRenderCtx->_swapchain._surfaceFormat.format;
				image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
				image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
				image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
				image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
				image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				image_view_info.subresourceRange.baseMipLevel = 0;
				image_view_info.subresourceRange.levelCount = 1;
				image_view_info.subresourceRange.baseArrayLayer = 0;
				image_view_info.subresourceRange.layerCount = 1;
				image_view_info.image = outRenderCtx->_overlayImages[i]._image;
				CheckResult(vkCreateImageView(ctx._logicalDevice, &image_view_info, NULL, &outRenderCtx->_overlayImages[i]._view));

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
				CheckResult(vkCreateSampler(ctx._logicalDevice, &samplerCreateInfo, NULL, &outRenderCtx->_overlayImages[i]._sampler));
			}
		}

		// Create the depth image.
		{
			auto& imageCreateInfo = outRenderCtx->_renderPass._depthImage._createInfo;
			imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageCreateInfo.arrayLayers = 1;
			imageCreateInfo.extent = { outRenderCtx->_swapchain._framebufferSize.width, outRenderCtx->_swapchain._framebufferSize.height, 1 };
			imageCreateInfo.format = VK_FORMAT_D32_SFLOAT;
			imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
			imageCreateInfo.mipLevels = 1;
			imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			vkCreateImage(ctx._logicalDevice, &imageCreateInfo, nullptr, &outRenderCtx->_renderPass._depthImage._image);

			// Allocate memory on the GPU for the image.
			outRenderCtx->_renderPass._depthImage._gpuMemory = VkHelper::AllocateGpuMemoryForImage(ctx._logicalDevice, ctx._physicalDevice, outRenderCtx->_renderPass._depthImage._image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			vkBindImageMemory(ctx._logicalDevice, outRenderCtx->_renderPass._depthImage._image, outRenderCtx->_renderPass._depthImage._gpuMemory, 0);

			auto& imageViewCreateInfo = outRenderCtx->_renderPass._depthImage._viewCreateInfo;
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.components = { {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY} };
			imageViewCreateInfo.format = VK_FORMAT_D32_SFLOAT;
			imageViewCreateInfo.image = outRenderCtx->_renderPass._depthImage._image;
			imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
			imageViewCreateInfo.subresourceRange.layerCount = 1;
			imageViewCreateInfo.subresourceRange.levelCount = 1;
			vkCreateImageView(ctx._logicalDevice, &imageViewCreateInfo, nullptr, &outRenderCtx->_renderPass._depthImage._view);

			auto& samplerCreateInfo = outRenderCtx->_renderPass._depthImage._samplerCreateInfo;
			samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCreateInfo.anisotropyEnable = false;
			samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
			samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
			samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
			vkCreateSampler(ctx._logicalDevice, &samplerCreateInfo, nullptr, &outRenderCtx->_renderPass._depthImage._sampler);
		}

		// Create the render pass. The render pass defines, at a high level, the order of operations needed to render an image. Each frame buffer needs to reference a render pass,
		// meaning that frame buffers and render passes are tightly coupled. A frame buffer is a container for the image generated by its render pass - in the simplest case,
		// frame buffer images (called attachments) will reference the swapchain image as attachment, which is an immutable (by the user application) image that is exclusively used to present
		// to a window surface.
		{
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
			uiRenderingSubpass.pInputAttachments = &inputAttachmentRefSubpass1;
			uiRenderingSubpass.pColorAttachments = &swapchainImageAttachmentReference;

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
			VkRenderPassCreateInfo renderPassCreateInfo = {};
			renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassCreateInfo.attachmentCount = 3;
			renderPassCreateInfo.pAttachments = attachmentDescriptions;
			renderPassCreateInfo.subpassCount = 2;
			renderPassCreateInfo.pSubpasses = subpassDescriptions;
			renderPassCreateInfo.dependencyCount = 3;
			renderPassCreateInfo.pDependencies = subpassDependencies;
			CheckResult(vkCreateRenderPass(ctx._logicalDevice, &renderPassCreateInfo, nullptr, &outRenderCtx->_renderPass._handle));
		}

		// Create the UI descriptor sets and their layout, as well as the pipeline layout.
		{
			VkDescriptorSetLayoutBinding setLayoutBindings[2];
			memset(setLayoutBindings, 0, sizeof(VkDescriptorSetLayoutBinding) * 2);

			// Input attachment from the scene subpass.
			setLayoutBindings[0].binding = 1;
			setLayoutBindings[0].descriptorCount = 1;
			setLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
			setLayoutBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

			// Overlay image.
			setLayoutBindings[1].binding = 0;
			setLayoutBindings[1].descriptorCount = 1;
			setLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			setLayoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

			DescriptorSetLayout uiDescriptorSetLayout;
			uiDescriptorSetLayout._name = "overlayImageDescriptorSetLayout";
			uiDescriptorSetLayout._id = 0;
			VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
			layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutCreateInfo.bindingCount = 2;
			layoutCreateInfo.pBindings = setLayoutBindings;
			CheckResult(vkCreateDescriptorSetLayout(ctx._logicalDevice, &layoutCreateInfo, NULL, &uiDescriptorSetLayout._layout));

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

			std::vector<VkDescriptorSet> sets; sets.resize(actualImageCount);
			for (uint32_t i = 0; i < actualImageCount; ++i) {
				VkDescriptorPool dp;
				CheckResult(vkCreateDescriptorPool(ctx._logicalDevice, &pool_info, NULL, &dp));
				sets[i] = VkHelper::AllocateDescriptorSet(ctx._logicalDevice, dp, uiDescriptorSetLayout._layout);

				std::array<VkDescriptorImageInfo, 2> descriptors{};
				descriptors[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				descriptors[0].imageView = outRenderCtx->_overlayImages[i]._view;
				descriptors[0].sampler = outRenderCtx->_overlayImages[i]._sampler;

				descriptors[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				descriptors[1].imageView = outRenderCtx->_renderPass._colorImages[i]._view;
				descriptors[1].sampler = VK_NULL_HANDLE;

				VkWriteDescriptorSet write0{};
				write0.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				write0.dstSet = sets[i];
				write0.dstBinding = 0;
				write0.dstArrayElement = 0;
				write0.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				write0.descriptorCount = 1;
				write0.pImageInfo = &descriptors[0];

				VkWriteDescriptorSet write1{};
				write1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				write1.dstSet = sets[i];
				write1.dstBinding = 1;
				write1.dstArrayElement = 0;
				write1.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
				write1.descriptorCount = 1;
				write1.pImageInfo = &descriptors[1];

				VkWriteDescriptorSet descriptorWrites[2] = { write0, write1 };
				vkUpdateDescriptorSets(ctx._logicalDevice, 2, descriptorWrites, 0, nullptr);
			}

			if (outRenderCtx->_uiPipeline._shaderResources._data.contains(uiDescriptorSetLayout)) outRenderCtx->_uiPipeline._shaderResources._data[uiDescriptorSetLayout] = sets;
			else outRenderCtx->_uiPipeline._shaderResources._data.try_emplace(uiDescriptorSetLayout, sets);

			VkPushConstantRange pushConstantRange = {};
			pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // Push constant used in the fragment shader
			pushConstantRange.offset = 0;
			pushConstantRange.size = sizeof(float); // We're only passing one float (gamma correction)

			VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
			pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutCreateInfo.setLayoutCount = 1;
			pipelineLayoutCreateInfo.pSetLayouts = &outRenderCtx->_uiPipeline._shaderResources._data.begin()->first._layout;
			pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
			pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
			CheckResult(vkCreatePipelineLayout(ctx._logicalDevice, &pipelineLayoutCreateInfo, nullptr, &outRenderCtx->_uiPipeline._layout));
		}

		CreateGraphicsPipelines(ctx, *outRenderCtx);

		// Here we record the commands that will be executed in the render loop. The engine uses deferred command execution, meaning that it will execute the command buffers 
		// that are recorded now in the future.
		for (size_t i = 0; i < actualImageCount; i++) {
			auto& currentFrameBuffer = outRenderCtx->_swapchain._frameBuffers[i];
			auto& cmdBufferOfCurrentFrame = outRenderCtx->_drawCommandBuffers[i];

			// We will render to the same depth image for each frame. 
			// We can just keep clearing and reusing the same depth image for every frame.
			VkImageView renderPassImages[3] = { outRenderCtx->_swapchain._images[i]._view, outRenderCtx->_renderPass._colorImages[i]._view, outRenderCtx->_renderPass._depthImage._view };
			VkFramebufferCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			createInfo.renderPass = outRenderCtx->_renderPass._handle;
			createInfo.attachmentCount = 3;
			createInfo.pAttachments = renderPassImages;
			createInfo.width = outRenderCtx->_swapchain._framebufferSize.width;
			createInfo.height = outRenderCtx->_swapchain._framebufferSize.height;
			createInfo.layers = 1;
			CheckResult(vkCreateFramebuffer(ctx._logicalDevice, &createInfo, nullptr, &currentFrameBuffer));

			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			vkBeginCommandBuffer(outRenderCtx->_drawCommandBuffers[i], &beginInfo);

			VkHelper::TransitionImageLayout(ctx._logicalDevice, ctx._commandPool, ctx._queue, outRenderCtx->_swapchain._images[i]._image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
			VkHelper::TransitionImageLayout(ctx._logicalDevice, ctx._commandPool, ctx._queue, outRenderCtx->_renderPass._colorImages[i]._image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			//VkHelper::TransitionImageLayout(ctx._logicalDevice, ctx._commandPool, ctx._queue, _uiCtx._overlayImages[i]._image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			VkClearValue swapchainImageClear{ { 0.0f, 0.0f, 0.0f, 1.0f } }; // R, G, B, A.
			VkClearValue sceneImageClear = { { 0.1f, 0.1f, 0.1f, 1.0f } };
			VkClearValue depthImageClear{ { 0.0f, 0.0f, 0.0f, 0.0f } }; depthImageClear.depthStencil.depth = 1.0f;
			VkClearValue clearValues[] = { swapchainImageClear, sceneImageClear, depthImageClear };

			VkRenderPassBeginInfo renderPassBeginInfo{};
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassBeginInfo.renderPass = outRenderCtx->_renderPass._handle;
			renderPassBeginInfo.framebuffer = currentFrameBuffer;
			renderPassBeginInfo.renderArea.offset.x = 0;
			renderPassBeginInfo.renderArea.offset.y = 0;
			renderPassBeginInfo.renderArea.extent = outRenderCtx->_swapchain._framebufferSize;
			renderPassBeginInfo.clearValueCount = 3;
			renderPassBeginInfo.pClearValues = clearValues;

			// Draw the environment map as a skybox.
			vkCmdBeginRenderPass(cmdBufferOfCurrentFrame, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(cmdBufferOfCurrentFrame, VK_PIPELINE_BIND_POINT_GRAPHICS, outRenderCtx->_envMapPipeline._handle);
			eCtx._scene._environmentMap._shaderResources.MergeResources(eCtx._mainCamera._shaderResources);
			eCtx._scene._environmentMap.Draw(outRenderCtx->_envMapPipeline._layout, cmdBufferOfCurrentFrame);

			// Draw the 3D objects.
			auto& shaderResources = outRenderCtx->_scenePipeline._shaderResources;
			VkDescriptorSet sets[4] = { shaderResources[0][0], shaderResources[1][0], shaderResources[2][0], shaderResources[4][0] };
			vkCmdBindPipeline(cmdBufferOfCurrentFrame, VK_PIPELINE_BIND_POINT_GRAPHICS, outRenderCtx->_scenePipeline._handle);
			vkCmdBindDescriptorSets(cmdBufferOfCurrentFrame, VK_PIPELINE_BIND_POINT_GRAPHICS, outRenderCtx->_scenePipeline._layout, 0, 1, &shaderResources[0][0], 0, nullptr);
			vkCmdBindDescriptorSets(cmdBufferOfCurrentFrame, VK_PIPELINE_BIND_POINT_GRAPHICS, outRenderCtx->_scenePipeline._layout, 2, 1, &shaderResources[2][0], 0, nullptr);
			vkCmdBindDescriptorSets(cmdBufferOfCurrentFrame, VK_PIPELINE_BIND_POINT_GRAPHICS, outRenderCtx->_scenePipeline._layout, 4, 1, &shaderResources[4][0], 0, nullptr);

			for (auto& gameObject : eCtx._scene._pRootGameObject->_children)
				gameObject->Draw(outRenderCtx->_scenePipeline._layout, cmdBufferOfCurrentFrame);

			// Draw UI.
			auto& uiShaderResources = outRenderCtx->_uiPipeline._shaderResources;
			vkCmdNextSubpass(cmdBufferOfCurrentFrame, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(cmdBufferOfCurrentFrame, VK_PIPELINE_BIND_POINT_GRAPHICS, outRenderCtx->_uiPipeline._handle);
			vkCmdBindDescriptorSets(cmdBufferOfCurrentFrame, VK_PIPELINE_BIND_POINT_GRAPHICS, outRenderCtx->_uiPipeline._layout, 0, 1, &uiShaderResources[0][i], 0, nullptr);
			vkCmdPushConstants(cmdBufferOfCurrentFrame, outRenderCtx->_uiPipeline._layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float), &eCtx._globalSettings._gammaCorrection);
			vkCmdDraw(cmdBufferOfCurrentFrame, 3, 1, 0, 0);
			vkCmdEndRenderPass(cmdBufferOfCurrentFrame);
			CheckResult(vkEndCommandBuffer(cmdBufferOfCurrentFrame));

		}

		// Create sempahores to synchronize drawing operations.
		{
			VkSemaphoreCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			CheckResult(vkCreateSemaphore(ctx._logicalDevice, &createInfo, nullptr, &outRenderCtx->_imageAvailableSemaphore));
			CheckResult(vkCreateSemaphore(ctx._logicalDevice, &createInfo, nullptr, &outRenderCtx->_renderingFinishedSemaphore));
		}
	}

	void DestroyRenderingResources(VkContext& ctx, VkRenderContext& rCtx) {
		vkQueueWaitIdle(ctx._queue);
		for (int i = 0; i < rCtx._swapchain._frameBuffers.size(); ++i) {
			vkDestroyFramebuffer(ctx._logicalDevice, rCtx._swapchain._frameBuffers[i], nullptr);
			VkHelper::DestroyImage(ctx._logicalDevice, rCtx._overlayImages[i]._image, rCtx._overlayImages[i]._view, rCtx._overlayImages[i]._sampler);
			VkHelper::DestroyImage(ctx._logicalDevice, rCtx._renderPass._colorImages[i]._image, rCtx._renderPass._colorImages[i]._view, rCtx._renderPass._colorImages[i]._sampler);
		}
		VkHelper::DestroyImage(ctx._logicalDevice, rCtx._renderPass._depthImage._image, rCtx._renderPass._depthImage._view, rCtx._renderPass._depthImage._sampler);
		vkDestroyRenderPass(ctx._logicalDevice, rCtx._renderPass._handle, nullptr);
		vkDestroySwapchainKHR(ctx._logicalDevice, rCtx._swapchain._handle, nullptr);
		rCtx._swapchain._handle = nullptr;
		rCtx._swapchain._oldSwapchainHandle = nullptr;
		vkDestroyPipeline(ctx._logicalDevice, rCtx._envMapPipeline._handle, nullptr);
		vkDestroyPipeline(ctx._logicalDevice, rCtx._uiPipeline._handle, nullptr);
		vkDestroyPipeline(ctx._logicalDevice, rCtx._scenePipeline._handle, nullptr);
		vkDestroySemaphore(ctx._logicalDevice, rCtx._imageAvailableSemaphore, nullptr);
		vkDestroySemaphore(ctx._logicalDevice, rCtx._renderingFinishedSemaphore, nullptr);
	}

	VkContext InitializeVulkan(GlobalSettings& settings, GLFWwindow* pWindow) {
		VkContext ctx{};

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
		for (size_t i = 0; i < glfwExtensionCount; i++) extensions.push_back(glfwExtensions[i]);
		if (settings._enableValidationLayers) extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		if (extensionCount == 0) Exit(1, "no extensions supported");

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = (uint32_t)extensions.size();
		createInfo.ppEnabledExtensionNames = extensions.data();

		if (settings._enableValidationLayers) {
			if (ValidationLayersSupported(settings._pValidationLayers)) {
				createInfo.enabledLayerCount = 1;
				createInfo.ppEnabledLayerNames = settings._pValidationLayers.data();
			}
		}

		vkCreateInstance(&createInfo, nullptr, &ctx._instance);

		Engine::CreateDebugCallback(ctx, settings);
		glfwCreateWindowSurface(ctx._instance, pWindow, NULL, &ctx._windowSurface);

		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(ctx._instance, &deviceCount, nullptr);
		if (deviceCount <= 0) Exit(1, "device count was zero");
		deviceCount = 1;
		std::vector<VkPhysicalDevice> outDevice(deviceCount);
		vkEnumeratePhysicalDevices(ctx._instance, &deviceCount, outDevice.data());
		if (deviceCount <= 0) Exit(1, "device count was zero");
		ctx._physicalDevice = outDevice[0];

		auto flags = (VkQueueFlagBits)(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT);
		std::vector<VkQueueFamilyProperties> queueFamilyProperties = PhysicalDevice::GetAllQueueFamilyProperties(ctx._physicalDevice);
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(ctx._physicalDevice, &queueFamilyCount, nullptr);
		vkGetPhysicalDeviceQueueFamilyProperties(ctx._physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

		for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyProperties.size(); queueFamilyIndex++) {
			if (queueFamilyProperties[queueFamilyIndex].queueCount > 0 && queueFamilyProperties[queueFamilyIndex].queueFlags & flags) {
				ctx._queueFamilyIndex = queueFamilyIndex; break;
			}
		}

		ctx._queueFamilyIndex = VkHelper::FindQueueFamilyIndex(ctx._physicalDevice, flags);
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(ctx._physicalDevice, ctx._queueFamilyIndex, ctx._windowSurface, &presentSupport);

		VkDeviceQueueCreateInfo graphicsQueueInfo{};
		float queuePriority = 1.0f;
		graphicsQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		graphicsQueueInfo.queueFamilyIndex = ctx._queueFamilyIndex;
		graphicsQueueInfo.queueCount = 1;
		graphicsQueueInfo.pQueuePriorities = &queuePriority;

		VkDeviceCreateInfo deviceCreateInfo = {};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.pQueueCreateInfos = &graphicsQueueInfo;
		deviceCreateInfo.queueCreateInfoCount = 1;

		// Devices features to enable.
		VkPhysicalDeviceFeatures enabledFeatures = {};
		enabledFeatures.samplerAnisotropy = VK_TRUE;
		enabledFeatures.shaderClipDistance = VK_TRUE;
		enabledFeatures.shaderCullDistance = VK_TRUE;

		const char* deviceExtensions = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
		deviceCreateInfo.enabledExtensionCount = 1;
		deviceCreateInfo.ppEnabledExtensionNames = &deviceExtensions;
		deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

		if (settings._enableValidationLayers) {
			deviceCreateInfo.enabledLayerCount = 1;
			deviceCreateInfo.ppEnabledLayerNames = settings._pValidationLayers.data();
		}

		vkCreateDevice(ctx._physicalDevice, &deviceCreateInfo, nullptr, &ctx._logicalDevice);
		vkGetDeviceQueue(ctx._logicalDevice, ctx._queueFamilyIndex, 0, &ctx._queue);
		VkFenceCreateInfo fci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, 0 };
		vkCreateFence(ctx._logicalDevice, &fci, NULL, &ctx._queueFence);
		ctx._commandPool = VkHelper::CreateCommandPool(ctx._logicalDevice, ctx._queueFamilyIndex);
		return ctx;
	}

	void InitializeNuklearUI(VkContext& ctx, VkRenderContext& rCtx) {
		std::vector<VkImageView> views; views.resize(rCtx._overlayImages.size());
		for (int i = 0; i < views.size(); ++i) views[i] = rCtx._overlayImages[i]._view;
		rCtx._uiCtx = nk_glfw3_init(rCtx._pWindow, ctx._logicalDevice, ctx._physicalDevice, ctx._queueFamilyIndex, views.data(), (uint32_t)views.size(), rCtx._swapchain._surfaceFormat.format, NK_GLFW3_INSTALL_CALLBACKS, 512 * 1024, 128 * 1024);

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
			nk_glfw3_font_stash_end(ctx._queue);
			/*nk_style_load_all_cursors(ctx, atlas->cursors);*/
			/*nk_style_set_font(ctx, &droid->handle);*/
		}
	}

	void InitializeEngine(VkContext* outCtx, VkRenderContext* outRenderCtx, EngineContext* outEngineCtx) {
		outEngineCtx->_globalSettings.Load(Engine::Paths::Settings());
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		outRenderCtx->_pWindow = glfwCreateWindow(outEngineCtx->_globalSettings._windowWidth, outEngineCtx->_globalSettings._windowHeight, "Frontline Legacy", nullptr, nullptr);
		glfwSetWindowSizeCallback(outRenderCtx->_pWindow, Engine::OnWindowResized);
		outEngineCtx->_input.Initialize(outRenderCtx->_pWindow);

		*outCtx = InitializeVulkan(outEngineCtx->_globalSettings, outRenderCtx->_pWindow);
		outEngineCtx->_scene = LoadScene(*outCtx);
		LoadEnvironmentMap(*outCtx, *outEngineCtx);
		auto descriptorSetLayouts = CreateSceneDescriptorSetLayouts(*outCtx, outEngineCtx->_scene);
		outRenderCtx->_scenePipeline._layout = CreateScenePipelineLayout(*outCtx, descriptorSetLayouts);
		CreateSceneShaderResources(*outCtx, *outRenderCtx, *outEngineCtx, descriptorSetLayouts);
		CreateRenderingResources(*outCtx, *outEngineCtx, outRenderCtx);
		InitializeNuklearUI(*outCtx, *outRenderCtx);
	}

	void WindowSizeChanged(VkContext& ctx, VkRenderContext& rCtx, EngineContext& eCtx) {
		DestroyRenderingResources(ctx, rCtx);
		CreateRenderingResources(ctx, eCtx, &rCtx);
		InitializeNuklearUI(ctx, rCtx);
	}

	void Draw(VkContext& ctx, VkRenderContext& rCtx, EngineContext& eCtx) {
		if (windowMinimized) return;
		vkResetFences(ctx._logicalDevice, 1, &ctx._queueFence);

		// Acquire image.
		uint32_t imageIndex;
		VkResult swapImageState = vkAcquireNextImageKHR(ctx._logicalDevice, rCtx._swapchain._handle, UINT64_MAX, rCtx._imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

		// Declare a lambda that takes three arguments
		auto checkSwapchainImageState = [&]() -> bool {
			if (swapImageState == VK_SUBOPTIMAL_KHR || swapImageState == VK_ERROR_OUT_OF_DATE_KHR || windowResized) { windowResized = false; WindowSizeChanged(ctx, rCtx, eCtx); return false; }
			else if (swapImageState != VK_SUCCESS) { std::cerr << "Error: image state is VkResult = " << swapImageState << std::endl; exit(1); }
			return true;
			};

		if (!checkSwapchainImageState()) return;

		// Refresh UI
		{
			if (!eCtx._input._cursorEnabled) goto skipUi;
			nk_glfw3_new_frame();

			/* GUI */
			/*if (nk_begin(rCtx._uiCtx, "Demo", nk_rect(50, 50, 230, 250),
				NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
				NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE)) {
				enum { EASY, HARD };
				static int op = EASY;
				static int property = 20;
				nk_layout_row_static(rCtx._uiCtx, 30, 80, 1);
				if (nk_button_label(rCtx._uiCtx, "button"))
					fprintf(stdout, "button pressed\n");

				nk_layout_row_dynamic(rCtx._uiCtx, 30, 2);
				if (nk_option_label(rCtx._uiCtx, "easy", op == EASY))
					op = EASY;
				if (nk_option_label(rCtx._uiCtx, "hard", op == HARD))
					op = HARD;

				nk_layout_row_dynamic(rCtx._uiCtx, 25, 1);
				nk_property_int(rCtx._uiCtx, "Compression:", 0, &property, 100, 10, 1);

				nk_layout_row_dynamic(rCtx._uiCtx, 20, 1);
				nk_label(rCtx._uiCtx, "background:", NK_TEXT_LEFT);
				nk_layout_row_dynamic(rCtx._uiCtx, 25, 1);
			}
			nk_end(rCtx._uiCtx);*/

			// Define the panel bounds to cover the entire screen
			float windowWidth = (float)rCtx._swapchain._framebufferSize.width;
			float windowHeight = (float)rCtx._swapchain._framebufferSize.height;

			// Begin a panel with no title, no borders, and non-movable
			if (nk_begin(rCtx._uiCtx, "Fullscreen Panel", nk_rect(0, 0, windowWidth, windowHeight), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
				// Style the panel background
				struct nk_style* style = &rCtx._uiCtx->style;
				style->window.fixed_background = nk_style_item_color(nk_rgba(10, 10, 10, 200)); // Semi-transparent black

				float buttonWidth = 150.0f;  // Example button width
				float buttonHeight = 50.0f; // Example button height

				nk_layout_space_begin(rCtx._uiCtx, NK_STATIC, windowHeight, INT_MAX);
				float buttonX = (windowWidth - buttonWidth) / 2.0f;  // Center horizontally
				float buttonY = (windowHeight - buttonHeight) / 2.0f; // Center vertically
				nk_layout_space_push(rCtx._uiCtx, nk_rect(buttonX, buttonY, buttonWidth, buttonHeight));
				if (nk_button_label(rCtx._uiCtx, "Play")) {
					printf("Button clicked!\n");
				}

				//float buttonX = (windowWidth - buttonWidth) / 2.0f;  // Center horizontally
				//float buttonY = (windowHeight - buttonHeight) / 2.0f; // Center vertically
				buttonY += buttonHeight + 5; // Center vertically
				nk_layout_space_push(rCtx._uiCtx, nk_rect(buttonX, buttonY, buttonWidth, buttonHeight));
				if (nk_button_label(rCtx._uiCtx, "Settings")) {
					printf("Button clicked!\n");
				}
				nk_layout_space_end(rCtx._uiCtx);
			}
			nk_end(rCtx._uiCtx);
		}
	skipUi:

		VkHelper::TransitionImageLayout(ctx._logicalDevice, ctx._commandPool, ctx._queue, rCtx._overlayImages[imageIndex]._image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		auto nk_semaphore = nk_glfw3_render(ctx._queue, imageIndex, rCtx._imageAvailableSemaphore, NK_ANTI_ALIASING_ON);
		/*VkDeviceMemory stagingMemory = nullptr;
		VkBuffer stagingBuffer = nullptr;
		auto imageData = VkHelper::DownloadImage(_logicalDevice, _physicalDevice, _commandPool, _queue, _uiCtx._overlayImages[imageIndex]._image, _swapchain._framebufferSize.width, _swapchain._framebufferSize.height, stagingMemory, stagingBuffer);
		Helper::SaveImageAsPng(Paths::TexturesPath() / "ui.png", imageData, _swapchain._framebufferSize.width, _swapchain._framebufferSize.height);
		VkHelper::UnmapAndDestroyStagingBuffer(_logicalDevice, stagingMemory, stagingBuffer);
		VkHelper::TransitionImageLayout(_logicalDevice, _commandPool, _queue, _uiCtx._overlayImages[imageIndex]._image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);*/

		// Wait for image to be available and draw.
		// This is the stage where the queue should wait on the semaphore.
		VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &nk_semaphore;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &rCtx._renderingFinishedSemaphore;
		submitInfo.pWaitDstStageMask = &waitDstStageMask;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &rCtx._drawCommandBuffers[imageIndex];

		vkQueueSubmit(ctx._queue, 1, &submitInfo, ctx._queueFence);
		vkWaitForFences(ctx._logicalDevice, 1, &ctx._queueFence, VK_TRUE, UINT64_MAX);

		// Present drawn image.
		// Note: semaphore here is not strictly necessary, because commands are processed in submission order within a single queue.
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &rCtx._renderingFinishedSemaphore;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &rCtx._swapchain._handle;
		presentInfo.pImageIndices = &imageIndex;

		swapImageState = vkQueuePresentKHR(ctx._queue, &presentInfo);
		checkSwapchainImageState();
	}

	void Update(VkContext& ctx, EngineContext& eCtx) {
		eCtx._time.Update();
		eCtx._input.Update();
		eCtx._mainCamera.Update(ctx, eCtx._input);
		eCtx._scene.Update(ctx);
	}

	void PhysicsUpdate(GLFWwindow* pWindow, VkContext* ctx, EngineContext* eCtx) {
		auto& time = Time::Instance();
		VkContext collisionCtx = GpuCollisionDetector::InitializeVulkan(ctx->_logicalDevice, ctx->_physicalDevice);
		time.PhysicsUpdate(*ctx, collisionCtx, *eCtx);

		GameObject* mp5k = nullptr;


		for (int i = 0; i < eCtx->_scene._pRootGameObject->_children.size(); ++i) {
			if (eCtx->_scene._pRootGameObject->_children[i]->_name == "MP5KCollision") mp5k = eCtx->_scene._pRootGameObject->_children[i];
		}

		while (!glfwWindowShouldClose(pWindow)) {
			auto timePhysicsUpdateStart = std::chrono::high_resolution_clock::now();
			time.PhysicsUpdate(*ctx, collisionCtx, *eCtx);
			eCtx->_scene.PhysicsUpdate(*ctx, collisionCtx, *eCtx);

			float deltaTimeSeconds = (float)Time::Instance()._physicsDeltaTime * 0.001f;

			if (!mp5k) continue;

			//auto pos = (freeCube->_body._mesh._vertices[3]._position + freeCube->_body._mesh._vertices[9]._position + freeCube->_body._mesh._vertices[15]._position + freeCube->_body._mesh._vertices[21]._position) / 4.0f;
			/*auto pos = mp5k->_body._mesh._vertices[3]._position;
			auto pos1 = mp5k->_body._mesh._vertices[15]._position;
			auto wst = mp5k->GetWorldSpaceTransform()._matrix;
			pos = glm::vec3(wst * glm::vec4(pos, 1.0f));
			pos1 = glm::vec3(wst * glm::vec4(pos1, 1.0f));*/
			glm::vec3 up = { 0.0f, 12.0f, 0.0f };
			glm::vec3 right = { 12.0f, 0.0f, 0.0f };
			glm::vec3 forward = { 0.0f, 0.0f, 12.0f };

			/*if (eCtx->_input.IsKeyHeldDown(GLFW_KEY_UP)) {
				freeCube->_body.AddForceAtPosition(f, pos, true, true, false, deltaTimeSeconds);
				freeCube->_body.AddForceAtPosition(f, pos1, true, true, false, deltaTimeSeconds);
			}
			if (eCtx->_input.IsKeyHeldDown(GLFW_KEY_DOWN)) {
				freeCube->_body.AddForceAtPosition(-f, pos, true, true, false, deltaTimeSeconds);
				freeCube->_body.AddForceAtPosition(-f, pos1, true, true, false, deltaTimeSeconds);
			}*/

			//if (eCtx->_input.IsKeyHeldDown(GLFW_KEY_LEFT_SHIFT)) mp5k->_body.AddTorque(right, deltaTimeSeconds, true);
			//if (eCtx->_input.IsKeyHeldDown(GLFW_KEY_LEFT_ALT)) mp5k->_body.AddTorque(-right, deltaTimeSeconds, true);
			if (eCtx->_input.IsKeyHeldDown(GLFW_KEY_LEFT_SHIFT)) mp5k->_body.AddForce(up, deltaTimeSeconds, true);
			if (eCtx->_input.IsKeyHeldDown(GLFW_KEY_LEFT_ALT)) mp5k->_body.AddForce(-up, deltaTimeSeconds, true);
			if (eCtx->_input.IsKeyHeldDown(GLFW_KEY_RIGHT)) mp5k->_body.AddForce(right, deltaTimeSeconds, true);
			if (eCtx->_input.IsKeyHeldDown(GLFW_KEY_LEFT)) mp5k->_body.AddForce(-right, deltaTimeSeconds, true);
			if (eCtx->_input.IsKeyHeldDown(GLFW_KEY_DOWN)) mp5k->_body.AddForce(-forward, deltaTimeSeconds, true);
			if (eCtx->_input.IsKeyHeldDown(GLFW_KEY_UP)) mp5k->_body.AddForce(forward, deltaTimeSeconds, true);
		}
	}

	void MainLoop(VkContext& ctx, VkRenderContext& rCtx, EngineContext& eCtx) {
		std::thread physicsThread(&PhysicsUpdate, rCtx._pWindow, &ctx, &eCtx);

		while (!glfwWindowShouldClose(rCtx._pWindow)) {
			Update(ctx, eCtx);
			Draw(ctx, rCtx, eCtx);
			glfwPollEvents();
		}

		physicsThread.join();
	}
}

int main() {
	Engine::VkContext ctx{};
	Engine::VkRenderContext rCtx{};
	Engine::EngineContext eCtx{};
	Engine::InitializeEngine(&ctx, &rCtx, &eCtx);
	Engine::MainLoop(ctx, rCtx, eCtx);
	Engine::Cleanup(true);

	return 0;
}
