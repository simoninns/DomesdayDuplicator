#pragma once
#include "UsbDeviceBase.h"
#include <atomic>
#include <condition_variable>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winusb.h>

class UsbDeviceWinUsb final : public UsbDeviceBase
{
public:
    // Constructors
    UsbDeviceWinUsb(const ILogger& log);
    ~UsbDeviceWinUsb() override;
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
        OVERLAPPED overlappedStructure = {};
        size_t diskBufferIndex;
        size_t diskBufferTransferIndex;
        size_t transferBufferByteOffset;
        bool lastTransferInDiskBuffer = false;
        bool transferSubmitted = false;
    };

private:
    // Device methods
    bool ConnectToDevice(const std::wstring& deviceInstancePath, HANDLE& deviceHandle, WINUSB_INTERFACE_HANDLE& winUsbInterfaceHandle) const;
    void DisconnectFromDevice(HANDLE deviceHandle, WINUSB_INTERFACE_HANDLE winUsbInterfaceHandle) const;
    bool GetDeviceInstancePathsByInterface(GUID* interfaceGuid, std::vector<std::wstring>& deviceInstancePaths) const;
    bool GetAllDomesdayDeviceInstancePaths(std::vector<std::wstring>& deviceInstancePaths) const;
    bool FindDomesdayDeviceInstancePath(const std::wstring& preferredDevicePath, std::wstring& deviceInstancePath) const;

private:
    // Settings
    uint16_t targetDeviceVendorId = 0;
    uint16_t targetDeviceProductId = 0;

    // Device connection
    bool connectedToDevice = false;
    HANDLE captureUsbDeviceHandle = nullptr;
    WINUSB_INTERFACE_HANDLE captureWinUsbInterfaceHandle = nullptr;
    UCHAR captureWinUsbBulkInPipeId = 0;
    size_t connectedBulkPipeMaxPacketSizeInBytes = 0;
    size_t captureWinUsbBulkInMaxTransferSizeInBytes = 0;

    // Data transfer
    std::vector<TransferBufferEntry> transferBuffers;
};
