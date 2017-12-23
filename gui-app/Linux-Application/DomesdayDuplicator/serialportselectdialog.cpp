#include "serialportselectdialog.h"
#include "ui_serialportselectdialog.h"

serialPortSelectDialog::serialPortSelectDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::serialPortSelectDialog)
{
    ui->setupUi(this);

    // Flag that the serial port is not configured and set defaults
    currentSettings.configured = false;
    currentSettings.name = QString(tr("None"));
    currentSettings.baudRate = QSerialPort::Baud1200;
    currentSettings.dataBits = QSerialPort::Data8;
    currentSettings.parity = QSerialPort::NoParity;
    currentSettings.stopBits = QSerialPort::OneStop;
    currentSettings.flowControl = QSerialPort::NoFlowControl;

    // Populate the port information
    fillPortsInfo();
}

serialPortSelectDialog::~serialPortSelectDialog()
{
    delete ui;
}

// Return the current serial port settings
serialPortSelectDialog::Settings serialPortSelectDialog::settings() const
{
    return currentSettings;
}

// Populate the combo box with the available serial ports
void serialPortSelectDialog::fillPortsInfo()
{
    ui->serialPortSelectComboBox->clear();

    const auto infos = QSerialPortInfo::availablePorts();

    // Add additional "None" option to allow de-selection of COM port
    ui->serialPortSelectComboBox->addItem(QString(tr("None")), QString(tr("None")));

    for (const QSerialPortInfo &info : infos) {
        QStringList list;
        list << info.portName();
        ui->serialPortSelectComboBox->addItem(list.first(), list);
    }

    // Set the current selection to the currently selected serial port
    ui->serialPortSelectComboBox->setCurrentIndex(ui->serialPortSelectComboBox->findData(currentSettings.name));

    // Configure the BPS radio buttons
    if (currentSettings.baudRate == QSerialPort::Baud1200) {
        ui->bps1200RadioButton->setChecked(true);
        ui->bps2400RadioButton->setChecked(false);
        ui->bps9600RadioButton->setChecked(false);
    }

    if (currentSettings.baudRate == QSerialPort::Baud2400) {
        ui->bps1200RadioButton->setChecked(false);
        ui->bps2400RadioButton->setChecked(true);
        ui->bps9600RadioButton->setChecked(false);
    }

    if (currentSettings.baudRate == QSerialPort::Baud9600) {
        ui->bps1200RadioButton->setChecked(false);
        ui->bps2400RadioButton->setChecked(false);
        ui->bps9600RadioButton->setChecked(true);
    }
}

// Update the serial settings
void serialPortSelectDialog::updateSettings()
{
    if (ui->serialPortSelectComboBox->currentText().isEmpty() ||
            ui->serialPortSelectComboBox->currentText() == QString(tr("None"))) {
        // No serial port is selected
        currentSettings.name = ui->serialPortSelectComboBox->currentText();
        currentSettings.configured = false;
        qDebug() << "serialPortSelectDialog::updateSettings(): Serial port not configured";
    } else {
        currentSettings.name = ui->serialPortSelectComboBox->currentText();
        if (ui->bps1200RadioButton->isChecked()) currentSettings.baudRate = QSerialPort::Baud1200;
        if (ui->bps2400RadioButton->isChecked()) currentSettings.baudRate = QSerialPort::Baud2400;
        if (ui->bps9600RadioButton->isChecked()) currentSettings.baudRate = QSerialPort::Baud9600;
        currentSettings.dataBits = QSerialPort::Data8;
        currentSettings.parity = QSerialPort::NoParity;
        currentSettings.stopBits = QSerialPort::OneStop;
        currentSettings.flowControl = QSerialPort::NoFlowControl;
        currentSettings.configured = true;
        qDebug() << "serialPortSelectDialog::updateSettings(): Serial port configured";

        // Emit a signal indicating that the serial configuration has changed
        emit serialPortChanged();
    }
}

// Function called when user accepts serial configuration
void serialPortSelectDialog::on_buttonBox_accepted()
{
    updateSettings();
    hide();
}

void serialPortSelectDialog::showEvent(QShowEvent *e)
{
    qDebug() << "serialPortSelectDialog::showEvent(): Event triggered";

    // Update the available serial ports
    fillPortsInfo();
}
