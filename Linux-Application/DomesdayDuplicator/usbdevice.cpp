#include "usbdevice.h"

// Class constructor
usbDevice::usbDevice()
{
    // Initialise class private variables
    usbDeviceOpenFlag = false;
    numberOfDevices = 0;
    selectedDevice = 0;
    transferFlag = false;
}

// Class destructor
usbDevice::~usbDevice()
{
    // Perform a cyusb close if an open device is present
    if (usbDeviceOpenFlag) cyusb_close();

    qDebug() << "usbDevice::~usbDevice(): USB device closed";
}

// Function looks for any connected devices and returns the number of found devices
int usbDevice::detectDevices(void)
{
    // Ensure that a device is not already connected
    if (usbDeviceOpenFlag) {
        qDebug() << "usbDevice::detectDevices(): Called, but device already open";
        return numberOfDevices;
    }

    // Look for duplicator device with VID=0x1D50 and PID=0x603B
    numberOfDevices = cyusb_open(0x1D50, 0x603B);

    // If no devices are found, close the cyusb instance
    if (numberOfDevices < 1) {
        qDebug() << "usbDevice::detectDevices(): No USB devices detected";
        cyusb_close();
        numberOfDevices = 0;
    } else {
        qDebug() << "usbDevice::detectDevices():" << numberOfDevices << "device(s) detected";
    }

    return numberOfDevices;
}

// Function connects to the specified device
void usbDevice::connectDevice(int deviceNumber)
{
    // Check that a device isn't already open
    if (usbDeviceOpenFlag) {
        qDebug() << "usbDevice::connectDevice(): Called, but device already open";
        return;
    }

    // Check that the requested device number is valid
    if (deviceNumber > numberOfDevices) {
        qDebug() << "usbDevice::connectDevice(): Requested device number is invalid";
        return;
    }

    int numberOfInterfaces, index = 0;
    int apiResponse;
    struct libusb_config_descriptor *configDescriptor = NULL;
    cyusb_handle *deviceHandle = NULL;

    // Get the device handle
    deviceHandle = cyusb_gethandle(deviceNumber);

    // Output the selected device's information to debug
    qDebug() << "usbDevice::connectDevice(): Device is" <<
        "VID=" << QString("0x%1").arg(cyusb_getvendor(deviceHandle) , 0, 16) <<
        "PID=" << QString("0x%1").arg(cyusb_getproduct(deviceHandle) , 0, 16) <<
        "BusNum=" << QString("0x%1").arg(cyusb_get_busnumber(deviceHandle) , 0, 16) <<
        "Addr=" << QString("0x%1").arg(cyusb_get_devaddr(deviceHandle) , 0, 16);

    // Get the configuration descriptor for the selected device
    apiResponse = cyusb_get_active_config_descriptor(deviceHandle, &configDescriptor);
    if (apiResponse) {
        qDebug() << "usbDevice::connectDevice(): cyusb_get_active_config_descriptor returned ERROR";
        return;
    }

    // Detach any active kernel drives for the device's interfaces
    numberOfInterfaces = configDescriptor->bNumInterfaces;
    while (numberOfInterfaces) {
        if (cyusb_kernel_driver_active(deviceHandle, index)) {
            cyusb_detach_kernel_driver(deviceHandle, index);
        }
        index++;
        numberOfInterfaces--;
    }
    cyusb_free_config_descriptor(configDescriptor);

    // Flag the device as open
    usbDeviceOpenFlag = true;
}

// Start transfer of data from device
void usbDevice::startTransfer(void)
{
    // Ensure a device is connected
    if (!usbDeviceOpenFlag) {
        qDebug() << "usbDevice::startTransfer(): Called, but no device is open";
        return;
    }

    if (transferFlag) {
        // Transfer already in progress!
        qDebug() << "usbDevice::startTransfer(): Called, but transfer is already in progress";
    } else {
        transferFlag = true;
        qDebug() << "usbDevice::startTransfer(): Transfer starting";

        // Set vendor specific (0x40) request 0xB5 with value 1 to start USB device transfer
        cyusb_handle *deviceHandle = NULL;
        deviceHandle = cyusb_gethandle(selectedDevice);
        cyusb_control_transfer(deviceHandle, 0x40, 0xB5, 0x01, 0x00, 0x00, 0x00, 1000);

        // Become the user mode driver
        // Claim the BULK in interface
        // Perform a BULK transfer

        // cyusb_detach_kernel_driver
        // cyusb_claim_interface
        // cyusb_bulk_transfer
    }
}

// Stop transfer of data from device
void usbDevice::stopTransfer(void)
{
    if (!transferFlag) {
        // Transfer not in progress!
        qDebug() << "usbDevice::stopTransfer(): Called, but transfer is not in progress";
    } else {
        transferFlag = false;
        qDebug() << "usbDevice::stopTransfer(): Transfer stopping";

        // Set vendor specific request 0xB5 with value 0 to stop USB device transfer
        cyusb_handle *deviceHandle = NULL;
        deviceHandle = cyusb_gethandle(selectedDevice);
        cyusb_control_transfer(deviceHandle, 0x40, 0xB5, 0x00, 0x00, 0x00, 0x00, 1000);
    }
}
