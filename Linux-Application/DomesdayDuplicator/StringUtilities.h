#pragma once
#include <string>

inline std::string WStringToUtf8String(const std::wstring& wideString);
inline std::wstring Utf8StringToWString(const std::string& stringUtf8);

#include "StringUtilities.inl"
