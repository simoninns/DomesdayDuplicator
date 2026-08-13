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
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalSpy>
#include <QSpinBox>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <thread>
#include <vector>

#include "capture_controller.h"
#include "capture_panel.h"
#include "fake_usb_device.h"
#include "theme_color_tokens.h"

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
    if (!directory_.empty()) {
      std::filesystem::remove_all(directory_);
    }
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
  QPushButton* CaptureButton() const {
    return panel_->findChild<QPushButton*>(
        QLatin1String(CapturePanel::kCaptureButtonName));
  }
  QLineEdit* DirectoryEdit() const {
    return panel_->findChild<QLineEdit*>(
        QLatin1String(CapturePanel::kDirectoryEditName));
  }
  QLineEdit* NameEdit() const {
    return panel_->findChild<QLineEdit*>(
        QLatin1String(CapturePanel::kNameEditName));
  }
  QSpinBox* CompressionSpin() const {
    return panel_->findChild<QSpinBox*>(
        QLatin1String(CapturePanel::kCompressionSpinName));
  }
  QSpinBox* DurationSpin() const {
    return panel_->findChild<QSpinBox*>(
        QLatin1String(CapturePanel::kDurationSpinName));
  }
  QSpinBox* LowSpaceSpin() const {
    return panel_->findChild<QSpinBox*>(
        QLatin1String(CapturePanel::kLowSpaceSpinName));
  }
  QLabel* FreeSpaceLabel() const {
    return panel_->findChild<QLabel*>(
        QLatin1String(CapturePanel::kFreeSpaceLabelName));
  }

  // The background colour a button's stylesheet sets, or an invalid colour if
  // it has none. Read out of the stylesheet rather than by grabbing pixels,
  // because a button is drawn by the platform style and its bevel and hover
  // state would make a pixel at any given point a coin toss.
  static QColor BackgroundOf(const QWidget* widget) {
    static const QRegularExpression pattern(
        QStringLiteral("background-color:\\s*(#[0-9a-fA-F]{6})"));
    const QRegularExpressionMatch match = pattern.match(widget->styleSheet());
    return match.hasMatch() ? QColor(match.captured(1)) : QColor();
  }

  // Point the capture destination somewhere writable that this test owns, so
  // that a test which actually starts a capture does not scatter files through
  // the developer's own Movies folder.
  void UseTemporaryDirectory() {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    directory_ = std::filesystem::temp_directory_path() /
                 (std::string("ddd-panel-test-") + info->name());
    std::filesystem::remove_all(directory_);
    std::filesystem::create_directories(directory_);

    CaptureSettings settings = controller_->settings();
    settings.capture_directory = QString::fromStdString(directory_.string());
    settings.compression_level = 0;
    controller_->SetSettings(settings);
  }

  std::filesystem::path directory_;
  std::unique_ptr<capture::FakeUsbDevice> device_;
  std::unique_ptr<CaptureController> controller_;
  std::unique_ptr<CapturePanel> panel_;
};

TEST_F(CapturePanelTest, EveryControlIsPresentAndFindable) {
  EXPECT_NE(DeviceCombo(), nullptr);
  EXPECT_NE(MonitorButton(), nullptr);
  EXPECT_NE(CaptureButton(), nullptr);
  EXPECT_NE(TestModeBox(), nullptr);
  EXPECT_NE(StatusLabel(), nullptr);
  EXPECT_NE(DirectoryEdit(), nullptr);
  EXPECT_NE(NameEdit(), nullptr);
  EXPECT_NE(CompressionSpin(), nullptr);
  EXPECT_NE(DurationSpin(), nullptr);
  EXPECT_NE(LowSpaceSpin(), nullptr);
  EXPECT_NE(FreeSpaceLabel(), nullptr);
}

TEST_F(CapturePanelTest, WithNoDeviceThereIsNothingToPress) {
  EXPECT_FALSE(MonitorButton()->isEnabled());
  EXPECT_FALSE(CaptureButton()->isEnabled());
  EXPECT_EQ(DeviceCombo()->count(), 0);
}

TEST_F(CapturePanelTest, AnAttachedDeviceFillsTheListAndEnablesTheButton) {
  device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper, "");
  controller_->Start();

  ASSERT_TRUE(PumpUntil([&] { return DeviceCombo()->count() == 1; }));
  EXPECT_TRUE(MonitorButton()->isEnabled());
  EXPECT_TRUE(CaptureButton()->isEnabled());
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

// --- Capture controls -----------------------------------------------------

// Capture is a state too, and the same reasoning applies as to the monitor
// button: the label is the next thing that will happen.
TEST_F(CapturePanelTest, TheCaptureButtonSaysWhatWillHappenNext) {
  UseTemporaryDirectory();
  device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper, "");
  controller_->Start();
  ASSERT_TRUE(PumpUntil([&] { return CaptureButton()->isEnabled(); }));

  EXPECT_TRUE(CaptureButton()->text().contains(QStringLiteral("Start")));

  CaptureButton()->click();
  ASSERT_TRUE(controller_->capturing());
  EXPECT_TRUE(CaptureButton()->text().contains(QStringLiteral("Stop")));

  CaptureButton()->click();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  EXPECT_TRUE(CaptureButton()->text().contains(QStringLiteral("Start")));

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

