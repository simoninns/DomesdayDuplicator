/************************************************************************

    test_data_analysis.cpp

    Checking a finished test-mode capture for breaks
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "test_data_analysis.h"

#include <vector>

#include "capture_reader.h"
#include "test_pattern_verifier.h"

namespace ddd::capture {
namespace {

// Digit grouping, so a sample offset thirty digits into a capture can be read
// rather than counted. Done by hand because std::locale's grouping is whatever
// the machine is configured for, and a verdict that a script greps for should
// not change shape with the environment.
std::string Grouped(uint64_t value) {
  std::string digits = std::to_string(value);
  for (size_t position = digits.size(); position > 3;) {
    position -= 3;
    digits.insert(position, ",");
  }
  return digits;
}

std::string DescribeFailure(const std::string& name,
                            const TestDataAnalysis& analysis) {
  return "FAILED: " + name + " — the test sequence breaks at sample " +
         Grouped(analysis.samples_checked) + ", where " +
         std::to_string(analysis.expected_value) + " was expected but " +
         std::to_string(analysis.actual_value) + " was read.";
}

std::string DescribePass(const std::string& name,
                         const TestDataAnalysis& analysis) {
  if (!analysis.sequence_length.has_value()) {
    // Worth saying rather than reporting a bare pass. The check exists to catch
    // a break somewhere in a long stream, and a file too short for the ramp to
    // have wrapped even once has not put that to the test.
    return "PASSED: " + name + " — " + Grouped(analysis.samples_checked) +
           " samples with no break, but the capture is too short for the test "
           "sequence to have wrapped, so this is weak evidence.";
  }

  return "PASSED: " + name + " — " + Grouped(analysis.samples_checked) +
         " samples checked, no breaks, test sequence length " +
         std::to_string(*analysis.sequence_length) + ".";
}

TestDataAnalysis Unreadable(const std::string& message) {
  TestDataAnalysis analysis;
  analysis.outcome = TestDataAnalysis::Outcome::kUnreadable;
  analysis.message = message;
  return analysis;
}

}  // namespace

int TestDataAnalysis::ExitCode() const {
  switch (outcome) {
    case Outcome::kPassed:
      return 0;
    case Outcome::kFailed:
      return 1;
    case Outcome::kUnreadable:
    case Outcome::kCancelled:
      return 2;
  }
  return 2;
}

TestDataAnalysis AnalyseTestData(const std::filesystem::path& file_path,
                                 const AnalysisProgress& progress,
                                 const AnalysisCancelled& cancelled) {
  const std::string name = file_path.filename().string();

  const std::optional<CaptureReader::Format> format =
      CaptureReader::FormatFromExtension(file_path);
  if (!format.has_value()) {
    return Unreadable(name +
                      " is not a capture file this application can read. "
                      "Expected .flac, .s16 or .raw.");
  }

  CaptureReader reader;
  std::string error_message;
  if (!reader.Open(file_path, *format, error_message)) {
    return Unreadable("Could not open " + name + ": " + error_message);
  }

  const std::optional<uint64_t> total_samples = reader.TotalSamples();

  TestPatternVerifier verifier;
  TestDataAnalysis analysis;

  std::vector<uint16_t> samples;
  bool end_of_file = false;
  bool stopped_early = false;

  while (!end_of_file) {
    if (cancelled && cancelled()) {
      stopped_early = true;
      break;
    }

    if (!reader.Read(samples, kAnalysisChunkSamples, end_of_file)) {
      return Unreadable("Failed to read " + name + ": " + reader.LastError());
    }
    if (samples.empty()) {
      break;
    }

    const bool still_good = verifier.Feed(samples.data(), samples.size());

    if (progress) {
      progress(verifier.GetResult().samples_checked, total_samples);
    }

    if (!still_good) {
      break;
    }
  }

  const TestPatternVerifier::Result& verdict = verifier.GetResult();
  analysis.samples_checked = verdict.samples_checked;
  analysis.expected_value = verdict.expected_value;
  analysis.actual_value = verdict.actual_value;
  analysis.sequence_length = verdict.sequence_length;

  if (!verdict.passed) {
    analysis.outcome = TestDataAnalysis::Outcome::kFailed;
    analysis.message = DescribeFailure(name, analysis);
    return analysis;
  }

  if (stopped_early) {
    analysis.outcome = TestDataAnalysis::Outcome::kCancelled;
    analysis.message = "Cancelled after " + Grouped(analysis.samples_checked) +
                       " samples, with no break found so far.";
    return analysis;
  }

  analysis.outcome = TestDataAnalysis::Outcome::kPassed;
  analysis.message = DescribePass(name, analysis);
  return analysis;
}

}  // namespace ddd::capture
