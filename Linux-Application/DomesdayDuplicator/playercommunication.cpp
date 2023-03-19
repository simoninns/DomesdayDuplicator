/************************************************************************

    playercommunication.cpp

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

#include "playercommunication.h"

// The timeout for normal commands is 5 seconds (these are commands that
// they player usually responds to quickly, like framecode request).
// The timeout for long commands is 30 seconds (there are commands like
// play and open tray).
#define N_TIMEOUT 5 * 1000
#define L_TIMEOUT 30 * 1000

PlayerCommunication::PlayerCommunication(QObject *parent) : QObject(parent)
{
}

PlayerCommunication::~PlayerCommunication()
{
}

// Supported player commands:
//
// connect(playerType) - Connect to a player
// disconnect(playerType) - Disconnect from a player
//
// getTrayState() - Returns open or closed
// getPlayerState() - Returns stopped, playing, paused or still frame
// getCurrentFrame() - Returns the current frame number
// getCurrentTimeCode() - Returns the current time code
// getDiscType() - Returns the disc type as unknown, CAV or CLV
// getUserCode() - Returns the user-code for the disc
// getMaximumFrameNumber() - Returns the number of frames on the disc
// getMaximumTimeCode() - Returns the length of the disc
//
// setTrayState(state) - Control the disc tray (open or closed)
// setPlayerState(state) - Sets player to stop, play, pause or still frame
//
// step(dir) - Step the player forwards/backwards 1 frame
// scan(dir) - Step the player forwards/backwards many frames
// multiSpeed(dir, speed) - Play forwards/backwards at non-x1 speeds
//
// setFramePosition(frameno) - Set the frame position to frameno
// setTimeCodePosition(timecode) Set the timecode position to timecode
//
// setStopFrame(frameno) - Set the stop register (frames)
// setStopTimeCode(timecode) - Set the stop registed (timecode)
//
// setOnScreenDisplay(state) - Set the on screen display on or off
// setAudio(audioMode) - Set the audio mode (channels 1 and 2 - on or off)
// setKeylock(bool) - Set the panel key lock on or off
//
// Note: All methods provided by this class are blocking.

PlayerCommunication::PlayerType PlayerCommunication::playerCodeToType(const QString& playerCode) const
{
    if (playerCode == "42") {
        return PlayerType::pioneerCLDV5000;
    }
    else if (playerCode == "37") {
        return PlayerType::pioneerCLDV2800;
    }
    else if (playerCode == "27") {
        return PlayerType::pioneerCLDV2600;
    }
    else if (playerCode == "18") {
        return PlayerType::pioneerCLDV2400;
    }
    else if (playerCode == "16") {
        return PlayerType::pioneerLDV4400;
    }
    else if (playerCode == "15") {
        return PlayerType::pioneerLDV4300D;
    }
    else if (playerCode == "07") {
        return PlayerType::pioneerLDV2200;
    }
    else if (playerCode == "06") {
        return PlayerType::pioneerLDV8000;
    }
    else if (playerCode == "05") {
        return PlayerType::pioneerLCV330;
    }
    else if (playerCode == "02") {
        return PlayerType::pioneerLDV4200;
    }
    return PlayerType::unknownPlayerType;
}

QString PlayerCommunication::playerCodeToName(const QString &playerCode) const
{
    if (playerCode == "42") {
        return "Pioneer CLD-V5000";
    }
    else if (playerCode == "37") {
        return "Pioneer CLD-V2800";
    }
    else if (playerCode == "27") {
        return "Pioneer CLD-V2600";
    }
    else if (playerCode == "18") {
        return "Pioneer CLD-V2400";
    }
    else if (playerCode == "16") {
        return "Pioneer LD-V4400";
    }
    else if (playerCode == "15") {
        return "Pioneer LD-V4300D";
    }
    else if (playerCode == "07") {
        return "Pioneer LD-V2200";
    }
    else if (playerCode == "06") {
        return "Pioneer LD-V8000";
    }
    else if (playerCode == "05") {
        return "Pioneer VC-V330";
    }
    else if (playerCode == "02") {
        return "Pioneer LD-V4200";
    }
    return QString("Unknown Player [") + playerCode + "]";
}

// Player connection and disconnection methods ------------------------------------------------------------------------

// Connect to a LaserDisc player
bool PlayerCommunication::connect(QString serialDevice, SerialSpeed serialSpeed)
{
    bool connectionSuccessful = false;

    // Create the serial port object
    serialPort = new QSerialPort();

    // Configure the serial port object
    serialPort->setPortName(serialDevice);
    serialPort->setDataBits(QSerialPort::Data8);
    serialPort->setParity(QSerialPort::NoParity);
    serialPort->setStopBits(QSerialPort::OneStop);
    serialPort->setFlowControl(QSerialPort::NoFlowControl);
    //qDebug() << "PlayerCommunication::connect(): Serial port name is" << serialDevice;

    // Define our set of supported baud rates
    SerialSpeed supportedBaudRates[] = { SerialSpeed::bps9600, SerialSpeed::bps4800, SerialSpeed::bps2400, SerialSpeed::bps1200 };
    QSerialPort::BaudRate supportedBaudRatesNative[] = { QSerialPort::Baud9600, QSerialPort::Baud4800, QSerialPort::Baud2400, QSerialPort::Baud1200 };
    unsigned int supportedBaudRatesCount = sizeof(supportedBaudRates) / sizeof(supportedBaudRates[0]);

    // Determine which baud rates to use in the connection attempt, and the timeout and retry settings.
    unsigned int baudRateIndex = 0;
    unsigned int baudRateSearchEnd = 0;
    unsigned int maxConnectAttempts = 1;
    qint32 serialQueryTimeout = 1500;
    switch (serialSpeed)
    {
    case SerialSpeed::autoDetect:
        baudRateIndex = 0;
        baudRateSearchEnd = supportedBaudRatesCount;
        serialQueryTimeout = 250;
        maxConnectAttempts = 3;
        break;
    case SerialSpeed::bps1200:
        baudRateIndex = 3;
        baudRateSearchEnd = baudRateIndex + 1;
        break;
    case SerialSpeed::bps2400:
        baudRateIndex = 2;
        baudRateSearchEnd = baudRateIndex + 1;
        break;
    case SerialSpeed::bps4800:
        baudRateIndex = 1;
        baudRateSearchEnd = baudRateIndex + 1;
        break;
    case SerialSpeed::bps9600:
        baudRateIndex = 0;
        baudRateSearchEnd = baudRateIndex + 1;
        break;
    }

    // Attempt to connect to the player
    SerialSpeed detectedSerialSpeed = PlayerCommunication::SerialSpeed::autoDetect;
    QString responseString;
    while (!connectionSuccessful && (baudRateIndex < baudRateSearchEnd)) {
        // Configure the baud rate of the serial port
        serialPort->setBaudRate(supportedBaudRatesNative[baudRateIndex]);
        if (!serialPort->open(QIODevice::ReadWrite)) {
            ++baudRateIndex;
            continue;
        }

        // Attempt to retrieve the player version information
        for (unsigned int i = 0; i < maxConnectAttempts; ++i) {
            // Send the "LVP Model Name Request", as documented in the Pioneer Level III User's Manual for various
            // Laserdisc players.
            sendSerialCommand("?X\r");

            // Attempt to retrieve the response to the model name request
            QString response = getSerialResponse(serialQueryTimeout);
            if (!response.startsWith("P15") || (response.size() < 5)) {
                continue;
            }

            // Retrieve the connection information, and flag that we've successfully connected to the player.
            responseString = response;
            detectedSerialSpeed = supportedBaudRates[baudRateIndex];
            connectionSuccessful = true;
            break;
        }

        // If we failed to establish communication with the player, close the serial connection.
        if (!connectionSuccessful) {
            serialPort->close();
        }
        ++baudRateIndex;
    }

    // Did the connection fail?
    if (!connectionSuccessful) {
        //qDebug() << "PlayerCommunication::connect(): Could not connect to LaserDisc player";
        return false;
    }
    qDebug() << "PlayerCommunication::connect(): Response string " << responseString << " received";

    // Save information on the player and the connection
    QString playerCode = responseString.mid(3, 2);
    QString playerVersionNumber = responseString.mid(5);
    PlayerType detectedPlayerType = playerCodeToType(playerCode);
    currentSerialSpeed = detectedSerialSpeed;
    currentPlayerType = detectedPlayerType;
    currentPlayerName = playerCodeToName(playerCode);
    currentPlayerVersionNumber = playerVersionNumber.trimmed();

    return true;
}

// Disconnect from a LaserDisc player
void PlayerCommunication::disconnect()
{
    qDebug() << "PlayerCommunication::disconnect(): Disconnecting from serial port";
    currentPlayerName = "";
    currentPlayerVersionNumber = "";
    currentPlayerType = none;
    currentSerialSpeed = autoDetect;
    serialPort->close();
}

// Player command methods ---------------------------------------------------------------------------------------------

PlayerCommunication::TrayState PlayerCommunication::getTrayState()
{
    sendSerialCommand("?P\r"); // Player active mode request
    QString response = getSerialResponse(N_TIMEOUT);

    // Process response
    if (response.contains("P00")) {
        // P00 = Door open
        return TrayState::open;
    }

    return TrayState::closed;
}

PlayerCommunication::PlayerType PlayerCommunication::getPlayerType()
{
    return currentPlayerType;
}

QString PlayerCommunication::getPlayerName()
{
    return currentPlayerName;
}

QString PlayerCommunication::getPlayerVersionNumber()
{
    return currentPlayerVersionNumber;
}

PlayerCommunication::SerialSpeed PlayerCommunication::getSerialSpeed()
{
    return currentSerialSpeed;
}

PlayerCommunication::PlayerState PlayerCommunication::getPlayerState()
{
    sendSerialCommand("?P\r"); // Player active mode request
    QString response = getSerialResponse(N_TIMEOUT);

    // Process response
    if (response.contains("P00")) {
        // P00 = Door open
        return PlayerState::stop;
    } else if (response.contains("P01")) {
        // P01 = Park
        return PlayerState::stop;
    } else if (response.contains("P02")) {
        // P02 = Set up (preparing to play)
        return PlayerState::play;
    } else if (response.contains("P03")) {
        // P03 = Disc unloading
        return PlayerState::stop;;
    } else if (response.contains("P04")) {
        // P04 = Play
        return PlayerState::play;
    } else if (response.contains("P05")) {
        // P05 = Still (picture displayed as still)
        return PlayerState::stillFrame;
    } else if (response.contains("P06")) {
        // P06 = Pause (paused without picture display)
        return PlayerState::pause;
    } else if (response.contains("P07")) {
        // P07 = Search
        return PlayerState::play;
    } else if (response.contains("P08")) {
        // P08 = Scan
        return PlayerState::play;
    } else if (response.contains("P09")) {
        // P09 = Multi-speed play back
        return PlayerState::play;
    } else if (response.contains("PA5")) {
        // PA5 = Undocumented response, sent at end of disc it seems?
        return PlayerState::play;
    } else {
        // Unknown response
        qDebug() << "PlayerCommunication::getPlayerState(): Unknown response from player to ?P - " << response;
    }

    return PlayerState::unknownPlayerState;
}

// Return the current frame or -1 if communication fails
qint32 PlayerCommunication::getCurrentFrame()
{
    sendSerialCommand("?F\r");
    QString response = getSerialResponse(N_TIMEOUT);

    qint32 frameNumber;
    if (!response.isEmpty()) frameNumber = static_cast<int>(response.left(5).toUInt());
    else frameNumber = -1;

    return frameNumber;
}

// Return the current timeCode or -1 if communication fails
qint32 PlayerCommunication::getCurrentTimeCode()
{
    sendSerialCommand("?F\r");
    QString response = getSerialResponse(N_TIMEOUT);

    qint32 timeCode;
    if (!response.isEmpty()) timeCode = static_cast<int>(response.left(7).toUInt());
    else timeCode = -1;

    return timeCode;
}

PlayerCommunication::DiscType PlayerCommunication::getDiscType()
{
    sendSerialCommand("?D\r");
    QString response = getSerialResponse(N_TIMEOUT);

    // Check for response
    if (response.isEmpty()) {
        qDebug() << "PlayerCommunication::getDiscType(): No response from player";
        return DiscType::unknownDiscType;
    }

    // Process the response
    if (response.at(1) == '0') {
        //qDebug() << "PlayerCommunication::getDiscType(): Disc is CAV";
        return DiscType::CAV;
    } else if (response.at(1) == '1') {
        //qDebug() << "PlayerCommunication::getDiscType(): Disc is CLV";
        return DiscType::CLV;
    }

    //qDebug() << "PlayerCommunication::getDiscType(): Unknown disc type";
    return DiscType::unknownDiscType;
}

QString PlayerCommunication::getUserCode()
{
    sendSerialCommand("$Y\r");
    return getSerialResponse(N_TIMEOUT);
}

qint32 PlayerCommunication::getMaximumFrameNumber()
{
    sendSerialCommand("FR60000SE\r"); // Frame seek to impossible frame number

    // Return the current frame number
    return getCurrentFrame();
}

qint32 PlayerCommunication::getMaximumTimeCode()
{
    sendSerialCommand("FR1595900SE\r"); // Frame seek to impossible time-code frame number

    // Return the current time code
    return getCurrentTimeCode();
}

bool PlayerCommunication::setTrayState(TrayState trayState)
{
    QString response;

    switch(trayState) {
    case TrayState::closed:
        sendSerialCommand("CO\r"); // Open door command
        response = getSerialResponse(L_TIMEOUT);
        break;
    case TrayState::open:
        sendSerialCommand("OP\r"); // Close door command
        response = getSerialResponse(L_TIMEOUT);
        break;
    case TrayState::unknownTrayState:
        qDebug() << "PlayerCommunication::setTrayState(): Invoker used TrayState::unknownTrayState - ignoring";
        break;
    }

    // Check for error in response
    if (response.isEmpty() || response.contains("E", Qt::CaseSensitive)) {
        // Error
        return false;
    }

    // Command response ok
    return true;
}

bool PlayerCommunication::setPlayerState(PlayerState playerState)
{
    QString response;

    switch(playerState) {
        case PlayerState::pause:
            sendSerialCommand("PA\r"); // Pause command
            response = getSerialResponse(N_TIMEOUT);
            break;
        case PlayerState::play:
            sendSerialCommand("PL\r"); // Play command
            response = getSerialResponse(L_TIMEOUT);
            break;
        case PlayerState::playWithStopCodesDisabled:
            sendSerialCommand("PL64RBMF\r"); // Enable audio during multi-speed ("64RB"), multi-speed forward ("MF).  Essentially, play disc with stop codes disabled
            response = getSerialResponse(L_TIMEOUT);
            break;
        case PlayerState::stillFrame:
            sendSerialCommand("ST\r"); // Still frame command
            response = getSerialResponse(N_TIMEOUT);
            break;
        case PlayerState::stop:
            sendSerialCommand("RJ\r"); // Reject (stop) command
            response = getSerialResponse(L_TIMEOUT);
            break;
        case PlayerState::unknownPlayerState:
            qDebug() << "PlayerCommunication::setPlayerState(): Invoker used PlayerState::unknownPlayerState - ignoring";
            break;
    }

    // Check for error in response
    if (response.isEmpty() || response.contains("E", Qt::CaseSensitive)) {
        // Error
        return false;
    }

    // Command response ok
    return true;
}

bool PlayerCommunication::step(Direction direction)
{
    QString response;

    switch(direction) {
        case Direction::forwards:
            sendSerialCommand("SF\r");
            response = getSerialResponse(N_TIMEOUT);
            break;
        case Direction::backwards:
            sendSerialCommand("SR\r");
            response = getSerialResponse(N_TIMEOUT);
            break;
    }

    // Check for error in response
    if (response.isEmpty() || response.contains("E", Qt::CaseSensitive)) {
        // Error
        return false;
    }

    // Command response ok
    return true;
}

bool PlayerCommunication::scan(Direction direction)
{
    QString response;

    switch(direction) {
        case Direction::forwards:
            sendSerialCommand("NF\r");
            response = getSerialResponse(N_TIMEOUT);
            break;
        case Direction::backwards:
            sendSerialCommand("NR\r");
            response = getSerialResponse(N_TIMEOUT);
            break;
    }

    // Check for error in response
    if (response.isEmpty() || response.contains("E", Qt::CaseSensitive)) {
        // Error
        return false;
    }

    // Command response ok
    return true;
}

bool PlayerCommunication::multiSpeed(Direction direction)
{
    QString response;

    switch(direction) {
        case Direction::forwards:
            sendSerialCommand("MF\r");
            response = getSerialResponse(N_TIMEOUT);
            break;
        case Direction::backwards:
            sendSerialCommand("MB\r");
            response = getSerialResponse(N_TIMEOUT);
            break;
    }

    // Check for error in response
    if (response.isEmpty() || response.contains("E", Qt::CaseSensitive)) {
        // Error
        return false;
    }

    // Command response ok
    return true;
}

bool PlayerCommunication::setPositionFrame(qint32 address)
{
    QString response;
    QString command;

    command = QString("FR%1SE\r").arg(address);
    sendSerialCommand(command);
    response = getSerialResponse(L_TIMEOUT);

    // Check for error in response
    if (response.isEmpty() || response.contains("E", Qt::CaseSensitive)) {
        // Error
        return false;
    }

    // Command response ok
    return true;
}

bool PlayerCommunication::setPositionTimeCode(qint32 address)
{
    QString response;
    QString command;

    command = QString("FR%1SE\r").arg(address);
    sendSerialCommand(command);
    response = getSerialResponse(L_TIMEOUT);

    // Check for error in response
    if (response.isEmpty() || response.contains("E", Qt::CaseSensitive)) {
        // Error
        return false;
    }

    // Command response ok
    return true;
}

bool PlayerCommunication::setPositionChapter(qint32 address)
{
    QString response;
    QString command;

    command = QString("CH%1SE\r").arg(address);
    sendSerialCommand(command);
    response = getSerialResponse(L_TIMEOUT);

    // Check for error in response
    if (response.isEmpty() || response.contains("E", Qt::CaseSensitive)) {
        // Error
        return false;
    }

    // Command response ok
    return true;
}

bool PlayerCommunication::setStopFrame(qint32 frame)
{
    QString response;

    (void)frame;
    qDebug() << "PlayerCommunication::setStopFrame(): Called, but function is not implemented yet";

    // Check for error in response
    if (response.isEmpty() || response.contains("E", Qt::CaseSensitive)) {
        // Error
        return false;
    }

    // Command response ok
    return true;
}

bool PlayerCommunication::setStopTimeCode(qint32 timeCode)
{
    QString response;

    (void)timeCode;
    qDebug() << "PlayerCommunication::setStopTimeCode(): Called, but function is not implemented yet";

    // Check for error in response
    if (response.isEmpty() || response.contains("E", Qt::CaseSensitive)) {
        // Error
        return false;
    }

    // Command response ok
    return true;
}

bool PlayerCommunication::setOnScreenDisplay(DisplayState displayState)
{
    QString response;
    QString command;

    if (displayState == PlayerCommunication::DisplayState::off) {
        sendSerialCommand("0DS\r");
        response = getSerialResponse(N_TIMEOUT);
    } else {
        sendSerialCommand("1DS\r");
        response = getSerialResponse(N_TIMEOUT);
    }

    // Check for error in response
    if (response.isEmpty() || response.contains("E", Qt::CaseSensitive)) {
        // Error
        return false;
    }

    // Command response ok
    return true;
}

bool PlayerCommunication::setAudio(AudioState audioState)
{
    QString response;
    qint32 parameter = 7;
    QString command;

    if (audioState == PlayerCommunication::AudioState::audioOff) parameter = 0;
    if (audioState == PlayerCommunication::AudioState::analogCh1) parameter = 1;
    if (audioState == PlayerCommunication::AudioState::analogCh2) parameter = 2;
    if (audioState == PlayerCommunication::AudioState::analogStereo) parameter = 3;
    if (audioState == PlayerCommunication::AudioState::digitalCh1) parameter = 5;
    if (audioState == PlayerCommunication::AudioState::digitalCh2) parameter = 6;
    if (audioState == PlayerCommunication::AudioState::digitalStereo) parameter = 7;

    command = QString("%1AD\r").arg(parameter);
    sendSerialCommand(command);
    getSerialResponse(N_TIMEOUT);

    // Check for error in response
    if (response.isEmpty() || response.contains("E", Qt::CaseSensitive)) {
        // Error
        return false;
    }

    // Command response ok
    return true;
}

bool PlayerCommunication::setKeyLock(KeyLockState keyLockState)
{
    QString response;

    switch(keyLockState) {
        case KeyLockState::locked:
            sendSerialCommand("1KL\r");
            getSerialResponse(N_TIMEOUT);
            break;
        case KeyLockState::unlocked:
            sendSerialCommand("0KL\r");
            getSerialResponse(N_TIMEOUT);
            break;
    }

    // Check for error in response
    if (response.isEmpty() || response.contains("E", Qt::CaseSensitive)) {
        // Error
        return false;
    }

    // Command response ok
    return true;
}

bool PlayerCommunication::setSpeed(qint32 speed)
{
    QString response;
    QString command;

    command = QString("%1SP\r").arg(speed);
    //qDebug() << "PlayerCommunication::setSpeed(): Sending command:" << command;
    sendSerialCommand(command);
    getSerialResponse(N_TIMEOUT);

    // Check for error in response
    if (response.isEmpty() || response.contains("E", Qt::CaseSensitive)) {
        // Error
        return false;
    }

    // Command response ok
    return true;
}

// Serial read/write methods ------------------------------------------------------------------------------------------

// Send a command via the serial connection
void PlayerCommunication::sendSerialCommand(QString command)
{
    //qDebug() << "PlayerCommunication::sendSerialCommand(): Sending command:" << command;
    // Maximum command string length is 20 characters
    serialPort->write(command.toUtf8().left(20));
}

// Receive a response via the serial connection
QString PlayerCommunication::getSerialResponse(qint32 timeoutInMilliseconds)
{
    QString response;

    // Start the response timer
    QElapsedTimer responseTimer;
    responseTimer.start();

    bool responseComplete = false;
    bool responseTimedOut = false;
    while (!responseComplete) {
        // Get any pending serial responses
        if (serialPort->waitForReadyRead(100)) {
            response.append(serialPort->readAll());

            // Check for command response terminator (CR)
            if (response.contains('\r')) responseComplete = true;
        }

        // Check for response timeout
        if ((responseTimer.elapsed() >= static_cast<qint64>(timeoutInMilliseconds)) && (!responseComplete)) {
            responseTimedOut = true;
            responseComplete = true;
            //qDebug() << "PlayerCommunication::getSerialResponse(): Serial response timed out!";
        }
    }

    // If the response timed out, clear the response buffer
    if (responseTimedOut) response.clear();

    return response;
}
