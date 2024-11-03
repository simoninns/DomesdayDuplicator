#include "UsbDeviceLibUsb.h"
#include <memory>
#include <functional>

//----------------------------------------------------------------------------------------------------------------------
// Constructors
//----------------------------------------------------------------------------------------------------------------------
UsbDeviceLibUsb::UsbDeviceLibUsb(const ILogger& log)
:UsbDeviceBase(log)
{ }

//----------------------------------------------------------------------------------------------------------------------
UsbDeviceLibUsb::~UsbDeviceLibUsb()
{
    // Ensure we're disconnected from the device
    DisconnectFromDevice();

    // Delete the libUSB context
    if (libUsbContext != nullptr)
    {
        libusb_exit(libUsbContext);
    }
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceLibUsb::Initialize(uint16_t vendorId, uint16_t productId)
{
    // Call the base class initialize method
    if (!UsbDeviceBase::Initialize(vendorId, productId))
    {
        Log().Error("Initialize(): Base class initialization failed");
        return false;
    }

    // Store the target device vendor and product ID
    targetDeviceVendorId = vendorId;
    targetDeviceProductId = productId;

    // Initialise libUSB
#if LIBUSB_API_VERSION >= 0x0100010A
    int libUsbIinitContextReturn = libusb_init_context(&libUsbContext, NULL, 0);
#else
    int libUsbIinitContextReturn = libusb_init(&libUsbContext);
#endif
    if (libUsbIinitContextReturn != 0)
    {
        Log().Error("Initialize(): Could not initialise libUSB library");
        libUsbContext = nullptr;
        return false;
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// Device methods
//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceLibUsb::DevicePresent(const std::string& preferredDevicePath) const
{
    // If we're currently connected to a device, it must be present, so return true.
    if (connectedToDevice)
    {
        return true;
    }

    // Check if there are any usb devices present
    std::vector<std::string> presentDevicePaths;
    if (!GetPresentDevicePaths(presentDevicePaths))
    {
        Log().Trace("devicePresent(): Failed to locate USB device");
        return false;
    }

    // Temporarily connect to the device to verify it's present and connectable
    libusb_device* usbDevice = nullptr;
    libusb_device_handle* usbDeviceHandle = nullptr;
    if (!ConnectToDevice(preferredDevicePath, usbDevice, usbDeviceHandle))
    {
        Log().Warning("devicePresent(): Failed to connect to device");
        return false;
    }
    DisconnectFromDevice(usbDevice, usbDeviceHandle);
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceLibUsb::GetPresentDevicePaths(std::vector<std::string>& devicePaths) const
{
    // Initialize our output list of device paths
    devicePaths.clear();

    // Get the number of attached USB devices
    libusb_device** usbDevicesRaw = nullptr;
    ssize_t deviceCount = libusb_get_device_list(libUsbContext, &usbDevicesRaw);
    if (deviceCount == 0)
    {
        Log().Trace("No USB devices found");
        return false;
    }
    else if (deviceCount < 0)
    {
        Log().Error("libusb_get_device_list failed with error code {0}:{1}", deviceCount, libusb_error_name((int)deviceCount));
        return false;
    }
    std::shared_ptr<libusb_device*> usbDevices(usbDevicesRaw, [](libusb_device** deviceList) { libusb_free_device_list(deviceList, 1); });

    // Filter to a list of all domesday usb devices connected to the system
    std::vector<libusb_device*> matchingDevices;
    if (!GetAllDomesdayDevices(usbDevices.get(), matchingDevices))
    {
        Log().Error("Failed to enumerate USB devices");
        return false;
    }

    // Return our list of device paths to the caller
    for (auto device : matchingDevices)
    {
        std::string devicePath;
        if (!GetDevicePath(device, devicePath))
        {
            Log().Error("Failed to get USB device port information. Skipping");
            continue;
        }
        devicePaths.push_back(devicePath);
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceLibUsb::DeviceConnected() const
{
    return connectedToDevice;
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceLibUsb::ConnectToDevice(const std::string& preferredDevicePath)
{
    // If we're already connected to the device, abort any further processing.
    if (connectedToDevice)
    {
        Log().Warning("connectToDevice(): Already connected to device.");
        return true;
    }
    captureUsbDevice = nullptr;
    captureUsbDeviceHandle = nullptr;

    // Attempt to connect to the device
    if (!ConnectToDevice(preferredDevicePath, captureUsbDevice, captureUsbDeviceHandle))
    {
        Log().Error("connectToDevice(): Failed to connect to device");
        captureUsbDeviceHandle = nullptr;
        return false;
    }
    connectedToDevice = true;
    bool releaseDeviceDisconnector = false;
    std::shared_ptr<void> deviceDisconnector(nullptr,
        [&](void*)
        {
            if (!releaseDeviceDisconnector)
            {
                DisconnectFromDevice();
            }
        });

    // Claim the required USB device interface for the transfer. Note that we need to do this in order to get good
    // information for the "wMaxPacketSize" field from endpoint below.
    int libUsbClaimInterfaceReturn = libusb_claim_interface(captureUsbDeviceHandle, 0);
    if (libUsbClaimInterfaceReturn != 0)
    {
        Log().Error("libusb_claim_interface failed with error code {0}:{1}", libUsbClaimInterfaceReturn, libusb_error_name(libUsbClaimInterfaceReturn));
        return false;
    }
    std::shared_ptr<void> libUsbInterfaceClaimDeallocator(nullptr,
        [&](void*)
        {
            int libUsbReleaseInterfaceReturn = libusb_release_interface(captureUsbDeviceHandle, 0);
            if (libUsbReleaseInterfaceReturn != 0)
            {
                Log().Error("libusb_release_interface failed with error code {0}:{1}", libUsbReleaseInterfaceReturn, libusb_error_name(libUsbReleaseInterfaceReturn));
            }
        });

    // Get the configuraton descriptor from the USB device
    libusb_config_descriptor* configDescriptor = nullptr;
    int libUsbGetActiveConfigDescriptorReturn = libusb_get_active_config_descriptor(captureUsbDevice, &configDescriptor);
    if (libUsbGetActiveConfigDescriptorReturn != 0)
    {
        Log().Error("libusb_get_active_config_descriptor failed with error code {0}:{1}", libUsbGetActiveConfigDescriptorReturn, libusb_error_name(libUsbGetActiveConfigDescriptorReturn));
        return false;
    }

    // Find the pipe info for the bulk in endpoint from the device
    bool foundBulkInPipeInfo = false;
    uint8_t bulkPipeInId = 0;
    size_t bulkPipeMaxPacketSizeInBytes = 0;
    const size_t alternateInterfaceNumber = 0;
    for (size_t interfaceNo = 0; interfaceNo < configDescriptor->bNumInterfaces; ++interfaceNo)
    {
        const libusb_interface& interface = configDescriptor->interface[interfaceNo];
        if (alternateInterfaceNumber <= interface.num_altsetting)
        {
            const libusb_interface_descriptor& interfaceDescriptor = interface.altsetting[alternateInterfaceNumber];
            for (size_t endpointNo = 0; endpointNo < interfaceDescriptor.bNumEndpoints; ++endpointNo)
            {
                const libusb_endpoint_descriptor& endpointDescriptor = interfaceDescriptor.endpoint[endpointNo];
                uint16_t maxPacketSize = endpointDescriptor.wMaxPacketSize;
                uint8_t endpointAddress = endpointDescriptor.bEndpointAddress;
                uint8_t endpointAttributes = endpointDescriptor.bmAttributes;
                if (((endpointAttributes & 0x02) == LIBUSB_ENDPOINT_TRANSFER_TYPE_BULK) && ((endpointAddress & 0x80) == LIBUSB_ENDPOINT_IN))
                {
                    bulkPipeInId = endpointAddress;
                    bulkPipeMaxPacketSizeInBytes = (size_t)maxPacketSize;
                    foundBulkInPipeInfo = true;
                }
            }
        }
    }
    libusb_free_config_descriptor(configDescriptor);
    if (!foundBulkInPipeInfo)
    {
        Log().Error("connectToDevice(): Failed to locate bulk in endpoint for device");
        return false;
    }
    
    // Since the device connection has succeeded, release the scoped destructor and let it live.
    releaseDeviceDisconnector = true;

    // Store our pipe information, and return true to the caller.
    connectedBulkPipeInId = bulkPipeInId;
    connectedBulkPipeMaxPacketSizeInBytes = bulkPipeMaxPacketSizeInBytes;
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceLibUsb::ConnectToDevice(const std::string& preferredDevicePath, libusb_device*& device, libusb_device_handle*& deviceHandle) const
{
    // Initialize the output arguments
    device = nullptr;
    deviceHandle = nullptr;

    // Get the number of attached USB devices
    libusb_device** usbDevicesRaw = nullptr;
    ssize_t deviceCount = libusb_get_device_list(libUsbContext, &usbDevicesRaw);
    if (deviceCount == 0)
    {
        Log().Trace("No USB devices found");
        return false;
    }
    else if (deviceCount < 0)
    {
        Log().Error("libusb_get_device_list failed with error code {0}:{1}", deviceCount, libusb_error_name((int)deviceCount));
        return false;
    }
    std::shared_ptr<libusb_device*> usbDevices(usbDevicesRaw, [](libusb_device** deviceList) { libusb_free_device_list(deviceList, 1); });

    // Filter to a list of all domesday usb devices connected to the system
    std::vector<libusb_device*> matchingDevices;
    if (!GetAllDomesdayDevices(usbDevices.get(), matchingDevices))
    {
        Log().Error("Failed to enumerate USB devices");
        return false;
    }

    // Ensure we found at least one Domesday Duplicator device
    if (matchingDevices.empty())
    {
        Log().Info("No Domesday Duplicator USB devices found");
        return false;
    }

    // Select the device to connect to. If we have a preferred device specified and it is present, we use that,
    // otherwise we default to the first detected device.
    libusb_device* targetDevice = matchingDevices.front();
    if (!preferredDevicePath.empty())
    {
        for (auto matchingDevice : matchingDevices)
        {
            std::string devicePath;
            if (!GetDevicePath(matchingDevice, devicePath))
            {
                Log().Error("Failed to get USB device port information. Skipping");
                continue;
            }
            if (devicePath == preferredDevicePath)
            {
                targetDevice = matchingDevice;
                break;
            }
        }
    }

    // Open the USB device
    libusb_device_handle* deviceHandleRaw = nullptr;
    int libUsbOpenReturn = libusb_open(targetDevice, &deviceHandleRaw);
    if (libUsbOpenReturn != 0)
    {
        Log().Error("libusb_open failed with error code {0}:{1}", libUsbOpenReturn, libusb_error_name(libUsbOpenReturn));
        deviceHandle = nullptr;
        return false;
    }

    // Return the device handles to the caller
    device = targetDevice;
    deviceHandle = deviceHandleRaw;
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
void UsbDeviceLibUsb::DisconnectFromDevice()
{
    // If we're not currently connected to the device, abort any further processing.
    if (!connectedToDevice)
    {
        Log().Error("disconnectFromDevice() called when no device was connected");
        return;
    }

    // Disconnect from the device
    connectedToDevice = false;
    DisconnectFromDevice(captureUsbDevice, captureUsbDeviceHandle);
    captureUsbDevice = nullptr;
    captureUsbDeviceHandle = nullptr;
}

//----------------------------------------------------------------------------------------------------------------------
void UsbDeviceLibUsb::DisconnectFromDevice(libusb_device* device, libusb_device_handle* deviceHandle) const
{
    // Disconnect from the device. This will decrement the internal reference count of the device itself, and should
    // remove it automatically.
    if (deviceHandle != nullptr)
    {
        libusb_close(deviceHandle);
    }
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceLibUsb::GetDevicePath(libusb_device* device, std::string& devicePath) const
{
    // Initialize our output device path
    devicePath.clear();

    // Obtain the bus number and port number list for the USB device
    uint8_t busNumber = libusb_get_bus_number(device);
    bool gotPortNumbers = false;
    const size_t maxPortCount = 7;
    std::vector<uint8_t> portNumbers;
    portNumbers.resize(maxPortCount);
    while (!gotPortNumbers)
    {
        int libUsbGetPortNumbersReturn = libusb_get_port_numbers(device, portNumbers.data(), (int)portNumbers.size());
        if (libUsbGetPortNumbersReturn == LIBUSB_ERROR_OVERFLOW)
        {
            portNumbers.resize(portNumbers.size() * 2);
            continue;
        }
        else if (libUsbGetPortNumbersReturn <= 0)
        {
            Log().Error("libusb_get_port_numbers failed with error code {0}:{1}", libUsbGetPortNumbersReturn, libusb_error_name(libUsbGetPortNumbersReturn));
            break;
        }
        portNumbers.resize(libUsbGetPortNumbersReturn);
        gotPortNumbers = true;
    }
    if (!gotPortNumbers)
    {
        Log().Error("Failed to get USB device port information.");
        return false;
    }

    // Build a path for this device. We build paths here complying with the sysfs USB path structure for linux, but
    // this is just to make it relatable. The libusb API has no support for building or working with these paths,
    // but we need some kind of identifier we can use to logically identify devices which is persistent across
    // reboots to support device selection.
    std::string devicePathTemp = "/sys/bus/usb/devices/" + std::to_string(busNumber) + "-" + std::to_string(portNumbers[0]);
    for (size_t i = 1; i < portNumbers.size(); ++i)
    {
        devicePathTemp += "." + std::to_string(portNumbers[i]);
    }
    devicePath = std::move(devicePathTemp);
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceLibUsb::GetAllDomesdayDevices(libusb_device** usbDevicesRaw, std::vector<libusb_device*>& domesdayDevices) const
{
    // Initialize our list of results
    domesdayDevices.clear();

    // Filter our list of devices down to a set which match our target vendor and product ID
    std::vector<libusb_device*> matchingDevices;
    size_t deviceCounter = 0;
    while (usbDevicesRaw[deviceCounter] != nullptr)
    {
        // Get the descriptor for the next USB device
        libusb_device* usbDevice = usbDevicesRaw[deviceCounter++];
        libusb_device_descriptor deviceDescriptor;
        int libUsbGetDeviceDescriptorReturn = libusb_get_device_descriptor(usbDevice, &deviceDescriptor);
        if (libUsbGetDeviceDescriptorReturn != 0)
        {
            Log().Error("libusb_get_device_descriptor failed with error code {0}:{1}", libUsbGetDeviceDescriptorReturn, libusb_error_name(libUsbGetDeviceDescriptorReturn));
            continue;
        }

        // If the device doesn't match the target vendor and product IDs for the domesday device, skip it.
        if ((deviceDescriptor.idVendor != targetDeviceVendorId) || (deviceDescriptor.idProduct != targetDeviceProductId))
        {
            continue;
        }

        // Ensure the device is connected with an interface that meets our speed requirements
        int libUsbGetDeviceSpeedReturn = libusb_get_device_speed(usbDevice);
        if (libUsbGetDeviceSpeedReturn < LIBUSB_SPEED_HIGH)
        {
            Log().Warning("Skipping USB device because it is only connected with speed {0}", libUsbGetDeviceSpeedReturn);
            continue;
        }

        // Since the target device matches, store it.
        Log().Trace("Found matching device with instance path: {0}");
        matchingDevices.push_back(usbDevice);
    }

    // Return the list of devices to the caller
    domesdayDevices = std::move(matchingDevices);
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceLibUsb::SendVendorSpecificCommand(const std::string& preferredDevicePath, uint8_t command, uint16_t value)
{
    // Connect to the target device
    libusb_device* usbDevice = nullptr;
    libusb_device_handle* usbDeviceHandle = nullptr;
    if (!ConnectToDevice(preferredDevicePath, usbDevice, usbDeviceHandle))
    {
        Log().Error("sendVendorSpecificCommand failed as the device connection attempt failed");
        return false;
    }
    std::shared_ptr<void> scopedDeviceDisconnectHandler(nullptr, [&](void*) { DisconnectFromDevice(usbDevice, usbDeviceHandle); });

    // Claim the required USB device interface for the transfer
    int libUsbClaimInterfaceReturn = libusb_claim_interface(usbDeviceHandle, 0);
    if (libUsbClaimInterfaceReturn != 0)
    {
        Log().Error("libusb_claim_interface failed with error code {0}:{1}", libUsbClaimInterfaceReturn, libusb_error_name(libUsbClaimInterfaceReturn));
        return false;
    }
    std::shared_ptr<void> libUsbInterfaceClaimDeallocator(nullptr,
        [&](void*)
        {
            int libUsbReleaseInterfaceReturn = libusb_release_interface(usbDeviceHandle, 0);
            if (libUsbReleaseInterfaceReturn != 0)
            {
                Log().Error("libusb_release_interface failed with error code {0}:{1}", libUsbReleaseInterfaceReturn, libusb_error_name(libUsbReleaseInterfaceReturn));
            }
        });

    // Perform a control transfer of type 0x40 (vendor specific command with no data packets)
    unsigned int timeout = 0;
    int libUsbControlTransferReturn = libusb_control_transfer(usbDeviceHandle, 0x40, command, value, 0, nullptr, 0, timeout);
    if (libUsbControlTransferReturn < 0)
    {
        Log().Error("libusb_control_transfer failed with error code {0}:{1}", libUsbControlTransferReturn, libusb_error_name(libUsbControlTransferReturn));
        return false;
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// Capture methods
//----------------------------------------------------------------------------------------------------------------------
void UsbDeviceLibUsb::CalculateDesiredBufferCountAndSize(bool useSmallUsbTransfers, size_t usbTransferQueueSizeInBytes, size_t diskBufferQueueSizeInBytes, size_t& bufferCount, size_t& bufferSizeInBytes) const
{
    // Calculate the optimal buffer size and count which will reach the target memory usage while staying within our
    // device communication constraints. We can't query transfer maximums on libusb like we can on WinUSB, so we use
    // a magic value of 2MB, which corresponds with our capped limits and known actual limits on WinUSB. We will
    // either seek to perform solid 2MB transfers, with one transfer per buffer, as in the case of small transfers
    // being disabled, or we will use multiple smaller transfers to fill multiple 2MB buffers in parallel, up to our
    // USB transfer queue limits.
    size_t cappedMaxTransferSizeForPipe = (2 * 1024 * 1024);
    bufferSizeInBytes = (cappedMaxTransferSizeForPipe / connectedBulkPipeMaxPacketSizeInBytes) * connectedBulkPipeMaxPacketSizeInBytes;
    bufferCount = diskBufferQueueSizeInBytes / bufferSizeInBytes;
}

//----------------------------------------------------------------------------------------------------------------------
void UsbDeviceLibUsb::UsbTransferThread()
{
    Log().Info("usbTransferThread(): Starting");

    // Determine how we're going to split our transfers across our disk buffers
    size_t diskBufferCount = GetDiskBufferCount();
    size_t diskBufferSizeInBytes = GetSingleDiskBufferSizeInBytes();
    size_t transferSizeInBytes = 0;
    size_t transfersPerDiskBuffer = 0;
    size_t simultaneousTransfers = 0;
    size_t diskBufferIncrementOnCompletion = 0;
    size_t diskBufferTransferSpan = 0;
    bool useSmallUsbTransfers = GetUseSmallUsbTransfers();
    if (!useSmallUsbTransfers)
    {
        diskBufferTransferSpan = diskBufferCount - 1;
        transferSizeInBytes = diskBufferSizeInBytes;
        transfersPerDiskBuffer = 1;
    }
    else
    {
        const size_t targetSmallTransferBlockSize = 128 * 1024;
        size_t transferBlockSize = (targetSmallTransferBlockSize / connectedBulkPipeMaxPacketSizeInBytes) * connectedBulkPipeMaxPacketSizeInBytes;
        size_t usbTransferQueueSizeInBytes = GetUsbTransferQueueSizeInBytes();
        diskBufferTransferSpan = std::min((usbTransferQueueSizeInBytes / diskBufferSizeInBytes), (diskBufferCount - 2));
        transferSizeInBytes = transferBlockSize;
        transfersPerDiskBuffer = (diskBufferSizeInBytes / transferBlockSize);
    }
    simultaneousTransfers = transfersPerDiskBuffer * diskBufferTransferSpan;
    diskBufferIncrementOnCompletion = diskBufferTransferSpan;

    // Initialize the memory for our transfer buffers
    std::vector<TransferBufferEntry> transferBuffers;
    transferBuffers.resize(simultaneousTransfers);
    size_t transferBuffersSizeInBytes = sizeof(*transferBuffers.data()) * transferBuffers.size();
    LockMemoryBufferIntoPhysicalMemory(transferBuffers.data(), transferBuffersSizeInBytes);
    std::shared_ptr<void> transferBuffersMemoryUnlocker(nullptr, [&](void*) { UnlockMemoryBuffer(transferBuffers.data(), transferBuffersSizeInBytes); });

    // Claim the required USB device interface for the transfer
    int libUsbClaimInterfaceReturn = libusb_claim_interface(captureUsbDeviceHandle, 0);
    if (libUsbClaimInterfaceReturn != 0)
    {
        Log().Error("libusb_claim_interface failed with error code {0}:{1}", libUsbClaimInterfaceReturn, libusb_error_name(libUsbClaimInterfaceReturn));
        SetUsbTransferFinished(TransferResult::UsbTransferFailure);
        return;
    }
    std::shared_ptr<void> libUsbInterfaceClaimDeallocator(nullptr,
        [&](void*)
        {
            int libUsbReleaseInterfaceReturn = libusb_release_interface(captureUsbDeviceHandle, 0);
            if (libUsbReleaseInterfaceReturn != 0)
            {
                Log().Error("libusb_release_interface failed with error code {0}:{1}", libUsbReleaseInterfaceReturn, libusb_error_name(libUsbReleaseInterfaceReturn));
            }
        });

    // Attempt to boost the thread priority to realtime
    ThreadPriorityRestoreInfo threadPriorityRestoreInfo = {};
    if (!SetCurrentThreadRealtimePriority(threadPriorityRestoreInfo))
    {
        Log().Warning("SetCurrentThreadRealtimePriority failed");
    }

    // Initialize our object state that's shared with the callback function
    captureComplete = false;
    transferFailure = false;
    transfersInFlight = 0;

    // Calculate the number of transfers to dump at the start of the capture process. We need to do this, as without
    // multiple read requests initially queued, we expect some buffer underflows when getting the read requests spun up.
    const size_t diskBufferCountToDump = 4;
    targetDumpedTransferCount = std::min(diskBufferCount, diskBufferCountToDump) * transfersPerDiskBuffer;
    size_t startingBufferIndex = diskBufferCount - (targetDumpedTransferCount / transfersPerDiskBuffer);
    size_t startingTransferIndex = 0;
    dumpedTransferCount = 0;

    // Set up the initial transfers
    size_t nextDiskBufferIndexForInitialTransfer = startingBufferIndex;
    size_t nextDiskBufferTransferIndex = startingTransferIndex;
    for (size_t transferNumber = 0; transferNumber < simultaneousTransfers; ++transferNumber)
    {
        // Allocate the libusb transfer structure for this transfer
        libusb_transfer* libUsbTransfer = libusb_alloc_transfer(0);
        if (libUsbTransfer == nullptr)
        {
            Log().Error("libusb_alloc_transfer failed for transfer number {0}", transferNumber);
            SetUsbTransferFinished(TransferResult::UsbTransferFailure);
            transferFailure = true;
            break;
        }

        // Set this transfer buffer to its initial state
        TransferBufferEntry& transferBufferEntry = transferBuffers[transferNumber];
        transferBufferEntry.object = this;
        transferBufferEntry.transfer = libUsbTransfer;
        transferBufferEntry.diskBufferIndex = nextDiskBufferIndexForInitialTransfer;
        transferBufferEntry.diskBufferTransferIndex = nextDiskBufferTransferIndex;
        transferBufferEntry.transferBufferByteOffset = nextDiskBufferTransferIndex * transferSizeInBytes;
        transferBufferEntry.diskBufferIncrementOnCompletion = diskBufferIncrementOnCompletion;
        transferBufferEntry.diskBufferCount = diskBufferCount;
        transferBufferEntry.transferSubmitted = false;
        transferBufferEntry.transferCancelled = false;

        // Set transfer flag to cause transfer error if there is a short packet
        transferBufferEntry.transfer->flags = LIBUSB_TRANSFER_SHORT_NOT_OK;

        // Configure the transfer with a 1 second timeout (targeted to disk buffer 0)
        int timeoutInMilliseconds = 0;
        DiskBufferEntry& bufferEntry = GetDiskBuffer(nextDiskBufferIndexForInitialTransfer);
        libusb_fill_bulk_transfer(transferBufferEntry.transfer, captureUsbDeviceHandle, connectedBulkPipeInId,
            bufferEntry.readBuffer.data() + transferBufferEntry.transferBufferByteOffset,
            (int)transferSizeInBytes, BulkTransferCallbackStatic, &transferBufferEntry, timeoutInMilliseconds);

        // Advance to the next transfer
        ++nextDiskBufferTransferIndex;
        transferBufferEntry.lastTransferInDiskBuffer = (nextDiskBufferTransferIndex >= transfersPerDiskBuffer);
        if (transferBufferEntry.lastTransferInDiskBuffer)
        {
            nextDiskBufferTransferIndex = 0;
            nextDiskBufferIndexForInitialTransfer = (nextDiskBufferIndexForInitialTransfer + 1) % diskBufferCount;
        }
    }

    // Submit the transfers via libUSB
    if (!transferFailure)
    {
        Log().Info("usbTransferThread(): Submitting {0} transfers", simultaneousTransfers);
        for (size_t transferNumber = 0; transferNumber < simultaneousTransfers; ++transferNumber)
        {
            TransferBufferEntry& transferBufferEntry = transferBuffers[transferNumber];
            int libUsbSubmitTransferReturn = libusb_submit_transfer(transferBufferEntry.transfer);
            if (libUsbSubmitTransferReturn != 0)
            {
                Log().Error("libusb_submit_transfer failed with error code {0}:{1}", libUsbSubmitTransferReturn, libusb_error_name(libUsbSubmitTransferReturn));
#ifndef _WIN32
                if (libUsbSubmitTransferReturn == LIBUSB_ERROR_NO_MEM)
                {
                    SetUsbTransferFinished(UsbDeviceBase::TransferResult::UsbMemoryLimit);
                }
                else
                {
#endif
                    SetUsbTransferFinished(TransferResult::UsbTransferFailure);
#ifndef _WIN32
                }
#endif
                transferFailure = true;
                break;
            }
            transferBufferEntry.transferSubmitted = true;
            ++transfersInFlight;
        }
    }

    // Process libusb events continuously until the transfer is aborted
    timeval libUsbTimeout;
    libUsbTimeout.tv_sec = 0;
    libUsbTimeout.tv_usec = 100 * 1000;
    while (!transferFailure && (transfersInFlight > 0))
    {
        libusb_handle_events_timeout_completed(libUsbContext, &libUsbTimeout, NULL);
    }
    Log().Info("usbTransferThread(): Capture complete. Wrapping up.");

    // Abort any remaining transfers in progress. This will usually be the case if an error occurred during USB
    // transfer, or the transfer is being forcefully aborted.
    if (transfersInFlight > 0)
    {
        // If a transfer failure hasn't been logged and there are still transfers in progress, we've messed up in the
        // program logic somewhere. Log an error.
        if (!transferFailure)
        {
            Log().Error("usbTransferThread(): Transfers were still in progress after a non-failed capture!");
        }

        // Cancel each transfer marked as still in progress
        for (size_t transferNumber = 0; transferNumber < simultaneousTransfers; transferNumber++)
        {
            TransferBufferEntry& transferBufferEntry = transferBuffers[transferNumber];
            if (transferBufferEntry.transferSubmitted)
            {
                transferBufferEntry.transferCancelled = true;
                int libUsbCancelTransferReturn = libusb_cancel_transfer(transferBufferEntry.transfer);
                if ((libUsbCancelTransferReturn != 0) && (libUsbCancelTransferReturn != LIBUSB_ERROR_NOT_FOUND))
                {
                    // Note that we expect LIBUSB_ERROR_NOT_FOUND errors when the transfer is already complete but not
                    // processed from the queue, so we filter them out here. We also ignore failures other than logging
                    // them, since we're aborting any current transfers anyway.
                    Log().Error("libusb_cancel_transfer failed with error code {0}:{1} for transfer {2}", libUsbCancelTransferReturn, libusb_error_name(libUsbCancelTransferReturn), transferNumber);
                }
            }
        }

        // Pump the libusb message queue until all the completion events have been processed
        while (transfersInFlight > 0)
        {
            libusb_handle_events_timeout_completed(libUsbContext, &libUsbTimeout, NULL);
        }
    }
    Log().Info("usbTransferThread(): Transfers complete. Freeing transfer structures.");

    // Free all the transfer structures now that they're completed or cancelled
    for (size_t transferNumber = 0; transferNumber < simultaneousTransfers; transferNumber++)
    {
        TransferBufferEntry& transferBufferEntry = transferBuffers[transferNumber];
        libusb_free_transfer(transferBufferEntry.transfer);
    }

    // Restore the current thread priority to its original settings. We don't really need to do this because the thread
    // is about to exit, but we'll do it anyway.
    RestoreCurrentThreadPriority(threadPriorityRestoreInfo);

    Log().Info("usbTransferThread(): Completed");

    // If we've been requested to stop the capture process, and an error hasn't been flagged, mark the process as
    // successful.
    if (!transferFailure)
    {
        SetUsbTransferFinished(TransferResult::Success);
    }
}

//----------------------------------------------------------------------------------------------------------------------
void LIBUSB_CALL UsbDeviceLibUsb::BulkTransferCallbackStatic(libusb_transfer* transfer)
{
    TransferBufferEntry* transferUserData = static_cast<TransferBufferEntry*>(transfer->user_data);
    transferUserData->object->BulkTransferCallback(transfer, transferUserData);
}

//----------------------------------------------------------------------------------------------------------------------
void UsbDeviceLibUsb::BulkTransferCallback(libusb_transfer* transfer, TransferBufferEntry* transferUserData)
{
    DiskBufferEntry& bufferEntry = GetDiskBuffer(transferUserData->diskBufferIndex);
    transferUserData->transferSubmitted = false;
    --transfersInFlight;

    // Check if the transfer has succeeded
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED)
    {
        // If the transfer was cancelled by request, abort any further processing.
        if (transferUserData->transferCancelled && (transfer->status == LIBUSB_TRANSFER_CANCELLED))
        {
            return;
        }

        // Log detailed info on what caused the transfer failure
        std::string statusAsString = std::to_string((int)transfer->status);
        switch (transfer->status)
        {
        case LIBUSB_TRANSFER_ERROR:
            statusAsString = "LIBUSB_TRANSFER_ERROR";
            break;
        case LIBUSB_TRANSFER_TIMED_OUT:
            statusAsString = "LIBUSB_TRANSFER_TIMED_OUT";
            break;
        case LIBUSB_TRANSFER_CANCELLED:
            statusAsString = "LIBUSB_TRANSFER_CANCELLED";
            break;
        case LIBUSB_TRANSFER_STALL:
            statusAsString = "LIBUSB_TRANSFER_STALL";
            break;
        case LIBUSB_TRANSFER_NO_DEVICE:
            statusAsString = "LIBUSB_TRANSFER_NO_DEVICE";
            break;
        case LIBUSB_TRANSFER_OVERFLOW:
            statusAsString = "LIBUSB_TRANSFER_OVERFLOW";
            break;
        }
        Log().Error("bulkTransferCallback(): Transfer completed with error {0} for buffer index {1}:{2}", statusAsString, transferUserData->diskBufferIndex, transferUserData->diskBufferTransferIndex);

        // Set the transfer failure flag
        SetUsbTransferFinished(TransferResult::UsbTransferFailure);
        transferFailure = true;
        return;
    }

    // Ensure we receieved as many bytes as we expected, since we don't handle the case where this isn't true. This
    // check should never fail however, as we specified the LIBUSB_TRANSFER_SHORT_NOT_OK flag when defining the
    // transfers.
    if (transfer->actual_length != transfer->length)
    {
        // Report the details in the debug log
        Log().Error("bulkTransferCallback(): Expected {0} bytes from USB transfer but got {1} bytes from buffer index {2}:{3}.", transfer->length, transfer->actual_length, transferUserData->diskBufferIndex, transferUserData->diskBufferTransferIndex);

        // Set the transfer failure flag
        SetUsbTransferFinished(TransferResult::UsbTransferFailure);
        transferFailure = true;
        return;
    }

    // If we've been requested to stop, check if we're stopping immediately or waiting for the current set of
    // buffers to flush.
    bool transferStopRequested = UsbTransferStopRequested();
    if (transferStopRequested && UsbTransferDumpBuffers())
    {
        SetUsbTransferFinished(TransferResult::ForcedAbort);
        transferFailure = true;
        return;
    }

    // Either discard this buffer, or send it downstream for processing.
    if (dumpedTransferCount < targetDumpedTransferCount)
    {
        // Dump the contents of this buffer without doing anything with it
        ++dumpedTransferCount;
    }
    else if (transferUserData->lastTransferInDiskBuffer)
    {
        // Mark the disk buffer as full. If our wait guards are correct elsewhere, the buffer should always be empty at
        // this point. To guard against errors, we ensure the buffer was already full here, and report failure if this
        // was not the case.
        if (bufferEntry.isDiskBufferFull.test_and_set())
        {
            Log().Error("bulkTransferCallback(): Disk buffer overflow at index {0}, transfer {1}", transferUserData->diskBufferIndex, transferUserData->diskBufferTransferIndex);
            SetUsbTransferFinished(TransferResult::ProgramError);
            transferFailure = true;
            return;
        }

        // Notify any threads waiting on this buffer that there is now data for processing
        bufferEntry.isDiskBufferFull.notify_all();
        Log().Trace("bulkTransferCallback(): Submitted disk buffer {0} for processing", transferUserData->diskBufferIndex);

        // If transfer has been requested to stop, mark the capture as complete now that we've reached a disk buffer
        // boundary.
        if (transferStopRequested)
        {
            captureComplete = true;
        }
    }

    // Increment the count of successfully completed transfers
    AddCompletedTransferCount(1);

    // If the capture is not complete, submit another transfer request.
    if (!captureComplete)
    {
        // Calculate the resubmission disk buffer index. In the case of a small usb transfer buffer, there may be many
        // more disk buffers than there are active transfer buffers, and this is how we "roll through" the disk buffers
        // with the transfers as they get resubmitted. In the case of large buffers, there's at least one more disk
        // buffer than there are active transfer buffers, to allow for overlapped processing and disk IO on two buffers
        // at once.
        size_t resubmissionDiskBufferIndex = (transferUserData->diskBufferIndex + transferUserData->diskBufferIncrementOnCompletion) % transferUserData->diskBufferCount;
        transferUserData->diskBufferIndex = resubmissionDiskBufferIndex;
        DiskBufferEntry& resubmissionBufferEntry = GetDiskBuffer(resubmissionDiskBufferIndex);

        // If we're about to queue the first read in the next disk buffer, wait for that buffer slot to complete
        // processing and be returned to us. If it hasn't been sent for processing, this will return immediately.
        if (transferUserData->diskBufferTransferIndex == 0)
        {
            resubmissionBufferEntry.isDiskBufferFull.wait(true);
        }

        // Queue a USB bulk read request for this transfer slot
        int timeoutInMilliseconds = 0;
        libusb_fill_bulk_transfer(transfer, transfer->dev_handle, transfer->endpoint, resubmissionBufferEntry.readBuffer.data() + transferUserData->transferBufferByteOffset, transfer->length, BulkTransferCallbackStatic, transfer->user_data, timeoutInMilliseconds);
        int libUsbSubmitTransferReturn = libusb_submit_transfer(transfer);
        if (libUsbSubmitTransferReturn != 0)
        {
            Log().Error("libusb_submit_transfer failed with error code {0}:{1}", libUsbSubmitTransferReturn, libusb_error_name(libUsbSubmitTransferReturn));
            SetUsbTransferFinished(TransferResult::UsbTransferFailure);
            transferFailure = true;
            return;
        }
        transferUserData->transferSubmitted = true;
        ++transfersInFlight;
    }
}
