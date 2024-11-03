#include "UsbDeviceBase.h"
#ifdef _WIN32
#include <memoryapi.h>
#else
#include <sched.h>
#include <sys/mman.h>
#endif
#include <iostream>
#include <thread>
#include <functional>
#include <cassert>

//----------------------------------------------------------------------------------------------------------------------
// Constructors
//----------------------------------------------------------------------------------------------------------------------
UsbDeviceBase::UsbDeviceBase(const ILogger& log)
:log(log)
{ }

//----------------------------------------------------------------------------------------------------------------------
UsbDeviceBase::~UsbDeviceBase()
{ }

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceBase::Initialize(uint16_t vendorId, uint16_t productId)
{
    // Attempt to retrieve the initial process working set allocation
#ifdef _WIN32
    BOOL getProcessWorkingSetSizeReturn = GetProcessWorkingSetSize(GetCurrentProcess(), &originalProcessMinimumWorkingSetSizeInBytes, &originalProcessMaximumWorkingSetSizeInBytes);
    if (getProcessWorkingSetSizeReturn == 0)
    {
        DWORD lastError = GetLastError();
        Log().Error("GetProcessWorkingSetSize failed with error code {0}", lastError);
        return false;
    }
#endif
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// Log methods
//----------------------------------------------------------------------------------------------------------------------
const ILogger& UsbDeviceBase::Log() const
{
    return log;
}

//----------------------------------------------------------------------------------------------------------------------
// Device methods
//----------------------------------------------------------------------------------------------------------------------
void UsbDeviceBase::SendConfigurationCommand(const std::string& preferredDevicePath, bool testMode)
{
    uint16_t configurationFlags = 0;

    configurationFlags |= (testMode ? 0x0001 : 0x0000); // Bit 0: Set test mode
    // Bit 1: Unused
    // Bit 2: Unused
    // Bit 3: Unused
    // Bit 4: Unused

    SendVendorSpecificCommand(preferredDevicePath, 0xB6, configurationFlags);
}

//----------------------------------------------------------------------------------------------------------------------
// Capture methods
//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceBase::StartCapture(const std::filesystem::path& filePath, CaptureFormat format, const std::string& preferredDevicePath, bool isTestMode, bool useSmallUsbTransfers, bool useAsyncFileIo, size_t usbTransferQueueSizeInBytes, size_t diskBufferQueueSizeInBytes)
{
    // If we're already performing a capture, abort any further processing.
    if (transferInProgress)
    {
        Log().Error("startCapture(): Capture was currently in progress");
        return false;
    }

    // Attempt to connect to the target device
    if (!ConnectToDevice(preferredDevicePath))
    {
        Log().Error("startCapture(): Failed to connect to the target device");
        captureResult = TransferResult::ConnectionFailure;
        return false;
    }

    // Flag whether we should be using asynchronous IO (Windows only)
#ifdef _WIN32
    useWindowsOverlappedFileIo = useAsyncFileIo;
#endif

    // Attempt to create/open the output file
#ifdef _WIN32
    if (useWindowsOverlappedFileIo)
    {
        windowsCaptureOutputFileHandle = CreateFileW(filePath.wstring().c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED, NULL);
        if (windowsCaptureOutputFileHandle == INVALID_HANDLE_VALUE)
        {
            DWORD lastError = GetLastError();
            Log().Error("CreateFileW returned {0} with error code {1}.", windowsCaptureOutputFileHandle, lastError);
            captureResult = TransferResult::FileCreationError;
            return false;
        }
    }
    else
    {
#endif
        captureOutputFile.clear();
        captureOutputFile.rdbuf()->pubsetbuf(0, 0);
        captureOutputFile.open(filePath, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!captureOutputFile.is_open())
        {
            Log().Error("startCapture(): Failed to create the output file at path {0}", filePath);
            captureResult = TransferResult::FileCreationError;
            return false;
        }
#ifdef _WIN32
    }
#endif

    // Calculate the optimal read buffer size and number of disk buffers, and initialize the structures. We use an
    // unusual case of wrapping an array new into a unique_ptr rather than std::vector here, as we have an atomic_flag
    // member in the structure which can't be moved.
    CalculateDesiredBufferCountAndSize(useSmallUsbTransfers, usbTransferQueueSizeInBytes, diskBufferQueueSizeInBytes, totalDiskBufferEntryCount, diskBufferSizeInBytes);
    diskBufferEntries.reset(new DiskBufferEntry[totalDiskBufferEntryCount]);
    for (size_t i = 0; i < totalDiskBufferEntryCount; ++i)
    {
        DiskBufferEntry& entry = diskBufferEntries[i];
        entry.readBuffer.resize(diskBufferSizeInBytes);
        entry.isDiskBufferFull.clear();
#ifdef _WIN32
        entry.diskWriteInProgress = false;
        if (useWindowsOverlappedFileIo)
        {
            entry.diskWriteOverlappedBuffer = {};
            entry.diskWriteOverlappedBuffer.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        }
#endif
    }

    // Record the capture settings
    captureFilePath = filePath;
    captureFormat = format;
    captureIsTestMode = isTestMode;
    currentUsbTransferQueueSizeInBytes = usbTransferQueueSizeInBytes;
    currentUseSmallUsbTransfers = useSmallUsbTransfers;

    // Initialize capture status
    transferInProgress = true;
    captureResult = TransferResult::Running;
    transferCount = 0;
    transferBufferWrittenCount = 0;
    transferFileSizeWrittenInBytes = 0;
    minSampleValue = std::numeric_limits<decltype(minSampleValue.load())>::max();
    maxSampleValue = 0;
    clippedMinSampleCount = 0;
    clippedMaxSampleCount = 0;
    captureThreadStopRequested.clear();
    captureThreadRunning.test_and_set();
    captureThreadRunning.notify_all();

    // Initialize our sequence/test data check state
    sequenceState = SequenceState::Sync;
    savedSequenceCounter = 0;
    expectedNextTestDataValue.reset();
    testDataMax.reset();

    // Spin up a thread to handle the execution of the capture process from here on
    std::thread captureThread(std::bind(std::mem_fn(&UsbDeviceBase::CaptureThread), this));
    captureThread.detach();
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
void UsbDeviceBase::StopCapture()
{
    // If a transfer isn't currently in progress, abort any further processing.
    if (!transferInProgress)
    {
        Log().Error("stopCapture(): No capture was currently in progress");
        return;
    }

    // Instruct the capture thread to terminate, and wait for confirmation that it has stopped.
    captureThreadStopRequested.test_and_set();
    captureThreadStopRequested.notify_all();
    captureThreadRunning.wait(true);

    // Release our memory holding the disk buffers
#ifdef _WIN32
    if (useWindowsOverlappedFileIo)
    {
        for (size_t i = 0; i < totalDiskBufferEntryCount; ++i)
        {
            DiskBufferEntry& entry = diskBufferEntries[i];
            CloseHandle(entry.diskWriteOverlappedBuffer.hEvent);
        }
    }
#endif
    diskBufferEntries.reset();

    // Close the output file
#ifdef _WIN32
    if (useWindowsOverlappedFileIo)
    {
        BOOL closeHandleReturn = CloseHandle(windowsCaptureOutputFileHandle);
        if (closeHandleReturn == 0)
        {
            DWORD lastError = GetLastError();
            Log().Error("CloseHandle failed with error code {0}.", lastError);
        }
    }
    else
    {
#endif
        captureOutputFile.close();
#ifdef _WIN32
    }
#endif

    // Disconnect from the target device
    DisconnectFromDevice();

    // Record that the capture process has completed
    Log().Info("stopCapture(): Ended capture process");
    transferInProgress = false;
}

//----------------------------------------------------------------------------------------------------------------------
void UsbDeviceBase::CaptureThread()
{
    // Determine how large our conversion buffers need to be based on the disk buffer size and the capture format
    size_t requiredConversionBufferSize = 0;
    switch (captureFormat)
    {
    case CaptureFormat::Signed16Bit:
        requiredConversionBufferSize = diskBufferSizeInBytes;
        break;
    case CaptureFormat::Unsigned10Bit:
        requiredConversionBufferSize = (diskBufferSizeInBytes / 8) * 5;
        break;
    case CaptureFormat::Unsigned10Bit4to1Decimation:
        requiredConversionBufferSize = (diskBufferSizeInBytes / (8 * 4)) * 5;
        break;
    }

    // Allocate our conversion buffers
    for (size_t i = 0; i < conversionBufferCount; ++i)
    {
        conversionBuffers[i].resize(requiredConversionBufferSize);
    }

    // Lock all the critical structures into physical memory. This stops these buffers getting paged out, which could
    // cause page faults and lead to missed data packets.
    for (size_t i = 0; i < conversionBufferCount; ++i)
    {
        LockMemoryBufferIntoPhysicalMemory(conversionBuffers[i].data(), conversionBuffers[i].size());
    }
    for (size_t i = 0; i < totalDiskBufferEntryCount; ++i)
    {
        DiskBufferEntry& entry = diskBufferEntries[i];
        LockMemoryBufferIntoPhysicalMemory(entry.readBuffer.data(), entry.readBuffer.size());
    }
    LockMemoryBufferIntoPhysicalMemory(diskBufferEntries.get(), sizeof(diskBufferEntries[0]) * totalDiskBufferEntryCount);
    LockMemoryBufferIntoPhysicalMemory(this, sizeof(*this));

    // Attempt to boost the process priority to realtime
    boostedProcessPriority = SetCurrentProcessRealtimePriority(processPriorityRestoreInfo);
    if (!boostedProcessPriority)
    {
        Log().Warning("captureThread(): Failed to boost process priority");
    }

    // Record that a capture process is starting
    Log().Info("captureThread(): Starting capture process");

    usbTransferRunning.test_and_set();
    processingRunning.test_and_set();
    usbTransferStopRequested.clear();
    processingStopRequested.clear();
    dumpAllCaptureDataInProgress.clear();
    usbTransferResult = TransferResult::Running;
    processingResult = TransferResult::Running;

    // Start a worker thread to process data after it's read
    std::thread processingThread(std::bind(std::mem_fn(&UsbDeviceBase::ProcessingThread), this));

    // Start a worker thread to transfer data from the USB device
    std::thread usbTransferThread(std::bind(std::mem_fn(&UsbDeviceBase::UsbTransferThread), this));

    // Run transfer continously until we're signalled to stop for some reason
    captureThreadStopRequested.wait(false);

    // Wind up the capture process, latching the appropriate result if an error has occurred.
    TransferResult result = TransferResult::ProgramError;
    bool errorCodeLatched = false;
    bool usbTransferFailed = false;
    bool usbTransferResultChecked = false;
    bool processingFailed = false;
    bool processingResultChecked = false;
    while (usbTransferRunning.test() || processingRunning.test())
    {
        // Check for a USB transfer failure
        if (!usbTransferResultChecked && !usbTransferRunning.test())
        {
            auto transferResultTemp = usbTransferResult.load();
            if (transferResultTemp == TransferResult::Running)
            {
                if (!errorCodeLatched)
                {
                    result = TransferResult::ProgramError;
                    errorCodeLatched = true;
                }
                usbTransferFailed = true;
            }
            else if (transferResultTemp != TransferResult::Success)
            {
                if (!errorCodeLatched)
                {
                    result = transferResultTemp;
                    errorCodeLatched = true;
                }
                usbTransferFailed = true;
            }
            usbTransferResultChecked = true;
        }

        // Check for a data processing failure
        if (!processingResultChecked && !processingRunning.test())
        {
            auto transferResultTemp = processingResult.load();
            if (transferResultTemp == TransferResult::Running)
            {
                if (!errorCodeLatched)
                {
                    result = TransferResult::ProgramError;
                    errorCodeLatched = true;
                }
                processingFailed = true;
            }
            else if (transferResultTemp != TransferResult::Success)
            {
                if (!errorCodeLatched)
                {
                    result = transferResultTemp;
                    errorCodeLatched = true;
                }
                processingFailed = true;
            }
            processingResultChecked = true;
        }

        // Request the USB transfer child worker thread to stop. If no errors have occurred or occur during the final
        // stages, this should cause it to emit all remaining queued transfers and stop gracefully.
        usbTransferStopRequested.test_and_set();
        usbTransferStopRequested.notify_all();

        // Request the processing child worker thread to stop if the USB transfer worker thread has completed, or if an
        // error has occurred. This should cause it to process all queued disk buffers and stop gracefully. We pad out
        // any empty disk buffers as being dumped, and signal their completion, to unblock the worker thread in case
        // it's currently waiting on the next buffer.
        if (!usbTransferRunning.test() || usbTransferFailed || processingFailed)
        {
            processingStopRequested.test_and_set();
            processingStopRequested.notify_all();
            for (size_t i = 0; i < totalDiskBufferEntryCount; ++i)
            {
                DiskBufferEntry& entry = diskBufferEntries[i];
                if (!entry.isDiskBufferFull.test())
                {
                    entry.dumpingBuffer.test_and_set();
                    entry.isDiskBufferFull.test_and_set();
                    entry.isDiskBufferFull.notify_all();
                }
            }
        }

        // If an error occurred, force our worker threads to unblock and dump any data currently in the buffers.
        if (usbTransferFailed || processingFailed)
        {
            dumpAllCaptureDataInProgress.test_and_set();
            for (size_t i = 0; i < totalDiskBufferEntryCount; ++i)
            {
                DiskBufferEntry& entry = diskBufferEntries[i];
                entry.isDiskBufferFull.clear();
                entry.isDiskBufferFull.notify_all();
            }
            for (size_t i = 0; i < totalDiskBufferEntryCount; ++i)
            {
                DiskBufferEntry& entry = diskBufferEntries[i];
                entry.isDiskBufferFull.test_and_set();
                entry.isDiskBufferFull.notify_all();
            }
        }

        // Yield the remainder of the timeslice, to help keep CPU resources free while we try and spin down.
#ifdef _WIN32
        Sleep(0);
#else
        sched_yield();
#endif
    }

    // Wait for our spawned threads to terminate
    usbTransferThread.join();
    processingThread.join();

    // Set the result of this transfer process
    if (!errorCodeLatched)
    {
        result = TransferResult::Success;
    }
    captureResult = result;

    // Restore the original process priority settings
    if (boostedProcessPriority)
    {
        RestoreCurrentProcessPriority(processPriorityRestoreInfo);
    }

    // Release all our memory buffer locks
    for (size_t i = 0; i < conversionBufferCount; ++i)
    {
        UnlockMemoryBuffer(conversionBuffers[i].data(), conversionBuffers[i].size());
    }
    for (size_t i = 0; i < totalDiskBufferEntryCount; ++i)
    {
        DiskBufferEntry& entry = diskBufferEntries[i];
        UnlockMemoryBuffer(entry.readBuffer.data(), entry.readBuffer.size());
    }
    UnlockMemoryBuffer(diskBufferEntries.get(), sizeof(diskBufferEntries[0]) * totalDiskBufferEntryCount);
    UnlockMemoryBuffer(this, sizeof(*this));

    // Flag that this thread is no longer running
    captureThreadRunning.clear();
    captureThreadRunning.notify_all();
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceBase::GetTransferInProgress() const
{
    return transferInProgress && (captureResult == TransferResult::Running);
}

//----------------------------------------------------------------------------------------------------------------------
UsbDeviceBase::TransferResult UsbDeviceBase::GetTransferResult() const
{
    return captureResult;
}

//----------------------------------------------------------------------------------------------------------------------
size_t UsbDeviceBase::GetNumberOfTransfers() const
{
    return transferCount;
}

//----------------------------------------------------------------------------------------------------------------------
size_t UsbDeviceBase::GetNumberOfDiskBuffersWritten() const
{
    return transferBufferWrittenCount;
}

//----------------------------------------------------------------------------------------------------------------------
size_t UsbDeviceBase::GetFileSizeWrittenInBytes() const
{
    return transferFileSizeWrittenInBytes;
}

//----------------------------------------------------------------------------------------------------------------------
size_t UsbDeviceBase::GetMinSampleValue() const
{
    return minSampleValue;
}

//----------------------------------------------------------------------------------------------------------------------
size_t UsbDeviceBase::GetMaxSampleValue() const
{
    return maxSampleValue;
}

//----------------------------------------------------------------------------------------------------------------------
size_t UsbDeviceBase::GetClippedMinSampleCount() const
{
    return clippedMinSampleCount;
}

//----------------------------------------------------------------------------------------------------------------------
size_t UsbDeviceBase::GetClippedMaxSampleCount() const
{
    return clippedMaxSampleCount;
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceBase::GetTransferHadSequenceNumbers() const
{
    return (sequenceState != SequenceState::Disabled);
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceBase::UsbTransferDumpBuffers() const
{
    return dumpAllCaptureDataInProgress.test();
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceBase::UsbTransferStopRequested() const
{
    return usbTransferStopRequested.test();
}

//----------------------------------------------------------------------------------------------------------------------
UsbDeviceBase::DiskBufferEntry& UsbDeviceBase::GetDiskBuffer(size_t bufferNo)
{
    assert(bufferNo < bufferCount);
    return diskBufferEntries[bufferNo];
}

//----------------------------------------------------------------------------------------------------------------------
size_t UsbDeviceBase::GetDiskBufferCount() const
{
    return totalDiskBufferEntryCount;
}

//----------------------------------------------------------------------------------------------------------------------
size_t UsbDeviceBase::GetSingleDiskBufferSizeInBytes() const
{
    return diskBufferSizeInBytes;
}

//----------------------------------------------------------------------------------------------------------------------
size_t UsbDeviceBase::GetUsbTransferQueueSizeInBytes() const
{
    return currentUsbTransferQueueSizeInBytes;
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceBase::GetUseSmallUsbTransfers() const
{
    return currentUseSmallUsbTransfers;
}

//----------------------------------------------------------------------------------------------------------------------
void UsbDeviceBase::SetUsbTransferFinished(TransferResult result)
{
    usbTransferResult = result;
    usbTransferRunning.clear();
    usbTransferRunning.notify_all();
    captureThreadStopRequested.test_and_set();
    captureThreadStopRequested.notify_all();
}

//----------------------------------------------------------------------------------------------------------------------
void UsbDeviceBase::SetProcessingFinished(TransferResult result)
{
    processingResult = result;
    processingRunning.clear();
    processingRunning.notify_all();
    captureThreadStopRequested.test_and_set();
    captureThreadStopRequested.notify_all();
}

//----------------------------------------------------------------------------------------------------------------------
void UsbDeviceBase::AddCompletedTransferCount(size_t incrementCount)
{
    transferCount += incrementCount;
}

//----------------------------------------------------------------------------------------------------------------------
// Processing methods
//----------------------------------------------------------------------------------------------------------------------
void UsbDeviceBase::ProcessingThread()
{
    ThreadPriorityRestoreInfo priorityRestoreInfo = {};
    bool boostedThreadPriority = SetCurrentThreadRealtimePriority(priorityRestoreInfo);
    std::shared_ptr<void> currentThreadPriorityReducer;
    if (!boostedThreadPriority)
    {
        Log().Warning("processingThread(): Failed to boost thread priority");
    }
    else
    {
        currentThreadPriorityReducer.reset((void*)nullptr, [&](void*) { RestoreCurrentThreadPriority(priorityRestoreInfo); });
    }

    bool transferComplete = false;
    bool processingFailure = false;
    size_t currentDiskBuffer = 0;
    while (!processingFailure && !transferComplete)
    {
        // If processing has been requested to stop, and we've reached the end of the buffered data, or we're being
        // stopped forcefully, break out of the processing loop. Note that if the stop isn't forceful, we let the
        // current loop iteration complete to allow our current pending write to be "flushed" in the case of overlapped
        // disk IO.
        DiskBufferEntry& bufferEntry = diskBufferEntries[currentDiskBuffer];
        bool flushOnly = false;
        if (processingStopRequested.test())
        {
            if (dumpAllCaptureDataInProgress.test())
            {
                transferComplete = true;
                continue;
            }
            if (!bufferEntry.isDiskBufferFull.test() || bufferEntry.dumpingBuffer.test())
            {
                flushOnly = true;
                transferComplete = true;
            }
        }

        // If this isn't a flush-only pass, retrieve, validate, process, and write the next block of data to the output
        // file.
        if (!flushOnly)
        {
            // Wait for the next disk buffer to be filled
            bufferEntry.isDiskBufferFull.wait(false);

            // If the buffer isn't really filled, we've just been woken because the buffer is being dumped while
            // stopping the capture process, loop around again. We expect processing is either being gracefully or
            // forcefully halted. The next loop iteration will allow us to determine what the case is and whether we
            // need to flush pending writes or abort the transfers in flight.
            if (bufferEntry.dumpingBuffer.test())
            {
                continue;
            }

            // Verify and strip the sequence markers from the sample data, and update our sample metrics.
            uint16_t minValue = std::numeric_limits<uint16_t>::max();
            uint16_t maxValue = std::numeric_limits<uint16_t>::min();
            size_t minClippedCount = 0;
            size_t maxClippedCount = 0;
            if (!ProcessSequenceMarkersAndUpdateSampleMetrics(currentDiskBuffer, minValue, maxValue, minClippedCount, maxClippedCount))
            {
                SetProcessingFinished(TransferResult::SequenceMismatch);
                processingFailure = true;
                continue;
            }
            minSampleValue = std::min(minSampleValue.load(), minValue);
            maxSampleValue = std::max(maxSampleValue.load(), maxValue);
            clippedMinSampleCount += minClippedCount;
            clippedMaxSampleCount += maxClippedCount;

            // If a buffer sample has been requested, capture it now.
            if (bufferSampleRequestPending.test() && (bufferSamplingRequestedLengthInBytes <= diskBufferSizeInBytes))
            {
                capturedBufferSample.assign(bufferEntry.readBuffer.data(), bufferEntry.readBuffer.data() + bufferSamplingRequestedLengthInBytes);
                bufferSampleRequestPending.clear();
                bufferSampleAvailable.test_and_set();
                bufferSampleAvailable.notify_all();
            }

            // Verify the test data sequence if required
            if (captureIsTestMode && !VerifyTestSequence(currentDiskBuffer))
            {
                SetProcessingFinished(TransferResult::VerificationError);
                processingFailure = true;
                continue;
            }

            // Convert the sample data into the requested data format
            auto& currentConversionBuffer = conversionBuffers[conversionBufferIndex];
            if (!ConvertRawSampleData(currentDiskBuffer, captureFormat, currentConversionBuffer))
            {
                SetProcessingFinished(TransferResult::ProgramError);
                processingFailure = true;
                continue;
            }

            // Write the data to the output file
#ifdef _WIN32
            if (useWindowsOverlappedFileIo)
            {
                // Append to the end of the file using overlapped file IO. The request is queued here, not completed.
                bufferEntry.diskWriteOverlappedBuffer.Offset = 0xFFFFFFFF;
                bufferEntry.diskWriteOverlappedBuffer.OffsetHigh = 0xFFFFFFFF;
                BOOL writeFileReturn = WriteFile(windowsCaptureOutputFileHandle, currentConversionBuffer.data(), (DWORD)currentConversionBuffer.size(), NULL, &bufferEntry.diskWriteOverlappedBuffer);
                DWORD lastError = GetLastError();
                if ((writeFileReturn != 0) || (lastError != ERROR_IO_PENDING))
                {
                    Log().Error("WriteFile returned {0} with error code {1}.", writeFileReturn, lastError);
                    SetProcessingFinished(TransferResult::FileWriteError);
                    processingFailure = true;
                    continue;
                }
                bufferEntry.diskWriteInProgress = true;
            }
            else
            {
#endif
                // Perform the file write in a blocking operation
                captureOutputFile.write((const char*)currentConversionBuffer.data(), currentConversionBuffer.size());
                if (!captureOutputFile.good())
                {
                    Log().Error("processingThread(): An error occurred when writing to the output file");
                    SetProcessingFinished(TransferResult::FileWriteError);
                    processingFailure = true;
                    continue;
                }

                // Mark the disk buffer as empty, notifying the USB transfer thread in case it's blocking waiting for this
                // buffer to be free.
                bufferEntry.isDiskBufferFull.clear();
                bufferEntry.isDiskBufferFull.notify_all();

                // Add the totals from this buffer to the transfer statistics
                ++transferBufferWrittenCount;
                transferFileSizeWrittenInBytes += currentConversionBuffer.size();
#ifdef _WIN32
            }
#endif
        }

        // If we're using overlapped file IO, complete the previously submitted write operation.
#ifdef _WIN32
        if (useWindowsOverlappedFileIo)
        {
            // Retrive the previous disk buffer entry
            size_t lastBufferIndex = (currentDiskBuffer + (totalDiskBufferEntryCount - 1)) % totalDiskBufferEntryCount;
            size_t lastConversionBufferIndex = (conversionBufferIndex + (conversionBufferCount - 1)) % conversionBufferCount;
            DiskBufferEntry& lastBufferEntry = diskBufferEntries[lastBufferIndex];

            // If the previous disk buffer entry had an overlapped write in progress, block waiting for it to complete
            // and check the result.
            if (lastBufferEntry.diskWriteInProgress)
            {
                // Block to check the result of the previous disk write operation
                DWORD bytesTransferred = 0;
                BOOL getOverlappedResultReturn = GetOverlappedResult(windowsCaptureOutputFileHandle, &lastBufferEntry.diskWriteOverlappedBuffer, &bytesTransferred, TRUE);
                if (getOverlappedResultReturn == 0)
                {
                    DWORD lastError = GetLastError();
                    Log().Error("GetOverlappedResult failed with error code {0}.", lastError);
                    SetProcessingFinished(TransferResult::FileWriteError);
                    processingFailure = true;
                    continue;
                }

                // Ensure that the correct number of bytes were written. This should always be the case if it succeeded,
                // but we check anyway.
                auto& lastConversionBuffer = conversionBuffers[lastConversionBufferIndex];
                if (bytesTransferred != lastConversionBuffer.size())
                {
                    Log().Error("processingThread(): Expected {0} bytes written to disk but only {1} bytes were saved from buffer index {2}.", lastConversionBuffer.size(), bytesTransferred, lastBufferIndex);
                    SetProcessingFinished(TransferResult::FileWriteError);
                    processingFailure = true;
                    continue;
                }

                // Mark the previous disk buffer as empty, notifying the USB transfer thread in case it's blocking
                // waiting for this buffer to be free.
                lastBufferEntry.diskWriteInProgress = false;
                lastBufferEntry.isDiskBufferFull.clear();
                lastBufferEntry.isDiskBufferFull.notify_all();

                // Add the totals from this last buffer to the transfer statistics
                ++transferBufferWrittenCount;
                transferFileSizeWrittenInBytes += lastConversionBuffer.size();
            }
        }
#endif

        // Advance to the next disk buffer in the queue
        currentDiskBuffer = (currentDiskBuffer + 1) % totalDiskBufferEntryCount;
#ifdef _WIN32
        if (useWindowsOverlappedFileIo)
        {
            conversionBufferIndex = (conversionBufferIndex + 1) % conversionBufferCount;
        }
#endif
    }

    // If we're using overlapped file IO and a processing failure occurred, cancel any IO operations still in progress
    // on the output file.
#ifdef _WIN32
    if (useWindowsOverlappedFileIo && processingFailure)
    {
        BOOL cancelIoReturn = CancelIo(windowsCaptureOutputFileHandle);
        if (cancelIoReturn == 0)
        {
            DWORD lastError = GetLastError();
            Log().Error("CancelIo failed with error code {0}.", lastError);
        }
    }
#endif

    // If we've been requested to stop the capture process, and an error hasn't been flagged, mark the process as
    // successful.
    if (!processingFailure)
    {
        SetProcessingFinished(TransferResult::Success);
    }
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceBase::ProcessSequenceMarkersAndUpdateSampleMetrics(size_t diskBufferIndex, uint16_t& minValue, uint16_t& maxValue, size_t& minClippedCount, size_t& maxClippedCount)
{
    // If sequence checking has already failed, return false immediately. This condition should not occur, as a
    // sequence failure is treated as an unrecoverable error and aborts the capture process. If this is to be changed,
    // note that we would still need to perform sequence number stripping and update our sample metrics below, so this
    // condition here should be removed and the rest of the function allowed to run.
    if (sequenceState == SequenceState::Failed)
    {
        return false;
    }

    // Retrieve the previous sequence counter number. If we don't have a saved sequence number because we're just
    // starting a capture and we need to synchronize with the device, extract the sequence number from the buffer
    // and calculate the previous sequence number from it as a starting point.
    const uint16_t minPossibleSampleValue = 0;
    const uint16_t maxPossibleSampleValue = 0b1111111111;
    const int COUNTER_SHIFT = 16;
    const uint32_t COUNTER_MAX = 0b111111;
    uint32_t sequenceCounter = savedSequenceCounter;
    uint8_t* diskBuffer = diskBufferEntries[diskBufferIndex].readBuffer.data();
    if (sequenceState == SequenceState::Sync)
    {
        // Initialize the sequence counter to an impossible value
        sequenceCounter = 0xFFFFFFFF;

        // Get the first sequence number from the buffer
        uint32_t firstSequenceNumber = (uint32_t)(diskBuffer[1] >> 2);

        // Find the first time the sequence number changes. Since each sequence number appears on (1 << COUNTER_SHIFT)
        // samples, at worst we will see a change within (1 << COUNTER_SHIFT) + 1 samples.
        for (size_t pointer = 2; pointer < ((1 << COUNTER_SHIFT) + 1) * 2; pointer += 2)
        {
            uint32_t sequenceNumber = diskBuffer[pointer + 1] >> 2;
            if (sequenceNumber != firstSequenceNumber)
            {
                // Found it -- compute sequenceCounter's value at the start of the buffer
                if (sequenceNumber == 0)
                {
                    sequenceNumber = COUNTER_MAX;
                }
                sequenceCounter = (sequenceNumber << COUNTER_SHIFT) - ((uint32_t)pointer / 2);
                break;
            }
        }

        // If no sequence numbers were detected, disable sequence checking, otherwise activate it.
        if (sequenceCounter == 0xFFFFFFFF)
        {
            Log().Warning("processSequenceMarkersAndUpdateSampleMetrics(): Data does not include sequence numbers");
            sequenceState = SequenceState::Disabled;
        }
        else
        {
            Log().Trace("processSequenceMarkersAndUpdateSampleMetrics(): Synchronised with data sequence numbers");
            sequenceState = SequenceState::Running;
        }
    }

    // Validate sequence number progression, and update our sample metrics.
    for (size_t i = 0; i < diskBufferSizeInBytes; i += 2)
    {
        // If sequence checking is active, validate sequence numbers for this sample.
        if (sequenceState == SequenceState::Running)
        {
            // Ensure the sequence number in this sample matches the expected value. Note that this is treated as an
            // unrecoverable and immediate fail, so we abort any further processing and return false here.
            uint32_t expected = sequenceCounter >> COUNTER_SHIFT;
            uint32_t sequenceNumber = (uint32_t)(diskBuffer[i + 1] >> 2);
            if (sequenceNumber != expected)
            {
                Log().Error("processSequenceMarkersAndUpdateSampleMetrics(): Sequence number mismatch! Expecting {0} but got {1}", expected, sequenceNumber);
                sequenceState = SequenceState::Failed;
                savedSequenceCounter = sequenceCounter;
                return false;
            }

            // Advance the sequence counter
            ++sequenceCounter;
            if (sequenceCounter == (COUNTER_MAX << COUNTER_SHIFT))
            {
                sequenceCounter = 0;
            }
        }

        // Remove the sequence number from the sample in the disk buffer (modify the source data)
        diskBuffer[i + 1] &= 0x03;

        // Get the original 10-bit unsigned value from the disk data buffer
        uint16_t originalValue = (uint16_t)diskBuffer[i + 0] | ((uint16_t)diskBuffer[i + 1] << 8);

        // Update our min/max values
        minValue = std::min(minValue, originalValue);
        maxValue = std::max(maxValue, originalValue);

        // If the sample value is either the minimum or maximum value, increment our clipped sample counts.
        if (originalValue == minPossibleSampleValue)
        {
            ++minClippedCount;
        }
        else if (originalValue == maxPossibleSampleValue)
        {
            ++maxClippedCount;
        }
    }

    // Save the resulting sequence counter so we can continue checking from the same position in the next buffer
    savedSequenceCounter = sequenceCounter;
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceBase::VerifyTestSequence(size_t diskBufferIndex)
{
    // Retrieve the stored expected next sample value. If we haven't processed a buffer in this capture yet,
    // latch the first sample value as the expected value so we can start from here.
    DiskBufferEntry& bufferEntry = diskBufferEntries[diskBufferIndex];
    uint16_t expectedValue = expectedNextTestDataValue.value_or((uint16_t)bufferEntry.readBuffer[0] | (uint16_t)((uint16_t)bufferEntry.readBuffer[1] << 8));

    // Verify each sample in the buffer matches our expected sequence progression
    const uint8_t* readBufferPointer = bufferEntry.readBuffer.data();
    for (size_t i = 0; i < bufferEntry.readBuffer.size(); i += 2)
    {
        // Get the original 10-bit unsigned value from the disk data buffer
        uint16_t actualValue = (uint16_t)readBufferPointer[0] | ((uint16_t)readBufferPointer[1] << 8);
        readBufferPointer += 2;

        // If the actual value doesn't match our expected value, but this is the first time the test sequence
        // has wrapped around to 0, check if this appears to be the wrap point for the sequence, and latch it.
        // Valid wrapp points are either 1021 (newer FPGA firmware) or 1024 (older FPGA firmware).
        if (!testDataMax.has_value() && (expectedValue != actualValue) && (actualValue == 0) && ((expectedValue == 1021) || (expectedValue == 1024)))
        {
            testDataMax = expectedValue;
            expectedValue = 1;
            continue;
        }

        // If the expected value differs from the actual value, log an error.
        if (expectedValue != actualValue)
        {
            // Data error
            Log().Error("verifyTestSequence(): Data error in test data verification! Expecting {0} but got {1}", expectedValue, actualValue);
            return false;
        }

        // Calculate the value we expect to find for the next sample
        ++expectedValue;
        if (testDataMax.has_value() && (expectedValue == testDataMax))
        {
            expectedValue = 0;
        }
    }

    // Store the next expected sample value so we can check against the next buffer
    expectedNextTestDataValue = expectedValue;
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceBase::ConvertRawSampleData(size_t diskBufferIndex, CaptureFormat captureFormat, std::vector<uint8_t>& outputBuffer) const
{
    const DiskBufferEntry& bufferEntry = diskBufferEntries[diskBufferIndex];
    const uint8_t* readBufferPointer = bufferEntry.readBuffer.data();
    size_t readBufferSizeInBytes = bufferEntry.readBuffer.size();

    // Convert the data to the required format
    uint8_t* writeBufferPointer = outputBuffer.data();
    if (captureFormat == CaptureFormat::Signed16Bit)
    {
        // Translate the data in the disk buffer to scaled 16-bit signed data
        for (size_t i = 0; i < readBufferSizeInBytes; i += 2)
        {
            // Get the original 10-bit unsigned value from the disk data buffer
            uint16_t originalValue = (uint16_t)readBufferPointer[0] | ((uint16_t)readBufferPointer[1] << 8);
            readBufferPointer += 2;

            // Sign and scale the data to 16-bits. Technically a line like this would use the entire 16-bit range:
            //uint16_t signedValue = ((uint16_t)((int16_t)originalValue - 0x0200) << 6) | ((originalValue >> 4) & 0x003F);
            // In our case here however, that would not be preferred, since we can't restore the lost 6 bits of
            // precision, and where we guess wrong we'd create very slight frequency distortions. It's better to leave
            // the data as 10-bit and just shift it up, which doesn't technically preserve the relative mplitude of the
            // signal, but we don't care about the overall amplitude in this case, it's the frequency we care about.
            uint16_t signedValue = (uint16_t)((int16_t)originalValue - 0x0200) << 6;
            writeBufferPointer[0] = (uint8_t)((uint16_t)signedValue & 0x00FF);
            writeBufferPointer[1] = (uint8_t)(((uint16_t)signedValue & 0xFF00) >> 8);
            writeBufferPointer += 2;
        }
    }
    else if (captureFormat == CaptureFormat::Unsigned10Bit)
    {
        // Translate the data in the disk buffer to unsigned 10-bit packed data
        for (size_t i = 0; i < readBufferSizeInBytes; i += 8)
        {
            // Get the original 4 10-bit words
            uint16_t originalWords[4];
            originalWords[0] = (uint16_t)readBufferPointer[0] | ((uint16_t)readBufferPointer[1] << 8);
            originalWords[1] = (uint16_t)readBufferPointer[2] | ((uint16_t)readBufferPointer[3] << 8);
            originalWords[2] = (uint16_t)readBufferPointer[4] | ((uint16_t)readBufferPointer[5] << 8);
            originalWords[3] = (uint16_t)readBufferPointer[6] | ((uint16_t)readBufferPointer[7] << 8);
            readBufferPointer += 8;

            // Convert into 5 bytes of packed 10-bit data
            writeBufferPointer[0] = (uint8_t)((originalWords[0] & 0x03FC) >> 2);
            writeBufferPointer[1] = (uint8_t)((originalWords[0] & 0x0003) << 6) | (uint8_t)((originalWords[1] & 0x03F0) >> 4);
            writeBufferPointer[2] = (uint8_t)((originalWords[1] & 0x000F) << 4) | (uint8_t)((originalWords[2] & 0x03C0) >> 6);
            writeBufferPointer[3] = (uint8_t)((originalWords[2] & 0x003F) << 2) | (uint8_t)((originalWords[3] & 0x0300) >> 8);
            writeBufferPointer[4] = (uint8_t)((originalWords[3] & 0x00FF));
            writeBufferPointer += 5;
        }
    }
    else if (captureFormat == CaptureFormat::Unsigned10Bit4to1Decimation)
    {
        // Translate the data in the disk buffer to unsigned 10-bit packed data with 4:1 decimation
        for (size_t i = 0; i < readBufferSizeInBytes; i += (8 * 4))
        {
            // Get the original 4 10-bit words
            uint16_t originalWords[4];
            originalWords[0] = (uint16_t)readBufferPointer[0 + 0] | ((uint16_t)readBufferPointer[1 + 0] << 8);
            originalWords[1] = (uint16_t)readBufferPointer[2 + 4] | ((uint16_t)readBufferPointer[3 + 4] << 8);
            originalWords[2] = (uint16_t)readBufferPointer[4 + 8] | ((uint16_t)readBufferPointer[5 + 8] << 8);
            originalWords[3] = (uint16_t)readBufferPointer[6 + 12] | ((uint16_t)readBufferPointer[7 + 12] << 8);
            readBufferPointer += 8 * 4;

            // Convert into 5 bytes of packed 10-bit data
            writeBufferPointer[0] = (uint8_t)((originalWords[0] & 0x03FC) >> 2);
            writeBufferPointer[1] = (uint8_t)((originalWords[0] & 0x0003) << 6) | (uint8_t)((originalWords[1] & 0x03F0) >> 4);
            writeBufferPointer[2] = (uint8_t)((originalWords[1] & 0x000F) << 4) | (uint8_t)((originalWords[2] & 0x03C0) >> 6);
            writeBufferPointer[3] = (uint8_t)((originalWords[2] & 0x003F) << 2) | (uint8_t)((originalWords[3] & 0x0300) >> 8);
            writeBufferPointer[4] = (uint8_t)((originalWords[3] & 0x00FF));
            writeBufferPointer += 5;
        }
    }
    else
    {
        Log().Error("convertRawSampleData(): Unknown capture format {0} specified", captureFormat);
        return false;
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// Buffer sampling methods
//----------------------------------------------------------------------------------------------------------------------
void UsbDeviceBase::QueueBufferSampleRequest(size_t requestedSampleLengthInBytes)
{
    bufferSamplingRequestedLengthInBytes = requestedSampleLengthInBytes;
    bufferSampleRequestPending.test_and_set();
}

//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceBase::GetNextBufferSample(std::vector<uint8_t>& bufferSample)
{
    // If no new buffer sample is available, abort any further processing.
    if (!bufferSampleAvailable.test())
    {
        return false;
    }

    // Copy the requested data into the buffer
    bufferSampleAvailable.clear();
    bufferSample.assign(capturedBufferSample.data(), capturedBufferSample.data() + capturedBufferSample.size());
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// Utility methods
//----------------------------------------------------------------------------------------------------------------------
bool UsbDeviceBase::LockMemoryBufferIntoPhysicalMemory(void* baseAddress, size_t sizeInBytes)
{
#ifdef _WIN32
    // Retrieve the Windows page allocation size
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    size_t systemPageSizeInBytes = (size_t)sysInfo.dwPageSize;

    // Increase the process working set size on Windows, to allow for the extra allocation required by the locked
    // buffers. If we don't do this, VirtualLock will fail if we exceed the initial allocation. Note that since memory
    // allocations may straddle page boundaries, we pad out an extra two memory pages per locked region.
    size_t workingSetSizeIncreaseOverBaseline = (lockedMemorySizeInBytes + sizeInBytes) + (systemPageSizeInBytes * 2 * (lockedMemoryBufferCount + 1));
    size_t newMinWorkingSetSize = originalProcessMinimumWorkingSetSizeInBytes + workingSetSizeIncreaseOverBaseline;
    size_t newMaxWorkingSetSize = originalProcessMaximumWorkingSetSizeInBytes + workingSetSizeIncreaseOverBaseline;
    BOOL setProcessWorkingSetSizeReturn = SetProcessWorkingSetSize(GetCurrentProcess(), newMinWorkingSetSize, newMaxWorkingSetSize);
    if (setProcessWorkingSetSizeReturn == 0)
    {
        DWORD lastError = GetLastError();
        Log().Error("SetProcessWorkingSetSize failed with error code {0}", lastError);
        return false;
    }
#endif

    // Lock the target memory buffer into physical memory
#ifdef _WIN32
    BOOL virtualLockReturn = VirtualLock(baseAddress, sizeInBytes);
    if (virtualLockReturn == 0)
    {
        DWORD lastError = GetLastError();
        Log().Error("VirtualLock failed with error code {0}", lastError);
        return false;
    }
#else
    if (mlock(baseAddress, sizeInBytes) == -1)
    {
        Log().Error("mlock failed");
        return false;
    }
#endif

    // Increase the totals of locked memory
    lockedMemorySizeInBytes += sizeInBytes;
    ++lockedMemoryBufferCount;
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
void UsbDeviceBase::UnlockMemoryBuffer(void* baseAddress, size_t sizeInBytes)
{
    // Release the lock on the target memory buffer
#ifdef _WIN32
    BOOL virtualUnlockReturn = VirtualUnlock(baseAddress, sizeInBytes);
    if (virtualUnlockReturn == 0)
    {
        DWORD lastError = GetLastError();
        Log().Error("VirtualUnlock failed with error code {0}", lastError);
        return;
    }
#else
    munlock(baseAddress, sizeInBytes);
#endif

    // Decrease the totals of locked memory
    lockedMemorySizeInBytes -= sizeInBytes;
    --lockedMemoryBufferCount;

#ifdef _WIN32
    // Retrieve the Windows page allocation size
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    size_t systemPageSizeInBytes = (size_t)sysInfo.dwPageSize;

    // Reduce the process working set size on Windows
    size_t workingSetSizeIncreaseOverBaseline = (lockedMemorySizeInBytes + sizeInBytes) + (systemPageSizeInBytes * 2 * (lockedMemoryBufferCount + 1));
    size_t newMinWorkingSetSize = originalProcessMinimumWorkingSetSizeInBytes + workingSetSizeIncreaseOverBaseline;
    size_t newMaxWorkingSetSize = originalProcessMaximumWorkingSetSizeInBytes + workingSetSizeIncreaseOverBaseline;
    BOOL setProcessWorkingSetSizeReturn = SetProcessWorkingSetSize(GetCurrentProcess(), newMinWorkingSetSize, newMaxWorkingSetSize);
    if (setProcessWorkingSetSizeReturn == 0)
    {
        DWORD lastError = GetLastError();
        Log().Error("SetProcessWorkingSetSize failed with error code {0}", lastError);
        return;
    }
#endif
}

//----------------------------------------------------------------------------------------------------------------------
#ifdef _WIN32
bool UsbDeviceBase::SetCurrentProcessRealtimePriority(ProcessPriorityRestoreInfo& priorityRestoreInfo)
{
    // Request the realtime priority class on Windows. If the process doesn't have administrator rights, we may not
    // obtain realtime priority, but in that case the call will still succeed, and give us the highest priority class
    // we can obtain with the current process privileges.
    int originalPriorityClass = GetPriorityClass(GetCurrentProcess());
    int requestedPriorityClass = REALTIME_PRIORITY_CLASS;
    BOOL setPriorityClassReturn = SetPriorityClass(GetCurrentProcess(), requestedPriorityClass);
    if (setPriorityClassReturn == 0)
    {
        DWORD lastError = GetLastError();
        Log().Error("SetPriorityClass failed with error code {0}", lastError);
        return false;
    }

    // Confirm the priority class we ended up obtaining
    int newPriorityClass = GetPriorityClass(GetCurrentProcess());
    priorityRestoreInfo.originalPriorityClass = originalPriorityClass;
    Log().Info("SetCurrentProcessRealtimePriority: Requesting process priority {0} gave us {1}", requestedPriorityClass, newPriorityClass);
    return true;
}
#endif

//----------------------------------------------------------------------------------------------------------------------
#ifndef _WIN32
bool UsbDeviceBase::SetCurrentProcessRealtimePriority(ProcessPriorityRestoreInfo& priorityRestoreInfo)
{
    // Process priority is not a supported concept on a Linux-based OS
    return true;
}
#endif

//----------------------------------------------------------------------------------------------------------------------
#ifdef _WIN32
void UsbDeviceBase::RestoreCurrentProcessPriority(const ProcessPriorityRestoreInfo& priorityRestoreInfo)
{
    // Restore the original process priority class
    BOOL setPriorityClassReturn = SetPriorityClass(GetCurrentProcess(), priorityRestoreInfo.originalPriorityClass);
    if (setPriorityClassReturn == 0)
    {
        DWORD lastError = GetLastError();
        Log().Error("SetPriorityClass failed with error code {0}", lastError);
        return;
    }
}
#endif

//----------------------------------------------------------------------------------------------------------------------
#ifndef _WIN32
void UsbDeviceBase::RestoreCurrentProcessPriority(const ProcessPriorityRestoreInfo& priorityRestoreInfo)
{
    // Process priority is not a supported concept on a Linux-based OS
}
#endif

//----------------------------------------------------------------------------------------------------------------------
#ifdef _WIN32
bool UsbDeviceBase::SetCurrentThreadRealtimePriority(ThreadPriorityRestoreInfo& priorityRestoreInfo)
{
    // Retrieve the current thread priority
    HANDLE currentThreadHandle = GetCurrentThread();
    int originalThreadPriority = GetThreadPriority(currentThreadHandle);
    if (originalThreadPriority == THREAD_PRIORITY_ERROR_RETURN)
    {
        DWORD lastError = GetLastError();
        Log().Error("GetThreadPriority failed with error code {0}", lastError);
        return false;
    }

    // Attempt to increase thread priority to time critical
    int requestedPriority = THREAD_PRIORITY_TIME_CRITICAL;
    BOOL setThreadPriorityReturn = SetThreadPriority(currentThreadHandle, requestedPriority);
    if (setThreadPriorityReturn == 0)
    {
        DWORD lastError = GetLastError();
        Log().Error("SetThreadPriority failed with error code {0}", lastError);
        return false;
    }

    // Confirm the thread priority we ended up obtaining
    int newThreadPriority = GetThreadPriority(currentThreadHandle);
    priorityRestoreInfo.originalPriority = originalThreadPriority;
    Log().Info("SetCurrentThreadRealtimePriority: Requesting thread priority {0} gave us {1}", requestedPriority, newThreadPriority);
    return true;
}
#endif

//----------------------------------------------------------------------------------------------------------------------
#ifndef _WIN32
bool UsbDeviceBase::SetCurrentThreadRealtimePriority(ThreadPriorityRestoreInfo& priorityRestoreInfo)
{
    int oldSchedPolicy;
    sched_param oldSchedParam;
    pthread_getschedparam(pthread_self(), &oldSchedPolicy, &oldSchedParam);

    priorityRestoreInfo.oldSchedPolicy = oldSchedPolicy;
    priorityRestoreInfo.oldSchedParam = oldSchedParam;

    int targetPolicy = SCHED_RR;
    int minSchedPriority = sched_get_priority_min(targetPolicy);
    int maxSchedPriority = sched_get_priority_max(targetPolicy);
    sched_param schedParams;
    if (minSchedPriority == -1 || maxSchedPriority == -1)
    {
        schedParams.sched_priority = 0;
    }
    else
    {
        // Put the priority about 3/4 of the way through its range
        schedParams.sched_priority = (minSchedPriority + (3 * maxSchedPriority)) / 4;
    }

    if (pthread_setschedparam(pthread_self(), targetPolicy, &schedParams) == 0)
    {
        Log().Info("SetCurrentThreadRealtimePriority: Thread priority set with policy SCHED_RR");
    }
    else
    {
        Log().Warning("SetCurrentThreadRealtimePriority: Unable to set thread priority");
    }
    return true;
}
#endif

//----------------------------------------------------------------------------------------------------------------------
#ifdef _WIN32
void UsbDeviceBase::RestoreCurrentThreadPriority(const ThreadPriorityRestoreInfo& priorityRestoreInfo)
{
    SetThreadPriority(GetCurrentThread(), priorityRestoreInfo.originalPriority);
}
#endif

//----------------------------------------------------------------------------------------------------------------------
#ifndef _WIN32
void UsbDeviceBase::RestoreCurrentThreadPriority(const ThreadPriorityRestoreInfo& priorityRestoreInfo)
{
    if (pthread_setschedparam(pthread_self(), priorityRestoreInfo.oldSchedPolicy, &priorityRestoreInfo.oldSchedParam) != 0)
    {
        Log().Warning("RestoreCurrentThreadPriority: Unable to restore original scheduling policy");
    }
}
#endif
