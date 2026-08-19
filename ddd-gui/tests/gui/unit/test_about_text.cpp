/************************************************************************

    test_about_text.cpp

    T1 tests for the About dialog's build provenance
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QString>
#include <string>

#include "about_text.h"
#include "version.h"

namespace ddd::gui {
namespace {

// The stamp as the About box shows it: the commit this build was made from.
QString CommitString() {
  return QString::fromStdString(std::string(capture::Commit()));
}

// The point of these tests: the About dialog is the second of two routes to the
// build's identity, and on Windows it is the only one that works, because the
// application is a GUI subsystem executable and --version reaches no console.
// A user who cannot say which build produced a bad capture cannot be helped, so
// this is a release-traceability requirement rather than a cosmetic one.

TEST(AboutTextTest, CarriesTheBuildCommit) {
  EXPECT_TRUE(AboutText().contains(CommitString()))
      << "About text does not name the build it came from";
}

TEST(AboutTextTest, AlwaysNamesAStampEvenWhenItIsUnknown) {
  // A build with no commit determined reports "unknown" rather than leaving
  // the line blank or dropping it: an absent stamp is indistinguishable from a
  // user who did not look, and the release gate needs the difference.
  EXPECT_FALSE(CommitString().isEmpty());
  EXPECT_TRUE(AboutText().contains(QStringLiteral("Build:")));
}

// The one stamp and nothing else. There used to be a release version in front
// of the commit, and a rule about which to show when; the About box now shows
// what the FX3 firmware and the FPGA gateware show, so a bug report quotes
// three hashes of the same kind.
TEST(AboutTextTest, TheStampIsTheCommitAndNothingElse) {
  if (capture::Commit() != "unknown") {
    EXPECT_EQ(
        CommitString(),
        QString::fromUtf8(capture::Commit().data(),
                          static_cast<qsizetype>(capture::Commit().size())));
  }
}

TEST(AboutTextTest, NamesTheApplicationAndItsLicence) {
  const QString text = AboutText();
  EXPECT_TRUE(text.contains(QStringLiteral("Domesday Duplicator")));
  EXPECT_TRUE(text.contains(QStringLiteral("General Public License")));
}

TEST(AboutTextTest, NamesTheAuthorAndTheCopyright) {
  const QString text = AboutText();

  EXPECT_TRUE(text.contains(QStringLiteral("Simon Inns")))
      << "About text does not say who wrote it";
  EXPECT_TRUE(text.contains(QStringLiteral("©")))
      << "About text carries no copyright notice";
}

TEST(AboutTextTest, CarriesTheNoticesTheLicenceAsksFor) {
  // The GPL asks an interactive program to show appropriate legal notices: the
  // copyright, the licence, and the absence of a warranty. The About dialog is
  // where a user goes to look for them, so it is where they are.
  const QString text = AboutText();

  EXPECT_TRUE(text.contains(QStringLiteral("version 3")));
  EXPECT_TRUE(text.contains(QStringLiteral("warranty")));
  EXPECT_TRUE(text.contains(QStringLiteral("free software")));
}

TEST(AboutTextTest, PointsAtTheSource) {
  // The licence entitles a user to the source. A licence notice that does not
  // say where it is leaves them to guess.
  EXPECT_TRUE(AboutText().contains(QStringLiteral("github.com/simoninns")));
}

TEST(AboutTextTest, IsStable) {
  // Called once per dialog opening; nothing in it may vary between calls, or
  // two users reading it aloud would not agree.
  EXPECT_EQ(AboutText(), AboutText());
}

}  // namespace
}  // namespace ddd::gui
