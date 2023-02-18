#pragma once

namespace Structural
{
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
}