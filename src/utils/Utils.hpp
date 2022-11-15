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

	class Converter
	{
	public:
		/// <summary>
		/// Convert values. Returns the converted value.
		/// </summary>
		template <typename FromType, typename ToType>
		static inline ToType Convert(FromType value)
		{
			return Convert<ToType>(value);
		}

		/// <summary>
		/// Converts uint32_t to float.
		/// </summary>
		/// <param name="value"></param>
		/// <returns></returns>
		template<> 
		static inline float Convert(uint32_t value)
		{
			int intermediateValue = 0;
			for (unsigned short i = 32; i > 0; --i) {
				unsigned char ithBitFromRight = ((uint32_t)value >> i) & 1;
				intermediateValue |= (ithBitFromRight << i);
			}
			return static_cast<float>(intermediateValue);
		}

		/// <summary>
		/// Converts string to bool.
		/// </summary>
		/// <param name="value"></param>
		/// <returns>true if the value is either "true" (case insensitive) or 1, false otherwise.</returns>
		template<>
		static inline bool Convert(std::string value)
		{
			std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });
			if (value == "true" || value == "1")
			{
				return true;
			}
			return false;
		}

		/// <summary>
		/// Converts string to int.
		/// </summary>
		/// <param name="value"></param>
		/// <returns></returns>
		template<>
		static inline int Convert(std::string value)
		{
			return std::stoi(value);
		}

		/// <summary>
		/// Converts string to float.
		/// </summary>
		/// <param name="value"></param>
		/// <returns></returns>
		template<>
		static inline float Convert(std::string value)
		{
			return std::stof(value);
		}
	};

	class File
	{
	public:

		/// <summary>
		/// Reads an ASCII or UNICODE text file.
		/// </summary>
		/// <param name="absolutePath"></param>
		/// <returns></returns>
		static inline std::string ReadAllText(std::filesystem::path absolutePath)
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
	};

	/// <summary>
	/// Prints a message to a stream given a function that does the printing and a message. 
	/// Default function prints to standard output (console).
	/// </summary>
	inline void Print(const std::string& message, void(*logFunction) (const std::string&) = [](const std::string& message) { std::cout << message << std::endl; })
	{
		logFunction(message);
	}
}