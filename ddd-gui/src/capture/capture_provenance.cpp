/************************************************************************

    capture_provenance.cpp

    What a capture file says about where it came from
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "capture_provenance.h"

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
  tags.reserve(7);

  tags.push_back({kTagTitle, provenance.title});
  tags.push_back({kTagEncoder, "ddd-gui " + provenance.application_version});
  tags.push_back({kTagDate, FormatProvenanceDate(provenance.started)});
  tags.push_back({kTagVersion, provenance.application_version});

  // The real rate, not the label in the FLAC header. FLAC's sample-rate field
  // stops at 655,350 Hz so the container says 40,000; this is the only place
  // the file states what the device actually ran at.
  tags.push_back({kTagSampleRate, std::to_string(kSampleRateHz)});

  tags.push_back({kTagTestMode, provenance.test_mode ? "true" : "false"});

  if (!provenance.front_end_gain.empty()) {
    tags.push_back({kTagFrontEndGain, provenance.front_end_gain});
  }

  return tags;
}

}  // namespace ddd::capture
