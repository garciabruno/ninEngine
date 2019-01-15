#pragma once
#include <string>

class StringConverter
{
public: 
	static std::wstring StringToWide(std::string str);
	static std::string WideToString(const std::wstring& wstr);
};
