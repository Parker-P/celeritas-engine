#include <GLFW/glfw3.h>
#include <map>

#include "singleton.h"

template <typename T> class Keybind {
	T action;
	int mode;
};

//This class is used to create keybinds. It is templated and the T type should be an action type within your application
template <typename T> class KeybindsManager : public Singleton<KeybindsManager> {
public:
	//The key is the glfw key code, the value is the keybind object
	std::map<int, Keybind<T>> keybinds_;
};