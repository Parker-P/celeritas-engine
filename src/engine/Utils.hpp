#pragma once
#include <type_traits>

class Utils {
public:

	/// <summary>
	/// Get the value of an enum as an integer
	/// </summary>
	/// <returns></returns>
	template <typename Enumeration>
	static auto AsInteger(Enumeration const value)
		-> typename std::underlying_type<Enumeration>::type
	{
		return static_cast<typename std::underlying_type<Enumeration>::type>(value);
	}
};