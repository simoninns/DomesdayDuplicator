#pragma once
#include "ILogger.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <fstream>
#include <optional>
#include <string>
#include <vector>
#include <atomic>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

class UsbDeviceBase
{
public:
    // Enumerations
    enum class CaptureFormat
    {
        Signed16Bit,
        Unsigned10Bit,
        Unsigned10Bit4to1Decimation,
    };
    enum class TransferResult
    {
        Running,
        Success,
        FileCreationError,
        BufferUnderflow,
        ConnectionFailure,
        UsbMemoryLimit,
        UsbTransferFailure,
        FileWriteError,
        SequenceMismatch,
        VerificationError,
        ProgramError,
        ForcedAbort,
    };

public:
    // Constructors
    UsbDeviceBase(const ILogger& log);
    virtual ~UsbDeviceBase();
    virtual bool Initialize(uint16_t vendorId, uint16_t productId);

    // Device methods
    virtual bool DevicePresent(const std::string& preferredDevicePath) const = 0;
    virtual bool GetPresentDevicePaths(std::vector<std::string>& devicePaths) const = 0;
    void SendConfigurationCommand(const std::string& preferredDevicePath, bool testMode);

    // Capture methods
    bool StartCapture(const std::filesystem::path& filePath, CaptureFormat format, const std::string& preferredDevicePath, bool isTestMode, bool useSmallUsbTransfers, bool useAsyncFileIo, size_t usbTransferQueueSizeInBytes, size_t diskBufferQueueSizeInBytes);
    void StopCapture();
    bool GetTransferInProgress() const;
    TransferResult GetTransferResult() const;
    size_t GetNumberOfTransfers() const;
    size_t GetNumberOfDiskBuffersWritten() const;
    size_t GetFileSizeWrittenInBytes() const;
    size_t GetProcessedSampleCount() const;
    size_t GetMinSampleValue() const;
    size_t GetMaxSampleValue() const;
    size_t GetClippedMinSampleCount() const;
    size_t GetClippedMaxSampleCount() const;
    size_t GetRecentMinSampleValue() const;
    size_t GetRecentMaxSampleValue() const;
    size_t GetRecentClippedMinSampleCount() const;
    size_t GetRecentClippedMaxSampleCount() const;
    bool GetTransferHadSequenceNumbers() const;

    // Buffer sampling methods
    void QueueBufferSampleRequest(size_t requestedSampleLengthInBytes);
    bool GetNextBufferSample(std::vector<uint8_t>& sampleBuffer);

protected:
    // Structures
    struct DiskBufferEntry
    {
        std::vector<uint8_t> readBuffer;
        std::atomic_flag isDiskBufferFull;
        std::atomic_flag dumpingBuffer;
#ifdef _WIN32
        OVERLAPPED diskWriteOverlappedBuffer;
        bool diskWriteInProgress = false;
#endif
    };
    struct ThreadPriorityRestoreInfo
    {
#ifdef _WIN32
        int originalPriority;
#else
        int oldSchedPolicy;
        sched_param oldSchedParam;
#endif
    };

protected:
    // Log methods
    const ILogger& Log() const;

    // Device methods
    virtual bool DeviceConnected() const = 0;
    virtual bool ConnectToDevice(const std::string& preferredDevicePath) = 0;
    virtual void DisconnectFromDevice() = 0;
    virtual bool SendVendorSpecificCommand(const std::string& preferredDevicePath, uint8_t command, uint16_t value) = 0;

    // Capture methods
    virtual void CalculateDesiredBufferCountAndSize(bool useSmallUsbTransfers, size_t usbTransferQueueSizeInBytes, size_t diskBufferQueueSizeInBytes, size_t& bufferCount, size_t& bufferSizeInBytes) const = 0;
    bool UsbTransferDumpBuffers() const;
    bool UsbTransferStopRequested() const;
    virtual void UsbTransferThread() = 0;
    DiskBufferEntry& GetDiskBuffer(size_t bufferNo);
    size_t GetDiskBufferCount() const;
    size_t GetSingleDiskBufferSizeInBytes() const;
    size_t GetUsbTransferQueueSizeInBytes() const;
    bool GetUseSmallUsbTransfers() const;
    void SetUsbTransferFinished(TransferResult result);
    void AddCompletedTransferCount(size_t incrementCount);

    // Utility methods
    bool LockMemoryBufferIntoPhysicalMemory(void* baseAddress, size_t sizeInBytes);
    void UnlockMemoryBuffer(void* baseAddress, size_t sizeInBytes);
    bool SetCurrentThreadRealtimePriority(ThreadPriorityRestoreInfo& priorityRestoreInfo);
    void RestoreCurrentThreadPriority(const ThreadPriorityRestoreInfo& priorityRestoreInfo);

private:
    // Enumerations
    enum class SequenceState
    {
        Sync,
        Running,
        Disabled,
        Failed,
    };

