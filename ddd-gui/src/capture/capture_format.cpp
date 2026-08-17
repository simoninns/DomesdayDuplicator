/************************************************************************

    capture_format.cpp

    What the capture application writes, and what it can read back
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "capture_format.h"

#include <algorithm>
#include <cctype>

#include "sample_format.h"

namespace ddd::capture {

bool IsSupportedDecimationFactor(int factor) {
  return factor == kUndecimatedFactor || factor == kTapeDecimationFactor;
}

uint32_t FlacSampleRateLabelFor(int decimation_factor) {
  if (!IsSupportedDecimationFactor(decimation_factor)) {
    return kFlacSampleRateLabel;
  }
  return kFlacSampleRateLabel / static_cast<uint32_t>(decimation_factor);
}

uint32_t SampleRateHzFor(int decimation_factor) {
  // An unsupported factor falls back to the converter's own rate rather than
  // dividing by it. A display is better off stating the undecimated rate — the
  // one the device runs at unless it has been told otherwise — than scaling
  // every frequency on the screen by a number nothing will ever ask the
  // gateware for.
  if (!IsSupportedDecimationFactor(decimation_factor)) {
    return kSampleRateHz;
  }
  return kSampleRateHz / static_cast<uint32_t>(decimation_factor);
}

const char* CaptureFileSuffix(CaptureOutputFormat format) {
  switch (format) {
    case CaptureOutputFormat::kFlac:
      return kCaptureFileSuffix;
    case CaptureOutputFormat::kSigned16Bit:
      return kSigned16BitCaptureFileSuffix;
  }
  return kCaptureFileSuffix;
}

std::filesystem::path AddCaptureFileSuffix(const std::filesystem::path& stem,
                                           CaptureOutputFormat format) {
  const std::string text = stem.string();
  const std::string suffix = CaptureFileSuffix(format);

  if (text.size() >= suffix.size() &&
      text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0) {
    return stem;
  }

  return std::filesystem::path(text + suffix);
}

std::string MatchedCaptureFileSuffix(const std::string& file_path) {
  for (const CaptureOutputFormat format :
       {CaptureOutputFormat::kFlac, CaptureOutputFormat::kSigned16Bit}) {
    const std::string candidate = CaptureFileSuffix(format);
    if (file_path.size() >= candidate.size() &&
        file_path.compare(file_path.size() - candidate.size(), candidate.size(),
                          candidate) == 0) {
      return candidate;
    }
  }
  return {};
}

std::string StripCaptureFileSuffix(const std::string& file_path) {
  const std::string suffix = MatchedCaptureFileSuffix(file_path);
  return suffix.empty() ? file_path
                        : file_path.substr(0, file_path.size() - suffix.size());
}

std::string LowerCaseExtension(const std::filesystem::path& file_path) {
  std::string extension = file_path.extension().string();
  if (!extension.empty() && extension.front() == '.') {
    extension.erase(extension.begin());
  }

  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  return extension;
}

}  // namespace ddd::capture
