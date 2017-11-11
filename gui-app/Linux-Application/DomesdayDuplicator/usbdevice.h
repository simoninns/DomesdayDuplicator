#ifndef USBDEVICE_H
#define USBDEVICE_H

#include <QDebug>

// Include the USB library wrapper
#include <cyusb.h>

class usbDevice
{
public:
    usbDevice();
    ~usbDevice();

    int detectDevices(void);
    void connectDevice(int);
    void startTransfer(void);
    void stopTransfer(void);

private:
    bool usbDeviceOpenFlag;
    int numberOfDevices;
    int selectedDevice;
    bool transferFlag;
};

#endif // USBDEVICE_H
