#pragma once
#include <iostream>
#include <ostream>
#include <vector>
#include <type_traits>

class Utils {
public:

	/// <summary>
	/// Get the value of an enum as an integer
	/// </summary>
	/// <returns></returns>
	template <typename Enumeration>
	static inline auto AsInteger(Enumeration const value)
		-> typename std::underlying_type<Enumeration>::type
	{
		return static_cast<typename std::underlying_type<Enumeration>::type>(value);
	}

	/// <summary>
	/// Returns the size of a vector in bytes
	/// </summary>
	template <typename T>
	static inline size_t GetVectorSizeInBytes(std::vector<T> vector)
	{
		return sizeof(decltype(vector)::value_type) * vector.size();
	}

	/// <summary>
	/// Prints a message to a stream given a function that does the printing and a message. 
	/// Default function prints to standard output (console).
	/// </summary>
	static inline void Print(const std::string& message, void(*logFunction) (const std::string&) = [](const std::string& message) { std::cout << message << std::endl; }) {
		logFunction(message);
	}
};