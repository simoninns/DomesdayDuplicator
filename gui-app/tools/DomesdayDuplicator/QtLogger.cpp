#include "QtLogger.h"
#include <QDebug>

//----------------------------------------------------------------------------------------------------------------------
// Constructors
//----------------------------------------------------------------------------------------------------------------------
QtLogger::QtLogger(SeverityFilter filter)
: filter(filter)
{ }

//----------------------------------------------------------------------------------------------------------------------
QtLogger::unique_ptr QtLogger::Create(SeverityFilter filter)
{
    return QtLogger::unique_ptr(new QtLogger(filter));
}

//----------------------------------------------------------------------------------------------------------------------
// Delete method
//----------------------------------------------------------------------------------------------------------------------
void QtLogger::Delete()
{
    delete this;
}

//----------------------------------------------------------------------------------------------------------------------
// Severity methods
//----------------------------------------------------------------------------------------------------------------------
bool QtLogger::IsLogSeverityEnabledInternal(Severity severity) const
{
    return ((unsigned int)filter & (unsigned int)severity) != 0;
}

//----------------------------------------------------------------------------------------------------------------------
// Logging methods
//----------------------------------------------------------------------------------------------------------------------
void QtLogger::ProcessLogMessage(Severity severity, const wchar_t* message, size_t messageLength) const
{
    switch (severity)
    {
    case Severity::Critical:
    case Severity::Error:
        qCritical() << QString::fromUtf8(WStringToUtf8String(std::wstring(message, messageLength)));
        break;
    case Severity::Warning:
        qWarning() << QString::fromUtf8(WStringToUtf8String(std::wstring(message, messageLength)));
        break;
    case Severity::Info:
        qInfo() << QString::fromUtf8(WStringToUtf8String(std::wstring(message, messageLength)));
        break;
    case Severity::Debug:
    case Severity::Trace:
        qDebug() << QString::fromUtf8(WStringToUtf8String(std::wstring(message, messageLength)));
        break;
    }
}
