/************************************************************************

    usbdevice.cpp

    Capture application for the Domesday Duplicator
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

#include "usbdevice.h"

// Static function for handling libUSB callback for USB device attached events
static int LIBUSB_CALL hotplug_callback_attach(libusb_context *ctx, libusb_device *dev, libusb_hotplug_event event, void *user_data)
{
    UsbDevice *usbDevice = static_cast<UsbDevice *>(user_data);
    struct libusb_device_descriptor desc;
    qint32 responseCode;

    (void)ctx;
    (void)dev;
    (void)event;
    (void)user_data;

    qDebug() << "hotplug_callback_attach(): A Domesday Duplicator USB device has been attached";

    responseCode = libusb_get_device_descriptor(dev, &desc);
    if (responseCode != LIBUSB_SUCCESS) {
        qDebug() << "hotplug_callback_attach(): Error getting device descriptor!";
    }

    qDebug() << "hotplug_callback_attach(): Device attached with VID =" << desc.idVendor << "and PID =" << desc.idProduct;

    // Send a signal indicating a device is attached
    emit usbDevice->deviceAttached();

    return 0;
}

// Static function for handling libUSB callback for USB device detached events
static int LIBUSB_CALL hotplug_callback_detach(libusb_context *ctx, libusb_device *dev, libusb_hotplug_event event, void *user_data)
{
    UsbDevice *usbDevice = static_cast<UsbDevice *>(user_data);
    struct libusb_device_descriptor desc;
    qint32 responseCode;

    (void)ctx;
    (void)dev;
    (void)event;
    (void)user_data;

    qDebug() << "hotplug_callback_detach(): A Domesday Duplicator USB device has been detached";

    responseCode = libusb_get_device_descriptor(dev, &desc);
    if (responseCode != LIBUSB_SUCCESS) {
        qDebug() << "hotplug_callback_detach(): Error getting device descriptor!";
    }

    qDebug() << "hotplug_callback_detach(): Device detached with VID =" << desc.idVendor << "and PID =" << desc.idProduct;

    // Send a signal indicating a device is detached
    emit usbDevice->deviceDetached();

    return 0;
}

// Class constructor
UsbDevice::UsbDevice(QObject *parent, quint16 vid, quint16 pid) : QThread (parent)
{
    qint32 responseCode;
    libusb_hotplug_callback_handle hotplugHandle[2];

    // Store the VID and PID of the target device
    deviceVid = vid;
    devicePid = pid;

    // Set up the libUSB event polling thread flags
    threadAbort = false;

    // Initialise libUSB
    responseCode = libusb_init(&libUsbContext);
    if (responseCode < 0) {
        qDebug() << "UsbDevice::UsbDevice(): Could not initialise libUSB library!";
        libusb_exit(libUsbContext);
        return;
    }

    // Verify that hot-plug is supported on this platform
    if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
        qDebug() << "UsbDevice::UsbDevice(): libUSB reports that this platform does not support hot-plug!";
        libusb_exit(libUsbContext);
        return;
    }

    // Register the USB device attached callback
    responseCode = libusb_hotplug_register_callback(libUsbContext, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, LIBUSB_HOTPLUG_NO_FLAGS,
                                                    deviceVid, devicePid, LIBUSB_HOTPLUG_MATCH_ANY, hotplug_callback_attach,
                                                    static_cast<void*>(this), &hotplugHandle[0]);
    if (LIBUSB_SUCCESS != responseCode) {
        qDebug() << "UsbDevice::UsbDevice(): Could not register USB device attached callback to libUSB!";
        libusb_exit(libUsbContext);
        return;
    }

    // Register the USB device detached callback
    responseCode = libusb_hotplug_register_callback(libUsbContext, LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, LIBUSB_HOTPLUG_NO_FLAGS,
                                                    deviceVid, devicePid, LIBUSB_HOTPLUG_MATCH_ANY, hotplug_callback_detach,
                                                    static_cast<void*>(this), &hotplugHandle[1]);
    if (LIBUSB_SUCCESS != responseCode) {
        qDebug() << "UsbDevice::UsbDevice(): Could not register USB device detached callback to libUSB!";
        libusb_exit(libUsbContext);
        return;
    }

    // Now start a thread to poll the libUSB event handler (otherwise no callbacks will happen)
    connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
    this->start();
}

// Class destructor
UsbDevice::~UsbDevice()
{
    // Stop the libUSB event processing thread
    threadAbort = true;
    this->wait();
}

// Run the hot-plug detection thread
void UsbDevice::run(void)
{
    qint32 responseCode;

    // Process until the thread abort flag is set
    qDebug() << "UsbDevice::run(): libUSB event poll thread started";
    while(!threadAbort) {
        // Process the libUSB events
        responseCode = libusb_handle_events_completed(libUsbContext, nullptr);
        if (responseCode < 0) {
            qDebug() << "UsbDevice::run(): libusb_handle_events returned an error!";
        }

        // Sleep the thread for 0.25 seconds to save CPU
        this->msleep(250);
    }

    qDebug() << "UsbDevice::run(): libUSB event poll thread stopped";
}

// Scan for the target USB device
bool UsbDevice::scanForDevice(void)
{
    qDebug() << "UsbDevice::scanForDevice(): Scanning for the USB device...";

    // Attempt to open the USB device
    // Open the USB device
    libusb_device_handle *usbDeviceHandle = open();
    if (usbDeviceHandle == nullptr) return false;

    // Device found, close it and return
    close(usbDeviceHandle);
    emit deviceAttached();
    return true;
}

// Send a configuration command to the USB device
void UsbDevice::sendConfigurationCommand(bool testMode)
{
    quint16 configurationFlags = 0;

    qDebug() << "UsbDevice::sendConfigurationCommand(): Test-mode =" << testMode;

    if (testMode) configurationFlags += 1; // Bit 0: Set test mode
    // Bit 1: Unused
    // Bit 2: Unused
    // Bit 3: Unused
    // Bit 4: Unused

    sendVendorSpecificCommand(0xB6, configurationFlags);
}

// Open the USB device
libusb_device_handle* UsbDevice::open(void)
{
    libusb_device **usbDevices;
    libusb_device *usbDevice;
    libusb_device_handle *usbDeviceHandle = nullptr;

    qint32 responseCode;
    ssize_t deviceCount;

    // Get the number of attached USB devices
    deviceCount = libusb_get_device_list(libUsbContext, &usbDevices);
    if (deviceCount < 0) {
        qDebug() << "UsbDevice::open(): libusb_get_device_list returned an error!";
        return usbDeviceHandle;
    }

    // Search the attached devices for the target device
    qint32 deviceCounter = 0;

    while ((usbDevice = usbDevices[deviceCounter++]) != nullptr) {
        // Get the descriptor for the current USB device
        struct libusb_device_descriptor deviceDescriptor;
        responseCode = libusb_get_device_descriptor(usbDevice, &deviceDescriptor);
        if (responseCode < 0) {
            qDebug() << "UsbDevice::open(): Failed to get USB device descriptor!";
            break;
        }

        // Does the VID and PID match the target device?
        if (deviceVid == deviceDescriptor.idVendor && devicePid == deviceDescriptor.idProduct) {
            // Open the USB device
            responseCode = libusb_open(usbDevice, &usbDeviceHandle);
            if (responseCode < 0) {
                qDebug() << "UsbDevice::open(): Failed to open USB device!";
                usbDeviceHandle = nullptr;
            }

            // Done
            break;
        }
    }

    // Free up the device list
    libusb_free_device_list(usbDevices, 1);

    return usbDeviceHandle;
}

// Close the USB device
void UsbDevice::close(libusb_device_handle *usbDeviceHandle)
{
    libusb_close(usbDeviceHandle);
}

// Set a vendor-specific USB command to the attached device
bool UsbDevice::sendVendorSpecificCommand(quint8 command, quint16 value)
{
    qint32 responseCode;
    bool result = true;

    // Open the USB device
    libusb_device_handle *usbDeviceHandle = open();

    // Did we get a valid device handle?
    if (usbDeviceHandle != nullptr) {
        // Perform a control transfer of type 0x40 (vendor specific command with no data packets)
        responseCode = libusb_control_transfer(usbDeviceHandle, 0x40,
                                               command, value,
                                               0, nullptr, 0, 1000);
        if (responseCode < 0) {
            qDebug() << "UsbDevice::sendVendorSpecificCommand(): Failed to send vendor specific command to USB device";
            result = false;
        }
    }

    // Close the USB device
    close(usbDeviceHandle);

    return result;
}
