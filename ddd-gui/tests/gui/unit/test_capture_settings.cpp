/************************************************************************

    test_capture_settings.cpp

    T1 tests for capture settings persistence
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSettings>
#include <QString>

#include "capture_format.h"
#include "capture_settings.h"
#include "disk_buffer_ring.h"
#include "free_space.h"
#include "sample_format.h"

namespace ddd::gui {
namespace {

// Each test gets a settings file of its own, named after the test. CTest runs
// discovered tests as separate processes and may run several at once, so a
// shared file would have one test's clear() land in the middle of another's
// read.
class CaptureSettingsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    QCoreApplication::setOrganizationName(QStringLiteral("Domesday86Test"));
    QCoreApplication::setApplicationName(
        QStringLiteral("ddd-gui-settings-%1").arg(QLatin1String(info->name())));
    QSettings().clear();
  }

  void TearDown() override { QSettings().clear(); }
};

TEST_F(CaptureSettingsTest, AFirstRunGetsTheDefaults) {
  const CaptureSettings settings = LoadCaptureSettings();

  EXPECT_EQ(settings.queue_size_bytes,
            capture::DiskBufferRing::kDefaultQueueSizeBytes);
  EXPECT_TRUE(settings.small_transfers);
  EXPECT_TRUE(settings.preferred_device_path.isEmpty());
}

TEST_F(CaptureSettingsTest, WhatWasSavedIsWhatComesBack) {
  CaptureSettings saved;
  saved.preferred_device_path = QStringLiteral("/sys/bus/usb/devices/3-2");
  saved.queue_size_bytes = size_t{128} << 20;
  saved.small_transfers = false;
  saved.transfer_queue_bytes = size_t{8} << 20;
  SaveCaptureSettings(saved);

  const CaptureSettings loaded = LoadCaptureSettings();
  EXPECT_EQ(loaded.preferred_device_path, saved.preferred_device_path);
  EXPECT_EQ(loaded.queue_size_bytes, saved.queue_size_bytes);
  EXPECT_EQ(loaded.small_transfers, saved.small_transfers);
  EXPECT_EQ(loaded.transfer_queue_bytes, saved.transfer_queue_bytes);
}

// Test mode is deliberately not persisted. An application that silently started
// in test mode because of something the user did last week would produce a
// capture full of ramps, and the user would have no reason to suspect it.
TEST_F(CaptureSettingsTest, TestModeIsNotRemembered) {
  CaptureSettings saved;
  saved.test_mode = true;
  SaveCaptureSettings(saved);

  EXPECT_FALSE(LoadCaptureSettings().test_mode);
}

// A settings file edited by hand, or written by a version with a different
// range, should produce a working capture rather than a refusal — or worse, a
// ring the engine would reject at allocation.
TEST_F(CaptureSettingsTest, AnOutOfRangeQueueSizeIsClampedRatherThanRefused) {
  {
    QSettings store;
    store.setValue(QStringLiteral("capture/queue_size_bytes"),
                   static_cast<qulonglong>(size_t{4} << 30));
  }
  EXPECT_EQ(LoadCaptureSettings().queue_size_bytes,
            capture::DiskBufferRing::kMaximumQueueSizeBytes);

  {
    QSettings store;
    store.setValue(QStringLiteral("capture/queue_size_bytes"), 1);
  }
  EXPECT_EQ(LoadCaptureSettings().queue_size_bytes,
            capture::DiskBufferRing::kMinimumQueueSizeBytes);
}

TEST_F(CaptureSettingsTest, AnOutOfRangeTransferQueueIsClampedToTheUsbfsLimit) {
  {
    QSettings store;
    store.setValue(QStringLiteral("capture/transfer_queue_bytes"),
                   static_cast<qulonglong>(size_t{1} << 30));
  }
  // Above the usbfs default, submission fails with ENOMEM — so the setting is
  // held below it rather than allowed to produce a capture that cannot start.
  EXPECT_LE(LoadCaptureSettings().transfer_queue_bytes, size_t{12} << 20);
}

TEST_F(CaptureSettingsTest, TheSettingsBecomeTheEngineOptions) {
  CaptureSettings settings;
  settings.small_transfers = false;
  settings.transfer_queue_bytes = size_t{6} << 20;

  const capture::UsbSourceOptions options = settings.UsbOptions();
  EXPECT_FALSE(options.small_transfers);
  EXPECT_EQ(options.transfer_queue_bytes, size_t{6} << 20);
}

TEST_F(CaptureSettingsTest, NoFrontEndGainIsDeclaredUntilOneIsChosen) {
  // The default that keeps a wrong voltage off the screen. A plausible one
  // would produce millivolt figures wrong by up to a factor of four, with
  // nothing on screen to reveal it.
  EXPECT_FALSE(CaptureSettings().DeclaredGain().declared());
  EXPECT_FALSE(LoadCaptureSettings().DeclaredGain().declared());
}

TEST_F(CaptureSettingsTest, TheDeclaredGainSurvivesARestart) {
  // Unlike test mode, which is deliberately forgotten: a gain switch stays
  // where it was put, and asking again every session for something that has not
  // changed is how a setting ends up ignored.
  CaptureSettings settings;
  settings.front_end_gain_switches = 0b1010;
  SaveCaptureSettings(settings);

  const CaptureSettings loaded = LoadCaptureSettings();
  EXPECT_EQ(loaded.front_end_gain_switches, 0b1010);
  EXPECT_NEAR(loaded.DeclaredGain().Gain(), 3.34, 0.005);
}

TEST_F(CaptureSettingsTest, AnImpossibleGainSettingReadsAsNoDeclaration) {
  // Settings files get edited by hand. Anything that cannot mean a switch
  // pattern means nobody has declared one — never a guess at what was intended.
  {
    QSettings store;
    store.setValue(QStringLiteral("hardware/front_end_gain_switches"), 99);
  }

  EXPECT_FALSE(LoadCaptureSettings().DeclaredGain().declared());
}

// --- Where a capture goes ------------------------------------------------

TEST_F(CaptureSettingsTest, TheDestinationSettingsSurviveARestart) {
  CaptureSettings saved;
  saved.capture_directory = QStringLiteral("/captures/laserdisc");
  saved.capture_name = QStringLiteral("Blade Runner side 1");
  saved.compression_level = 5;
  saved.duration_limit_seconds = 45 * 60;
  saved.low_space_warning_minutes = 3;
  SaveCaptureSettings(saved);

  const CaptureSettings loaded = LoadCaptureSettings();
  EXPECT_EQ(loaded.capture_directory, saved.capture_directory);
  EXPECT_EQ(loaded.capture_name, saved.capture_name);
  EXPECT_EQ(loaded.compression_level, saved.compression_level);
  EXPECT_EQ(loaded.duration_limit_seconds, saved.duration_limit_seconds);
  EXPECT_EQ(loaded.low_space_warning_minutes, saved.low_space_warning_minutes);
}

// Stored as typed and resolved when it is used, so that a settings file written
// on one machine and read on another does not pin captures to a home directory
// that is not there.
TEST_F(CaptureSettingsTest, AnUnsetFolderResolvesToSomewhereThatExists) {
  const CaptureSettings settings = LoadCaptureSettings();

  EXPECT_TRUE(settings.capture_directory.isEmpty());
  EXPECT_FALSE(settings.ResolvedCaptureDirectory().isEmpty());
  EXPECT_EQ(settings.ResolvedCaptureDirectory(), DefaultCaptureDirectory());
}

TEST_F(CaptureSettingsTest, AChosenFolderIsUsedAsGiven) {
  CaptureSettings settings;
  settings.capture_directory = QStringLiteral("/captures/laserdisc");

  EXPECT_EQ(settings.ResolvedCaptureDirectory(),
            QStringLiteral("/captures/laserdisc"));
}

// The smallest file the encoder can produce, which is what an archival capture
// wants: the encoding cost is paid once and the storage cost is paid for years.
// It is affordable only because libFLAC 1.5 encodes on several threads — see
// FlacWriter::Options, and the soak test that measures it at the device's full
// rate.
TEST_F(CaptureSettingsTest, TheCompressionDefaultIsTheSmallestFile) {
  EXPECT_EQ(LoadCaptureSettings().compression_level, 8);

  // And the GUI default is the engine's, not a second opinion that could drift
  // away from it.
  EXPECT_EQ(CaptureSettings{}.compression_level,
            capture::FlacWriter::Options{}.compression_level);
}

TEST_F(CaptureSettingsTest, ANonsensicalCompressionLevelIsClamped) {
  QSettings store;
  store.setValue(QStringLiteral("capture/compression_level"), 99);
  EXPECT_EQ(LoadCaptureSettings().compression_level, 8);

  store.setValue(QStringLiteral("capture/compression_level"), -4);
  EXPECT_EQ(LoadCaptureSettings().compression_level, 0);
}

TEST_F(CaptureSettingsTest, ANonsensicalDurationLimitIsClamped) {
  QSettings store;
  store.setValue(QStringLiteral("capture/duration_limit_seconds"), -60);
  EXPECT_EQ(LoadCaptureSettings().duration_limit_seconds, 0);

  store.setValue(QStringLiteral("capture/duration_limit_seconds"), 999'999'999);
  EXPECT_EQ(LoadCaptureSettings().duration_limit_seconds,
            CaptureSettings::kMaximumDurationLimitSeconds);
}

// No limit by default. One that fired in the middle of a side would be worse
// than no limit at all.
TEST_F(CaptureSettingsTest, ThereIsNoDurationLimitUntilOneIsSet) {
  EXPECT_EQ(LoadCaptureSettings().duration_limit_seconds, 0);
}

// --- Format and sample rate ----------------------------------------------

// FLAC and every sample, which is what a LaserDisc capture wants and what the
// application has always done.
TEST_F(CaptureSettingsTest, TheDefaultsAreCompressedAndUndecimated) {
  const CaptureSettings settings = LoadCaptureSettings();
  EXPECT_EQ(settings.output_format, capture::CaptureOutputFormat::kFlac);
  EXPECT_EQ(settings.decimation_factor, capture::kUndecimatedFactor);
}

// Both persisted, unlike test mode: somebody capturing tape wants half rate
// every session, and being asked again each time is how a setting ends up
// ignored.
TEST_F(CaptureSettingsTest, TheFormatAndTheRateSurviveARestart) {
  CaptureSettings saved;
  saved.output_format = capture::CaptureOutputFormat::kSigned16Bit;
  saved.decimation_factor = capture::kTapeDecimationFactor;
  SaveCaptureSettings(saved);

  const CaptureSettings loaded = LoadCaptureSettings();
  EXPECT_EQ(loaded.output_format, capture::CaptureOutputFormat::kSigned16Bit);
  EXPECT_EQ(loaded.decimation_factor, capture::kTapeDecimationFactor);
}

// Not clamped to the nearest value, because there is no nearest sensible one: a
// 3 would be a rate nothing downstream expects. Every sample is the reading
// that cannot produce a wrong file.
TEST_F(CaptureSettingsTest, AnUnwritableDecimationFactorReadsAsEverySample) {
  QSettings store;
  store.setValue(QStringLiteral("capture/decimation_factor"), 3);
  EXPECT_EQ(LoadCaptureSettings().decimation_factor,
            capture::kUndecimatedFactor);

  store.setValue(QStringLiteral("capture/decimation_factor"), 0);
  EXPECT_EQ(LoadCaptureSettings().decimation_factor,
            capture::kUndecimatedFactor);
}

TEST_F(CaptureSettingsTest, AnUnknownFormatNameReadsAsTheDefault) {
  QSettings store;
  store.setValue(QStringLiteral("capture/output_format"),
                 QStringLiteral("lds"));
  EXPECT_EQ(LoadCaptureSettings().output_format,
            capture::CaptureOutputFormat::kFlac);
}

// The test pattern is a ramp checked sample by sample, and a decimated one
// would read as a break on the first buffer. Test mode captures every sample
// whatever the setting says, and the interface disables the control to match.
TEST_F(CaptureSettingsTest, TestModeCapturesEverySampleWhateverWasChosen) {
  CaptureSettings settings;
  settings.decimation_factor = capture::kTapeDecimationFactor;
  EXPECT_EQ(settings.EffectiveDecimationFactor(),
            capture::kTapeDecimationFactor);

  settings.test_mode = true;
  EXPECT_EQ(settings.EffectiveDecimationFactor(), capture::kUndecimatedFactor);

  // And the stored choice is still there, so leaving test mode restores it
  // rather than quietly resetting what the user set.
  EXPECT_EQ(settings.decimation_factor, capture::kTapeDecimationFactor);
}

// The free-space readouts are a time rather than a size, so they need this.
// Uncompressed is twice what FLAC costs and decimating halves it — a figure
// that assumed a compressed 40 Msps capture would be out by four at the
// extremes, in the direction that lets a volume fill before the warning fires.
TEST_F(CaptureSettingsTest, TheDiskRateFollowsTheFormatAndTheRate) {
  CaptureSettings settings;
  const double flac = settings.EstimatedBytesPerSecond();
  EXPECT_DOUBLE_EQ(flac, capture::kEstimatedCaptureBytesPerSecond);

  settings.output_format = capture::CaptureOutputFormat::kSigned16Bit;
  const double raw = settings.EstimatedBytesPerSecond();
  EXPECT_DOUBLE_EQ(raw, static_cast<double>(capture::kWireBytesPerSecond));
  EXPECT_GT(raw, flac);

  settings.decimation_factor = capture::kTapeDecimationFactor;
  EXPECT_DOUBLE_EQ(settings.EstimatedBytesPerSecond(), raw / 2.0);
}

TEST_F(CaptureSettingsTest, TheGainIsNotPassedToTheEngine) {
  // It is a display calibration, not an acquisition parameter. Nothing below
  // the GUI reads it, and the options handed to the USB source are the proof.
  CaptureSettings declared;
  declared.front_end_gain_switches = 0b1111;

  const capture::UsbSourceOptions with = declared.UsbOptions();
  const capture::UsbSourceOptions without = CaptureSettings().UsbOptions();

  EXPECT_EQ(with.small_transfers, without.small_transfers);
  EXPECT_EQ(with.transfer_queue_bytes, without.transfer_queue_bytes);
  EXPECT_EQ(with.discard_slots, without.discard_slots);
}

}  // namespace
}  // namespace ddd::gui
