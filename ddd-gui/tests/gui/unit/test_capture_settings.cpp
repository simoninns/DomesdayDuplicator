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

#include "capture_settings.h"
#include "disk_buffer_ring.h"

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

}  // namespace
}  // namespace ddd::gui
