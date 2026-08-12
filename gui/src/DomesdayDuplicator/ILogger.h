#pragma once
#include <string>
#include <type_traits>

class ILogger
{
public:
    // Enumerations
    enum class Severity : unsigned int
    {
        Critical = 0x01, // Fatal error or application crash
        Error    = 0x02, // Recoverable error
        Warning  = 0x04, // Noncritical problem
        Info     = 0x08, // Informational message
        Debug    = 0x10, // Debugging info. Not recorded by default.
        Trace    = 0x20, // Trace info. Logged to file but not displayed by default.
    };
    enum class SeverityFilter : unsigned int
    {
        None             = 0,
        Critical         = int(Severity::Critical),
        Error            = int(Severity::Error),
        Warning          = int(Severity::Warning),
        Info             = int(Severity::Info),
        Trace            = int(Severity::Trace),
        Debug            = int(Severity::Debug),
        CriticalOrHigher = Critical,
        ErrorOrHigher    = Critical | Error,
        WarningOrHigher  = Critical | Error | Warning,
        InfoOrHigher     = Critical | Error | Warning | Info,
        DebugOrHigher    = Critical | Error | Warning | Info | Debug,
        TraceOrHigher    = Critical | Error | Warning | Info | Debug | Trace,
        All              = TraceOrHigher,
    };

public:
    // Nested types
    struct Deleter
    {
        inline void operator()(ILogger* target)
        {
            target->Delete();
        }
    };

public:
    // Delete method
    virtual void Delete() = 0;

    // Severity methods
    inline bool IsLogSeverityEnabled(Severity severity) const;

    // Generic logging methods
    inline void Log(Severity severity, const std::string& message) const;
    inline void Log(Severity severity, const std::wstring& message) const;
    template<class... Args>
    void Log(Severity severity, const std::string& formatString, Args&&... args) const;
    template<class... Args>
    void Log(Severity severity, const std::wstring& formatString, Args&&... args) const;

    // Severity logging methods
    inline void Critical(const char* message) const;
    inline void Critical(const wchar_t* message) const;
    inline void Critical(const std::string& message) const;
    inline void Critical(const std::wstring& message) const;
    template<class... Args>
    void Critical(const char* formatString, Args&&... args) const;
    template<class... Args>
    void Critical(const wchar_t* formatString, Args&&... args) const;
    template<class... Args>
    void Critical(const std::string& formatString, Args&&... args) const;
    template<class... Args>
    void Critical(const std::wstring& formatString, Args&&... args) const;
    inline void Error(const char* message) const;
    inline void Error(const wchar_t* message) const;
    inline void Error(const std::string& message) const;
    inline void Error(const std::wstring& message) const;
    template<class... Args>
    void Error(const char* formatString, Args&&... args) const;
    template<class... Args>
    void Error(const wchar_t* formatString, Args&&... args) const;
    template<class... Args>
    void Error(const std::string& formatString, Args&&... args) const;
    template<class... Args>
    void Error(const std::wstring& formatString, Args&&... args) const;
    inline void Warning(const char* message) const;
    inline void Warning(const wchar_t* message) const;
    inline void Warning(const std::string& message) const;
    inline void Warning(const std::wstring& message) const;
    template<class... Args>
    void Warning(const char* formatString, Args&&... args) const;
    template<class... Args>
    void Warning(const wchar_t* formatString, Args&&... args) const;
    template<class... Args>
    void Warning(const std::string& formatString, Args&&... args) const;
    template<class... Args>
    void Warning(const std::wstring& formatString, Args&&... args) const;
    inline void Info(const char* message) const;
    inline void Info(const wchar_t* message) const;
    inline void Info(const std::string& message) const;
    inline void Info(const std::wstring& message) const;
    template<class... Args>
    void Info(const char* formatString, Args&&... args) const;
    template<class... Args>
    void Info(const wchar_t* formatString, Args&&... args) const;
    template<class... Args>
    void Info(const std::string& formatString, Args&&... args) const;
    template<class... Args>
    void Info(const std::wstring& formatString, Args&&... args) const;
    inline void Debug(const char* message) const;
    inline void Debug(const wchar_t* message) const;
    inline void Debug(const std::string& message) const;
    inline void Debug(const std::wstring& message) const;
    template<class... Args>
    void Debug(const char* formatString, Args&&... args) const;
    template<class... Args>
    void Debug(const wchar_t* formatString, Args&&... args) const;
    template<class... Args>
    void Debug(const std::string& formatString, Args&&... args) const;
    template<class... Args>
    void Debug(const std::wstring& formatString, Args&&... args) const;
    inline void Trace(const char* message) const;
    inline void Trace(const wchar_t* message) const;
    inline void Trace(const std::string& message) const;
    inline void Trace(const std::wstring& message) const;
    template<class... Args>
    void Trace(const char* formatString, Args&&... args) const;
    template<class... Args>
    void Trace(const wchar_t* formatString, Args&&... args) const;
    template<class... Args>
    void Trace(const std::string& formatString, Args&&... args) const;
    template<class... Args>
    void Trace(const std::wstring& formatString, Args&&... args) const;

protected:
    // Constructors
    ~ILogger() = default;
    ILogger() = default;

    // Severity methods
    virtual bool IsLogSeverityEnabledInternal(Severity severity) const = 0;

    // Logging methods
    virtual void ProcessLogMessage(Severity severity, const wchar_t* message, size_t messageLength) const = 0;

    // String conversion methods
    inline static std::wstring Utf8StringToWString(const std::string& stringUtf8);
    inline static std::string WStringToUtf8String(const std::wstring& wideString);

private:
    // Argument resolver
    template<class T, bool IsEnum = std::is_enum<T>::value, bool IsIntegral = std::is_integral<T>::value>
    class ArgResolver
    {};
    template<class T>
    class ArgResolver<T, false, false>
    {
    public:
        inline static void ResolveArg(const T& arg, std::wstring& argResolved);
    };
    template<class T>
    class ArgResolver<T, false, true>
    {
    public:
        inline static void ResolveArg(T arg, std::wstring& argResolved);
    };
    template<class T>
    class ArgResolver<T, true, false>
    {
    public:
        inline static void ResolveArg(T arg, std::wstring& argResolved);
    };

private:
    // Logging methods
    inline void LogInternal(Severity severity, const char* message) const;
    inline void LogInternal(Severity severity, const wchar_t* message) const;
    inline void LogInternal(Severity severity, const std::string& message) const;
    inline void LogInternal(Severity severity, const std::wstring& message) const;
    template<class... Args>
    void LogInternal(Severity severity, const char* formatString, Args&&... args) const;
    template<class... Args>
    void LogInternal(Severity severity, const wchar_t* formatString, Args&&... args) const;
    template<class... Args>
    void LogInternal(Severity severity, const std::string& formatString, Args&&... args) const;
    template<class... Args>
    void LogInternal(Severity severity, const std::wstring& formatString, Args&&... args) const;

    // Format string methods
    inline std::wstring ResolveFormatString(const std::wstring& formatString, size_t argCount, const std::wstring* formatStringArgs) const;
    template<class T>
    void ResolveArgs(std::wstring* argAsString, T&& arg) const;
    template<class T, class... Args>
    void ResolveArgs(std::wstring* argAsString, T&& arg, Args&&... args) const;
};

#include "ILogger.inl"
