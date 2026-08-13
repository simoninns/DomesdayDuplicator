/************************************************************************

    test_device_monitor.cpp

    T1 tests for hot-plug detection
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#include "device_monitor.h"
#include "fake_usb_device.h"
#include "usb_device_info.h"

namespace ddd::capture {
namespace {

using namespace std::chrono_literals;

// Fast enough that the tests take milliseconds, and the interval is a parameter
// precisely so they can. The production default is 200 ms.
constexpr std::chrono::milliseconds kTestInterval{2};

// Everything the monitor has reported, and a way to wait for it without
// sleeping for a fixed time and hoping.
class Reports {
 public:
  DeviceMonitor::Callback Callback() {
    return [this](const std::vector<DeviceInfo>& devices) {
      const std::lock_guard<std::mutex> guard(mutex_);
      reports_.push_back(devices);
    };
  }

  size_t Count() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return reports_.size();
  }

  std::vector<DeviceInfo> Latest() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return reports_.empty() ? std::vector<DeviceInfo>{} : reports_.back();
  }

  // Wait for at least `count` reports. Returns false on timeout, so a test
  // fails with a message rather than hanging.
  bool WaitFor(size_t count, std::chrono::milliseconds limit = 2000ms) const {
    const auto deadline = std::chrono::steady_clock::now() + limit;
    while (std::chrono::steady_clock::now() < deadline) {
      if (Count() >= count) {
        return true;
      }
      std::this_thread::sleep_for(1ms);
    }
    return false;
  }

 private:
  mutable std::mutex mutex_;
  std::vector<std::vector<DeviceInfo>> reports_;
};

TEST(DeviceMonitorTest, TheFirstReportArrivesWithoutAnythingChanging) {
  // A panel built after the monitor started would otherwise show nothing until
  // someone unplugged something.
  FakeUsbDevice device;
  device.SetSingleDevice("bus-1", DeviceSpeed::kSuper, "");

  Reports reports;
  DeviceMonitor monitor(&device, nullptr);
  monitor.Start(reports.Callback(), kTestInterval);

  ASSERT_TRUE(reports.WaitFor(1));
  EXPECT_EQ(reports.Latest().size(), 1U);
}

TEST(DeviceMonitorTest, AnAttachedDeviceIsReported) {
  FakeUsbDevice device;

  Reports reports;
  DeviceMonitor monitor(&device, nullptr);
  monitor.Start(reports.Callback(), kTestInterval);

  ASSERT_TRUE(reports.WaitFor(1));
  EXPECT_TRUE(reports.Latest().empty());

  device.SetSingleDevice("bus-1", DeviceSpeed::kSuper, "");

  ASSERT_TRUE(reports.WaitFor(2));
  ASSERT_EQ(reports.Latest().size(), 1U);
  EXPECT_EQ(reports.Latest().front().path, "bus-1");
}

TEST(DeviceMonitorTest, ADetachedDeviceIsReported) {
  FakeUsbDevice device;
  device.SetSingleDevice("bus-1", DeviceSpeed::kSuper, "");

  Reports reports;
  DeviceMonitor monitor(&device, nullptr);
  monitor.Start(reports.Callback(), kTestInterval);

  ASSERT_TRUE(reports.WaitFor(1));

  device.SetDevices({});

  ASSERT_TRUE(reports.WaitFor(2));
  EXPECT_TRUE(reports.Latest().empty());
}

// The requirement in the plan is that an attach or detach is reflected in the
// GUI within 500 ms. This measures the monitor's own contribution to that:
// polling at the production interval, how long from the device appearing to the
// callback.
TEST(DeviceMonitorTest, AnAttachIsNoticedWellInsideHalfASecond) {
  FakeUsbDevice device;

  Reports reports;
  DeviceMonitor monitor(&device, nullptr);
  monitor.Start(reports.Callback(), DeviceMonitor::kDefaultInterval);

  ASSERT_TRUE(reports.WaitFor(1));

  const auto attached_at = std::chrono::steady_clock::now();
  device.SetSingleDevice("bus-1", DeviceSpeed::kSuper, "");

  ASSERT_TRUE(reports.WaitFor(2));
  const auto noticed_after = std::chrono::steady_clock::now() - attached_at;

  const auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(noticed_after);
  EXPECT_LT(milliseconds.count(), 500)
      << "took " << milliseconds.count() << " ms to notice an attach";
}

// Redrawing five times a second whether or not anything happened would be a
// waste on a machine otherwise trying to sustain 80 MB/s, and would make the
// device list flicker for no reason.
TEST(DeviceMonitorTest, NothingIsReportedWhileNothingChanges) {
  FakeUsbDevice device;
  device.SetSingleDevice("bus-1", DeviceSpeed::kSuper, "");

  Reports reports;
  DeviceMonitor monitor(&device, nullptr);
  monitor.Start(reports.Callback(), kTestInterval);

  ASSERT_TRUE(reports.WaitFor(1));

  // Long enough for many polls at a 2 ms interval.
  std::this_thread::sleep_for(100ms);

  EXPECT_EQ(reports.Count(), 1U);
  EXPECT_GT(monitor.PollCount(), 1U) << "the monitor never polled at all";
}

// The same device moved to a faster port is a different situation, and a user
// who has just done exactly that to fix a speed warning needs to see it.
TEST(DeviceMonitorTest, ADeviceThatChangesSpeedCountsAsAChange) {
  FakeUsbDevice device;
  device.SetSingleDevice("bus-1", DeviceSpeed::kHigh, "");

  Reports reports;
  DeviceMonitor monitor(&device, nullptr);
  monitor.Start(reports.Callback(), kTestInterval);

  ASSERT_TRUE(reports.WaitFor(1));

  device.SetSingleDevice("bus-1", DeviceSpeed::kSuper, "");

  ASSERT_TRUE(reports.WaitFor(2));
  EXPECT_EQ(reports.Latest().front().speed, DeviceSpeed::kSuper);
}

// A failed enumeration is not an empty one. Reporting "no devices" because the
// USB subsystem was momentarily unavailable would make the device vanish from
// the panel and come back a fifth of a second later, which reads as a flapping
// cable rather than as what it is.
TEST(DeviceMonitorTest, AFailedEnumerationDoesNotLookLikeAnEmptyOne) {
  FakeUsbDevice device;
  device.SetSingleDevice("bus-1", DeviceSpeed::kSuper, "");

  Reports reports;
  DeviceMonitor monitor(&device, nullptr);
  monitor.Start(reports.Callback(), kTestInterval);

  ASSERT_TRUE(reports.WaitFor(1));

  device.SetEnumerationFails(true);
  std::this_thread::sleep_for(50ms);

  EXPECT_EQ(reports.Count(), 1U);
  ASSERT_EQ(reports.Latest().size(), 1U);
  EXPECT_EQ(reports.Latest().front().path, "bus-1");
}

// Enumeration opens devices and does control transfers on them. Doing that to a
// device that is streaming would put avoidable traffic on the bus for an answer
// that is already obvious.
TEST(DeviceMonitorTest, SuspendingStopsEnumerating) {
  FakeUsbDevice device;

  Reports reports;
  DeviceMonitor monitor(&device, nullptr);
  monitor.Start(reports.Callback(), kTestInterval);

  ASSERT_TRUE(reports.WaitFor(1));

  monitor.SetSuspended(true);
  std::this_thread::sleep_for(20ms);
  const uint64_t polls_at_suspend = monitor.PollCount();

  std::this_thread::sleep_for(50ms);
  EXPECT_EQ(monitor.PollCount(), polls_at_suspend);
}

// Resuming polls immediately rather than waiting out the interval, because the
// reason for resuming is usually that a capture has just ended and the user is
// looking at the device list again.
TEST(DeviceMonitorTest, ResumingReportsWhatChangedWhileSuspended) {
  FakeUsbDevice device;
  device.SetSingleDevice("bus-1", DeviceSpeed::kSuper, "");

  Reports reports;
  DeviceMonitor monitor(&device, nullptr);
  monitor.Start(reports.Callback(), 10s);

  ASSERT_TRUE(reports.WaitFor(1));

  monitor.SetSuspended(true);
  device.SetDevices({});
  monitor.SetSuspended(false);

  // The interval is ten seconds, so anything arriving quickly can only have
  // come from the resume cutting the wait short.
  ASSERT_TRUE(reports.WaitFor(2, 1000ms))
      << "resuming did not poll until the interval elapsed";
  EXPECT_TRUE(reports.Latest().empty());
}

TEST(DeviceMonitorTest, StoppingIsSafeWhileAPollIsInFlight) {
  FakeUsbDevice device;

  Reports reports;
  DeviceMonitor monitor(&device, nullptr);
  monitor.Start(reports.Callback(), kTestInterval);

  ASSERT_TRUE(reports.WaitFor(1));

  monitor.Stop();
  const uint64_t polls = monitor.PollCount();

  std::this_thread::sleep_for(20ms);
  EXPECT_EQ(monitor.PollCount(), polls);

  // Stopping twice must not be a double join.
  monitor.Stop();
}

TEST(DeviceMonitorTest, TheDeviceListCanBeAskedForRatherThanWaitedOn) {
  FakeUsbDevice device;
  device.SetSingleDevice("bus-1", DeviceSpeed::kSuper, "");

  Reports reports;
  DeviceMonitor monitor(&device, nullptr);
  monitor.Start(reports.Callback(), kTestInterval);

  ASSERT_TRUE(reports.WaitFor(1));
  ASSERT_EQ(monitor.Devices().size(), 1U);
  EXPECT_EQ(monitor.Devices().front().path, "bus-1");
}

}  // namespace
}  // namespace ddd::capture
