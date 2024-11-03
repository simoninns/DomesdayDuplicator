#ifdef _WIN32
#include "UsbDeviceWinUsb.h"
#include "StringUtilities.h"
#include <memory>
#include <cfgmgr32.h>
#include <initguid.h>

//----------------------------------------------------------------------------------------------------------------------
// This is the GUID for the USB device class
// (A5DCBF10-6530-11D2-901F-00C04FB951ED)
DEFINE_GUID(GUID_DEVINTERFACE_USB_DEVICE, 0xA5DCBF10L, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED);

//----------------------------------------------------------------------------------------------------------------------
// Constructors
//----------------------------------------------------------------------------------------------------------------------
UsbDeviceWinUsb::UsbDeviceWinUsb(const ILogger& log)
:UsbDeviceBase(log)
{ }

//----------------------------------------------------------------------------------------------------------------------
UsbDeviceWinUsb::~UsbDeviceWinUsb()
{
    // Ensure we're disconnected from the device
    DisconnectFromDevice();
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceWinUsb::Initialize(uint16_t vendorId, uint16_t productId)
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
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// Device methods
//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceWinUsb::DevicePresent(const std::string& preferredDevicePath) const
{
    // If we're currently connected to a device, it must be present, so return true.
    if (connectedToDevice)
    {
        return true;
    }

    // Attempt to locate the USB device
    std::wstring deviceInstancePath;
    if (!FindDomesdayDeviceInstancePath(Utf8StringToWString(preferredDevicePath), deviceInstancePath))
    {
        Log().Trace("DevicePresent(): Failed to locate USB device");
        return false;
    }

    // Temporarily connect to the device to verify it's present and connectable
    HANDLE deviceHandle;
    WINUSB_INTERFACE_HANDLE winUsbInterfaceHandle;
    if (!ConnectToDevice(deviceInstancePath, deviceHandle, winUsbInterfaceHandle))
    {
        Log().Warning("DevicePresent(): Failed to connect to device with instance path {0}", deviceInstancePath);
        return false;
    }
    DisconnectFromDevice(deviceHandle, winUsbInterfaceHandle);
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceWinUsb::GetPresentDevicePaths(std::vector<std::string>& devicePaths) const
{
    // Initialize our output list of device paths
    devicePaths.clear();

    // Retrieve all connected domesday device instance paths
    std::vector<std::wstring> deviceInstancePathsWideString;
    if (!GetAllDomesdayDeviceInstancePaths(deviceInstancePathsWideString))
    {
        return false;
    }

    // Convert the device paths to UTF8 strings and return them to the caller
    for (const std::wstring& devicePath : deviceInstancePathsWideString)
    {
        devicePaths.push_back(WStringToUtf8String(devicePath));
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceWinUsb::DeviceConnected() const
{
    return connectedToDevice;
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceWinUsb::ConnectToDevice(const std::string& preferredDevicePath)
{
    // If we're already connected to the device, abort any further processing.
    if (connectedToDevice)
    {
        Log().Warning("ConnectToDevice(): Already connected to device.");
        return true;
    }

    // Attempt to locate the USB device
    std::wstring deviceInstancePath;
    if (!FindDomesdayDeviceInstancePath(Utf8StringToWString(preferredDevicePath), deviceInstancePath))
    {
        Log().Error("ConnectToDevice(): Failed to locate USB device");
        return false;
    }

    // Attempt to connect to the device
    if (!ConnectToDevice(deviceInstancePath, captureUsbDeviceHandle, captureWinUsbInterfaceHandle))
    {
        Log().Error("ConnectToDevice(): Failed to connect to device with instance path {0}", deviceInstancePath);
        captureUsbDeviceHandle = nullptr;
        captureWinUsbInterfaceHandle = nullptr;
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

    // Find the pipe info for the bulk in endpoint from the device. We expect this to be at alternate interface number 0
    // so we hardcode that here.
    const UCHAR alternateInterfaceNumber = 0;
    USB_INTERFACE_DESCRIPTOR interfaceDescriptor = {};
    BOOL winUsbQueryInterfaceSettingsReturn = WinUsb_QueryInterfaceSettings(captureWinUsbInterfaceHandle, alternateInterfaceNumber, &interfaceDescriptor);
    if (winUsbQueryInterfaceSettingsReturn != TRUE)
    {
        DWORD lastError = GetLastError();
        Log().Error("WinUsb_QueryInterfaceSettings failed with error code {0} for device with instance path {1}", lastError, deviceInstancePath);
        return false;
    }
    bool foundBulkInPipeInfo = false;
    WINUSB_PIPE_INFORMATION pipeInfo = {};
    for (int i = 0; i < interfaceDescriptor.bNumEndpoints; ++i)
    {
        BOOL winUsbQueryPipeReturn = WinUsb_QueryPipe(captureWinUsbInterfaceHandle, 0, (UCHAR)i, &pipeInfo);
        if (winUsbQueryPipeReturn != TRUE)
        {
            DWORD lastError = GetLastError();
            Log().Error("WinUsb_QueryPipe failed with error code {0} for endpoint with index {1} for device with instance path {2}", lastError, i, deviceInstancePath);
            continue;
        }
        if ((pipeInfo.PipeType == UsbdPipeTypeBulk) && USB_ENDPOINT_DIRECTION_IN(pipeInfo.PipeId))
        {
            foundBulkInPipeInfo = true;
            break;
        }
    }
    if (!foundBulkInPipeInfo)
    {
        Log().Error("ConnectToDevice(): Failed to locate bulk in endpoint for device with instance path {0}", deviceInstancePath);
        return false;
    }

    // Get the maximum transfer size for the pipe
    ULONG maxTransferSizeForPipe;
    ULONG maxTransferSizeForPipeBufferSize = sizeof(maxTransferSizeForPipe);
    BOOL winUsbGetPipePolicyReturn = WinUsb_GetPipePolicy(captureWinUsbInterfaceHandle, pipeInfo.PipeId, MAXIMUM_TRANSFER_SIZE, &maxTransferSizeForPipeBufferSize, &maxTransferSizeForPipe);
    if (winUsbGetPipePolicyReturn != TRUE)
    {
        DWORD lastError = GetLastError();
        Log().Error("WinUsb_GetPipePolicy failed with error code {0} for device with instance path {1}", lastError, deviceInstancePath);
        return false;
    }
    if (maxTransferSizeForPipeBufferSize != sizeof(maxTransferSizeForPipe))
    {
        Log().Error("WinUsb_GetPipePolicy returned {0} bytes when {1} were expected", maxTransferSizeForPipeBufferSize, sizeof(maxTransferSizeForPipe));
        return false;
    }

    // Since the device connection has succeeded, release the scoped destructor and let it live.
    releaseDeviceDisconnector = true;

    // Store our pipe information, and return true to the caller.
    captureWinUsbBulkInPipeId = pipeInfo.PipeId;
    connectedBulkPipeMaxPacketSizeInBytes = (size_t)pipeInfo.MaximumPacketSize;
    captureWinUsbBulkInMaxTransferSizeInBytes = (size_t)maxTransferSizeForPipe;
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceWinUsb::ConnectToDevice(const std::wstring& deviceInstancePath, HANDLE& deviceHandle, WINUSB_INTERFACE_HANDLE& winUsbInterfaceHandle) const
{
    // Initialize the output arguments
    deviceHandle = nullptr;
    winUsbInterfaceHandle = nullptr;

    // Open a handle to the target device
    HANDLE deviceHandleRaw = CreateFileW(deviceInstancePath.c_str(),
        GENERIC_WRITE | GENERIC_READ,
        FILE_SHARE_WRITE | FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        NULL);
    if (deviceHandleRaw == INVALID_HANDLE_VALUE)
    {
        DWORD lastError = GetLastError();
        Log().Trace("CreateFileW failed with error code {0} for device with instance path {1}", lastError, deviceInstancePath);
        return false;
    }

    // Open the target device in WinUSB
    WINUSB_INTERFACE_HANDLE winUsbInterfaceHandleRaw;
    BOOL winUsbInitializeReturn = WinUsb_Initialize(deviceHandleRaw, &winUsbInterfaceHandleRaw);
    if (winUsbInitializeReturn == FALSE)
    {
        DWORD lastError = GetLastError();
        Log().Trace("WinUsb_Initialize failed with error code {0} for device with instance path {1}", lastError, deviceInstancePath);
        CloseHandle(deviceHandleRaw);
        return false;
    }

    // Return the device handles to the caller
    deviceHandle = deviceHandleRaw;
    winUsbInterfaceHandle = winUsbInterfaceHandleRaw;
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
void UsbDeviceWinUsb::DisconnectFromDevice()
{
    // If we're not currently connected to the device, abort any further processing.
    if (!connectedToDevice)
    {
        Log().Error("DisconnectFromDevice() called when no device was connected");
        return;
    }

    // Disconnect from the device
    connectedToDevice = false;
    DisconnectFromDevice(captureUsbDeviceHandle, captureWinUsbInterfaceHandle);
    captureUsbDeviceHandle = nullptr;
    captureWinUsbInterfaceHandle = nullptr;
}

//----------------------------------------------------------------------------------------------------------------------
void UsbDeviceWinUsb::DisconnectFromDevice(HANDLE deviceHandle, WINUSB_INTERFACE_HANDLE winUsbInterfaceHandle) const
{
    // Close the WinUSB interface handle
    BOOL winUsbFreeReturn = WinUsb_Free(winUsbInterfaceHandle);
    if (winUsbFreeReturn != TRUE)
    {
        Log().Error("WinUsb_Free failed with return code {0}", winUsbFreeReturn);
    }

    // Close the device handle
    BOOL closeHandleReturn = CloseHandle(deviceHandle);
    if (closeHandleReturn != TRUE)
    {
        DWORD lastError = GetLastError();
        Log().Error("CloseHandle failed with error code {0}", lastError);
    }
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceWinUsb::GetDeviceInstancePathsByInterface(GUID* interfaceGuid, std::vector<std::wstring>& deviceInstancePaths) const
{
    // Initialize our list of results
    deviceInstancePaths.clear();

    // Get a list of all device instances which support the target interface, and are currently present in the system.
    std::vector<std::wstring> deviceListLocal;
    bool obtainedFullDeviceInstanceList = false;
    while (!obtainedFullDeviceInstanceList)
    {
        // Get the number of entries in the instance list. Note this may change prior to retrieving the list below.
        ULONG deviceInterfaceListRequiredBufferSize = 0;
        CONFIGRET getDeviceInterfaceListSizeReturn = CM_Get_Device_Interface_List_SizeW(&deviceInterfaceListRequiredBufferSize, interfaceGuid, NULL, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
        if (getDeviceInterfaceListSizeReturn != CR_SUCCESS)
        {
            Log().Error("CM_Get_Device_Interface_List_Size returned {0}", getDeviceInterfaceListSizeReturn);
            return false;
        }

        // Allocate a buffer to hold the instance list, plus one extra for double null termination.
        std::vector<wchar_t> deviceInstanceBuffer;
        deviceInstanceBuffer.resize(deviceInterfaceListRequiredBufferSize + 1, 0);

        // Retrieve the instance list in the raw buffer. If the buffer is too small due to device changes, repeat
        // the process.
        CONFIGRET getDeviceInterfaceListReturn = CM_Get_Device_Interface_ListW(interfaceGuid, NULL, deviceInstanceBuffer.data(), (ULONG)deviceInstanceBuffer.size(), CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
        if (getDeviceInterfaceListReturn == CR_BUFFER_SMALL)
        {
            continue;
        }
        if (getDeviceInterfaceListReturn != CR_SUCCESS)
        {
            Log().Error("CM_Get_Device_Interface_List returned {0}", getDeviceInterfaceListReturn);
            return false;
        }

        // Extract the list of device instance paths from the raw buffer
        size_t deviceInstanceBufferIndex = 0;
        while (deviceInstanceBuffer[deviceInstanceBufferIndex] != 0)
        {
            std::wstring deviceInstancePath = &deviceInstanceBuffer[deviceInstanceBufferIndex];
            deviceListLocal.push_back(deviceInstancePath);
            deviceInstanceBufferIndex += deviceInstancePath.size() + 1;
        }
        obtainedFullDeviceInstanceList = true;
    }

    // Return the results to the caller
    deviceInstancePaths = std::move(deviceListLocal);
    Log().Trace("GetDeviceInstancePathsByInterface: Found {0} devices", deviceInstancePaths.size());
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceWinUsb::GetAllDomesdayDeviceInstancePaths(std::vector<std::wstring>& deviceInstancePaths) const
{
    // Initialize our list of results
    deviceInstancePaths.clear();

    // Obtain device instance paths for all USB devices in the system
    std::vector<std::wstring> allDeviceInstancePaths;
    if (!GetDeviceInstancePathsByInterface((LPGUID)&GUID_DEVINTERFACE_USB_DEVICE, allDeviceInstancePaths))
    {
        Log().Error("GetDeviceInstancePathsByInterface failed");
        return false;
    }

    // Filter our list of device instance paths down to a set which match our target vendor and product ID
    std::vector<std::wstring> matchingDeviceInstancePaths;
    for (const auto& deviceInstancePath : allDeviceInstancePaths)
    {
        // Connect to the device using WinUSB. This will fail for most USB devices, so we only log it at the trace
        // level.
        HANDLE deviceHandle;
        WINUSB_INTERFACE_HANDLE winUsbInterfaceHandle;
        if (!ConnectToDevice(deviceInstancePath, deviceHandle, winUsbInterfaceHandle))
        {
            Log().Trace("GetAllDomesdayDeviceInstancePaths failed for device with instance path: {0}", deviceInstancePath);
            continue;
        }
        std::shared_ptr<void> scopedDeviceDisconnectHandler((void*)nullptr, [&](void*) { DisconnectFromDevice(deviceHandle, winUsbInterfaceHandle); });

        // Retrieve the USB device descriptor for the target device
        USB_DEVICE_DESCRIPTOR deviceDescriptor = {};
        ULONG transferredLength = 0;
        BOOL winUsbGetDescriptorReturn = WinUsb_GetDescriptor(winUsbInterfaceHandle, USB_DEVICE_DESCRIPTOR_TYPE, 0, 0, (UCHAR*)&deviceDescriptor, sizeof(deviceDescriptor), &transferredLength);
        if (winUsbGetDescriptorReturn == FALSE)
        {
            DWORD lastError = GetLastError();
            Log().Error("WinUsb_GetDescriptor failed with error code {0} for device with instance path: {1}", lastError, deviceInstancePath);
            continue;
        }

        // If the device doesn't match the target vendor and product IDs for the domesday device, skip it.
        if ((deviceDescriptor.idVendor != targetDeviceVendorId) || (deviceDescriptor.idProduct != targetDeviceProductId))
        {
            continue;
        }

        // Ensure the device is connected with an interface that meets our speed requirements
        UCHAR deviceSpeed = 0;
        ULONG queryDeviceInfoBufferLengthExpected = sizeof(deviceSpeed);
        ULONG queryDeviceInfoBufferLength = queryDeviceInfoBufferLengthExpected;
        BOOL winUsbQueryDeviceInformationReturn = WinUsb_QueryDeviceInformation(winUsbInterfaceHandle, DEVICE_SPEED, &queryDeviceInfoBufferLength, &deviceSpeed);
        if (winUsbQueryDeviceInformationReturn != TRUE)
        {
            DWORD lastError = GetLastError();
            Log().Error("WinUsb_QueryDeviceInformation failed with error code {0} for device with instance path: {1}", lastError, deviceInstancePath);
            continue;
        }
        if (queryDeviceInfoBufferLength != queryDeviceInfoBufferLengthExpected)
        {
            Log().Error("WinUsb_QueryDeviceInformation returned {0} bytes, but {1} were expected", queryDeviceInfoBufferLength, queryDeviceInfoBufferLengthExpected);
            continue;
        }
        if (deviceSpeed < FullSpeed)
        {
            Log().Warning("Skipping USB device at path {0} because it is only connected with speed {1}", deviceInstancePath, deviceSpeed);
            continue;
        }

        // Since the target device matches, store the instance path.
        Log().Trace("Found matching USB device with instance path: {0}", deviceInstancePath);
        matchingDeviceInstancePaths.push_back(deviceInstancePath);
    }

    // Return the list of device paths to the caller
    deviceInstancePaths = std::move(matchingDeviceInstancePaths);
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceWinUsb::FindDomesdayDeviceInstancePath(const std::wstring& preferredDevicePath, std::wstring& deviceInstancePath) const
{
    // Obtain a list of all domesday usb devices connected to the system
    std::vector<std::wstring> matchingDeviceInstancePaths;
    if (!GetAllDomesdayDeviceInstancePaths(matchingDeviceInstancePaths))
    {
        Log().Error("Failed to enumerate USB devices");
        return false;
    }

    // Ensure we found at least one Domesday Duplicator device
    if (matchingDeviceInstancePaths.empty())
    {
        Log().Info("No Domesday Duplicator USB devices found");
        return false;
    }

    // Select the device to connect to. If we have a preferred device specified and it is present, we use that,
    // otherwise we default to the first detected device.
    std::wstring targetDeviceInstancePath = matchingDeviceInstancePaths.front();
    if (!preferredDevicePath.empty())
    {
        for (const std::wstring& matchingDeviceInstancePath : matchingDeviceInstancePaths)
        {
            if (matchingDeviceInstancePath == preferredDevicePath)
            {
                targetDeviceInstancePath = matchingDeviceInstancePath;
                break;
            }
        }
    }

    // Return the device instance path to the caller
    deviceInstancePath = targetDeviceInstancePath;
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceWinUsb::SendVendorSpecificCommand(const std::string& preferredDevicePath, uint8_t command, uint16_t value)
{
    // Obtain the path for the domesday device we're going to connect to 
    std::wstring deviceInstancePath;
    if (!FindDomesdayDeviceInstancePath(Utf8StringToWString(preferredDevicePath), deviceInstancePath))
    {
        Log().Error("SendVendorSpecificCommand failed as no device could be found");
        return false;
    }

    // Attempt to connect to the device
    HANDLE deviceHandle;
    WINUSB_INTERFACE_HANDLE winUsbInterfaceHandle;
    if (!ConnectToDevice(deviceInstancePath, deviceHandle, winUsbInterfaceHandle))
    {
        Log().Error("SendVendorSpecificCommand failed as the device connection attempt failed");
        return false;
    }
    std::shared_ptr<void> scopedDeviceDisconnectHandler((void*)nullptr, [&](void*) { DisconnectFromDevice(deviceHandle, winUsbInterfaceHandle); });

    // Perform a control transfer of type 0x40 (vendor specific command with no data packets)
    WINUSB_SETUP_PACKET setupPacket = {};
    setupPacket.RequestType = (UCHAR)0x40;
    setupPacket.Request = (UCHAR)command;
    setupPacket.Value = (USHORT)value;
    setupPacket.Index = 0;
    setupPacket.Length = 0;
    BOOL winUsbControlTransferReturn = WinUsb_ControlTransfer(winUsbInterfaceHandle, setupPacket, NULL, 0, NULL, NULL);
    if (winUsbControlTransferReturn != TRUE)
    {
        DWORD lastError = GetLastError();
        Log().Error("WinUsb_ControlTransfer failed with error code {0}", lastError);
        return false;
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// Capture methods
//----------------------------------------------------------------------------------------------------------------------
void UsbDeviceWinUsb::CalculateDesiredBufferCountAndSize(bool useSmallUsbTransfers, size_t usbTransferQueueSizeInBytes, size_t diskBufferQueueSizeInBytes, size_t& bufferCount, size_t& bufferSizeInBytes) const
{
    // Calculate the optimal buffer size and count which will reach the target memory usage while staying within our
    // device communication constraints. Note that we don't calculate the actual USB packet size here, this is just
    // about making sure our disk buffers are aligned to clean multiplies of our USB transfer size, and within our
    // maximum single solid transfer size.
    size_t cappedMaxTransferSizeForPipe = (2 * 1024 * 1024);
    size_t maxTransferSizeForPipe = std::min(captureWinUsbBulkInMaxTransferSizeInBytes, cappedMaxTransferSizeForPipe);
    bufferSizeInBytes = (maxTransferSizeForPipe / connectedBulkPipeMaxPacketSizeInBytes) * connectedBulkPipeMaxPacketSizeInBytes;
    bufferCount = diskBufferQueueSizeInBytes / bufferSizeInBytes;
}

//----------------------------------------------------------------------------------------------------------------------
void UsbDeviceWinUsb::UsbTransferThread()
{
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
    transferBuffers.resize(simultaneousTransfers);
    size_t transferBuffersSizeInBytes = sizeof(*transferBuffers.data()) * transferBuffers.size();
    LockMemoryBufferIntoPhysicalMemory(transferBuffers.data(), transferBuffersSizeInBytes);
    for (TransferBufferEntry& entry : transferBuffers)
    {
        entry.overlappedStructure.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    }
    std::shared_ptr<void> transferBuffersDeallocator(nullptr,
        [&](void*)
        {
            for (TransferBufferEntry& entry : transferBuffers)
            {
                CloseHandle(entry.overlappedStructure.hEvent);
            }
            UnlockMemoryBuffer(transferBuffers.data(), transferBuffersSizeInBytes);
        });

    // Attempt to boost the thread priority to realtime
    ThreadPriorityRestoreInfo threadPriorityRestoreInfo = {};
    if (!SetCurrentThreadRealtimePriority(threadPriorityRestoreInfo))
    {
        Log().Warning("SetCurrentThreadRealtimePriority failed");
    }

    // Enable raw IO mode on the bulk endpoint. This will give us the best possible throughput, and minimize the
    // likelihood of missing packets. This mode has been tested and shown to be more resilient under high CPU loads.
    // See the following doco for more info about raw mode:
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/winusb-functions-for-pipe-policy-modification
    bool transferFailure = false;
    BOOL enableRawIo = TRUE;
    BOOL winUsbSetPipePolicyReturn = WinUsb_SetPipePolicy(captureWinUsbInterfaceHandle, captureWinUsbBulkInPipeId, RAW_IO, (ULONG)sizeof(enableRawIo), &enableRawIo);
    if (winUsbSetPipePolicyReturn != TRUE)
    {
        DWORD lastError = GetLastError();
        Log().Error("WinUsb_SetPipePolicy failed with error code {0}", lastError);
        SetUsbTransferFinished(TransferResult::UsbTransferFailure);
        transferFailure = true;
    }

    // Calculate the number of transfers to dump at the start of the capture process. We need to do this, as without
    // multiple read requests initially queued, we expect some buffer underflows when getting the read requests spun up.
    const size_t diskBufferCountToDump = 4;
    size_t targetDumpedTransferCount = std::min(diskBufferCount, diskBufferCountToDump) * transfersPerDiskBuffer;
    size_t startingBufferIndex = (diskBufferCount - (targetDumpedTransferCount / transfersPerDiskBuffer)) + (diskBufferCount - diskBufferIncrementOnCompletion);
    size_t startingTransferIndex = 0;
    size_t dumpedTransferCount = 0;

    // Set up the initial transfers
    size_t nextDiskBufferIndexForInitialTransfer = startingBufferIndex;
    size_t nextDiskBufferTransferIndex = startingTransferIndex;
    for (size_t transferNumber = 0; transferNumber < simultaneousTransfers; ++transferNumber)
    {
        // Set this transfer buffer to its initial state
        TransferBufferEntry& transferBufferEntry = transferBuffers[transferNumber];
        transferBufferEntry.diskBufferIndex = nextDiskBufferIndexForInitialTransfer;
        transferBufferEntry.diskBufferTransferIndex = nextDiskBufferTransferIndex;
        transferBufferEntry.transferBufferByteOffset = nextDiskBufferTransferIndex * transferSizeInBytes;
        transferBufferEntry.transferSubmitted = false;

        // Advance to the next transfer
        ++nextDiskBufferTransferIndex;
        transferBufferEntry.lastTransferInDiskBuffer = (nextDiskBufferTransferIndex >= transfersPerDiskBuffer);
        if (transferBufferEntry.lastTransferInDiskBuffer)
        {
            nextDiskBufferTransferIndex = 0;
            nextDiskBufferIndexForInitialTransfer = (nextDiskBufferIndexForInitialTransfer + 1) % diskBufferCount;
        }
    }

    // Read data from the USB device until we have a fault or are asked to stop
    bool captureComplete = false;
    bool transferComplete = false;
    size_t currentTransferIndex = 0;
    while (!transferFailure && !transferComplete)
    {
        // If we've been requested to stop, check if we're stopping immediately or waiting for the current set of
        // buffers to flush.
        bool transferStopRequested = UsbTransferStopRequested();
        if (transferStopRequested && UsbTransferDumpBuffers())
        {
            BOOL winUsbAbortPipeReturn = WinUsb_AbortPipe(captureWinUsbInterfaceHandle, captureWinUsbBulkInPipeId);
            if (winUsbAbortPipeReturn == FALSE)
            {
                DWORD lastError = GetLastError();
                Log().Error("WinUsb_AbortPipe failed with error code {0}", lastError);
            }
            SetUsbTransferFinished(TransferResult::ForcedAbort);
            transferFailure = true;
            continue;
        }

        // If the next transfer hasn't already been queued, and we're in the process of stopping the capture, we've
        // wrapped back around our transfer queue. In this case it's time to wrap up, so we mark the transfer as
        // complete and break out of the transfer loop.
        TransferBufferEntry& transferBufferEntry = transferBuffers[currentTransferIndex];
        if (!transferBufferEntry.transferSubmitted && captureComplete)
        {
            transferComplete = true;
            continue;
        }

        // If we have already scheduled a transfer fo the current slot, wait for it to complete, submitting a disk
        // buffer for processing if necessary. Note that we aim to keep two disk buffers in a process ready state at any
        // one time, both for pipeline efficiency here, as well as to support asynchronous IO for the disk write thread,
        // so that it can overlap buffer processing and writing to disk.
        if (transferBufferEntry.transferSubmitted)
        {
            // Wait for the buffer to complete the read process
            DWORD bytesTransferred;
            BOOL winUsbGetOverlappedResultReturn = WinUsb_GetOverlappedResult(captureWinUsbInterfaceHandle, &transferBufferEntry.overlappedStructure, &bytesTransferred, TRUE);
            if (winUsbGetOverlappedResultReturn == FALSE)
            {
                DWORD lastError = GetLastError();
                Log().Error("WinUsb_GetOverlappedResult failed with error code {0}", lastError);
                SetUsbTransferFinished(TransferResult::UsbTransferFailure);
                transferFailure = true;
                continue;
            }

            // Ensure that we received the number of bytes that we requested
            if (bytesTransferred != transferSizeInBytes)
            {
                Log().Error("UsbTransferThread(): Expected {0} bytes from USB transfer but got {1} bytes from buffer index {2}:{3}.", transferSizeInBytes, bytesTransferred, transferBufferEntry.diskBufferIndex, transferBufferEntry.diskBufferTransferIndex);
                SetUsbTransferFinished(TransferResult::UsbTransferFailure);
                transferFailure = true;
                continue;
            }

            // Flag that a transfer is no longer in progress for this slot
            transferBufferEntry.transferSubmitted = false;

            // Either discard this transfer, or send it downstream for processing.
            if (dumpedTransferCount < targetDumpedTransferCount)
            {
                // Dump the contents of this transfer without doing anything with it
                ++dumpedTransferCount;
            }
            else if (transferBufferEntry.lastTransferInDiskBuffer)
            {
                // Mark the disk buffer as full. If our wait guards are correct elsewhere, the buffer should always be
                // empty at this point. To guard against errors, we ensure the buffer was already full here, and report
                // failure if this was not the case.
                DiskBufferEntry& diskBufferEntry = GetDiskBuffer(transferBufferEntry.diskBufferIndex);
                if (diskBufferEntry.isDiskBufferFull.test_and_set())
                {
                    Log().Error("UsbTransferThread(): Disk buffer overflow at index {0}:{1}", transferBufferEntry.diskBufferIndex, transferBufferEntry.diskBufferTransferIndex);
                    SetUsbTransferFinished(TransferResult::ProgramError);
                    transferFailure = true;
                    continue;
                }

                // Notify any threads waiting on this buffer that there is now data for processing
                diskBufferEntry.isDiskBufferFull.notify_all();
                Log().Trace("UsbTransferThread(): Submitted disk buffer {0} for processing", transferBufferEntry.diskBufferIndex);

                // If transfer has been requested to stop, mark the capture as complete now that we've reached a disk
                // buffer boundary.
                if (transferStopRequested)
                {
                    captureComplete = true;
                }
            }

            // Increment the count of successfully completed transfers
            AddCompletedTransferCount(1);
        }

        // If the capture is not complete, submit another transfer request.
        if (!captureComplete)
        {
            // Calculate the resubmission disk buffer index. In the case of a small usb transfer buffer, there may be
            // many more disk buffers than there are active transfer buffers, and this is how we "roll through" the disk
            // buffers with the transfers as they get resubmitted. In the case of large buffers, there's at least one
            // more disk buffer than there are active transfer buffers, to allow for overlapped processing and disk IO
            // on two buffers at once.
            size_t resubmissionDiskBufferIndex = (transferBufferEntry.diskBufferIndex + diskBufferIncrementOnCompletion) % diskBufferCount;
            transferBufferEntry.diskBufferIndex = resubmissionDiskBufferIndex;
            DiskBufferEntry& resubmissionBufferEntry = GetDiskBuffer(resubmissionDiskBufferIndex);

            // If we're about to queue the first read in the next disk buffer, wait for that buffer slot to complete
            // processing and be returned to us. If it hasn't been sent for processing, this will return immediately.
            if (transferBufferEntry.diskBufferTransferIndex == 0)
            {
                resubmissionBufferEntry.isDiskBufferFull.wait(true);
            }

            // Queue a USB bulk read request for this transfer slot
            BOOL winUsbReadPipeReturn = WinUsb_ReadPipe(captureWinUsbInterfaceHandle, captureWinUsbBulkInPipeId, resubmissionBufferEntry.readBuffer.data() + transferBufferEntry.transferBufferByteOffset, (ULONG)transferSizeInBytes, NULL, &transferBufferEntry.overlappedStructure);
            if (winUsbReadPipeReturn == FALSE)
            {
                DWORD lastError = GetLastError();
                if (lastError != ERROR_IO_PENDING)
                {
                    Log().Error("WinUsb_ReadPipe failed with error code {0}", lastError);
                    SetUsbTransferFinished(TransferResult::UsbTransferFailure);
                    transferFailure = true;
                    continue;
                }
            }
            transferBufferEntry.transferSubmitted = true;
            Log().Trace("UsbTransferThread(): Queued buffer {0}:{1} for read", transferBufferEntry.diskBufferIndex, transferBufferEntry.diskBufferTransferIndex);

            // If the previous buffer has already completed, it means there was likely a time when no reads were queued.
            // This means we have likely missed data, so report an error and abort.
            if (dumpedTransferCount >= targetDumpedTransferCount)
            {
                size_t lastTransferIndex = (currentTransferIndex + (simultaneousTransfers - 1)) % simultaneousTransfers;
                TransferBufferEntry& lastTransferBufferEntry = transferBuffers[lastTransferIndex];
                if (lastTransferBufferEntry.transferSubmitted)
                {
                    DWORD bytesTransferred;
                    BOOL winUsbGetOverlappedResultReturn = WinUsb_GetOverlappedResult(captureWinUsbInterfaceHandle, &lastTransferBufferEntry.overlappedStructure, &bytesTransferred, FALSE);
                    DWORD lastError = GetLastError();
                    if (winUsbGetOverlappedResultReturn != 0)
                    {
                        Log().Error("Capture buffer underflow at index {0}:{1}. WinUsb_GetOverlappedResult returned {2} with error code {3}.", lastTransferBufferEntry.diskBufferIndex, lastTransferBufferEntry.diskBufferTransferIndex, winUsbGetOverlappedResultReturn, lastError);
                        SetUsbTransferFinished(TransferResult::BufferUnderflow);
                        transferFailure = true;
                        continue;
                    }
                    else if (lastError != ERROR_IO_INCOMPLETE)
                    {
                        Log().Error("WinUsb_GetOverlappedResult failed with error code {0}", lastError);
                        SetUsbTransferFinished(TransferResult::UsbTransferFailure);
                        transferFailure = true;
                        continue;
                    }
                }
            }
        }

        // Advance to the next transfer slot
        size_t nextTransferIndex = (currentTransferIndex + 1) % simultaneousTransfers;
        currentTransferIndex = nextTransferIndex;
    }

    // Restore the current thread priority to its original settings. We don't really need to do this because the thread
    // is about to exit, but we'll do it anyway.
    RestoreCurrentThreadPriority(threadPriorityRestoreInfo);

    // If we've been requested to stop the capture process, and an error hasn't been flagged, mark the process as
    // successful.
    if (!transferFailure)
    {
        SetUsbTransferFinished(TransferResult::Success);
    }
}

#endif