    // Structures
    struct ProcessPriorityRestoreInfo
    {
#ifdef _WIN32
        int originalPriorityClass;
#endif
    };

private:
    // Capture methods
    void CaptureThread();
    void SetProcessingFinished(TransferResult result);

    // Processing methods
    void ProcessingThread();
    bool ProcessSequenceMarkersAndUpdateSampleMetrics(size_t diskBufferIndex, size_t& processedSampleCount, uint16_t& minValue, uint16_t& maxValue, size_t& minClippedCount, size_t& maxClippedCount);
    bool VerifyTestSequence(size_t diskBufferIndex);
    bool ConvertRawSampleData(size_t diskBufferIndex, CaptureFormat captureFormat, std::vector<uint8_t>& outputBuffer) const;

    // Utility methods
    bool SetCurrentProcessRealtimePriority(ProcessPriorityRestoreInfo& priorityRestoreInfo);
    void RestoreCurrentProcessPriority(const ProcessPriorityRestoreInfo& priorityRestoreInfo);

private:
    // Logging state
    const ILogger& log;

    // Capture settings
    std::filesystem::path captureFilePath;
    CaptureFormat captureFormat;
    bool captureIsTestMode = false;
    size_t currentUsbTransferQueueSizeInBytes = 0;
    bool currentUseSmallUsbTransfers = false;

    // Capture status
    bool transferInProgress = false;
    bool useWindowsOverlappedFileIo = false;
    std::atomic<TransferResult> captureResult = TransferResult::Success;
    std::atomic<size_t> transferCount = 0;
    std::atomic<size_t> transferBufferWrittenCount = 0;
    std::atomic<size_t> transferFileSizeWrittenInBytes = 0;
    std::atomic<size_t> processedSampleCount = 0;
    std::atomic<uint16_t> minSampleValue = 0;
    std::atomic<uint16_t> maxSampleValue = 0;
    std::atomic<size_t> clippedMinSampleCount = 0;
    std::atomic<size_t> clippedMaxSampleCount = 0;
    std::atomic<uint16_t> recentMinSampleValue = 0;
    std::atomic<uint16_t> recentMaxSampleValue = 0;
    std::atomic<size_t> recentClippedMinSampleCount = 0;
    std::atomic<size_t> recentClippedMaxSampleCount = 0;
    std::atomic_flag captureThreadRunning;
    std::atomic_flag captureThreadStopRequested;
    std::atomic_flag usbTransferRunning;
    std::atomic_flag processingRunning;
    std::atomic_flag dumpAllCaptureDataInProgress;
    std::atomic_flag usbTransferStopRequested;
    std::atomic_flag processingStopRequested;
    std::atomic<TransferResult> usbTransferResult;
    std::atomic<TransferResult> processingResult;

    // Disk buffer state
    size_t totalDiskBufferEntryCount = 0;
    size_t diskBufferSizeInBytes = 0;
    std::unique_ptr<DiskBufferEntry[]> diskBufferEntries;

    // Conversion buffer state
#ifdef _WIN32
    static const size_t conversionBufferCount = 2;
#else
    static const size_t conversionBufferCount = 1;
#endif
    size_t conversionBufferIndex = 0;
    std::vector<uint8_t> conversionBuffers[conversionBufferCount];

    // Capture output file state
    std::ofstream captureOutputFile;
#ifdef _WIN32
    HANDLE windowsCaptureOutputFileHandle;
#endif

    // Sequence/test data state
    SequenceState sequenceState = SequenceState::Sync;
    uint32_t savedSequenceCounter = 0;
    std::optional<uint16_t> expectedNextTestDataValue;
    std::optional<uint16_t> testDataMax;

    // Buffer sample state
    std::atomic_flag bufferSampleRequestPending;
    std::atomic_flag bufferSampleAvailable;
    std::atomic<size_t> bufferSamplingRequestedLengthInBytes;
    std::vector<uint8_t> capturedBufferSample;

    // Locked memory state
    size_t lockedMemorySizeInBytes = 0;
    size_t lockedMemoryBufferCount = 0;
    size_t originalProcessMinimumWorkingSetSizeInBytes = 0;
    size_t originalProcessMaximumWorkingSetSizeInBytes = 0;

    // Process priority state
    ProcessPriorityRestoreInfo processPriorityRestoreInfo;
    bool boostedProcessPriority = false;
};
