#pragma once
#include <iostream>
#include <fstream>
#include <ostream>
#include <vector>
#include <optional>
#include <bitset>
#include <type_traits>
#include <filesystem>
#include <glm/glm.hpp>

namespace Utils
{
	inline std::string Format(float value)
	{
		return (value >= 0.0f) ? " " + std::to_string(value) : std::to_string(value);
	}

	inline std::ostream& operator << (std::ostream& stream, const glm::mat4& matrix)
	{
		stream << Format(matrix[0][0]) << ", " << Format(matrix[0][1]) << ", " << Format(matrix[0][2]) << ", " << Format(matrix[0][3]) << "\n";
		stream << Format(matrix[1][0]) << ", " << Format(matrix[1][1]) << ", " << Format(matrix[1][2]) << ", " << Format(matrix[1][3]) << "\n";
		stream << Format(matrix[2][0]) << ", " << Format(matrix[2][1]) << ", " << Format(matrix[2][2]) << ", " << Format(matrix[2][3]) << "\n";
		stream << Format(matrix[3][0]) << ", " << Format(matrix[3][1]) << ", " << Format(matrix[3][2]) << ", " << Format(matrix[3][3]);
		return stream;
	}

	inline std::ostream& operator<< (std::ostream& stream, const glm::vec3& vector)
	{
		return stream << "(" << vector.x << ", " << vector.y << ", " << vector.z << ")";
	}

	inline std::ostream& operator<< (std::ostream& stream, const glm::vec3&& vector)
	{
		return stream << "(" << vector.x << ", " << vector.y << ", " << vector.z << ")";
	}

	/// <summary>
	/// Get the value of an enum as an integer
	/// </summary>
	/// <returns></returns>
	template <typename Enumeration>
	inline auto AsInteger(Enumeration const value)
		-> typename std::underlying_type<Enumeration>::type
	{
		return static_cast<typename std::underlying_type<Enumeration>::type>(value);
	}

	/// <summary>
	/// Returns the size of a vector in bytes
	/// </summary>
	template <typename T>
	inline size_t GetVectorSizeInBytes(std::vector<T> vector)
	{
		return sizeof(decltype(vector)::value_type) * vector.size();
	}

	/// <summary>
	/// Convert values. Returns the converted value or nullopt if value could not be converted.
	/// </summary>
	template <typename FromType, typename ToType>
	inline std::optional<ToType> Convert(FromType value)
	{
		ToType convertedValue;
		if (std::is_same_v<FromType, uint32_t>) {
			if (std::is_same_v<ToType, float>) {
				int intermediateValue = 0;
				for (unsigned short i = 32; i > 0; --i) {
					unsigned char ithBitFromRight = (value >> i) & 1;
					intermediateValue |= (ithBitFromRight << i);
				}
				convertedValue = static_cast<float>(intermediateValue);
				return convertedValue;
			}
		}

		return std::nullopt;
	}

	/*namespace File
	{
		
	}*/

	inline std::string ReadTextFile(std::filesystem::path absolutePath)
	{
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

	/// <summary>
	/// Prints a message to a stream given a function that does the printing and a message. 
	/// Default function prints to standard output (console).
	/// </summary>
	inline void Print(const std::string& message, void(*logFunction) (const std::string&) = [](const std::string& message) { std::cout << message << std::endl; })
	{
		logFunction(message);
	}
}