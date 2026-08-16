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
