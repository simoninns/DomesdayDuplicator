/************************************************************************

    test_about_text.cpp

    T1 tests for the About dialog's build provenance
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QString>

#include "about_text.h"
#include "version.h"

namespace ddd::gui {
namespace {

QString VersionString() {
  const auto version = capture::Version();
  return QString::fromUtf8(version.data(),
                           static_cast<qsizetype>(version.size()));
}

// The point of these tests: the About dialog is the second of two routes to the
// build's identity, and on Windows it is the only one that works, because the
// application is a GUI subsystem executable and --version reaches no console.
// A user who cannot say which build produced a bad capture cannot be helped, so
// this is a release-traceability requirement rather than a cosmetic one.

TEST(AboutTextTest, CarriesTheBuildVersion) {
  EXPECT_TRUE(AboutText().contains(VersionString()))
      << "About text does not name the build it came from";
}

TEST(AboutTextTest, AlwaysNamesAVersionEvenWhenItIsUnknown) {
  // A build with no version determined reports "unknown" rather than leaving
  // the line blank or dropping it: an absent version is indistinguishable from
  // a user who did not look, and the release gate needs the difference.
  EXPECT_FALSE(VersionString().isEmpty());
  EXPECT_TRUE(AboutText().contains(QStringLiteral("Build:")));
}

TEST(AboutTextTest, NamesTheApplicationAndItsLicence) {
  const QString text = AboutText();
  EXPECT_TRUE(text.contains(QStringLiteral("Domesday Duplicator")));
  EXPECT_TRUE(text.contains(QStringLiteral("General Public License")));
}

TEST(AboutTextTest, IsStable) {
  // Called once per dialog opening; nothing in it may vary between calls, or
  // two users reading it aloud would not agree.
  EXPECT_EQ(AboutText(), AboutText());
}

}  // namespace
}  // namespace ddd::gui
