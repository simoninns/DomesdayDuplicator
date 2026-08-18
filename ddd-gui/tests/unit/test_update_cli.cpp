/************************************************************************

    test_update_cli.cpp

    T1 unit test for ddd-update's command line and exit codes
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

#include "update_cli.h"

namespace ddd::capture {
namespace {

// The exit code is the whole interface as far as a script driving a bench
// procedure is concerned, so it is what these check. Anything past the bundle
// needs a device and belongs to the T5 procedures in TESTING.md.

TEST(UpdateCliOptions, TakesABundlePath) {
  const UpdateCliOptions options = ParseUpdateCliOptions({"bundle.dddfw"});

  EXPECT_TRUE(options.problem.empty());
  EXPECT_EQ(options.bundle_path, "bundle.dddfw");
  EXPECT_FALSE(options.accept_development_key);
  EXPECT_FALSE(options.dry_run);
}

// Accepting the development key is a per-invocation opt-in and never a
// remembered setting, because a signature from a key whose secret half is
// public proves nothing about origin.
TEST(UpdateCliOptions, TheDevelopmentKeyIsOptedIntoExplicitly) {
  const UpdateCliOptions options =
      ParseUpdateCliOptions({"--dev-update-key", "bundle.dddfw"});

  EXPECT_TRUE(options.problem.empty());
  EXPECT_TRUE(options.accept_development_key);
}

TEST(UpdateCliOptions, TakesADevicePathAndADryRun) {
  const UpdateCliOptions options = ParseUpdateCliOptions(
      {"--dry-run", "--device", "/sys/bus/usb/devices/1-2", "bundle.dddfw"});

  EXPECT_TRUE(options.problem.empty());
  EXPECT_TRUE(options.dry_run);
  EXPECT_EQ(options.device_path, "/sys/bus/usb/devices/1-2");
  EXPECT_EQ(options.bundle_path, "bundle.dddfw");
}

TEST(UpdateCliOptions, RefusesADeviceOptionWithNothingAfterIt) {
  const UpdateCliOptions options = ParseUpdateCliOptions({"--device"});
  EXPECT_FALSE(options.problem.empty());
}

TEST(UpdateCliOptions, RefusesAnOptionItDoesNotKnow) {
  const UpdateCliOptions options =
      ParseUpdateCliOptions({"--force", "bundle.dddfw"});
  EXPECT_FALSE(options.problem.empty());
}

// Two bundles is a mistake worth catching rather than silently installing
// the second one.
TEST(UpdateCliOptions, RefusesTwoBundles) {
  const UpdateCliOptions options =
      ParseUpdateCliOptions({"one.dddfw", "two.dddfw"});
  EXPECT_FALSE(options.problem.empty());
}

TEST(UpdateCliOptions, RefusesNoBundleAtAll) {
  const UpdateCliOptions options = ParseUpdateCliOptions({});
  EXPECT_FALSE(options.problem.empty());
}

TEST(UpdateCli, HelpSucceedsAndSaysHowToUseIt) {
  std::ostringstream out;
  std::ostringstream error;

  EXPECT_EQ(RunUpdateCli({"--help"}, out, error), kUpdateCliSuccess);
  EXPECT_NE(out.str().find("ddd-update"), std::string::npos);
  EXPECT_NE(out.str().find("--dev-update-key"), std::string::npos);
  EXPECT_TRUE(error.str().empty());
}

TEST(UpdateCli, NoArgumentsIsAUsageError) {
  std::ostringstream out;
  std::ostringstream error;

  EXPECT_EQ(RunUpdateCli({}, out, error), kUpdateCliUsage);
  EXPECT_NE(error.str().find("No bundle given"), std::string::npos);
}

// A file that is not there stops before any device is touched, and says
// which file it was.
TEST(UpdateCli, AMissingBundleIsABundleError) {
  std::ostringstream out;
  std::ostringstream error;

  const int code =
      RunUpdateCli({"/nonexistent/path/to/bundle.dddfw"}, out, error);

  EXPECT_EQ(code, kUpdateCliBundle);
  EXPECT_NE(error.str().find("bundle.dddfw"), std::string::npos);
}

TEST(UpdateCliUsageText, NamesEveryExitCode) {
  const std::string usage = UpdateCliUsage();

  EXPECT_NE(usage.find("--device"), std::string::npos);
  EXPECT_NE(usage.find("--dry-run"), std::string::npos);
  EXPECT_NE(usage.find("Exit codes"), std::string::npos);
}

}  // namespace
}  // namespace ddd::capture
