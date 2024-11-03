/************************************************************************

    mainwindow.h

    Capture application for the Domesday Duplicator
    DomesdayDuplicator - LaserDisc RF sampler
    Copyright (C) 2018-2019 Simon Inns

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
#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QDate>
#include <QTimer>
#include <QMessageBox>
#include "aboutdialog.h"
#include "configurationdialog.h"
#include "configuration.h"
#include "UsbDeviceBase.h"
#include "playercommunication.h"
#include "playercontrol.h"
#include "playerremotedialog.h"
#include "automaticcapturedialog.h"
#include "advancednamingdialog.h"
#include "amplitudemeasurement.h"
#include "ILogger.h"
#include <chrono>
#include <filesystem>
#include <optional>
#include <memory>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const ILogger& log, QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void configurationChangedSignalHandler();
    void remoteControlCommandSignalHandler(PlayerRemoteDialog::RemoteButtons button);
    void remoteControlSearchSignalHandler(qint32 position, PlayerRemoteDialog::PositionMode positionMode);
    void remoteControlManualSerialCommandHandler(QString commandString);
    void startAutomaticCaptureDialogSignalHandler(AutomaticCaptureDialog::CaptureType captureType,
                                                              qint32 startAddress, qint32 endAddress,
                                                              AutomaticCaptureDialog::DiscType discTypeParam);
    void stopAutomaticCaptureDialogSignalHandler();
    void updateAutomaticCaptureStatus();
    void automaticCaptureCompleteSignalHandler(bool success);

    void playerConnectedSignalHandler();
    void playerDisconnectedSignalHandler();

    void startCaptureSignalHandler();
    void stopCaptureSignalHandler();

    void updateCaptureStatus();
    void updateDeviceStatus();
    void updatePlayerControlInformation();
    void updateStorageInformation();
    void updateAmplitudeDataBuffer();
    void updateAmplitudeLabel();

    void on_actionExit_triggered();
    void on_actionTest_mode_toggled(bool arg1);
    void on_actionAbout_triggered();
    void on_actionPreferences_triggered();
    void on_capturePushButton_clicked();
    void on_actionPlayer_remote_triggered();
    void on_actionAutomatic_capture_triggered();
    void on_limitDurationCheckBox_stateChanged(int arg1);
    void on_actionAdvanced_naming_triggered();

private:
    struct AmplitudeRecord
    {
        std::chrono::time_point<std::chrono::steady_clock> sampleTime;
        double amplitude;
    };
    struct TimeCodeRecord
    {
        std::chrono::time_point<std::chrono::steady_clock> sampleTime;
        qint32 timeCodeOrFrameNumber;
    };
    struct PlayerStatusRecord
    {
        std::chrono::time_point<std::chrono::steady_clock> sampleTime;
        PlayerCommunication::PlayerState playerState;
    };

private:
    void StopCapture();
    void StartCapture();

private:
    const ILogger& log;
    std::unique_ptr<Configuration> configuration;
    std::unique_ptr<UsbDeviceBase> usbDevice;
    std::unique_ptr<QLabel> usbStatusLabel;
    std::unique_ptr<QStorageInfo> storageInfo;

    std::unique_ptr<Ui::MainWindow> ui;
    std::unique_ptr<AboutDialog> aboutDialog;
    std::unique_ptr<AutomaticCaptureDialog> automaticCaptureDialog;
    std::unique_ptr<ConfigurationDialog> configurationDialog;
    std::unique_ptr<PlayerRemoteDialog> playerRemoteDialog;
    std::unique_ptr<PlayerControl> playerControl;
    std::unique_ptr<AdvancedNamingDialog> advancedNamingDialog;
    std::unique_ptr<AmplitudeMeasurement> amplitudeMeasurement;

    std::atomic<bool> isCaptureRunning = false;
    bool isCaptureStopping = false;
    bool playerStopRequested = false;
    std::filesystem::path captureFilePath;
    std::unique_ptr<QTimer> captureStatusUpdateTimer;
    std::unique_ptr<QTimer> playerControlTimer;
    std::unique_ptr<QTimer> deviceStatusTimer;
    std::unique_ptr<QTimer> automaticCaptureTimer;
    std::unique_ptr<QTimer> storageInfoTimer;
    std::unique_ptr<QTimer> amplitudeTimer;
    std::chrono::time_point<std::chrono::steady_clock> captureStartTime;
    std::chrono::time_point<std::chrono::steady_clock> captureEndTime;
    std::vector<AmplitudeRecord> amplitudeRecord;
    std::vector<TimeCodeRecord> playerTimeCodeRecord;
    std::vector<PlayerStatusRecord> playerStatusRecord;

    bool isPlayerConnected = false;
    bool usbDevicePresentLastCheck = false;
    bool isPlayerConnectedLastCheck = false;
    std::atomic<PlayerCommunication::DiscType> playerDiscTypeCached = PlayerCommunication::DiscType::unknownDiscType;
    std::atomic<qint32> minPlayerTimeCode = -1;
    std::atomic<qint32> maxPlayerTimeCode = -1;
    std::atomic<qint32> minPlayerFrameNumber = -1;
    std::atomic<qint32> maxPlayerFrameNumber = -1;

    // Saved disk naming/notes metadata
    std::optional<QString> namingDiskTitle;
    std::optional<bool> namingDiskIsCav;
    std::optional<bool> namingDiskIsNtsc;
    std::optional<int> namingDiskSide;
    std::optional<AdvancedNamingDialog::AudioType> namingDiskAudioType;
    std::optional<QString> namingDiskMintMarks;
    std::optional<QString> namingDiskNotes;
    std::optional<QString> namingDiskMetadataNotes;

    // Remote control states
    PlayerCommunication::DisplayState remoteDisplayState;
    PlayerCommunication::AudioState remoteAudioState;
    qint32 remoteSpeed;
    PlayerCommunication::ChapterFrameMode remoteChapterFrameMode;

    void startPlayerControl();
    void updatePlayerRemoteDialog();
    void updateAmplitudeUI();

signals:
    void plotAmplitude();
    void bufferAmplitude();
};
