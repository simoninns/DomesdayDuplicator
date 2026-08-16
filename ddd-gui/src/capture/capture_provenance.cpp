/************************************************************************

    capture_provenance.cpp

    What a capture file says about where it came from
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "capture_provenance.h"

#include "capture_format.h"
#include "capture_naming.h"
#include "sample_format.h"

namespace ddd::capture {

std::string FormatProvenanceDate(std::time_t when) {
  // The first ten characters of the capture timestamp are already
  // YYYY-MM-DD, and reusing them is what guarantees the DATE tag and the
  // filename can never disagree about which day a capture was taken.
  const std::string timestamp = FormatCaptureTimestamp(when);
  return timestamp.substr(0, timestamp.find('_'));
}

std::vector<FlacWriter::Tag> BuildProvenanceTags(
    const CaptureProvenance& provenance) {
  std::vector<FlacWriter::Tag> tags;
  tags.reserve(8);

  tags.push_back({kTagTitle, provenance.title});
  tags.push_back({kTagEncoder, "ddd-gui " + provenance.application_version});
  tags.push_back({kTagDate, FormatProvenanceDate(provenance.started)});
  tags.push_back({kTagVersion, provenance.application_version});

  // The real rate, not the label in the FLAC header. FLAC's sample-rate field
  // stops at 655,350 Hz so the container says 40,000; this is the only place
  // the file states what the device actually ran at.
  //
  // The rate of the *file*, so a 2:1 decimated capture says 20,000,000 here. A
  // reader wanting the device's own rate multiplies by the decimation factor
  // beside it; a reader wanting to know how fast to play the samples it has
  // reads this one and is right without having to know anything else.
  const int decimation =
      IsSupportedDecimationFactor(provenance.decimation_factor)
          ? provenance.decimation_factor
          : 1;
  tags.push_back(
      {kTagSampleRate,
       std::to_string(kSampleRateHz / static_cast<uint32_t>(decimation))});
  tags.push_back({kTagDecimation, std::to_string(decimation)});

  tags.push_back({kTagTestMode, provenance.test_mode ? "true" : "false"});

  if (!provenance.front_end_gain.empty()) {
    tags.push_back({kTagFrontEndGain, provenance.front_end_gain});
  }

  return tags;
}

}  // namespace ddd::capture
