/************************************************************************

    test_platform_description.cpp

    T1 tests for the platform line in the log
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QString>

#include "platform_description.h"

namespace ddd::gui {
namespace {

// Built from facts a test chooses rather than from the machine it runs on:
// asserting on this machine's own kernel version would be a test that passes
// here and says nothing anywhere else.

TEST(DescribePlatformTest, NamesTheSystemTheKernelTheArchitectureAndQt) {
  const QString text = DescribePlatform(
      QStringLiteral("Ubuntu 24.04.1 LTS"), QStringLiteral("linux"),
      QStringLiteral("6.8.0-45-generic"), QStringLiteral("x86_64"),
      QStringLiteral("6.7.2"), QStringLiteral("6.7.2"));

  EXPECT_EQ(text,
            QStringLiteral("Ubuntu 24.04.1 LTS, kernel linux 6.8.0-45-generic, "
                           "x86_64, Qt 6.7.2"));
}

// The kernel is named on every platform, not only on Linux: on macOS the
// Darwin version is the one a kernel-level USB fault is filed against, and the
// product name alone does not give it.
TEST(DescribePlatformTest, CarriesTheKernelOnEveryPlatform) {
  EXPECT_TRUE(
      DescribePlatform(QStringLiteral("macOS 14.5"), QStringLiteral("darwin"),
                       QStringLiteral("23.5.0"), QStringLiteral("arm64"),
                       QStringLiteral("6.7.2"), QStringLiteral("6.7.2"))
          .contains(QStringLiteral("kernel darwin 23.5.0")));

  EXPECT_TRUE(DescribePlatform(QStringLiteral("Windows 11 Version 23H2"),
                               QStringLiteral("winnt"),
                               QStringLiteral("10.0.22631"),
                               QStringLiteral("x86_64"),
                               QStringLiteral("6.7.2"), QStringLiteral("6.7.2"))
                  .contains(QStringLiteral("kernel winnt 10.0.22631")));
}

// A packaged build that ships one Qt and loads another is a fault that reads
// as an application bug until the log says otherwise. Only said when the two
// differ, because on every ordinary build they do not.
TEST(DescribePlatformTest, SaysBothQtVersionsOnlyWhenTheyDiffer) {
  EXPECT_TRUE(DescribePlatform(QStringLiteral("Fedora 40"),
                               QStringLiteral("linux"), QStringLiteral("6.9.4"),
                               QStringLiteral("x86_64"),
                               QStringLiteral("6.7.2"), QStringLiteral("6.6.1"))
                  .contains(QStringLiteral("Qt 6.7.2 (built against 6.6.1)")));

  EXPECT_TRUE(DescribePlatform(QStringLiteral("Fedora 40"),
                               QStringLiteral("linux"), QStringLiteral("6.9.4"),
                               QStringLiteral("x86_64"),
                               QStringLiteral("6.7.2"), QStringLiteral("6.7.2"))
                  .endsWith(QStringLiteral("Qt 6.7.2")));
}

TEST(DescribePlatformTest, LeavesOutWhatTheSystemCouldNotAnswer) {
  const QString text = DescribePlatform(QString(), QStringLiteral("linux"),
                                        QString(), QStringLiteral("x86_64"),
                                        QStringLiteral("6.7.2"), QString());

  EXPECT_EQ(text, QStringLiteral("kernel linux, x86_64, Qt 6.7.2"));
}

// A line that says nothing looks like a fault in the logging rather than in
// what it was asking.
TEST(DescribePlatformTest, SaysSoWhenNothingCouldBeAnswered) {
  EXPECT_EQ(DescribePlatform(QString(), QString(), QString(), QString(),
                             QString(), QString()),
            QStringLiteral("not known"));
}

// The real thing, on whatever this is running on. Nothing is asserted about
// its content — that is the previous tests' job — only that the machine
// answered at all, which is the failure this would have on a platform whose
// QSysInfo returns nothing.
TEST(PlatformDescriptionTest, AnswersOnThisMachine) {
  const QString text = PlatformDescription();

  EXPECT_FALSE(text.isEmpty());
  EXPECT_NE(text, QStringLiteral("not known"));
  EXPECT_TRUE(text.contains(QStringLiteral("Qt ")));
}

}  // namespace
}  // namespace ddd::gui
