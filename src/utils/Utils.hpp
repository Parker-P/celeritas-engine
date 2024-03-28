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
#include "structural/Singleton.hpp"
#include "utils/Logger.hpp"

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

	/**
	 * @brief Get the value of an enum as an integer.
	 * @param value
	 * @return 
	 */
	template <typename Enumeration>
	inline auto AsInteger(Enumeration const value)
		-> typename std::underlying_type<Enumeration>::type
	{
		return static_cast<typename std::underlying_type<Enumeration>::type>(value);
	}

	/**
	 * @brief Returns the size of a vector in bytes.
	 * @param vector
	 * @return 
	 */
	template <typename T>
	inline size_t GetVectorSizeInBytes(std::vector<T> vector)
	{
		return sizeof(decltype(vector)::value_type) * vector.size();
	}

	class Converter
	{
	public:

		/**
		 * @brief Convert values. Returns the converted value.
		 * @param value
		 * @return 
		 */
		template <typename FromType, typename ToType>
		static inline ToType Convert(FromType value)
		{
			return Convert<ToType>(value);
		}

		/**
		 * @brief Converts uint32_t to float.
		 * @param value
		 * @return 
		 */
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

		/**
		 * @brief Converts string to bool.
		 * @param value
		 * @return true if the value is either "true" (case insensitive) or 1, false otherwise.
		 */
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
		
		/**
		 * @brief Converts string to int.
		 * @param value
		 * @return 
		 */
		template<>
		static inline int Convert(std::string value)
		{
			return std::stoi(value);
		}

		/**
		 * @brief Converts string to float.
		 * @param value
		 * @return 
		 */
		template<>
		static inline float Convert(std::string value)
		{
			return std::stof(value);
		}
	};

	class File
	{
	public:

		/**
		 * @brief Reads an ASCII or UNICODE text file.
		 * @param absolutePath
		 * @return The text the file contains as a std::string.
		 */
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

	inline void Exit(int errorCode, const char* message) {
		Utils::Logger::Log(message);
		throw std::exception(message);
		std::exit(errorCode);
	}

	/**
	 * @brief Prints a message to a stream given a function that does the printing and a message. 
	 * Default function prints to standard output (console).
	 * @param message The message to print.
	 * @param logFunction Logging function. Defaults to a function printing to the console window.
	 */
	inline void Print(const std::string& message, void(*logFunction) (const std::string&) = [](const std::string& message) { std::cout << message << std::endl; })
	{
		logFunction(message);
	}
}