// One press from idle. Someone who has not been monitoring and presses Capture
// means "capture"; making them start the stream first would be ceremony.
TEST_F(CapturePanelTest, PressingCaptureFromIdleStartsTheStreamToo) {
  UseTemporaryDirectory();
  device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper, "");
  controller_->Start();
  ASSERT_TRUE(PumpUntil([&] { return CaptureButton()->isEnabled(); }));

  ASSERT_FALSE(controller_->monitoring());
  CaptureButton()->click();

  EXPECT_TRUE(controller_->monitoring());
  EXPECT_TRUE(controller_->capturing());
  EXPECT_TRUE(MonitorButton()->text().contains(QStringLiteral("Stop")));

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

TEST_F(CapturePanelTest, TheDestinationIsFixedOnceTheFileIsOpen) {
  UseTemporaryDirectory();
  device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper, "");
  controller_->Start();
  ASSERT_TRUE(PumpUntil([&] { return CaptureButton()->isEnabled(); }));

  CaptureButton()->click();
  ASSERT_TRUE(controller_->capturing());

  // The file is already open, so these cannot take effect and say so rather
  // than accepting input that would be ignored.
  EXPECT_FALSE(DirectoryEdit()->isEnabled());
  EXPECT_FALSE(NameEdit()->isEnabled());
  EXPECT_FALSE(CompressionSpin()->isEnabled());

  // But these are read as the capture runs, so they stay live: noticing halfway
  // through that the disk is filling and wanting a warning sooner is
  // reasonable.
  EXPECT_TRUE(DurationSpin()->isEnabled());
  EXPECT_TRUE(LowSpaceSpin()->isEnabled());

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));

  EXPECT_TRUE(DirectoryEdit()->isEnabled());
  EXPECT_TRUE(NameEdit()->isEnabled());
  EXPECT_TRUE(CompressionSpin()->isEnabled());

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

// The name is forced in test mode, so the field stops accepting one. Disabled
// rather than silently ignored: a field that took text and then did not use it
// would be a lie about what the application was going to do.
TEST_F(CapturePanelTest, TestModeTakesTheNameFieldAway) {
  EXPECT_TRUE(NameEdit()->isEnabled());
  EXPECT_TRUE(
      NameEdit()->placeholderText().startsWith(QStringLiteral("RF-Sample_")))
      << NameEdit()->placeholderText().toStdString();

  TestModeBox()->setChecked(true);

  EXPECT_FALSE(NameEdit()->isEnabled());
  EXPECT_TRUE(
      NameEdit()->placeholderText().startsWith(QStringLiteral("TestData_")))
      << NameEdit()->placeholderText().toStdString();

  TestModeBox()->setChecked(false);
  EXPECT_TRUE(NameEdit()->isEnabled());
}

TEST_F(CapturePanelTest, TheDestinationSettingsReachTheController) {
  CompressionSpin()->setValue(5);
  EXPECT_EQ(controller_->settings().compression_level, 5);

  DurationSpin()->setValue(30);
  EXPECT_EQ(controller_->settings().duration_limit_seconds, 30 * 60)
      << "the panel offers minutes and the setting is held in seconds";

  LowSpaceSpin()->setValue(3);
  EXPECT_EQ(controller_->settings().low_space_warning_minutes, 3);
}

TEST_F(CapturePanelTest, AZeroDurationReadsAsNoLimitRatherThanAsZero) {
  DurationSpin()->setValue(0);
  EXPECT_EQ(controller_->settings().duration_limit_seconds, 0);
  EXPECT_TRUE(DurationSpin()->text().contains(QStringLiteral("No limit")))
      << DurationSpin()->text().toStdString();
}

// A time, not a size. "412 GB free" does not answer the question a user has,
// which is whether this will last the side they are about to play.
TEST_F(CapturePanelTest, FreeSpaceIsShownAsHowMuchCaptureItHolds) {
  UseTemporaryDirectory();
  ASSERT_TRUE(PumpUntil([&] {
    return FreeSpaceLabel()->text().contains(QStringLiteral("of capture"));
  }));

  EXPECT_TRUE(FreeSpaceLabel()->text().contains(QStringLiteral("free")))
      << FreeSpaceLabel()->text().toStdString();
}

// Unknown, and specifically not zero. Zero reads as "the disk is full" and
// would stop somebody capturing to a folder they were about to create.
TEST_F(CapturePanelTest, AFolderThatIsNotThereReadsAsUnknownRatherThanFull) {
  DirectoryEdit()->setText(QStringLiteral("/no/such/folder/anywhere"));
  emit DirectoryEdit() -> editingFinished();

  EXPECT_TRUE(FreeSpaceLabel()->text().contains(QStringLiteral("Unknown"),
                                                Qt::CaseInsensitive))
      << FreeSpaceLabel()->text().toStdString();
  EXPECT_FALSE(FreeSpaceLabel()->text().contains(QStringLiteral("0 MB")))
      << FreeSpaceLabel()->text().toStdString();
}

