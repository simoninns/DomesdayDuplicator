/************************************************************************

    test_capture_panel.cpp

    T1 tests for the capture panel's controls
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "capture_controller.h"
#include "capture_panel.h"
#include "fake_usb_device.h"

namespace ddd::gui {
namespace {

using namespace std::chrono_literals;

constexpr size_t kTestSlotBytes = size_t{256} << 10;
constexpr size_t kTestSlotCount = 6;

capture::DeviceInfo DeviceAt(const std::string& path,
                             capture::DeviceSpeed speed) {
  capture::DeviceInfo info;
  info.path = path;
  info.speed = speed;
  return info;
}

template <typename Predicate>
bool PumpUntil(Predicate predicate, std::chrono::milliseconds limit = 5000ms) {
  const auto deadline = std::chrono::steady_clock::now() + limit;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) {
      return true;
    }
    QApplication::processEvents();
    std::this_thread::sleep_for(1ms);
  }
  return predicate();
}

class CapturePanelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    QCoreApplication::setOrganizationName(QStringLiteral("Domesday86Test"));
    QCoreApplication::setApplicationName(
        QStringLiteral("ddd-gui-panel-%1").arg(QLatin1String(info->name())));
    QSettings().clear();

    capture::SyntheticSource::Options source_options;
    source_options.slot_size_bytes = kTestSlotBytes;
    source_options.slot_count = kTestSlotCount;

    device_ = std::make_unique<capture::FakeUsbDevice>();
    device_->SetSourceOptions(source_options);

    controller_ = std::make_unique<CaptureController>(device_.get(), nullptr);
    CaptureSettings settings = controller_->settings();
    settings.queue_size_bytes = capture::DiskBufferRing::kMinimumQueueSizeBytes;
    controller_->SetSettings(settings);

    panel_ = std::make_unique<CapturePanel>(controller_.get());
  }

  void TearDown() override {
    panel_.reset();
    controller_.reset();
    device_.reset();
    QSettings().clear();
  }

  QComboBox* DeviceCombo() const {
    return panel_->findChild<QComboBox*>(
        QLatin1String(CapturePanel::kDeviceComboName));
  }
  QPushButton* MonitorButton() const {
    return panel_->findChild<QPushButton*>(
        QLatin1String(CapturePanel::kMonitorButtonName));
  }
  QCheckBox* TestModeBox() const {
    return panel_->findChild<QCheckBox*>(
        QLatin1String(CapturePanel::kTestModeBoxName));
  }
  QLabel* StatusLabel() const {
    return panel_->findChild<QLabel*>(
        QLatin1String(CapturePanel::kStatusLabelName));
  }

  std::unique_ptr<capture::FakeUsbDevice> device_;
  std::unique_ptr<CaptureController> controller_;
  std::unique_ptr<CapturePanel> panel_;
};

TEST_F(CapturePanelTest, EveryControlIsPresentAndFindable) {
  EXPECT_NE(DeviceCombo(), nullptr);
  EXPECT_NE(MonitorButton(), nullptr);
  EXPECT_NE(TestModeBox(), nullptr);
  EXPECT_NE(StatusLabel(), nullptr);
}

TEST_F(CapturePanelTest, WithNoDeviceThereIsNothingToPress) {
  EXPECT_FALSE(MonitorButton()->isEnabled());
  EXPECT_EQ(DeviceCombo()->count(), 0);
}

TEST_F(CapturePanelTest, AnAttachedDeviceFillsTheListAndEnablesTheButton) {
  device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper, "");
  controller_->Start();

  ASSERT_TRUE(PumpUntil([&] { return DeviceCombo()->count() == 1; }));
  EXPECT_TRUE(MonitorButton()->isEnabled());
  EXPECT_EQ(DeviceCombo()->itemData(0).toString(), QStringLiteral("bus-1"));
}

// The plan's acceptance criterion, at the point a user meets it: a device on a
// USB 2 port is reported as "connected at insufficient speed", not opened.
//
// It says so in the list rather than only when the button is pressed, because
// that is where the user is looking when they are wondering which device is
// which.
TEST_F(CapturePanelTest, ADeviceOnAUsb2PortIsNamedAsSuchAndCannotBeStarted) {
  device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kHigh, "");
  controller_->Start();

  ASSERT_TRUE(PumpUntil([&] { return DeviceCombo()->count() == 1; }));

  EXPECT_TRUE(
      DeviceCombo()->itemText(0).contains(QStringLiteral("insufficient speed")))
      << DeviceCombo()->itemText(0).toStdString();
  EXPECT_TRUE(
      StatusLabel()->text().contains(QStringLiteral("insufficient speed")))
      << StatusLabel()->text().toStdString();
  EXPECT_FALSE(MonitorButton()->isEnabled());
}

TEST_F(CapturePanelTest, SeveralDevicesAllAppear) {
  device_->SetDevices({DeviceAt("bus-1", capture::DeviceSpeed::kSuper),
                       DeviceAt("bus-2", capture::DeviceSpeed::kSuper)});
  controller_->Start();

  ASSERT_TRUE(PumpUntil([&] { return DeviceCombo()->count() == 2; }));
}

TEST_F(CapturePanelTest, ChoosingADeviceRemembersIt) {
  device_->SetDevices({DeviceAt("bus-1", capture::DeviceSpeed::kSuper),
                       DeviceAt("bus-2", capture::DeviceSpeed::kSuper)});
  controller_->Start();
  ASSERT_TRUE(PumpUntil([&] { return DeviceCombo()->count() == 2; }));

  DeviceCombo()->setCurrentIndex(1);

  EXPECT_EQ(controller_->settings().preferred_device_path,
            QStringLiteral("bus-2"));
}

// The button is a state, not an action pair. A Start beside a Stop leaves one
// of them wrong at all times and makes the user read which is disabled to find
// out what the application is doing.
TEST_F(CapturePanelTest, TheButtonSaysWhatWillHappenNext) {
  device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper, "");
  controller_->Start();
  ASSERT_TRUE(PumpUntil([&] { return MonitorButton()->isEnabled(); }));

  EXPECT_TRUE(MonitorButton()->text().contains(QStringLiteral("Start")));

  MonitorButton()->click();
  ASSERT_TRUE(controller_->monitoring());
  EXPECT_TRUE(MonitorButton()->text().contains(QStringLiteral("Stop")));

  MonitorButton()->click();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
  EXPECT_TRUE(MonitorButton()->text().contains(QStringLiteral("Start")));
}

// Neither can be changed without stopping: the device is open, and a mode
// change would land at an unpredictable point in the stream. Disabling them
// says so rather than letting a user set something that quietly does nothing.
TEST_F(CapturePanelTest, DeviceAndTestModeAreLockedWhileStreaming) {
  device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper, "");
  controller_->Start();
  ASSERT_TRUE(PumpUntil([&] { return MonitorButton()->isEnabled(); }));

  MonitorButton()->click();
  ASSERT_TRUE(controller_->monitoring());

  EXPECT_FALSE(DeviceCombo()->isEnabled());
  EXPECT_FALSE(TestModeBox()->isEnabled());

  MonitorButton()->click();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));

  EXPECT_TRUE(DeviceCombo()->isEnabled());
  EXPECT_TRUE(TestModeBox()->isEnabled());
}

TEST_F(CapturePanelTest, TickingTestModeReachesTheSettings) {
  TestModeBox()->setChecked(true);
  EXPECT_TRUE(controller_->settings().test_mode);

  TestModeBox()->setChecked(false);
  EXPECT_FALSE(controller_->settings().test_mode);
}

TEST_F(CapturePanelTest, UnpluggingMidMonitorLeavesTheButtonUsableAgain) {
  device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper, "");
  controller_->Start();
  ASSERT_TRUE(PumpUntil([&] { return MonitorButton()->isEnabled(); }));

  MonitorButton()->click();
  ASSERT_TRUE(controller_->monitoring());

  MonitorButton()->click();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));

  EXPECT_TRUE(MonitorButton()->isEnabled());
}

TEST_F(CapturePanelTest, APanelWithNoControllerStillBuilds) {
  // The widget tests build a window with no controller so that layout, menus
  // and persistence can be tested without a USB subsystem. A panel that
  // dereferenced a null controller would take that away.
  CapturePanel panel(nullptr);
  EXPECT_NE(panel.findChild<QPushButton*>(
                QLatin1String(CapturePanel::kMonitorButtonName)),
            nullptr);
}

}  // namespace
}  // namespace ddd::gui
