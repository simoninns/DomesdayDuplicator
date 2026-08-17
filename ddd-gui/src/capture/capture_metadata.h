/************************************************************************

    capture_metadata.h

    The YAML sidecar written beside every capture
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <string>

#include "capture_naming.h"

namespace ddd::capture {

// Everything known about a capture, written next to it as a text file.
//
// A FLAC capture already carries its essentials in its own tags — see
// capture_provenance.h — and that stays true and stays the thing to rely on: a
// file that travels alone still says what rate it was written at and whether it
// is signal or a test ramp. This is the other half. It holds what does not
// belong in a tag block: what the person at the bench typed about the disc,
// what the player was asked and answered, what an examination of the disc
// measured, and how the capture itself went. It is also the only provenance an
// uncompressed `.ddd.s16` capture has, since that format has nowhere to put a
// tag.
//
// **YAML rather than the old application's JSON.** Both are text and both are
// parsed by everything, so the choice is about the reader who is not a program:
// a sidecar's whole purpose is to be legible in five years by somebody with a
// text editor and no tooling, and YAML lets a document carry comments
// explaining its own fields. It also loses the JSON habit of quoting numbers
// and the brace-and-comma noise that dominates a document of this shape. The
// structure is deliberately close to the old application's — a reader written
// for one is a short edit from the other.
//
// Strings throughout, and formatted by whoever fills this in rather than by
// this header, on the same reasoning as DiscProvenance: the engine links no
// player code and should not start to. The player side turns a DiscType into
// "CAV" once, and the engine writes whatever it was handed.
//
// **Every field is empty until something establishes it, and an empty field is
// not written.** That is the rule the whole document is built on. A sidecar
// says what is known and nothing else, because the alternative — a field
// carrying a default nobody checked — is indistinguishable from a measurement
// once the session is over.

// What the player said about itself, where there was one to ask.
//
// Recorded for every capture taken with player control connected, not only for
// an automatic one: a manual capture of a disc in a player is still a capture
// whose provenance includes which player it came off.
struct PlayerIdentity {
  std::string model_name;

  // The two-character identity the player answers with, and the whole reply it
  // came in. Both, because the second is what somebody will be asked for when a
  // definition needs writing for a player this build does not recognise.
  std::string model_id_code;
  std::string model_code;

  std::string firmware_version;

  // Where the link was, and at what rate. Not provenance about the disc, but it
  // is the first thing anyone diagnosing a strange capture asks about.
  std::string port;
  uint32_t baud_rate = 0;

  // False when the player identified itself with a model ID no definition in
  // this build claims, so it was driven with the generic command set. Worth
  // recording: it is the difference between "this player was understood" and
  // "this player was guessed at".
  bool recognised_model = false;

  bool empty() const {
    return model_name.empty() && model_id_code.empty() && model_code.empty() &&
           firmware_version.empty() && port.empty();
  }
};

// One fact an examination established, with how it was established.
//
// The provenance travels with the value rather than being implied by the
// section it sits in, and that is the point of this type: a disc length that
// came from seeking to the end and a disc length that came from a decode nobody
// has ever checked are both numbers, and a document that showed them alike
// would be a document that has to be believed rather than read.
struct ScannedFact {
  std::string value;

  // "reported", "measured", "inferred" or "declared" — the wording the player
  // library uses, passed through. Empty for a fact nothing established, which
  // is the same thing as an empty value.
  std::string source;

  bool known() const { return !value.empty(); }
};

// What the examination of the disc found out before the capture started.
//
// Present only where an examination actually happened, which today means an
// automatic capture: its first page examines the disc and hands the result
// down. A capture taken by hand carries an empty scan rather than a stale one —
// the disc in the player five minutes and one disc change later is not the disc
// that was examined, and a sidecar asserting otherwise would be worse than one
// that says nothing.
struct DiscScan {
  // Whether there was an examination at all. Distinct from every field being
  // unknown, which is what an examination of a player that refused every query
  // produces — and which is itself a fact worth recording.
  bool examined = false;

  ScannedFact disc_present;
  ScannedFact tray;
  ScannedFact disc_type;
  ScannedFact addressing;
  ScannedFact disc_size;
  ScannedFact disc_side;
  ScannedFact video_standard;
  ScannedFact programme_start;
  ScannedFact programme_end;
  ScannedFact programme_duration;
  ScannedFact lead_in_reachable;
  ScannedFact chapters;

  // The two user codes, each as what happened and then what came back.
  //
  // The outcome is not decoration. A disc that carries no user code, a disc
  // whose user code could not be read, and a disc nobody asked about all
  // produce nothing to show, and recording them alike would record the absence
  // of evidence as evidence of absence — see UserCodeReading, where telling
  // those three apart is the whole design.
  std::string standard_user_code_outcome;
  std::string standard_user_code;
  std::string pioneer_user_code_outcome;
  std::string pioneer_user_code;

  // The disc-status reply exactly as it arrived, undecoded.
  //
  // Every documented character of it is decoded into the fields above, so this
  // is the working rather than the answer. It is here because a sidecar saying
  // "side 2" and showing the characters it read that from is one somebody can
  // check, and because the reports that a future decode would have to be
  // written from are exactly the ones that carried it.
  std::string disc_status_reply;
};

// How the capture itself went.
//
// **Every figure here is about the recording and nothing else.** That is the
// rule for the whole document and it is worth stating where the numbers are:
// metadata is data about the data, so a reading that includes the monitoring
// session either side of the file describes something that was never recorded,
// and putting it in the file's own metadata makes it a claim about the file.
//
// Several figures the Statistics panel shows are therefore not here at all —
// ring depth, encoder backlog, the device's back-pressure peak. Those describe
// how hard this machine was working during the session, which is a real thing
// to want to know and is not a property of the recording. They are shown live,
// where they are useful, and they are not written into a file that outlives the
// session by years.
struct CaptureOutcome {
  // False when the stream ended in a failure rather than because somebody
  // stopped it. The file is still readable either way — the FLAC stream is
  // closed properly on the way out however a run ends — so this is about how
  // much of the disc reached it.
  bool completed = true;

  // The failure, in the engine's own words. Empty for a capture that completed.
  std::string detail;

  double duration_seconds = 0.0;
  uint64_t samples = 0;
  uint64_t bytes = 0;

  // What the device lost inside itself while this file was being written.
  //
  // These two stay when the ring and back-pressure figures go, and the
  // distinction is the whole rule in one example: a back-pressure peak says the
  // device's buffer got full, which is a fact about a bad minute on this
  // machine. A dropped word says a sample that existed on the disc is not in
  // this file, which is a fact about the recording — and the one fault a
  // sequence check downstream cannot tell apart from a healthy stream, because
  // the device drops the samples before it ever numbers them.
  //
  // Differences rather than totals: the pipeline's counters run for the whole
  // session, so what is recorded here is the figure at the end of the capture
  // less the figure at its start.
  uint64_t device_overflow_events = 0;
  uint64_t device_dropped_words = 0;

  // How the sequence-marker check ended, in the engine's own vocabulary:
  // "running" for a check that was locked on and found nothing wrong, "failed"
  // for one that found a break, "disabled" for gateware that emits no markers,
  // "synchronising" for a capture too short to have locked on.
  //
  // The state rather than a boolean, and that is the point. This is the
  // integrity claim the whole instrument exists to be able to make, and
  // "disabled" is not "intact" — it is "nothing checked" — so a field that
  // could only say yes or no would have to lie about one of the four.
  //
  // It is about this file although the validator runs for the whole session,
  // and that is not an oversight: a mismatch ends the run where it is found, so
  // a break cannot have happened earlier in the session and be inherited by a
  // capture that started afterwards. There would have been no afterwards.
  std::string sequence_check;

  // Test mode only. `checked` is false for an ordinary capture, and for a test
  // capture that was stopped before a full ramp had gone past.
  bool test_pattern_checked = false;
  bool test_pattern_passed = false;
};

// What the signal in this file looked like.
//
// **Measured over the file's own samples and no others.** The accumulators the
// Statistics panel reads run for the whole session and are not reset when a
// writer is attached — resetting them would clear the display somebody is
// watching at the moment they press the button — so these come from a second
// span the engine opens when the file opens and closes when it closes. See
// SampleMetricsSnapshot's capture_ figures, which is where that is done.
//
// The alternative was to write the session's figures and label them, and it was
// wrong: a maximum that includes a minute of setting up before the file existed
// is a claim about a recording that never contained it, and a label does not
// stop a reader a decade later from treating a number in a file's metadata as a
// number about that file.
struct SignalSummary {
  bool known = false;

  uint32_t minimum_value = 0;
  uint32_t maximum_value = 0;
  double rms = 0.0;

  uint64_t clipped_low_samples = 0;
  uint64_t clipped_high_samples = 0;
};

// The whole document.
struct CaptureMetadata {
  // The capture file this sits beside, as its name alone — not its path. A
  // sidecar carrying the directory it was written in becomes wrong the moment
  // the pair is copied to an archive drive, which is a thing that happens to
  // every one of these files.
  std::string capture_file_name;

  std::string application_version;

  // "FLAC" or "signed 16-bit", in the words the interface uses.
  std::string format;

  bool test_mode = false;
  int decimation_factor = 1;
  uint32_t sample_rate_hz = 0;

  // The declared front-end gain, as a sentence, or empty for a gain that was
  // never declared. Empty is the important case: a figure nobody checked would
  // read as calibration data.
  std::string front_end_gain;

  std::time_t started = 0;
  std::time_t finished = 0;

  CaptureNamingFields naming;
  CaptureOutcome outcome;
  SignalSummary signal;
  PlayerIdentity player;
  DiscScan disc;
};

// The suffix a sidecar is written with: the capture's name with the format's
// extension replaced, so `Casper_side1.ddd.flac` is accompanied by
// `Casper_side1.ddd.yaml`.
//
// `.ddd.yaml` rather than `.yaml` alone for the reason the capture's own suffix
// is compound: the ".ddd" says which application's document this is, and a
// directory of captures should not acquire files that could be anybody's.
inline constexpr const char* kCaptureMetadataSuffix = ".ddd.yaml";

// Where the sidecar for this capture goes.
std::filesystem::path CaptureMetadataPath(
    const std::filesystem::path& capture_path);

// The document, as text.
//
// A pure function of the value, which is what makes the whole format testable:
// every field, including the ones that only appear when a player refused a
// query, can be checked without a device, a disc or a filesystem.
std::string BuildCaptureMetadataYaml(const CaptureMetadata& metadata);

// Write it. Returns false with the reason in `error`.
//
// Failing to write a sidecar must never be treated as failing a capture — the
// recording is on disk and is complete, and a message box claiming otherwise
// would send somebody looking for a fault in the wrong place. Callers report
// this and carry on.
bool WriteCaptureMetadataFile(const std::filesystem::path& path,
                              const CaptureMetadata& metadata,
                              std::string& error);

}  // namespace ddd::capture
