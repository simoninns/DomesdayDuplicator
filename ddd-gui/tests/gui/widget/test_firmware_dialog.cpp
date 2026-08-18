/************************************************************************

    test_firmware_dialog.cpp

    The Firmware dialog as a widget
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QApplication>
#include <QLabel>
#include <QScrollArea>

#include "firmware_dialog.h"
#include "wire_protocol.h"

namespace ddd::gui {
namespace {

FirmwareVersions MatchedSet() {
  FirmwareVersions versions;
  versions.application = QStringLiteral("7713495d");
  versions.device_attached = true;
  versions.product_string = QStringLiteral("Domesday Duplicator (7713495d)");
  versions.gateware.present = true;
  versions.gateware.map_version = capture::kIdentityMapVersion;
  versions.gateware.commit = "7713495d";
  return versions;
}

TEST(FirmwareDialogTest, TheTextIsShown) {
  FirmwareDialog dialog(MatchedSet());

  auto* const text =
      dialog.findChild<QLabel*>(QLatin1String(FirmwareDialog::kTextLabelName));
  ASSERT_NE(text, nullptr) << "the dialog has no text label";

  EXPECT_TRUE(text->text().contains(QStringLiteral("7713495d")))
      << "the dialog does not show the commit it was given";
}

TEST(FirmwareDialogTest, TheTextCanBeSelected) {
  // The first useful thing to do with a pair of mismatched commits is paste
  // them into a bug report.
  FirmwareDialog dialog(MatchedSet());

  auto* const text =
      dialog.findChild<QLabel*>(QLatin1String(FirmwareDialog::kTextLabelName));
  ASSERT_NE(text, nullptr);

  EXPECT_TRUE((text->textInteractionFlags() & Qt::TextSelectableByMouse) != 0)
      << "the versions cannot be selected and copied";
}

TEST(FirmwareDialogTest, TheTextScrollsWhenTheDialogIsSmall) {
  // A word-wrapped label reports its height from its width and a layout need
  // not grant it, so without the scroll area the verdict paragraph — the point
  // of the dialog — can end up below the bottom edge with no way to reach it.
  FirmwareDialog dialog(MatchedSet());

  auto* const scroll = dialog.findChild<QScrollArea*>(
      QLatin1String(FirmwareDialog::kScrollAreaName));
  ASSERT_NE(scroll, nullptr) << "the dialog has no scroll area";

  dialog.resize(dialog.minimumSizeHint());
  dialog.show();
  QApplication::processEvents();

  EXPECT_TRUE(scroll->widgetResizable())
      << "the scroll area does not resize its contents to the dialog width";
}

TEST(FirmwareDialogTest, ItOpensWithNoDeviceAttached) {
  // The case somebody most often reaches this dialog in: no hardware plugged
  // in, wondering what version they have.
  FirmwareVersions versions;
  versions.application = QStringLiteral("7713495d");

  FirmwareDialog dialog(versions);

  auto* const text =
      dialog.findChild<QLabel*>(QLatin1String(FirmwareDialog::kTextLabelName));
  ASSERT_NE(text, nullptr);

  EXPECT_TRUE(text->text().contains(QStringLiteral("No device attached")))
      << "the dialog does not say why the device versions are missing";
  EXPECT_TRUE(text->text().contains(QStringLiteral("7713495d")))
      << "the dialog does not show the application's own build";
}

}  // namespace
}  // namespace ddd::gui
