#pragma once
#include "UsbDeviceBase.h"
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <libusb.h>

class UsbDeviceLibUsb final : public UsbDeviceBase
{
public:
    // Constructors
    UsbDeviceLibUsb(const ILogger& log);
    ~UsbDeviceLibUsb() override;
    bool Initialize(uint16_t vendorId, uint16_t productId) override;

    // Device methods
    bool DevicePresent(const std::string& preferredDevicePath) const override;
    bool GetPresentDevicePaths(std::vector<std::string>& devicePaths) const override;

protected:
    // Device methods
    bool DeviceConnected() const override;
    bool ConnectToDevice(const std::string& preferredDevicePath) override;
    void DisconnectFromDevice() override;
    bool SendVendorSpecificCommand(const std::string& preferredDevicePath, uint8_t command, uint16_t value) override;

    // Capture methods
    void CalculateDesiredBufferCountAndSize(bool useSmallUsbTransfers, size_t usbTransferQueueSizeInBytes, size_t diskBufferQueueSizeInBytes, size_t& bufferCount, size_t& bufferSizeInBytes) const override;
    void UsbTransferThread() override;

private:
    // Structures
    struct TransferBufferEntry
    {
        UsbDeviceLibUsb* object;
        libusb_transfer* transfer;
        size_t diskBufferIndex;
        size_t diskBufferTransferIndex;
        size_t transferBufferByteOffset;
        size_t diskBufferIncrementOnCompletion;
        size_t diskBufferCount;
        bool lastTransferInDiskBuffer;
        bool transferSubmitted;
        bool transferCancelled;
    };

private:
    // Device methods
    bool ConnectToDevice(const std::string& preferredDevicePath, libusb_device*& device, libusb_device_handle*& deviceHandle) const;
    void DisconnectFromDevice(libusb_device* device, libusb_device_handle* deviceHandle) const;
    bool GetDevicePath(libusb_device* device, std::string& devicePath) const;
    bool GetAllDomesdayDevices(libusb_device** usbDevicesRaw, std::vector<libusb_device*>& domesdayDevices) const;

    // Capture methods
    static void LIBUSB_CALL BulkTransferCallbackStatic(libusb_transfer* transfer);
    void BulkTransferCallback(libusb_transfer* transfer, TransferBufferEntry* transferUserData);

private:
    // Settings
    uint16_t targetDeviceVendorId = 0;
    uint16_t targetDeviceProductId = 0;

    // Device connection
    bool connectedToDevice = false;
    libusb_context* libUsbContext = nullptr;
    libusb_device* captureUsbDevice = nullptr;
    libusb_device_handle* captureUsbDeviceHandle = nullptr;
    uint8_t connectedBulkPipeInId = 0;
    size_t connectedBulkPipeMaxPacketSizeInBytes = 0;

    // Data transfer
    bool captureComplete = false;
    bool transferFailure = false;
    size_t transfersInFlight = 0;
    size_t dumpedTransferCount = 0;
    size_t targetDumpedTransferCount = 0;
};
