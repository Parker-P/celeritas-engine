// STL.
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
#include <bitset>

// Math.
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

// Scene loading.
#include <tinygltf/tiny_gltf.h>

// Project local classes.
#include "utils/Json.h"
#include "structural/IUpdatable.hpp"
#include "structural/Singleton.hpp"
#include "settings/GlobalSettings.hpp"
#include "settings/Paths.hpp"
#include "engine/Time.hpp"
#include "engine/input/Input.hpp"
#include "engine/math/Transform.hpp"

#include "utils/Utils.hpp"

#include "engine/vulkan/PhysicalDevice.hpp"
#include "engine/vulkan/Queue.hpp"
#include "engine/vulkan/Buffer.hpp"
#include "engine/vulkan/Image.hpp"
#include "engine/vulkan/ShaderResources.hpp"


#include "engine/structural/IPipelineable.hpp"
#include "engine/structural/Drawable.hpp"
#include "utils/BoxBlur.hpp"

#include "engine/scenes/Material.hpp"
#include "engine/scenes/Vertex.hpp"
#include "engine/scenes/PointLight.hpp"
#include "engine/scenes/CubicalEnvironmentMap.hpp"
#include "engine/scenes/Scene.hpp"
#include "engine/scenes/GameObject.hpp"
#include "engine/scenes/Camera.hpp"
#include "engine/scenes/Mesh.hpp"

#include "engine/vulkan/VulkanApplication.hpp"