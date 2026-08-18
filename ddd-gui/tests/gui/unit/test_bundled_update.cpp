/************************************************************************

    test_bundled_update.cpp

    Where a packaged build's provisioning set is looked for
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QStringList>
#include <QTemporaryDir>

#include "bundled_update.h"

namespace ddd::gui {
namespace {

// The search is the whole of this feature that cannot be checked on the machine
// that will run it: a Flatpak's /app/share, a macOS .app's Resources and an
// MSI's install folder are three layouts, no two of which exist on the same
// computer. Stated here as data instead.

TEST(BundledUpdateTest, TheNameIsTheSameOnEveryPlatform) {
  const QStringList paths = BundledUpdateSearchPaths(
      QStringLiteral("/opt/thing/bin"), {QStringLiteral("/usr/share")});

  ASSERT_FALSE(paths.isEmpty());
  for (const QString& path : paths) {
    EXPECT_TRUE(path.endsWith(QLatin1String("/update.dddfw")))
        << path.toStdString();
  }
}

TEST(BundledUpdateTest, ItLooksBesideTheExecutableFirst) {
  const QStringList paths = BundledUpdateSearchPaths(
      QStringLiteral("/c/Program Files/Domesday Duplicator"),
      {QStringLiteral("/usr/share")});

  // The Windows installer harvests one staging directory into one folder, so
  // this is the layout the MSI produces — and it is first because it is the
  // application's own copy rather than one the machine happens to carry.
  ASSERT_FALSE(paths.isEmpty());
  EXPECT_EQ(paths.first().toStdString(),
            "/c/Program Files/Domesday Duplicator/update.dddfw");
}

TEST(BundledUpdateTest, ItLooksInsideAMacOsBundle) {
  const QStringList paths = BundledUpdateSearchPaths(
      QStringLiteral("/Applications/DomesdayDuplicator.app/Contents/MacOS"),
      QStringList());

  EXPECT_TRUE(paths.contains(
      QStringLiteral("/Applications/DomesdayDuplicator.app/Contents/Resources/"
                     "update.dddfw")))
      << paths.join(QLatin1Char(' ')).toStdString();
}

TEST(BundledUpdateTest, ItLooksInTheDataDirectoriesUnderTheApplicationId) {
  // /app/share is what a Flatpak's XDG data path begins with, and it is where
  // the CMake install rule puts the file inside the sandbox. This is the one
  // candidate the Linux packaging depends on.
  const QStringList paths = BundledUpdateSearchPaths(
      QStringLiteral("/app/bin"),
      {QStringLiteral("/app/share"), QStringLiteral("/usr/share")});

  EXPECT_TRUE(
      paths.contains(QStringLiteral("/app/share/" DDD_APP_ID "/update.dddfw")))
      << paths.join(QLatin1Char(' ')).toStdString();
  EXPECT_TRUE(
      paths.contains(QStringLiteral("/usr/share/" DDD_APP_ID "/update.dddfw")))
      << paths.join(QLatin1Char(' ')).toStdString();
}

TEST(BundledUpdateTest, TheDataDirectoriesKeepTheirOrder) {
  // Most-preferred first, as QStandardPaths reports them: on a desktop that
  // begins with the user's own ~/.local/share, which is how somebody supplies
  // a newer set to an installed build without a package. It is verified like
  // every other file, so preferring it is a convenience and not a trust.
  const QStringList paths = BundledUpdateSearchPaths(
      QString(), {QStringLiteral("/home/someone/.local/share"),
                  QStringLiteral("/usr/share")});

  ASSERT_EQ(paths.size(), 2);
  EXPECT_EQ(paths.at(0).toStdString(),
            "/home/someone/.local/share/" DDD_APP_ID "/update.dddfw");
  EXPECT_EQ(paths.at(1).toStdString(),
            "/usr/share/" DDD_APP_ID "/update.dddfw");
}

TEST(BundledUpdateTest, AnUnknownApplicationDirectoryOffersNothingOfItsOwn) {
  // A build with no QCoreApplication — which is what any test binary is —
  // asks nothing of a path it does not have, rather than searching "/".
  const QStringList paths = BundledUpdateSearchPaths(QString(), QStringList());

  EXPECT_TRUE(paths.isEmpty());
}

TEST(BundledUpdateTest, ABuildThatBundlesNothingFindsNothing) {
  // The honest state, and the one every build from source is in: the wizard
  // opens with its file picker and no preselection. Checked against a real
  // empty directory rather than only against the candidate list, because
  // "found nothing" is what the caller acts on.
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());

  const QStringList paths =
      BundledUpdateSearchPaths(directory.path(), {directory.path()});

  for (const QString& path : paths) {
    EXPECT_FALSE(QFile::exists(path)) << path.toStdString();
  }
}

TEST(BundledUpdateTest, AFileInTheFirstPlaceLookedIsTheOneFound) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());

  const QStringList paths =
      BundledUpdateSearchPaths(directory.path(), {directory.path()});
  ASSERT_FALSE(paths.isEmpty());

  QFile file(paths.first());
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.write("manifest.json");
  file.close();

  EXPECT_TRUE(QFile::exists(paths.first()));
  EXPECT_EQ(QDir(directory.path())
                .filePath(QLatin1String(kBundledUpdateName))
                .toStdString(),
            paths.first().toStdString());
}

}  // namespace
}  // namespace ddd::gui
