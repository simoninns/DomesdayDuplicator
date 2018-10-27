
/************************************************************************

    main.cpp

    dddconv - Domesday Duplicator capture file conversion
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

************************************************************************/

#include <QCoreApplication>
#include <QDebug>
#include <QtGlobal>
#include <QCommandLineParser>

#include "dataconversion.h"

// Global for debug output
static bool showDebug = false;

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
        if (context.file != nullptr) fprintf(stderr, "Info: [%s:%d] %s\n", context.file, context.line, localMsg.constData());
        else fprintf(stderr, "Info: %s\n", localMsg.constData());
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

    QCoreApplication a(argc, argv);

    // Set application name and version
    QCoreApplication::setApplicationName("dddconv");
    QCoreApplication::setApplicationVersion("1.0");
    QCoreApplication::setOrganizationDomain("domesday86.com");

    // Set up the command line parser
    QCommandLineParser parser;
    parser.setApplicationDescription(
                "Domesday Duplicator file conversion utility\n"
                "\n"
                "(c)2018 Simon Inns\n"
                "GPLv3 Open-Source - https://www.domesday86.com");
    parser.addHelpOption();
    parser.addVersionOption();

    // Option to show debug (-d)
    QCommandLineOption showDebugOption(QStringList() << "d" << "debug",
                                       QCoreApplication::translate("main", "Show debug"));
    parser.addOption(showDebugOption);

    // Option to specify input video file (-i)
    QCommandLineOption sourceVideoFileOption(QStringList() << "i" << "input",
                QCoreApplication::translate("main", "Specify input video file"),
                QCoreApplication::translate("main", "file"));
    parser.addOption(sourceVideoFileOption);

    // Option to specify output video file (-o)
    QCommandLineOption targetVideoFileOption(QStringList() << "o" << "output",
                QCoreApplication::translate("main", "Specify output video file"),
                QCoreApplication::translate("main", "file"));
    parser.addOption(targetVideoFileOption);

    // Option to unpack 10-bit data (-u)
    QCommandLineOption showUnpackOption(QStringList() << "u" << "unpack",
                                       QCoreApplication::translate("main", "Unpack 10-bit data into 16-bit (default)"));
    parser.addOption(showUnpackOption);

    // Option to unpack 10-bit data (-u)
    QCommandLineOption showPackOption(QStringList() << "p" << "pack",
                                       QCoreApplication::translate("main", "Pack 16-bit data into 10-bit"));
    parser.addOption(showPackOption);

    // Process the command line arguments given by the user
    parser.process(a);

    // Get the configured settings from the parser
    bool isDebugOn = parser.isSet(showDebugOption);
    bool isUnpacking = parser.isSet(showUnpackOption);
    bool isPacking = parser.isSet(showPackOption);
    QString inputFileName = parser.value(sourceVideoFileOption);
    QString outputFileName = parser.value(targetVideoFileOption);

    // Process the command line options
    if (isDebugOn) showDebug = true;

    bool modeUnpack = true;
    if (isPacking) modeUnpack = false;

    // Check that both pack and unpack are not set
    if (isUnpacking && isPacking) {
        // Quit with error
        qFatal("Specify only --unpack (-u) or --pack (-p) - not both!");
    }

    // Initialise the data conversion object
    DataConversion dataConversion(inputFileName, outputFileName, !modeUnpack);

    // Process the data conversion
    dataConversion.process();

    // Quit with success
    return 0;
}
