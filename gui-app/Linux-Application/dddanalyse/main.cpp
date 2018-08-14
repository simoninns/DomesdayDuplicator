/************************************************************************

    main.cpp

    LaserDisc RF sample analysis utility
    DomesdayDuplicator - LaserDisc RF sampler

    Copyright (C) 2018 Simon Inns

    This file is part of Domesday Duplicator.

    Domesday Duplicator is free software: you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Email: simon.inns@gmail.com

************************************************************************/

#include <QCoreApplication>
#include <QDebug>
#include <QtGlobal>
#include <QCommandLineParser>

#include "dddanalyse.h"

// Global for debug output
extern bool showDebug;
bool showDebug = false;

// Qt debug message handler
void debugOutputHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Use:
    // context.file - to show the filename
    // context.line - to show the line number
    // context.function - to show the function name

    QByteArray localMsg = msg.toLocal8Bit();
    switch (type) {
    case QtDebugMsg: // These are debug messages meant for developers
        if (showDebug) {
            // If the code was compiled as 'release' the context.file will be NULL
            if (context.file != nullptr) fprintf(stderr, "Debug: [%s:%d] %s\n", context.file, context.line, localMsg.constData());
            else fprintf(stderr, "Debug: %s\n", localMsg.constData());
        }
        break;
    case QtInfoMsg: // These are information messages meant for end-users
        fprintf(stderr, "%s\n", localMsg.constData());
        break;
    case QtWarningMsg:
        if (context.file != nullptr) fprintf(stderr, "Warning: [%s:%d] %s\n", context.file, context.line, localMsg.constData());
        else fprintf(stderr, "Warning: %s\n", localMsg.constData());
        break;
    case QtCriticalMsg:
        if (context.file != nullptr) fprintf(stderr, "Critical: [%s:%d] %s\n", context.file, context.line, localMsg.constData());
        else fprintf(stderr, "Critical: %s\n", localMsg.constData());
        break;
    case QtFatalMsg:
        if (context.file != nullptr) fprintf(stderr, "Fatal: [%s:%d] %s\n", context.file, context.line, localMsg.constData());
        else fprintf(stderr, "Fatal: %s\n", localMsg.constData());
        abort();
    }
}

int main(int argc, char *argv[])
{
    // Install the local debug message handler
    qInstallMessageHandler(debugOutputHandler);

    // Define the core application
    QCoreApplication app(argc, argv);

    // General command line options parser set-up
    QCoreApplication::setApplicationName("ld-rectify");
    QCoreApplication::setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription(
                "DomesdayDuplicator LaserDisc RF sample analysis utility\n"
                "Part of the DomesdayDuplicator project\n"
                "(c)2018 Simon Inns\n"
                "GPLv3 Open-Source - github: https://github.com/simoninns/DomesdayDuplicator");
    parser.addHelpOption();
    parser.addVersionOption();

    // Option to show debug (-d)
    QCommandLineOption showDebugOption(QStringList() << "d" << "show-debug",
                                       QCoreApplication::translate("main", "Show debug"));
    parser.addOption(showDebugOption);

    // Option to specify input RF sample file (-i)
    QCommandLineOption sourceSampleFileOption(QStringList() << "i" << "source-sample-file",
                QCoreApplication::translate("main", "Specify input RF sample file"),
                QCoreApplication::translate("main", "file"));
    parser.addOption(sourceSampleFileOption);

    // Option to specify output sample file (-o)
//    QCommandLineOption targetSampleFileOption(QStringList() << "o" << "target-sample-file",
//                QCoreApplication::translate("main", "Specify output RF sample file"),
//                QCoreApplication::translate("main", "file"));
//    parser.addOption(targetSampleFileOption);

    // Option to select test data verification mode (-t)
//    QCommandLineOption testDataModeOption(QStringList() << "t" << "test-data",
//                                     QCoreApplication::translate("main", "Test data verification mode"));
//    parser.addOption(testDataModeOption);

    // Process the command line arguments given by the user
    parser.process(app);

    // Get the configured settings from the parser
    bool isDebugOn = parser.isSet(showDebugOption);
    QString sourceSampleFileName = parser.value(sourceSampleFileOption);
    //QString targetSampleFileName = parser.value(targetSampleFileOption);
    //bool isTestDataMode = parser.isSet(testDataModeOption);

    // Process the command line options
    if (isDebugOn) showDebug = true;

    // Contruct the TBC object
    DddAnalyse dddAnalyse;

    // Set the input sample rate
    //if (isTestDataMode) dddAnalyse.setTestDataMode();

    // Set the source video file name
    if (!sourceSampleFileName.isEmpty()) dddAnalyse.setSourceSampleFileName(sourceSampleFileName);

    // Set the target video file name
    //if (!targetSampleFileName.isEmpty()) dddAnalyse.setTargetSampleFileName(targetSampleFileName);

    // Process the sample file
    dddAnalyse.processSample();

    // All done
    //return a.exec();
    return 0;
}
