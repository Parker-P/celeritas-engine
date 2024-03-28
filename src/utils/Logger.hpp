#pragma once

namespace Utils 
{
	class Logger : public Structural::Singleton<Logger>
	{
	public:
		static void Log(const char* message);
		static void Log(std::string message);
		static void Log(const wchar_t* message);
		static void Log(const std::wstring message);
	};
}
