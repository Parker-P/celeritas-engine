#include <iostream>
#include "structural/Singleton.hpp"
#include "Logger.hpp"

void Utils::Logger::Log(const char* message)
{
	std::cout << message << std::endl;
}

void Utils::Logger::Log(std::string message)
{
	std::cout << message << std::endl;
}

void Utils::Logger::Log(const wchar_t* message)
{
	std::wcout << message << std::endl;
}

void Utils::Logger::Log(const std::wstring message)
{
	std::wcout << message << std::endl;
}
