#pragma once
#include "ILogger.h"
#include <memory>

class QtLogger final : public ILogger
{
public:
    // Nested types
    typedef std::unique_ptr<QtLogger, ILogger::Deleter> unique_ptr;

public:
    // Constructors
    static QtLogger::unique_ptr Create(SeverityFilter filter);

    // Delete method
    void Delete() override;

protected:
    // Severity methods
    bool IsLogSeverityEnabledInternal(Severity severity) const override;

    // Logging methods
    void ProcessLogMessage(Severity severity, const wchar_t* message, size_t messageLength) const override;

private:
    // Constructors
    inline QtLogger(SeverityFilter filter);

private:
    SeverityFilter filter;
};
