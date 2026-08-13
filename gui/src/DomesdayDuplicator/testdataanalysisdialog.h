/************************************************************************

    testdataanalysisdialog.h

    Capture application for the Domesday Duplicator
    DomesdayDuplicator - LaserDisc RF sampler

    Step 4 of the capture-integrity procedure (TESTING.md §5), moved here from dddutil when
    P7-12 removed that application. Two entry points share one implementation:

      - this dialog, for someone at the bench with a capture they have just taken;
      - RunHeadless(), behind --analyse-test-data, so the T5 gate can be scripted rather
        than clicked.

    The check itself is in src/common/testdataanalyser.h and knows nothing about Qt or
    files; this is the file reading, the threading and the progress reporting around it.

    This file is part of the Domesday Duplicator.
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#ifndef TESTDATAANALYSISDIALOG_H
#define TESTDATAANALYSISDIALOG_H

#include <QDialog>
#include <QString>
#include <QThread>
#include <atomic>
#include <memory>

#include "testdataanalyser.h"

namespace Ui
{
class TestDataAnalysisDialog;
}

// Runs the analysis off the UI thread. A 30 GB capture takes minutes to read, and doing
// that in the event loop would leave the application looking hung for the duration.
class TestDataAnalysisWorker : public QThread
{
    Q_OBJECT

public:
    explicit TestDataAnalysisWorker(QString filePath, QObject *parent = nullptr);

    void requestCancel();

signals:
    void progress(int percentage, qint64 samplesChecked);
    void finished(bool ok, QString message);

protected:
    void run() override;

private:
    QString filePath;
    std::atomic<bool> cancelRequested{ false };
};

class TestDataAnalysisDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TestDataAnalysisDialog(QWidget *parent = nullptr);
    ~TestDataAnalysisDialog() override;

    // Ask for a file and analyse it, reporting the result in the dialog
    void chooseFileAndAnalyse(const QString &startingDirectory);

    // The --analyse-test-data implementation. Prints a one-line verdict and returns a
    // process exit code: 0 for an intact ramp, 1 for a break, 2 for a file that could not
    // be read at all — three outcomes a script can tell apart, rather than a bare boolean.
    static int RunHeadless(const QString &filePath);

private slots:
    void onProgress(int percentage, qint64 samplesChecked);
    void onFinished(bool ok, QString message);
    void onCancelClicked();

private:
    void startAnalysis(const QString &filePath);

private:
    std::unique_ptr<Ui::TestDataAnalysisDialog> ui;
    TestDataAnalysisWorker *worker = nullptr;
};

#endif // TESTDATAANALYSISDIALOG_H
