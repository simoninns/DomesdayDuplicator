#include "domesdayduplicator.h"
#include "ui_domesdayduplicator.h"

#include "usbdevice.h"

// Class constructor
domesdayDuplicator::domesdayDuplicator(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::domesdayDuplicator)
{
    ui->setupUi(this);

    // Set the window's title
    this->setWindowTitle(tr("Domesday Duplicator"));

    // Add a label to the status bar for showing the status
    status = new QLabel;
    ui->statusBar->addWidget(status);
    status->setText(tr("No duplicator connected"));

    // Set the transfer status in the transfer button text
    ui->transferButton->setText(tr("Start transfer"));
    transferInProgressFlag = false;

    // Show start up in debug
    qDebug() << "Domesday duplicator main window running";

    // Open a device
    domDupDevice = new usbDevice();

    // Detect any connected devices
    int numberOfDevicesFound = 0;
    numberOfDevicesFound = domDupDevice->detectDevices();

    // If a device was found connect to the first device (only supports one device!)
    if (numberOfDevicesFound > 0) {
        domDupDevice->connectDevice(0);
        status->setText(tr("Duplicator connected"));
    }

    // We need to support device hot-plug and hot-unplug detection, otherwise this won't
    // make much sense to the user... Still; keeping it simple for initial testing...
}

// Class destructor
domesdayDuplicator::~domesdayDuplicator()
{
    delete ui;
}

// Transfer button triggered
void domesdayDuplicator::on_transferButton_clicked()
{
    if (transferInProgressFlag) {
        // Stop transfer
        transferInProgressFlag = false;
        ui->transferButton->setText(tr("Start transfer"));

        qDebug() << "domesdayDuplicator::on_transferButton_clicked() - Stopping transfer";
        domDupDevice->stopTransfer();
    } else {
        // Start transfer
        transferInProgressFlag = true;
        ui->transferButton->setText(tr("Stop transfer"));

        qDebug() << "domesdayDuplicator::on_transferButton_clicked() - Starting transfer";
        domDupDevice->startTransfer();
    }
}
