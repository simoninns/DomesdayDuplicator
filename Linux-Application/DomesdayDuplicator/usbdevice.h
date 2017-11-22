#ifndef USBDEVICE_H
#define USBDEVICE_H

#include <QObject>
#include <QDebug>
#include "QtUsb/qusb.h"

const quint8 USB_PIPE_IN = 0x81;        // Bulk output endpoint for responses
const quint8 USB_PIPE_OUT = 0x01;       // Bulk input endpoint for commands
const quint16 USB_TIMEOUT_MSEC = 300;   // USB timeout in microseconds

// VID and PID of the Domesday Duplicator USB device
#define VID 0x1D50
#define PID 0x603B

class usbDevice : public QObject
{
    Q_OBJECT
public:
    explicit usbDevice(QObject *parent = 0);
    ~usbDevice(void);
    bool isConnected(void);

signals:
    void statusChanged(bool status);

public slots:
    void onDevInserted(QtUsb::FilterList list);
    void onDevRemoved(QtUsb::FilterList list);

private:
    QUsbManager mUsbManager;

};

#endif // USBDEVICE_H