// --- The buttons say what they are doing without being read ---------------

TEST_F(CapturePanelTest, AnIdleButtonKeepsTheWindowsOwnColours) {
  EXPECT_TRUE(MonitorButton()->styleSheet().isEmpty());
  EXPECT_TRUE(CaptureButton()->styleSheet().isEmpty());
}

TEST_F(CapturePanelTest, MonitoringTurnsItsButtonGreenAndCapturingTurnsItsRed) {
  UseTemporaryDirectory();
  device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper, "");
  controller_->Start();
  ASSERT_TRUE(PumpUntil([&] { return MonitorButton()->isEnabled(); }));

  MonitorButton()->click();
  ASSERT_TRUE(controller_->monitoring());

  const QColor monitoring = BackgroundOf(MonitorButton());
  ASSERT_TRUE(monitoring.isValid())
      << MonitorButton()->styleSheet().toStdString();
  EXPECT_GT(monitoring.green(), monitoring.red());
  EXPECT_GT(monitoring.green(), monitoring.blue());

  // The capture button is not coloured by the monitor starting. They are
  // separate states and the colour has to say which one is running.
  EXPECT_TRUE(CaptureButton()->styleSheet().isEmpty());

  CaptureButton()->click();
  ASSERT_TRUE(controller_->capturing());

  const QColor capturing = BackgroundOf(CaptureButton());
  ASSERT_TRUE(capturing.isValid())
      << CaptureButton()->styleSheet().toStdString();
  EXPECT_GT(capturing.red(), capturing.green());
  EXPECT_GT(capturing.red(), capturing.blue());

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  EXPECT_TRUE(CaptureButton()->styleSheet().isEmpty())
      << "a stopped capture left its button coloured";

  // And monitoring is still running, so its button is still green — the state
  // the capture ended back into.
  EXPECT_TRUE(BackgroundOf(MonitorButton()).isValid());

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
  EXPECT_TRUE(MonitorButton()->styleSheet().isEmpty());
}

// A stylesheet takes a button off the native drawing path, and the size the
// stylesheet path computes from border and padding alone is not the size the
// platform style chose — so a coloured button changes height unless its natural
// height is pinned. Left unfixed, the whole panel shifts under the pointer at
// the moment the button is pressed, which is far more distracting than the
// colour is useful.
TEST_F(CapturePanelTest, ColouringAButtonDoesNotChangeItsSize) {
  UseTemporaryDirectory();
  device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper, "");
  controller_->Start();
  ASSERT_TRUE(PumpUntil([&] { return MonitorButton()->isEnabled(); }));

  panel_->resize(320, 520);
  panel_->show();
  QApplication::processEvents();

  const QSize idle = MonitorButton()->size();
  ASSERT_GT(idle.height(), 0);

  MonitorButton()->click();
  ASSERT_TRUE(controller_->monitoring());
  QApplication::processEvents();

  EXPECT_EQ(MonitorButton()->size(), idle);

  MonitorButton()->click();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
  QApplication::processEvents();

  EXPECT_EQ(MonitorButton()->size(), idle);
}

// "Not too bright". These sit in a window whose whole job is to display a
// signal, and a saturated green and red would be the brightest things in it —
// the eye would go to the controls instead of to the waveform. Both bands here
// are wide, because the point is to catch a colour that has drifted into being
// an alarm rather than to pin a particular shade.
TEST_F(CapturePanelTest, TheActiveColoursAreMutedRatherThanSaturated) {
  for (const bool dark : {false, true}) {
    for (const auto token : {theme_tokens::PlotColorToken::kMonitoringActive,
                             theme_tokens::PlotColorToken::kCapturingActive}) {
      const QColor colour = theme_tokens::PlotColor(token, dark);

      // Not a pure hue: a fully saturated green or red is what an alarm looks
      // like, and this is an ordinary running state.
      EXPECT_LT(colour.saturation(), 160)
          << colour.name().toStdString() << " is too saturated";

      // Dark enough to sit behind white text, and light enough not to read as
      // a disabled control.
      EXPECT_GT(colour.lightness(), 50) << colour.name().toStdString();
      EXPECT_LT(colour.lightness(), 150) << colour.name().toStdString();
    }
  }
}

// Computed rather than chosen, so that changing a token cannot quietly produce
// a control whose label cannot be read.
TEST_F(CapturePanelTest, TheLabelStaysReadableOnEveryActiveColour) {
  for (const bool dark : {false, true}) {
    for (const auto token : {theme_tokens::PlotColorToken::kMonitoringActive,
                             theme_tokens::PlotColorToken::kCapturingActive}) {
      const QColor background = theme_tokens::PlotColor(token, dark);
      const QColor text = theme_tokens::ReadableTextOn(background);

      const int contrast = std::abs(text.lightness() - background.lightness());
      EXPECT_GT(contrast, 90) << "text " << text.name().toStdString() << " on "
                              << background.name().toStdString();
    }
  }
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
