#include <cassert>
#include <cstring>
#include <iostream>
#include <sstream>
#include <utility>

//----------------------------------------------------------------------------------------------------------------------
// Severity methods
//----------------------------------------------------------------------------------------------------------------------
bool ILogger::IsLogSeverityEnabled(Severity severity) const
{
    return IsLogSeverityEnabledInternal(severity);
}

//----------------------------------------------------------------------------------------------------------------------
// Generic logging methods
//----------------------------------------------------------------------------------------------------------------------
void ILogger::Log(Severity severity, const std::string& message) const
{
    LogInternal(severity, message);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Log(Severity severity, const std::wstring& message) const
{
    LogInternal(severity, message);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Log(Severity severity, const std::string& formatString, Args&&... args) const
{
    LogInternal(severity, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Log(Severity severity, const std::wstring& formatString, Args&&... args) const
{
    LogInternal(severity, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
// Severity logging methods
//----------------------------------------------------------------------------------------------------------------------
void ILogger::Critical(const char* message) const
{
    LogInternal(Severity::Critical, message);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Critical(const wchar_t* message) const
{
    LogInternal(Severity::Critical, message);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Critical(const std::string& message) const
{
    LogInternal(Severity::Critical, message);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Critical(const std::wstring& message) const
{
    LogInternal(Severity::Critical, message);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Critical(const char* formatString, Args&&... args) const
{
    LogInternal(Severity::Critical, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Critical(const wchar_t* formatString, Args&&... args) const
{
    LogInternal(Severity::Critical, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Critical(const std::string& formatString, Args&&... args) const
{
    LogInternal(Severity::Critical, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Critical(const std::wstring& formatString, Args&&... args) const
{
    LogInternal(Severity::Critical, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Error(const char* message) const
{
    LogInternal(Severity::Error, message);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Error(const wchar_t* message) const
{
    LogInternal(Severity::Error, message);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Error(const std::string& message) const
{
    LogInternal(Severity::Error, message);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Error(const std::wstring& message) const
{
    LogInternal(Severity::Error, message);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Error(const char* formatString, Args&&... args) const
{
    LogInternal(Severity::Error, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Error(const wchar_t* formatString, Args&&... args) const
{
    LogInternal(Severity::Error, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Error(const std::string& formatString, Args&&... args) const
{
    LogInternal(Severity::Error, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Error(const std::wstring& formatString, Args&&... args) const
{
    LogInternal(Severity::Error, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Warning(const char* message) const
{
    LogInternal(Severity::Warning, message);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Warning(const wchar_t* message) const
{
    LogInternal(Severity::Warning, message);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Warning(const std::string& message) const
{
    LogInternal(Severity::Warning, message);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Warning(const std::wstring& message) const
{
    LogInternal(Severity::Warning, message);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Warning(const char* formatString, Args&&... args) const
{
    LogInternal(Severity::Warning, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Warning(const wchar_t* formatString, Args&&... args) const
{
    LogInternal(Severity::Warning, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Warning(const std::string& formatString, Args&&... args) const
{
    LogInternal(Severity::Warning, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Warning(const std::wstring& formatString, Args&&... args) const
{
    LogInternal(Severity::Warning, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Info(const char* message) const
{
    LogInternal(Severity::Info, message);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Info(const wchar_t* message) const
{
    LogInternal(Severity::Info, message);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Info(const std::string& message) const
{
    LogInternal(Severity::Info, message);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Info(const std::wstring& message) const
{
    LogInternal(Severity::Info, message);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Info(const char* formatString, Args&&... args) const
{
    LogInternal(Severity::Info, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Info(const wchar_t* formatString, Args&&... args) const
{
    LogInternal(Severity::Info, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Info(const std::string& formatString, Args&&... args) const
{
    LogInternal(Severity::Info, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Info(const std::wstring& formatString, Args&&... args) const
{
    LogInternal(Severity::Info, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Debug(const char* message) const
{
    LogInternal(Severity::Debug, message);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Debug(const wchar_t* message) const
{
    LogInternal(Severity::Debug, message);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Debug(const std::string& message) const
{
    LogInternal(Severity::Debug, message);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Debug(const std::wstring& message) const
{
    LogInternal(Severity::Debug, message);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Debug(const char* formatString, Args&&... args) const
{
    LogInternal(Severity::Debug, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Debug(const wchar_t* formatString, Args&&... args) const
{
    LogInternal(Severity::Debug, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Debug(const std::string& formatString, Args&&... args) const
{
    LogInternal(Severity::Debug, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Debug(const std::wstring& formatString, Args&&... args) const
{
    LogInternal(Severity::Debug, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Trace(const char* message) const
{
    LogInternal(Severity::Trace, message);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Trace(const wchar_t* message) const
{
    LogInternal(Severity::Trace, message);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Trace(const std::string& message) const
{
    LogInternal(Severity::Trace, message);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::Trace(const std::wstring& message) const
{
    LogInternal(Severity::Trace, message);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Trace(const char* formatString, Args&&... args) const
{
    LogInternal(Severity::Trace, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Trace(const wchar_t* formatString, Args&&... args) const
{
    LogInternal(Severity::Trace, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Trace(const std::string& formatString, Args&&... args) const
{
    LogInternal(Severity::Trace, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::Trace(const std::wstring& formatString, Args&&... args) const
{
    LogInternal(Severity::Trace, formatString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::LogInternal(Severity severity, const char* message) const
{
    // If no log targets are set to log messages of the target severity, abort any further processing.
    if (!IsLogSeverityEnabled(severity))
    {
        return;
    }

    // Process the log message
    std::wstring messageConverted = Utf8StringToWString(message);
    ProcessLogMessage(severity, messageConverted.data(), messageConverted.size());
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::LogInternal(Severity severity, const wchar_t* message) const
{
    // If no log targets are set to log messages of the target severity, abort any further processing.
    if (!IsLogSeverityEnabled(severity))
    {
        return;
    }

    // Process the log message
    size_t stringLength = std::wcslen(message);
    ProcessLogMessage(severity, message, stringLength);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::LogInternal(Severity severity, const std::string& message) const
{
    // If no log targets are set to log messages of the target severity, abort any further processing.
    if (!IsLogSeverityEnabled(severity))
    {
        return;
    }

    // Process the log message
    std::wstring messageConverted = Utf8StringToWString(message);
    ProcessLogMessage(severity, messageConverted.data(), messageConverted.size());
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::LogInternal(Severity severity, const std::wstring& message) const
{
    // If no log targets are set to log messages of the target severity, abort any further processing.
    if (!IsLogSeverityEnabled(severity))
    {
        return;
    }

    // Process the log message
    ProcessLogMessage(severity, message.c_str(), message.size());
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::LogInternal(Severity severity, const char* formatString, Args&&... args) const
{
    // If no log targets are set to log messages of the target severity, abort any further processing.
    if (!IsLogSeverityEnabled(severity))
    {
        return;
    }

    // Convert all supplied arguments to a string representation
    const size_t argCount = sizeof...(Args);
    std::wstring argsResolved[argCount];
    ResolveArgs(argsResolved, std::forward<Args>(args)...);

    // Resolve the format string and argument values down to a single string
    std::wstring formatStringConverted = Utf8StringToWString(formatString);
    std::wstring messageResolved = ResolveFormatString(formatStringConverted, argCount, argsResolved);

    // Process the log message
    ProcessLogMessage(severity, messageResolved.data(), messageResolved.size());
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::LogInternal(Severity severity, const wchar_t* formatString, Args&&... args) const
{
    // If no log targets are set to log messages of the target severity, abort any further processing.
    if (!IsLogSeverityEnabled(severity))
    {
        return;
    }

    // Convert all supplied arguments to a string representation
    const size_t argCount = sizeof...(Args);
    std::wstring argsResolved[argCount];
    ResolveArgs(argsResolved, std::forward<Args>(args)...);

    // Resolve the format string and argument values down to a single string
    std::wstring messageResolved = ResolveFormatString(formatString, argCount, argsResolved);

    // Process the log message
    ProcessLogMessage(severity, messageResolved.data(), messageResolved.size());
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::LogInternal(Severity severity, const std::string& formatString, Args&&... args) const
{
    // If no log targets are set to log messages of the target severity, abort any further processing.
    if (!IsLogSeverityEnabled(severity))
    {
        return;
    }

    // Convert all supplied arguments to a string representation
    const size_t argCount = sizeof...(Args);
    std::wstring argsResolved[argCount];
    ResolveArgs(argsResolved, std::forward<Args>(args)...);

    // Resolve the format string and argument values down to a single string
    std::wstring formatStringConverted = Utf8StringToWString(formatString);
    std::wstring messageResolved = ResolveFormatString(formatStringConverted, argCount, argsResolved);

    // Process the log message
    ProcessLogMessage(severity, messageResolved.data(), messageResolved.size());
}

//----------------------------------------------------------------------------------------------------------------------
template<class... Args>
void ILogger::LogInternal(Severity severity, const std::wstring& formatString, Args&&... args) const
{
    // If no log targets are set to log messages of the target severity, abort any further processing.
    if (!IsLogSeverityEnabled(severity))
    {
        return;
    }

    // Convert all supplied arguments to a string representation
    const size_t argCount = sizeof...(Args);
    std::wstring argsResolved[argCount];
    ResolveArgs(argsResolved, std::forward<Args>(args)...);

    // Resolve the format string and argument values down to a single string
    std::wstring messageResolved = ResolveFormatString(formatString, argCount, argsResolved);

    // Process the log message
    ProcessLogMessage(severity, messageResolved.data(), messageResolved.size());
}

//----------------------------------------------------------------------------------------------------------------------
// Format string methods
//----------------------------------------------------------------------------------------------------------------------
std::wstring ILogger::ResolveFormatString(const std::wstring& formatString, size_t argCount, const std::wstring* formatStringArgs) const
{
    // Allocate a variable to hold our resolved string content
    std::wstring resolvedString;
    resolvedString.reserve(formatString.size() * 2);

    // Parse the format string, storing literal text and argument values in our resolved string.
    size_t formatStringCurrentPos = 0;
    while (formatStringCurrentPos < formatString.size())
    {
        // Attempt to find the next argument insert position
        size_t formatStringStartPos = formatString.find_first_of(L'{', formatStringCurrentPos);
        if (formatStringStartPos == std::wstring::npos)
        {
            break;
        }
        size_t formatStringEndPos = formatString.find_first_of(L'}', formatStringStartPos);
        if (formatStringEndPos == std::wstring::npos)
        {
            break;
        }

        // Retrieve the argument index number
        size_t argIndex;
        std::wstring argIndexAsString = formatString.substr(formatStringStartPos + 1, formatStringEndPos - (formatStringStartPos + 1));
        argIndex = (size_t)std::stoi(argIndexAsString);

        // Ensure the argument index number is valid
        assert(argIndex < argCount);

        // Append any text leading up to this argument in the format string, and the string representation of the target
        // argument.
        resolvedString.append(formatString.data() + formatStringCurrentPos, formatStringStartPos - formatStringCurrentPos);
        if (argIndex >= argCount)
        {
            resolvedString.append(L"{MISSING ARG " + argIndexAsString + L"}");
        }
        else
        {
            resolvedString.append(formatStringArgs[argIndex]);
        }

        // Advance past the argument in the format string
        formatStringCurrentPos = formatStringEndPos + 1;
    }
    resolvedString.append(formatString.data() + formatStringCurrentPos);

    // Return the resolved string to the caller
    return resolvedString;
}

//----------------------------------------------------------------------------------------------------------------------
template<class T>
void ILogger::ResolveArgs(std::wstring* argAsString, T&& arg) const
{
    ArgResolver<typename std::remove_cv<typename std::remove_reference<T>::type>::type>::ResolveArg(arg, *argAsString);
}

//----------------------------------------------------------------------------------------------------------------------
template<class T, class... Args>
void ILogger::ResolveArgs(std::wstring* argAsString, T&& arg, Args&&... args) const
{
    ArgResolver<typename std::remove_cv<typename std::remove_reference<T>::type>::type>::ResolveArg(arg, *(argAsString++));
    ResolveArgs(argAsString, std::forward<Args>(args)...);
}

//----------------------------------------------------------------------------------------------------------------------
// String conversion methods
//----------------------------------------------------------------------------------------------------------------------
std::string ILogger::WStringToUtf8String(const std::wstring& wideString)
{
    // Convert the encoding of the supplied string
    std::string stringUTF8;
    size_t sourceStringPos = 0;
    size_t sourceStringSize = wideString.size();
    stringUTF8.reserve(sourceStringSize * 2);
    while (sourceStringPos < sourceStringSize)
    {
        // Check if a surrogate pair is used for this character
        bool usesSurrogatePair = (((unsigned int)wideString[sourceStringPos] & 0xF800) == 0xD800);

        // Ensure that the requested number of code units are left in the source string
        if (usesSurrogatePair && ((sourceStringPos + 2) > sourceStringSize))
        {
            break;
        }

        // Decode the character from UTF-16 encoding
        unsigned int unicodeCodePoint;
        if (usesSurrogatePair)
        {
            unicodeCodePoint = 0x10000 + ((((unsigned int)wideString[sourceStringPos] & 0x03FF) << 10) | ((unsigned int)wideString[sourceStringPos + 1] & 0x03FF));
        }
        else
        {
            unicodeCodePoint = (unsigned int)wideString[sourceStringPos];
        }

        // Encode the character into UTF-8 encoding
        if (unicodeCodePoint <= 0x7F)
        {
            stringUTF8.push_back((char)unicodeCodePoint);
        }
        else if (unicodeCodePoint <= 0x07FF)
        {
            char convertedCodeUnit1 = (char)(0xC0 | (unicodeCodePoint >> 6));
            char convertedCodeUnit2 = (char)(0x80 | (unicodeCodePoint & 0x3F));
            stringUTF8.push_back(convertedCodeUnit1);
            stringUTF8.push_back(convertedCodeUnit2);
        }
        else if (unicodeCodePoint <= 0xFFFF)
        {
            char convertedCodeUnit1 = (char)(0xE0 | (unicodeCodePoint >> 12));
            char convertedCodeUnit2 = (char)(0x80 | ((unicodeCodePoint >> 6) & 0x3F));
            char convertedCodeUnit3 = (char)(0x80 | (unicodeCodePoint & 0x3F));
            stringUTF8.push_back(convertedCodeUnit1);
            stringUTF8.push_back(convertedCodeUnit2);
            stringUTF8.push_back(convertedCodeUnit3);
        }
        else
        {
            char convertedCodeUnit1 = (char)(0xF0 | (unicodeCodePoint >> 18));
            char convertedCodeUnit2 = (char)(0x80 | ((unicodeCodePoint >> 12) & 0x3F));
            char convertedCodeUnit3 = (char)(0x80 | ((unicodeCodePoint >> 6) & 0x3F));
            char convertedCodeUnit4 = (char)(0x80 | (unicodeCodePoint & 0x3F));
            stringUTF8.push_back(convertedCodeUnit1);
            stringUTF8.push_back(convertedCodeUnit2);
            stringUTF8.push_back(convertedCodeUnit3);
            stringUTF8.push_back(convertedCodeUnit4);
        }

        // Advance past the converted code units
        sourceStringPos += (usesSurrogatePair) ? 2 : 1;
    }

    // Return the converted string to the caller
    return stringUTF8;
}

//----------------------------------------------------------------------------------------------------------------------
std::wstring ILogger::Utf8StringToWString(const std::string& stringUtf8)
{
    // Ensure the wchar_t type is a supported size (2-byte = UTF-16, 4-byte = UTF-32)
    static_assert((sizeof(wchar_t) == 2) || (sizeof(wchar_t) == 4), "Expected wchar_t to be 2 or 4 bytes");

    // Convert the encoding of the supplied string. We assume UTF8 encoding here, which will also work for basic ASCII
    // without codepage usage. Any codepage conversion is on the caller, as we can't know or understand the myriad of
    // encodings that may be used in this case.
    std::wstring stringAsWideString;
    size_t sourceStringPos = 0;
    size_t sourceStringSize = stringUtf8.size();
    stringAsWideString.reserve(sourceStringSize);
    while (sourceStringPos < sourceStringSize)
    {
        // Determine the number of code units required for the next character
        static const unsigned int codeUnitCountLookup[] = {1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 4};
        unsigned int codeUnitCount = codeUnitCountLookup[(uint8_t)stringUtf8[sourceStringPos] >> 4];

        // Ensure that the requested number of code units are left in the source string
        if ((sourceStringPos + codeUnitCount) > sourceStringSize)
        {
            break;
        }

        // Convert the encoding of this character
        switch (codeUnitCount)
        {
        case 1:
        {
            stringAsWideString.push_back((wchar_t)stringUtf8[sourceStringPos]);
            break;
        }
        case 2:
        {
            unsigned int unicodeCodePoint = (((unsigned int)stringUtf8[sourceStringPos] & 0x1F) << 6) | ((unsigned int)stringUtf8[sourceStringPos + 1] & 0x3F);
            stringAsWideString.push_back((wchar_t)unicodeCodePoint);
            break;
        }
        case 3:
        {
            unsigned int unicodeCodePoint = (((unsigned int)stringUtf8[sourceStringPos] & 0x0F) << 12) | (((unsigned int)stringUtf8[sourceStringPos + 1] & 0x3F) << 6) | ((unsigned int)stringUtf8[sourceStringPos + 2] & 0x3F);
            stringAsWideString.push_back((wchar_t)unicodeCodePoint);
            break;
        }
        case 4:
        {
            unsigned int unicodeCodePoint = (((unsigned int)stringUtf8[sourceStringPos] & 0x07) << 18) | (((unsigned int)stringUtf8[sourceStringPos + 1] & 0x3F) << 12) | (((unsigned int)stringUtf8[sourceStringPos + 2] & 0x3F) << 6) | ((unsigned int)stringUtf8[sourceStringPos + 3] & 0x3F);
            if constexpr (sizeof(wchar_t) == 2)
            {
                wchar_t convertedCodeUnit1 = 0xD800 | (((unicodeCodePoint - 0x10000) >> 10) & 0x03FF);
                wchar_t convertedCodeUnit2 = 0xDC00 | ((unicodeCodePoint - 0x10000) & 0x03FF);
                stringAsWideString.push_back(convertedCodeUnit1);
                stringAsWideString.push_back(convertedCodeUnit2);
            }
            else
            {
                stringAsWideString.push_back((wchar_t)unicodeCodePoint);
            }
            break;
        }
        }

        // Advance past the converted code units
        sourceStringPos += codeUnitCount;
    }

    // Return the converted string to the caller
    return stringAsWideString;
}

//----------------------------------------------------------------------------------------------------------------------
// Argument resolver
//----------------------------------------------------------------------------------------------------------------------
template<>
class ILogger::ArgResolver<float, false, true>
{
public:
    inline static void ResolveArg(float arg, std::wstring& argResolved);
};

//----------------------------------------------------------------------------------------------------------------------
template<>
class ILogger::ArgResolver<double, false, true>
{
public:
    inline static void ResolveArg(double arg, std::wstring& argResolved);
};

//----------------------------------------------------------------------------------------------------------------------
template<>
class ILogger::ArgResolver<std::string, false, false>
{
public:
    inline static void ResolveArg(const std::string& arg, std::wstring& argResolved);
};

//----------------------------------------------------------------------------------------------------------------------
template<>
class ILogger::ArgResolver<std::wstring, false, false>
{
public:
    inline static void ResolveArg(const std::wstring& arg, std::wstring& argResolved);
};

//----------------------------------------------------------------------------------------------------------------------
template<class T>
void ILogger::ArgResolver<T, false, false>::ResolveArg(const T& arg, std::wstring& argResolved)
{
    std::wostringstream stringStream;
    stringStream << arg;
    argResolved = stringStream.str();
}

//----------------------------------------------------------------------------------------------------------------------
template<class T>
void ILogger::ArgResolver<T, false, true>::ResolveArg(T arg, std::wstring& argResolved)
{
    std::wostringstream stringStream;
    stringStream << arg;
    argResolved = stringStream.str();
}

//----------------------------------------------------------------------------------------------------------------------
template<class T>
void ILogger::ArgResolver<T, true, false>::ResolveArg(T arg, std::wstring& argResolved)
{
    std::wostringstream stringStream;
    stringStream << (unsigned int)arg;
    argResolved = stringStream.str();
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::ArgResolver<float, false, true>::ResolveArg(float arg, std::wstring& argResolved)
{
    std::wostringstream stringStream;
    stringStream << std::fixed << arg;
    argResolved = stringStream.str();
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::ArgResolver<double, false, true>::ResolveArg(double arg, std::wstring& argResolved)
{
    std::wostringstream stringStream;
    stringStream << std::fixed << arg;
    argResolved = stringStream.str();
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::ArgResolver<std::string, false, false>::ResolveArg(const std::string& arg, std::wstring& argResolved)
{
    argResolved = Utf8StringToWString(arg);
}

//----------------------------------------------------------------------------------------------------------------------
void ILogger::ArgResolver<std::wstring, false, false>::ResolveArg(const std::wstring& arg, std::wstring& argResolved)
{
    argResolved = arg;
}

//----------------------------------------------------------------------------------------
// Enumeration operators
//----------------------------------------------------------------------------------------
inline ILogger::SeverityFilter operator|(ILogger::SeverityFilter left, ILogger::SeverityFilter right)
{
    return (ILogger::SeverityFilter)((std::underlying_type<ILogger::SeverityFilter>::type)left | (std::underlying_type<ILogger::SeverityFilter>::type)right);
}

//----------------------------------------------------------------------------------------
inline ILogger::SeverityFilter& operator|=(ILogger::SeverityFilter& left, ILogger::SeverityFilter right)
{
    left = (left | right);
    return left;
}

//----------------------------------------------------------------------------------------
inline ILogger::SeverityFilter operator&(ILogger::SeverityFilter left, ILogger::SeverityFilter right)
{
    return (ILogger::SeverityFilter)((std::underlying_type<ILogger::SeverityFilter>::type)left & (std::underlying_type<ILogger::SeverityFilter>::type)right);
}

//----------------------------------------------------------------------------------------
inline ILogger::SeverityFilter& operator&=(ILogger::SeverityFilter& left, ILogger::SeverityFilter right)
{
    left = (left & right);
    return left;
}
