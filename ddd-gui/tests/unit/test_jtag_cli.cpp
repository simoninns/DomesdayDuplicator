/************************************************************************

    test_jtag_cli.cpp

    T1 unit test for ddd-jtag's contract
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "jtag_cli.h"
#include "svf_fixtures.h"

namespace ddd::capture {
namespace {

// A programming file on disk, cleaned up after itself. Named after the test
// that made it so that two running at once cannot share one.
class SvfFile {
 public:
  explicit SvfFile(const std::string& text) {
    const ::testing::TestInfo* info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    path_ = std::filesystem::temp_directory_path() /
            (std::string("ddd-jtag-test-") +
             (info != nullptr ? info->name() : "unnamed") + ".svf");

    std::ofstream file(path_);
    file << text;
  }

  ~SvfFile() {
    std::error_code ignored;
    std::filesystem::remove(path_, ignored);
  }

  SvfFile(const SvfFile&) = delete;
  SvfFile& operator=(const SvfFile&) = delete;

  std::string path() const { return path_.string(); }

 private:
  std::filesystem::path path_;
};

int RunTool(const std::vector<std::string>& args, std::string& out,
            std::string& error) {
  std::ostringstream output;
  std::ostringstream problems;
  const int code = RunJtagCli(args, output, problems);
  out = output.str();
  error = problems.str();
  return code;
}

TEST(JtagCliOptions, TakesAProgrammingFile) {
  const JtagCliOptions options = ParseJtagCliOptions({"provisioning.svf"});

  EXPECT_EQ(options.svf_path, "provisioning.svf");
  EXPECT_FALSE(options.dry_run);
  EXPECT_TRUE(options.problem.empty());
}

TEST(JtagCliOptions, TakesADryRun) {
  const JtagCliOptions options =
      ParseJtagCliOptions({"--dry-run", "provisioning.svf"});

  EXPECT_TRUE(options.dry_run);
  EXPECT_EQ(options.svf_path, "provisioning.svf");
}

TEST(JtagCliOptions, RefusesAnOptionItDoesNotKnow) {
  EXPECT_FALSE(ParseJtagCliOptions({"--program-everything"}).problem.empty());
}

TEST(JtagCliOptions, RefusesTwoFiles) {
  EXPECT_FALSE(ParseJtagCliOptions({"one.svf", "two.svf"}).problem.empty());
}

TEST(JtagCliOptions, RefusesNoFileAtAll) {
  EXPECT_FALSE(ParseJtagCliOptions({}).problem.empty());
}

TEST(JtagCli, HelpSucceedsAndSaysHowToUseIt) {
  std::string out;
  std::string error;

  EXPECT_EQ(RunTool({"--help"}, out, error), kJtagCliSuccess);
  EXPECT_NE(out.find("ddd-jtag"), std::string::npos);
  EXPECT_NE(out.find("--dry-run"), std::string::npos);
  EXPECT_TRUE(error.empty());
}

TEST(JtagCli, NoArgumentsIsAUsageError) {
  std::string out;
  std::string error;

  EXPECT_EQ(RunTool({}, out, error), kJtagCliUsage);
  EXPECT_FALSE(error.empty());
}

TEST(JtagCli, AMissingFileIsAFileError) {
  std::string out;
  std::string error;

  EXPECT_EQ(RunTool({"--dry-run", "/no/such/file.svf"}, out, error),
            kJtagCliFile);
  EXPECT_NE(error.find("Could not read"), std::string::npos);
}

// The mode that may be run automatically: everything this application
// contributes, checked against a file, with nothing attached and nothing
// written (AGENTS.md §4).
TEST(JtagCli, ADryRunReadsTheFileAndSaysWhatItWouldHaveClocked) {
  const SvfFile file(kQuartusOpeningSvf);

  std::string out;
  std::string error;

  // The whole file is read, and the answers it expects are not held against
  // the nothing that comes back: with no device attached, a comparison could
  // only ever fail and would say nothing about the file.
  EXPECT_EQ(RunTool({"--dry-run", file.path()}, out, error), kJtagCliSuccess)
      << error;
  EXPECT_NE(out.find("Reading "), std::string::npos);
  EXPECT_NE(out.find("statements"), std::string::npos) << out;
  EXPECT_TRUE(error.empty()) << error;
}

TEST(JtagCli, ADryRunOfAFileWithNothingToCheckSucceeds) {
  const SvfFile file("STATE IDLE;\nRUNTEST 4096 TCK;\nSDR 8 TDI (FF);\n");

  std::string out;
  std::string error;

  EXPECT_EQ(RunTool({"--dry-run", file.path()}, out, error), kJtagCliSuccess);
  EXPECT_NE(out.find("bits shifted"), std::string::npos) << out;
  EXPECT_NE(out.find("idle clocks"), std::string::npos) << out;
  EXPECT_NE(out.find("none"), std::string::npos)
      << "a dry run described itself as having used a cable";
}

TEST(JtagCli, AFileThatCannotBeUnderstoodIsAFileError) {
  const SvfFile file("SDR 4 TDI (5) FLIBBLE (2);\n");

  std::string out;
  std::string error;

  EXPECT_EQ(RunTool({"--dry-run", file.path()}, out, error), kJtagCliFile);
  EXPECT_FALSE(error.empty());
}

TEST(JtagCliUsageText, NamesEveryExitCode) {
  const std::string usage = JtagCliUsage();

  for (const char* code : {"0 success", "2 usage", "3 file", "4 no cable",
                           "5 programming failed"}) {
    EXPECT_NE(usage.find(code), std::string::npos) << code;
  }
}

}  // namespace
}  // namespace ddd::capture
