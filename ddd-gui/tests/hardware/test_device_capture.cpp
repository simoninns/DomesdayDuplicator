/************************************************************************

    test_device_capture.cpp

    T5 tests against an attached Domesday Duplicator
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "capture_pipeline.h"
#include "firmware_version.h"
#include "logger.h"
#include "sample_format.h"
#include "sample_sink.h"
#include "usb_device.h"
#include "version.h"
#include "wire_protocol.h"

namespace ddd::capture {
namespace {

using namespace std::chrono_literals;

// The T5 tier: these need a device attached, and they are excluded from every
// ordinary run by their `hil` label. A test that passed because no hardware was
// present would be worse than no test, so each one refuses rather than skips
// when there is nothing to talk to — and the whole binary is only ever run
// deliberately.
//
//   ctest --test-dir build -L hil
//
// Nothing here writes to the FX3 EEPROM or the FPGA flash. These stream from
// the device and send the 0xB6 configuration request, which is what the
// application does in normal use; reprogramming is a manual procedure and stays
// one (AGENTS.md §4).

// Long enough to prove the transfer geometry sustains the rate through many
// laps of the ring, short enough to run in a development loop. The hour-long
// soak in TESTING.md §5 is still a manual procedure.
constexpr std::chrono::seconds kMonitorSeconds{10};

// The narrowest thing a source needs to run: a stop flag and a transfer count.
// Used by the tests that drive a source directly, without the pipeline, so they
// can measure the transfer path on its own.
class ProbeControl : public SourceControl {
 public:
  bool StopRequested() const override { return stop.load(); }
  bool AbortRequested() const override { return false; }
  void AddCompletedTransfers(uint64_t count) override { transfers += count; }
  void Log(const std::string& message) override {
    std::cout << "[          ] source: " << message << "\n";
  }

  std::atomic<bool> stop{false};
  std::atomic<uint64_t> transfers{0};
};

class HardwareTest : public ::testing::Test {
 protected:
  void SetUp() override {
    logger_ = std::make_unique<CallbackLogger>(
        [](LogLevel level, const std::string& message) {
          std::cout << "[          ] " << LogLevelName(level) << ": " << message
                    << "\n";
        },
        LogLevel::kInfo);

    device_ = MakeUsbDevice(logger_.get());
    ASSERT_NE(device_, nullptr) << "the USB backend could not be started";

    std::vector<DeviceInfo> devices;
    ASSERT_TRUE(device_->Enumerate(devices)) << "enumeration failed";
    ASSERT_FALSE(devices.empty())
        << "no Domesday Duplicator is attached — this tier needs one";

    attached_ = devices.front();
  }

  std::unique_ptr<CallbackLogger> logger_;
  std::unique_ptr<IUsbDevice> device_;
  DeviceInfo attached_;
};

TEST_F(HardwareTest, TheAttachedDeviceIsFoundAtAUsableSpeed) {
  std::cout << "[          ] device at " << attached_.path << ", "
            << DeviceSpeedName(attached_.speed) << ", product string \""
            << attached_.product_string << "\"\n";

  EXPECT_FALSE(attached_.path.empty()) << "the device has no usable identity";
  EXPECT_TRUE(attached_.CanCarryCapture())
      << "the device is attached at " << DeviceSpeedName(attached_.speed)
      << ", which cannot carry 80 MB/s — move it to a USB 3 port";
}

TEST_F(HardwareTest, TheFirmwareSaysWhichBuildItIsRunning) {
  const FirmwareIdentity firmware = DescribeFirmware(attached_.product_string);

  std::cout << "[          ] firmware commit \"" << firmware.commit
            << "\", application " << Commit() << "\n";

  // Nothing here compares the two. They come from separate release streams —
  // gui-v* and fw-v* — so a development machine routinely has one newer than
  // the other and that is not a fault. What matters is that the device
  // reported something readable, because a device that reports nothing warns
  // on every connection.
  EXPECT_TRUE(firmware.NamesCommit())
      << "the device's product string carried no readable commit: \""
      << attached_.product_string << "\"";
}

// The transfer path alone, with nothing downstream of it: how fast does the
// device actually deliver, and does the ring hand every slot over cleanly?
//
// Separate from the monitor test because it answers a different question and
// can answer it even when the data itself is wrong. The rate is diagnostic in
// its own right: the device's output is clocked by a 40 MHz ADC, so a correct
// device delivers 80 MB/s and cannot deliver more. Anything faster is not
// coming from the ADC.
TEST_F(HardwareTest, TheTransferPathDeliversAtTheDeviceRate) {
  TransferResult opened = TransferResult::kConnectionFailure;
  std::unique_ptr<ISampleSource> source =
      device_->OpenSource(attached_.path, {}, opened);
  ASSERT_NE(source, nullptr) << "the device could not be opened: "
                             << TransferResultDescription(opened);

  DiskBufferRing ring(
      source->PlanGeometry(DiskBufferRing::kDefaultQueueSizeBytes));
  ASSERT_EQ(source->Prepare(ring), TransferResult::kSuccess);

  ProbeControl control;
  std::thread producer([&] { source->Run(ring, control); });

  constexpr size_t kSlots = 200;
  const auto started = std::chrono::steady_clock::now();
  size_t received = 0;
  for (size_t slot = 0; slot < kSlots; ++slot) {
    if (!ring.WaitForSlotFull(slot % ring.slot_count())) {
      break;
    }
    ring.MarkSlotFree(slot % ring.slot_count());
    ++received;
  }
  const double seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - started)
          .count();

  control.stop = true;
  ring.Abort();
  producer.join();
  source->Finish();

  ASSERT_EQ(received, kSlots)
      << "the device stopped delivering after " << received << " buffers";

  const double bytes = static_cast<double>(received) *
                       static_cast<double>(ring.slot_size_bytes());
  const double rate = bytes / seconds;

  std::cout << "[          ] " << (bytes / (1024.0 * 1024.0)) << " MB in "
            << seconds << " s = " << (rate / (1024.0 * 1024.0))
            << " MB/s, peak queue depth " << ring.PeakSlotsInUse() << " of "
            << ring.slot_count() << "\n";

  const double expected = static_cast<double>(kWireBytesPerSecond);

  // Below the rate means transfers are being dropped: the device cannot slow
  // down, so anything it sends that the host does not take is simply lost.
  EXPECT_GT(rate, expected * 0.95)
      << "delivered " << (rate / 1e6) << " MB/s against " << (expected / 1e6)
      << " MB/s expected";

  // Above the rate means the data is not coming from the ADC at all. A 40 Msps
  // converter cannot produce more than 80 MB/s however fast the link is, so a
  // higher figure means something else is filling the endpoint — an
  // unprogrammed FPGA, or gateware that is not the sampler.
  EXPECT_LT(rate, expected * 1.05)
      << "delivered " << (rate / 1e6)
      << " MB/s, which is faster than a 40 Msps converter can produce — the "
         "data is not coming from the ADC";
}

// The regression test for a hang found on this hardware, and one the synthetic
// source cannot reach because it has no completion callbacks.
//
// Aborting mid-stream leaves transfers that have already completed but not yet
// been reaped. Their callbacks still run, and if one of them resubmits, the
// shutdown sweep waits for a transfer created after the sweep began — forever.
TEST_F(HardwareTest, AbortingMidStreamStopsPromptly) {
  TransferResult opened = TransferResult::kConnectionFailure;
  std::unique_ptr<ISampleSource> source =
      device_->OpenSource(attached_.path, {}, opened);
  ASSERT_NE(source, nullptr) << "the device could not be opened: "
                             << TransferResultDescription(opened);

  // On the heap because the failure path below has to keep both alive after the
  // test returns.
  auto ring = std::make_unique<DiskBufferRing>(
      source->PlanGeometry(DiskBufferRing::kDefaultQueueSizeBytes));
  ASSERT_EQ(source->Prepare(*ring), TransferResult::kSuccess);

  ProbeControl control;
  std::atomic<bool> returned{false};
  std::thread producer([&] {
    source->Run(*ring, control);
    returned = true;
  });

  // Long enough that transfers have wrapped the ring several times, so the
  // abort lands somewhere arbitrary rather than during start-up.
  for (size_t slot = 0; slot < 300; ++slot) {
    if (!ring->WaitForSlotFull(slot % ring->slot_count())) {
      break;
    }
    ring->MarkSlotFree(slot % ring->slot_count());
  }

  const auto aborted_at = std::chrono::steady_clock::now();
  control.stop = true;
  ring->Abort();

  // Polled rather than joined, so a hang is a failing test rather than a test
  // run that never ends.
  while (!returned.load() &&
         std::chrono::steady_clock::now() - aborted_at < 10s) {
    std::this_thread::sleep_for(5ms);
  }
  const bool stopped = returned.load();
  const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - aborted_at);

  if (stopped) {
    producer.join();
    source->Finish();
  } else {
    // A transfer thread still inside libusb is writing into the ring, so
    // neither it nor the source may be destroyed. Both are handed to a store
    // that outlives the test rather than freed: the test has already failed,
    // and the only thing left to get right is not turning that failure into a
    // crash that hides which test failed.
    static std::vector<std::unique_ptr<ISampleSource>> abandoned_sources;
    static std::vector<std::unique_ptr<DiskBufferRing>> abandoned_rings;
    abandoned_sources.push_back(std::move(source));
    abandoned_rings.push_back(std::move(ring));
    producer.detach();
  }

  ASSERT_TRUE(stopped) << "the transfer thread did not return within ten "
                          "seconds of being aborted";

  std::cout << "[          ] aborted in " << took.count() << " ms\n";
  EXPECT_LT(took.count(), 2000);
}

// The whole capture path against the real device: open, claim, stream at
// 80 MB/s, validate every sequence marker, and stop cleanly.
//
// This is the test the synthetic source cannot be a substitute for. Everything
// it exercises that the synthetic source does not — the endpoint, the cable,
// the host controller, the kernel's usbfs limits, the transfer geometry as the
// hardware actually completes it — is where the interesting failures live.
TEST_F(HardwareTest, MonitorModeSustainsTheRateWithNoSamplesLost) {
  ASSERT_TRUE(device_->WriteRegister(attached_.path, kRegisterTestMode, 0))
      << "the device would not accept the test-mode register write";

  TransferResult opened = TransferResult::kConnectionFailure;
  std::unique_ptr<ISampleSource> source =
      device_->OpenSource(attached_.path, {}, opened);
  ASSERT_NE(source, nullptr) << "the device could not be opened: "
                             << TransferResultDescription(opened);

  CapturePipeline pipeline(logger_.get());
  CapturePipeline::Options options;

  ASSERT_TRUE(
      pipeline.Start(source.get(), std::make_unique<NullSink>(), options))
      << pipeline.ResultDetail();

  std::this_thread::sleep_for(kMonitorSeconds);

  const CaptureStats running = pipeline.stats().Read();

  pipeline.RequestStop();
  pipeline.Wait();

  const CaptureStats final_stats = pipeline.stats().Read();

  std::cout << "[          ] " << final_stats.buffers_processed << " buffers, "
            << final_stats.transfers_completed << " transfers, "
            << (running.throughput_bytes_per_second / (1024.0 * 1024.0))
            << " MB/s, peak queue depth " << final_stats.peak_slots_in_use
            << " of " << final_stats.slot_count << ", samples "
            << final_stats.metrics.minimum_value << " to "
            << final_stats.metrics.maximum_value << "\n";

  EXPECT_EQ(pipeline.Result(), TransferResult::kSuccess)
      << TransferResultName(pipeline.Result()) << ": "
      << pipeline.ResultDetail();

  // The sequence markers are the only evidence a capture is bit-perfect. A
  // device whose gateware emits them and a run that reached kRunning together
  // mean every sample between the first and the last arrived.
  EXPECT_EQ(final_stats.sequence_state, SequenceState::kRunning)
      << "sequence state was " << SequenceStateName(final_stats.sequence_state);

  EXPECT_GT(final_stats.buffers_processed, 0U);

  // Within a few per cent of the wire rate. Below that and something is
  // dropping transfers rather than merely running slowly, because the device
  // has no way to slow down.
  const double expected_bytes = static_cast<double>(kWireBytesPerSecond) *
                                static_cast<double>(kMonitorSeconds.count());
  const double achieved_bytes =
      static_cast<double>(final_stats.buffers_processed) *
      static_cast<double>(
          final_stats.slot_count > 0 ? pipeline.ring()->slot_size_bytes() : 0);
  EXPECT_GT(achieved_bytes, expected_bytes * 0.95)
      << "moved " << achieved_bytes << " bytes where " << expected_bytes
      << " were expected";
}

// The test-mode path, end to end. The gateware replaces the ADC input with a
// known ramp, so every sample's correct value is known in advance — which makes
// this the only check that covers the whole path from the ADC pins to the
// buffer without needing anything connected to the RF input.
TEST_F(HardwareTest, TheTestPatternArrivesIntact) {
  ASSERT_TRUE(device_->WriteRegister(attached_.path, kRegisterTestMode, 1))
      << "the device would not accept the test-mode register write";

  TransferResult opened = TransferResult::kConnectionFailure;
  std::unique_ptr<ISampleSource> source =
      device_->OpenSource(attached_.path, {}, opened);
  ASSERT_NE(source, nullptr) << "the device could not be opened: "
                             << TransferResultDescription(opened);

  CapturePipeline pipeline(logger_.get());
  CapturePipeline::Options options;
  options.test_mode = true;

  ASSERT_TRUE(
      pipeline.Start(source.get(), std::make_unique<NullSink>(), options))
      << pipeline.ResultDetail();

  std::this_thread::sleep_for(3s);

  pipeline.RequestStop();
  pipeline.Wait();

  // Put it back into normal mode whatever happened, so a failing test does not
  // leave a device that captures ramps until someone notices.
  device_->WriteRegister(attached_.path, kRegisterTestMode, 0);

  const TestPatternVerifier::Result verdict = pipeline.test_pattern_result();
  std::cout << "[          ] checked " << verdict.samples_checked
            << " samples against the ramp\n";

  EXPECT_EQ(pipeline.Result(), TransferResult::kSuccess)
      << TransferResultName(pipeline.Result()) << ": "
      << pipeline.ResultDetail();
  EXPECT_TRUE(verdict.passed)
      << "the ramp broke after " << verdict.samples_checked
      << " samples: expected " << verdict.expected_value << ", got "
      << verdict.actual_value;
  EXPECT_GT(verdict.samples_checked, 0U) << "the verifier never ran";
}

// The decimated path, which is the half of the capture path nothing else here
// reaches.
//
// Three separate claims, and the test fails on any of them:
//
//   the device accepts the factor and reports it back;
//   the stream that arrives is half the rate it was;
//   and it is still an unbroken ramp.
//
// The third is the one worth having. The gateware puts its test generator
// downstream of the decimator, so a decimated test capture is every sample
// plus one exactly as a full-rate one is — which means the sequence markers
// and the ramp both still apply, and a decimator that dropped a sample at a
// buffer seam, or that got its phase wrong across one, shows up here as a
// break rather than as a subtly wrong signal nobody can see.
//
// What this cannot check is the anti-alias filter. A ramp is not a spectrum
// and every sample of it is in the passband, so the filter's response is
// pinned by fpga/tests/tb_halfBandDecimator.v in simulation and by
// fpga/make-halfband-coefficients.py in arithmetic. Measuring it on hardware
// needs a signal generator on the RF input — TESTING.md section 5.
TEST_F(HardwareTest, TheDecimatedTestPatternArrivesIntactAtHalfTheRate) {
  ASSERT_TRUE(device_->WriteRegister(attached_.path, kRegisterTestMode, 1))
      << "the device would not accept the test-mode register write";
  ASSERT_TRUE(device_->WriteRegister(attached_.path, kRegisterDecimation,
                                     kDecimationHalfRate))
      << "the device would not accept the decimation register write";

  // Read it back before streaming a byte. The gateware normalises a factor it
  // cannot do rather than storing it, so this is the device stating what the
  // capture path is about to do rather than echoing what it was asked.
  std::vector<uint8_t> confirmed;
  ASSERT_TRUE(
      device_->ReadRegisters(attached_.path, kRegisterDecimation, 1, confirmed))
      << "the decimation register could not be read back";
  ASSERT_FALSE(confirmed.empty());
  EXPECT_EQ(confirmed.front(), kDecimationHalfRate)
      << "the gateware did not accept 2:1 decimation";

  TransferResult opened = TransferResult::kConnectionFailure;
  std::unique_ptr<ISampleSource> source =
      device_->OpenSource(attached_.path, {}, opened);
  ASSERT_NE(source, nullptr) << "the device could not be opened: "
                             << TransferResultDescription(opened);

  CapturePipeline pipeline(logger_.get());
  CapturePipeline::Options options;
  options.test_mode = true;

  ASSERT_TRUE(
      pipeline.Start(source.get(), std::make_unique<NullSink>(), options))
      << pipeline.ResultDetail();

  constexpr auto kDecimatedSeconds = 3s;
  std::this_thread::sleep_for(kDecimatedSeconds);

  const CaptureStats running = pipeline.stats().Read();

  pipeline.RequestStop();
  pipeline.Wait();

  // Put the device back whatever happened, so a failing test does not leave a
  // device that captures ramps at half rate until somebody notices.
  device_->WriteRegister(attached_.path, kRegisterTestMode, 0);
  device_->WriteRegister(attached_.path, kRegisterDecimation,
                         kDecimationEverySample);

  const CaptureStats final_stats = pipeline.stats().Read();
  const TestPatternVerifier::Result verdict = pipeline.test_pattern_result();

  const double megabytes_per_second =
      running.throughput_bytes_per_second / (1024.0 * 1024.0);
  std::cout << "[          ] " << megabytes_per_second << " MB/s, checked "
            << verdict.samples_checked << " samples against the ramp\n";

  EXPECT_EQ(pipeline.Result(), TransferResult::kSuccess)
      << TransferResultName(pipeline.Result()) << ": "
      << pipeline.ResultDetail();

  // Half the wire rate, and the bounds are wide because what is being
  // distinguished is 40 MB/s from 80, not one figure from a neighbouring one.
  // A decimator that was not running would land at the top of this range and
  // outside it.
  const double wire_rate =
      static_cast<double>(kWireBytesPerSecond) / (1024.0 * 1024.0);
  EXPECT_GT(megabytes_per_second, wire_rate * 0.40)
      << "the stream was slower than half rate";
  EXPECT_LT(megabytes_per_second, wire_rate * 0.60)
      << "the stream did not halve — the decimator is not running";

  // And it is still a capture, not merely half of one.
  EXPECT_EQ(final_stats.sequence_state, SequenceState::kRunning)
      << "sequence state was " << SequenceStateName(final_stats.sequence_state);

  EXPECT_TRUE(verdict.passed)
      << "the decimated ramp broke after " << verdict.samples_checked
      << " samples: expected " << verdict.expected_value << ", got "
      << verdict.actual_value;
  EXPECT_GT(verdict.samples_checked, 0U) << "the verifier never ran";
}

}  // namespace
}  // namespace ddd::capture
