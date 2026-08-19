/************************************************************************

    svf_player.cpp

    Playing a Serial Vector Format programming file through a JTAG cable
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "svf_player.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "jtag_cable.h"
#include "logger.h"

namespace ddd::capture {
namespace {

// --- The TAP state machine -------------------------------------------------
//
// Sixteen states and one input. Everything a JTAG session does is a walk
// through this graph, and every SVF statement is either a walk, a shift, or
// a wait.

struct Transition {
  TapState on_zero;
  TapState on_one;
};

// Indexed by TapState, in the order the enumeration declares them.
constexpr std::array<Transition, 16> kTransitions{{
    {TapState::kIdle, TapState::kReset},          // kReset
    {TapState::kIdle, TapState::kDrSelect},       // kIdle
    {TapState::kDrCapture, TapState::kIrSelect},  // kDrSelect
    {TapState::kDrShift, TapState::kDrExit1},     // kDrCapture
    {TapState::kDrShift, TapState::kDrExit1},     // kDrShift
    {TapState::kDrPause, TapState::kDrUpdate},    // kDrExit1
    {TapState::kDrPause, TapState::kDrExit2},     // kDrPause
    {TapState::kDrShift, TapState::kDrUpdate},    // kDrExit2
    {TapState::kIdle, TapState::kDrSelect},       // kDrUpdate
    {TapState::kIrCapture, TapState::kReset},     // kIrSelect
    {TapState::kIrShift, TapState::kIrExit1},     // kIrCapture
    {TapState::kIrShift, TapState::kIrExit1},     // kIrShift
    {TapState::kIrPause, TapState::kIrUpdate},    // kIrExit1
    {TapState::kIrPause, TapState::kIrExit2},     // kIrPause
    {TapState::kIrShift, TapState::kIrUpdate},    // kIrExit2
    {TapState::kIdle, TapState::kDrSelect},       // kIrUpdate
}};

size_t StateIndex(TapState state) { return static_cast<size_t>(state); }

// The shortest run of TMS values that walks from one state to another.
//
// Computed rather than tabulated: the graph is sixteen nodes, the search is
// instant, and a hand-written table of sixteen-by-sixteen bit patterns is
// exactly the sort of thing that is wrong in one cell and works everywhere
// else for a year.
//
// Test-Logic-Reset is excluded as a stepping stone. It is reachable from
// several states in one clock and would therefore turn up in plenty of
// shortest paths — and passing through it resets the instruction register,
// which in the middle of a flash write is the difference between programming
// a device and confusing it.
std::vector<bool> PathBetween(TapState from, TapState to) {
  if (from == to) {
    return {};
  }

  std::array<int, 16> came_from{};
  std::array<bool, 16> reached{};
  std::array<bool, 16> arrived_on_one{};
  came_from.fill(-1);
  reached.fill(false);

  std::vector<TapState> frontier{from};
  reached[StateIndex(from)] = true;

  while (!frontier.empty()) {
    std::vector<TapState> next;
    for (TapState state : frontier) {
      const Transition& transition = kTransitions[StateIndex(state)];
      const std::array<std::pair<TapState, bool>, 2> steps{
          {{transition.on_zero, false}, {transition.on_one, true}}};

      for (const auto& [candidate, tms] : steps) {
        const size_t index = StateIndex(candidate);
        if (reached[index]) {
          continue;
        }
        if (candidate == TapState::kReset && to != TapState::kReset) {
          continue;
        }
        reached[index] = true;
        came_from[index] = static_cast<int>(StateIndex(state));
        arrived_on_one[index] = tms;
        if (candidate == to) {
          std::vector<bool> path;
          size_t walk = index;
          while (static_cast<TapState>(walk) != from) {
            path.push_back(arrived_on_one[walk]);
            walk = static_cast<size_t>(came_from[walk]);
          }
          std::reverse(path.begin(), path.end());
          return path;
        }
        next.push_back(candidate);
      }
    }
    frontier = std::move(next);
  }

  // Unreachable in this graph: every state reaches every other. Returning an
  // empty path rather than asserting keeps a hypothetical unreachable pair
  // from taking the application with it.
  return {};
}

bool IsStableState(TapState state) {
  return state == TapState::kReset || state == TapState::kIdle ||
         state == TapState::kDrPause || state == TapState::kIrPause;
}

std::optional<TapState> StateNamed(std::string_view name) {
  static constexpr std::array<std::pair<std::string_view, TapState>, 16> kNames{
      {{"RESET", TapState::kReset},
       {"IDLE", TapState::kIdle},
       {"DRSELECT", TapState::kDrSelect},
       {"DRCAPTURE", TapState::kDrCapture},
       {"DRSHIFT", TapState::kDrShift},
       {"DREXIT1", TapState::kDrExit1},
       {"DRPAUSE", TapState::kDrPause},
       {"DREXIT2", TapState::kDrExit2},
       {"DRUPDATE", TapState::kDrUpdate},
       {"IRSELECT", TapState::kIrSelect},
       {"IRCAPTURE", TapState::kIrCapture},
       {"IRSHIFT", TapState::kIrShift},
       {"IREXIT1", TapState::kIrExit1},
       {"IRPAUSE", TapState::kIrPause},
       {"IREXIT2", TapState::kIrExit2},
       {"IRUPDATE", TapState::kIrUpdate}}};

  for (const auto& [text, state] : kNames) {
    if (text == name) {
      return state;
    }
  }
  return std::nullopt;
}

// --- Bit runs --------------------------------------------------------------
//
// Packed the way IJtagCable wants them: bit i of the run is bit i % 8 of byte
// i / 8, so the first bit clocked is the least significant bit of byte zero.
// SVF writes its values as one big hexadecimal number, whose least
// significant bit is the first bit shifted, so the conversion reads the text
// backwards and nothing anywhere reverses a bit.

size_t BytesForBits(size_t bits) { return (bits + 7) / 8; }

bool BitAt(const std::vector<uint8_t>& bits, size_t index) {
  return ((bits[index / 8] >> (index % 8)) & 1U) != 0;
}

void SetBitAt(std::vector<uint8_t>& bits, size_t index, bool value) {
  const uint8_t mask = static_cast<uint8_t>(1U << (index % 8));
  if (value) {
    bits[index / 8] |= mask;
  } else {
    bits[index / 8] &= static_cast<uint8_t>(~mask);
  }
}

std::optional<int> HexDigit(char character) {
  if (character >= '0' && character <= '9') {
    return character - '0';
  }
  if (character >= 'A' && character <= 'F') {
    return character - 'A' + 10;
  }
  if (character >= 'a' && character <= 'f') {
    return character - 'a' + 10;
  }
  return std::nullopt;
}

// A run of `bit_count` bits with every bit set — the mask a scan gets when
// the file names none, which means "compare all of it".
std::vector<uint8_t> AllBits(size_t bit_count) {
  std::vector<uint8_t> bits(BytesForBits(bit_count), 0xFF);
  for (size_t index = bit_count; index < bits.size() * 8; ++index) {
    SetBitAt(bits, index, false);
  }
  return bits;
}

bool HexToBits(std::string_view hex, size_t bit_count,
               std::vector<uint8_t>& bits, std::string& problem) {
  bits.assign(BytesForBits(bit_count), 0);

  size_t nibble = 0;
  for (size_t position = hex.size(); position > 0; --position) {
    const std::optional<int> digit = HexDigit(hex[position - 1]);
    if (!digit.has_value()) {
      problem =
          "the file has something that is not a hexadecimal digit in a "
          "value";
      return false;
    }

    for (int offset = 0; offset < 4; ++offset) {
      const bool set = ((*digit >> offset) & 1) != 0;
      const size_t index = nibble * 4 + static_cast<size_t>(offset);
      if (index < bit_count) {
        SetBitAt(bits, index, set);
      } else if (set) {
        // A value wider than the scan it belongs to. Refused rather than
        // truncated: the two readings of such a file differ by whatever the
        // extra bits were, and one of them writes something nobody described.
        problem =
            "the file has a value with more bits in it than the scan it "
            "belongs to";
        return false;
      }
    }
    ++nibble;
  }
  return true;
}

// A window onto a scan value, in SVF's own way round: one big number, most
// significant first.
//
// A window rather than the whole value because a scan can be hundreds of bits
// wide and only one of them is ever the reason a run stopped. Printing the
// most significant digits and stopping there — which is what this used to do
// — is truthful and useless: the status check that ends a Cyclone IV
// configure is 732 bits, so the text showed bits 731 down to 604 while the
// bit that disagreed was at 286. The bit that matters has to be *in* the
// window, so the window is placed around it and the range is named.
struct HexWindow {
  std::string text;

  // The range the text covers, inclusive, in the file's own bit numbering.
  size_t lowest_bit = 0;
  size_t highest_bit = 0;
};

HexWindow BitsToHexAround(const std::vector<uint8_t>& bits, size_t bit_count,
                          size_t around, size_t maximum_digits) {
  // Nothing to show, and the arithmetic below would run off the bottom of
  // size_t working that out. The only caller has already refused a scan of no
  // bits; this is here so that the next one does not have to.
  if (bit_count == 0) {
    return {};
  }

  const size_t digits = (bit_count + 3) / 4;

  // One past the highest nibble shown, so that a value no wider than the
  // window is shown whole and nothing is elided that did not need to be.
  size_t top = digits;
  if (digits > maximum_digits) {
    const size_t centre = around / 4;
    top = std::min(digits, centre + (maximum_digits + 1) / 2);
    top = std::max(top, maximum_digits);
  }
  const size_t bottom = top > maximum_digits ? top - maximum_digits : 0;

  HexWindow window;
  window.lowest_bit = bottom * 4;
  window.highest_bit = std::min(top * 4, bit_count) - 1;

  if (top < digits) {
    window.text += "…";
  }
  for (size_t nibble = top; nibble > bottom; --nibble) {
    int value = 0;
    for (int offset = 0; offset < 4; ++offset) {
      const size_t index = (nibble - 1) * 4 + static_cast<size_t>(offset);
      if (index < bit_count && BitAt(bits, index)) {
        value |= 1 << offset;
      }
    }
    window.text.push_back("0123456789ABCDEF"[value]);
  }
  if (bottom > 0) {
    window.text += "…";
  }
  return window;
}

// --- The file --------------------------------------------------------------

struct Token {
  std::string text;

  // Whether it arrived in parentheses, which is what tells a value from a
  // keyword: SVF's scan values are the only bracketed thing in the grammar.
  bool bracketed = false;
};

// One SVF statement, and where it started.
struct Statement {
  std::vector<Token> tokens;
  size_t line = 0;
};

// The parts of a scan a statement can carry. Absent is not the same as empty:
// TDI, MASK and SMASK are remembered from the previous scan of the same kind
// when a statement leaves them out, and TDO is not — a scan with no TDO
// compares nothing rather than repeating the last comparison.
struct ScanFields {
  std::optional<std::string> tdi;
  std::optional<std::string> tdo;
  std::optional<std::string> mask;
  std::optional<std::string> smask;
};

// What a previous scan of one kind left behind.
struct StickyScan {
  size_t bit_count = 0;
  std::vector<uint8_t> tdi;
  std::vector<uint8_t> mask;
  std::vector<uint8_t> smask;
};

class Player {
 public:
  Player(IJtagCable& cable, ILogger* logger,
         const SvfPlayer::ProgressCallback& progress,
         const SvfPlayer::StopCallback& stop, bool device_attached)
      : cable_(cable),
        logger_(logger),
        progress_(progress),
        stop_(stop),
        device_attached_(device_attached) {}

  SvfPlayResult Play(std::string_view text);

 private:
  bool NextStatement(Statement& statement);
  bool Execute(const Statement& statement);

  bool DoScan(const Statement& statement, bool instruction);
  bool DoRunTest(const Statement& statement);
  bool DoState(const Statement& statement);
  bool DoEndState(const Statement& statement, TapState& end_state);
  bool DoHeaderOrTrailer(const Statement& statement);
  bool DoFrequency(const Statement& statement);
  bool DoTrst(const Statement& statement);

  bool ParseScanFields(const Statement& statement, size_t first,
                       ScanFields& fields);

  bool MoveTo(TapState target);
  bool Shift(const std::vector<uint8_t>& tdi, size_t bit_count,
             std::vector<uint8_t>* tdo);

  bool Fail(const std::string& problem);
  bool FailAt(size_t line, const std::string& problem);

  static std::optional<double> Number(const Token& token);

  IJtagCable& cable_;
  ILogger* logger_ = nullptr;
  const SvfPlayer::ProgressCallback& progress_;
  const SvfPlayer::StopCallback& stop_;
  bool device_attached_ = true;

  std::string_view text_;
  size_t position_ = 0;
  size_t line_ = 1;
  size_t reported_position_ = 0;

  std::string statement_keyword_;

  TapState current_ = TapState::kReset;
  TapState end_dr_ = TapState::kIdle;
  TapState end_ir_ = TapState::kIdle;

  StickyScan sticky_dr_;
  StickyScan sticky_ir_;

  SvfPlayResult result_;
};

// The shortest wait worth holding open on the host's side.
//
// Below this the file is asking for a handful of cycles between one register
// load and the next, which the cable has already spent more than by the time
// anything here could measure it. Above it, the wait is a flash erase or a
// programming pulse, where the number is a real duration and getting it short
// means writing a device that has not finished doing the last thing it was
// told to.
constexpr double kShortestEnforcedWaitSeconds = 0.001;

// How many hexadecimal digits of a scan a failure message carries. Wide
// enough that the bits either side of the one that disagreed are there to be
// compared, narrow enough to sit in a wizard's message box.
constexpr size_t kMessageHexDigits = 32;

// How much of the file to get through before saying so again. Every
// statement would be tens of thousands of calls into a user interface for
// one flash image; a sixty-fourth of a megabyte is a few hundred updates
// across the whole run, which is smoother than any bar can draw anyway.
constexpr size_t kProgressStepBytes = size_t{64} << 10;

SvfPlayResult Player::Play(std::string_view text) {
  text_ = text;
  position_ = 0;
  line_ = 1;

  if (logger_ != nullptr) {
    logger_->Info("Playing a JTAG programming file through the " +
                  std::string(cable_.Name()) + " cable");
  }

  // Start from Test-Logic-Reset, reached the way the standard says any state
  // reaches it: five clocks with TMS high. A file's first statement assumes
  // a known state and the cable has no idea what the last run left behind.
  const std::vector<uint8_t> tms(1, 0x1F);
  const std::vector<uint8_t> tdi(1, 0x00);
  if (!cable_.Shift(tms, tdi, 5, nullptr)) {
    Fail("The cable stopped answering.");
    return result_;
  }
  current_ = TapState::kReset;

  Statement statement;
  while (NextStatement(statement)) {
    if (statement.tokens.empty()) {
      break;
    }

    if (stop_ && stop_()) {
      cable_.Flush();
      result_.stopped = true;
      result_.line = statement.line;
      result_.problem = "Stopped before the end of the file.";
      return result_;
    }

    if (!Execute(statement)) {
      cable_.Flush();
      return result_;
    }
    ++result_.statements;

    if (progress_ && position_ - reported_position_ >= kProgressStepBytes) {
      reported_position_ = position_;
      progress_(position_, text_.size());
    }
  }

  if (!result_.problem.empty()) {
    return result_;
  }

  if (!cable_.Flush()) {
    Fail("The cable stopped answering.");
    return result_;
  }

  if (progress_) {
    progress_(text_.size(), text_.size());
  }

  result_.succeeded = true;
  return result_;
}

// Reads one statement, stopping at the semicolon that ends it. Returns false
// at the end of the file and on a malformed one, which are told apart by
// whether a problem was recorded.
bool Player::NextStatement(Statement& statement) {
  statement.tokens.clear();
  statement.line = line_;

  // Cleared here rather than left standing: a failure while *reading* the
  // next statement is not a failure in the last one that ran, and naming
  // that one would send a reader to the wrong line.
  statement_keyword_.clear();

  std::string word;

  // The line a statement is *on* is the line its first character is on, not
  // the line the reader happened to be at when it started looking.
  // Everything a user is told about a file names this number, so a statement
  // after a run of blank lines and comments has to name its own — and it has
  // to be noted as the character is read, because by the time a token is
  // finished the reader may have crossed a line ending to find that out.
  bool started = false;
  const auto note_line = [this, &statement, &started] {
    if (!started) {
      started = true;
      statement.line = line_;
    }
  };

  const auto finish_word = [&statement, &word] {
    if (!word.empty()) {
      statement.tokens.push_back(Token{word, false});
      word.clear();
    }
  };

  while (position_ < text_.size()) {
    const char character = text_[position_];

    if (character == '\n') {
      ++line_;
      ++position_;
      finish_word();
      continue;
    }

    if (static_cast<unsigned char>(character) <= ' ') {
      ++position_;
      finish_word();
      continue;
    }

    // Comments: SVF's own "!" to the end of the line, and the "//" that
    // every tool also accepts. Quartus writes the first kind, including in
    // the middle of a file, and one of them carries the checksum a reader
    // might want, so they are skipped rather than refused.
    if (character == '!') {
      while (position_ < text_.size() && text_[position_] != '\n') {
        ++position_;
      }
      finish_word();
      continue;
    }
    if (character == '/' && position_ + 1 < text_.size() &&
        text_[position_ + 1] == '/') {
      while (position_ < text_.size() && text_[position_] != '\n') {
        ++position_;
      }
      finish_word();
      continue;
    }

    if (character == ';') {
      ++position_;
      finish_word();
      if (statement.tokens.empty()) {
        // A stray semicolon. Nothing to do and nothing to complain about.
        started = false;
        continue;
      }
      return true;
    }

    if (character == '(') {
      note_line();
      finish_word();
      ++position_;
      std::string value;
      bool closed = false;
      while (position_ < text_.size()) {
        const char inner = text_[position_++];
        if (inner == ')') {
          closed = true;
          break;
        }
        if (inner == '\n') {
          ++line_;
          continue;
        }
        if (static_cast<unsigned char>(inner) <= ' ') {
          // Quartus wraps long values across lines and indents the
          // continuations, so a value's own whitespace means nothing.
          continue;
        }
        value.push_back(inner);
      }
      if (!closed) {
        FailAt(statement.line, "The file ends in the middle of a value.");
        return false;
      }
      statement.tokens.push_back(Token{value, true});
      continue;
    }

    note_line();
    word.push_back(
        static_cast<char>(std::toupper(static_cast<unsigned char>(character))));
    ++position_;
  }

  finish_word();
  if (!statement.tokens.empty()) {
    FailAt(statement.line,
           "The file ends in the middle of a statement, with no semicolon.");
    return false;
  }
  return false;
}

bool Player::Execute(const Statement& statement) {
  const std::string& command = statement.tokens.front().text;

  // Noted before anything is done with it, so that a failure inside any of
  // the calls below can say which kind of statement it was in without every
  // one of them having to be told.
  statement_keyword_ = command;

  if (command == "SDR") {
    return DoScan(statement, false);
  }
  if (command == "SIR") {
    return DoScan(statement, true);
  }
  if (command == "RUNTEST") {
    return DoRunTest(statement);
  }
  if (command == "STATE") {
    return DoState(statement);
  }
  if (command == "ENDDR") {
    return DoEndState(statement, end_dr_);
  }
  if (command == "ENDIR") {
    return DoEndState(statement, end_ir_);
  }
  if (command == "HIR" || command == "HDR" || command == "TIR" ||
      command == "TDR") {
    return DoHeaderOrTrailer(statement);
  }
  if (command == "FREQUENCY") {
    return DoFrequency(statement);
  }
  if (command == "TRST") {
    return DoTrst(statement);
  }

  return FailAt(statement.line,
                "The file uses \"" + command +
                    "\", which this application does not understand.");
}

bool Player::ParseScanFields(const Statement& statement, size_t first,
                             ScanFields& fields) {
  for (size_t index = first; index < statement.tokens.size(); ++index) {
    const Token& keyword = statement.tokens[index];
    if (keyword.bracketed || index + 1 >= statement.tokens.size()) {
      return FailAt(statement.line, "A scan in the file is malformed.");
    }
    const Token& value = statement.tokens[index + 1];
    if (!value.bracketed) {
      return FailAt(statement.line, "A scan in the file is malformed.");
    }
    ++index;

    if (keyword.text == "TDI") {
      fields.tdi = value.text;
    } else if (keyword.text == "TDO") {
      fields.tdo = value.text;
    } else if (keyword.text == "MASK") {
      fields.mask = value.text;
    } else if (keyword.text == "SMASK") {
      fields.smask = value.text;
    } else {
      return FailAt(statement.line, "A scan in the file names \"" +
                                        keyword.text +
                                        "\", which is not part of one.");
    }
  }
  return true;
}

bool Player::DoScan(const Statement& statement, bool instruction) {
  if (statement.tokens.size() < 2) {
    return FailAt(statement.line, "A scan in the file has no length.");
  }
  const std::optional<double> length = Number(statement.tokens[1]);
  if (!length.has_value() || *length < 0) {
    return FailAt(statement.line, "A scan in the file has no length.");
  }
  const size_t bit_count = static_cast<size_t>(*length);

  ScanFields fields;
  if (!ParseScanFields(statement, 2, fields)) {
    return false;
  }

  StickyScan& sticky = instruction ? sticky_ir_ : sticky_dr_;

  // A change of length throws the remembered values away: they are the wrong
  // width, and the format says a statement that changes the length has to
  // say what it is shifting.
  if (bit_count != sticky.bit_count) {
    if (!fields.tdi.has_value() && bit_count > 0) {
      return FailAt(statement.line,
                    "A scan in the file changes length without saying what to "
                    "shift.");
    }
    sticky.bit_count = bit_count;
    sticky.tdi.assign(BytesForBits(bit_count), 0);
    sticky.mask = AllBits(bit_count);
    sticky.smask = AllBits(bit_count);
  }

  std::string problem;
  if (fields.tdi.has_value() &&
      !HexToBits(*fields.tdi, bit_count, sticky.tdi, problem)) {
    return FailAt(statement.line, "Reading a scan failed: " + problem + ".");
  }
  if (fields.mask.has_value() &&
      !HexToBits(*fields.mask, bit_count, sticky.mask, problem)) {
    return FailAt(statement.line, "Reading a scan failed: " + problem + ".");
  }
  if (fields.smask.has_value() &&
      !HexToBits(*fields.smask, bit_count, sticky.smask, problem)) {
    return FailAt(statement.line, "Reading a scan failed: " + problem + ".");
  }

  std::vector<uint8_t> expected;
  if (fields.tdo.has_value() &&
      !HexToBits(*fields.tdo, bit_count, expected, problem)) {
    return FailAt(statement.line, "Reading a scan failed: " + problem + ".");
  }

  if (bit_count == 0) {
    return true;
  }

  if (!MoveTo(instruction ? TapState::kIrShift : TapState::kDrShift)) {
    return false;
  }

  const bool compare = fields.tdo.has_value() && device_attached_;

  std::vector<uint8_t> received;
  if (!Shift(sticky.tdi, bit_count, compare ? &received : nullptr)) {
    return false;
  }
  current_ = instruction ? TapState::kIrExit1 : TapState::kDrExit1;

  if (compare) {
    for (size_t index = 0; index < bit_count; ++index) {
      if (!BitAt(sticky.mask, index)) {
        continue;
      }
      if (BitAt(received, index) == BitAt(expected, index)) {
        continue;
      }

      // The one failure that means the hardware disagreed rather than the
      // file being wrong, so it is worth every detail it can carry: an
      // erase that did not take, a device that is not the one the file was
      // built for, and a cable reading a bit late all land here.
      //
      // Both windows cover the same range by construction, so the range is
      // named once. Without it the two values cannot be lined up against the
      // bit number, and a report of one of these is unreadable.
      const HexWindow said =
          BitsToHexAround(received, bit_count, index, kMessageHexDigits);
      const HexWindow wanted =
          BitsToHexAround(expected, bit_count, index, kMessageHexDigits);
      return FailAt(
          statement.line,
          "The device did not answer as the programming file expected at bit " +
              std::to_string(index) + " of a " + std::to_string(bit_count) +
              "-bit scan: across bits " + std::to_string(said.highest_bit) +
              " to " + std::to_string(said.lowest_bit) + " it said " +
              said.text + " where " + wanted.text + " was expected.");
    }
  }

  return MoveTo(instruction ? end_ir_ : end_dr_);
}

bool Player::DoRunTest(const Statement& statement) {
  TapState run_state = TapState::kIdle;
  std::optional<TapState> end_state;
  uint64_t clocks = 0;
  double seconds = 0;

  size_t index = 1;
  if (index < statement.tokens.size()) {
    const std::optional<TapState> named =
        StateNamed(statement.tokens[index].text);
    if (named.has_value()) {
      run_state = *named;
      ++index;
    }
  }

  while (index < statement.tokens.size()) {
    const std::string& token = statement.tokens[index].text;

    if (token == "ENDSTATE") {
      if (index + 1 >= statement.tokens.size()) {
        return FailAt(statement.line, "A wait in the file is malformed.");
      }
      end_state = StateNamed(statement.tokens[index + 1].text);
      if (!end_state.has_value()) {
        return FailAt(statement.line,
                      "A wait in the file names a state that does not exist.");
      }
      index += 2;
      continue;
    }

    if (token == "MAXIMUM") {
      // An upper bound on how long the wait may take. Nothing here can run
      // slower than the cable does, and the lower bound below is what
      // matters to a flash, so this is read past rather than enforced.
      index += 3;
      continue;
    }

    const std::optional<double> number = Number(statement.tokens[index]);
    if (!number.has_value() || index + 1 >= statement.tokens.size()) {
      return FailAt(statement.line, "A wait in the file is malformed.");
    }
    const std::string& unit = statement.tokens[index + 1].text;
    if (unit == "TCK") {
      clocks = static_cast<uint64_t>(*number);
    } else if (unit == "SEC") {
      seconds = *number;
    } else if (unit == "SCK") {
      return FailAt(statement.line,
                    "The file asks for cycles of a system clock, which this "
                    "cable does not have.");
    } else {
      return FailAt(statement.line, "A wait in the file is malformed.");
    }
    index += 2;
  }

  const auto began = std::chrono::steady_clock::now();

  if (!MoveTo(run_state)) {
    return false;
  }
  if (clocks > 0) {
    if (!cable_.RunClock(static_cast<size_t>(clocks))) {
      return Fail("The cable stopped answering.");
    }
    result_.run_clocks += clocks;
  }

  // How long this wait is *meant* to take, which is not the same question as
  // how many cycles it is.
  //
  // Quartus writes its waits as cycle counts worked out from the frequency
  // the file declares: the same provisioning file emitted for 4.5 MHz and for
  // 6 MHz differs only in that every count is a third larger in the second
  // one, and the erase at the front of it is 100 seconds either way. So the
  // counts mean seconds, and a cable clocking faster than the file's declared
  // rate would shorten every one of them — including the erase a flash needs
  // in milliseconds rather than in cycles.
  //
  // The cable's clock is its own and cannot be set from here, so the wait is
  // held open on this side instead: clock the cycles, then make sure the
  // intended time has passed before the next statement is sent. When the
  // cable is slower than the file declares — which is the ordinary case over
  // USB — this costs nothing at all, because the time has passed already.
  double intended = seconds;
  if (clocks > 0 && result_.frequency_hz > 0) {
    intended =
        std::max(intended, static_cast<double>(clocks) / result_.frequency_hz);
  }

  if (device_attached_ && intended >= kShortestEnforcedWaitSeconds) {
    // Flushed first so that the cycles are on their way before the clock is
    // read: what has to be true is that the *next* statement does not reach
    // the device early, and that is what waiting here guarantees.
    if (!cable_.Flush()) {
      return Fail("The cable stopped answering.");
    }
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - began)
            .count();
    if (elapsed < intended) {
      std::this_thread::sleep_for(
          std::chrono::duration<double>(intended - elapsed));
    }
  }

  return MoveTo(end_state.value_or(run_state));
}

bool Player::DoState(const Statement& statement) {
  if (statement.tokens.size() < 2) {
    return FailAt(statement.line, "A state change in the file names no state.");
  }
  for (size_t index = 1; index < statement.tokens.size(); ++index) {
    const std::optional<TapState> state =
        StateNamed(statement.tokens[index].text);
    if (!state.has_value()) {
      return FailAt(statement.line,
                    "The file names a state that does not exist: " +
                        statement.tokens[index].text + ".");
    }
    if (!MoveTo(*state)) {
      return false;
    }
  }
  return true;
}

bool Player::DoEndState(const Statement& statement, TapState& end_state) {
  if (statement.tokens.size() != 2) {
    return FailAt(statement.line, "An end state in the file is malformed.");
  }
  const std::optional<TapState> state = StateNamed(statement.tokens[1].text);
  if (!state.has_value() || !IsStableState(*state)) {
    return FailAt(statement.line,
                  "The file ends its scans in a state a scan cannot rest in.");
  }
  end_state = *state;
  return true;
}

bool Player::DoHeaderOrTrailer(const Statement& statement) {
  if (statement.tokens.size() < 2) {
    return FailAt(statement.line, "A header in the file has no length.");
  }
  const std::optional<double> length = Number(statement.tokens[1]);
  if (!length.has_value()) {
    return FailAt(statement.line, "A header in the file has no length.");
  }
  if (*length == 0) {
    return true;
  }

  // A non-empty header or trailer means there are other devices on the JTAG
  // chain, whose instruction and data registers this file expects to be
  // shifted through. This project's board has one device on the chain, and a
  // player that guessed at a longer one would be shifting a flash image into
  // whatever was actually there.
  return FailAt(statement.line,
                "This programming file is for a JTAG chain with more than one "
                "device on it, and this application drives a chain with one.");
}

bool Player::DoFrequency(const Statement& statement) {
  if (statement.tokens.size() < 2) {
    // "FREQUENCY;" with nothing after it means "no maximum", which is
    // exactly what a cable with a fixed clock offers anyway.
    result_.frequency_hz = 0;
    return true;
  }
  const std::optional<double> hertz = Number(statement.tokens[1]);
  if (!hertz.has_value()) {
    return FailAt(statement.line, "The file's clock rate is malformed.");
  }
  result_.frequency_hz = *hertz;
  return true;
}

bool Player::DoTrst(const Statement& statement) {
  if (statement.tokens.size() != 2) {
    return FailAt(statement.line,
                  "A reset line setting in the file is "
                  "malformed.");
  }
  const std::string& setting = statement.tokens[1].text;
  if (setting == "ON") {
    // The cable's connector carries four JTAG signals and no TRST, so a file
    // that wants the line driven cannot be played correctly. Refused rather
    // than ignored: a file asking for a reset it does not get is a file
    // whose later statements assume something that never happened.
    return FailAt(statement.line,
                  "This programming file drives a JTAG reset line, which this "
                  "cable does not have.");
  }
  if (setting != "OFF" && setting != "Z" && setting != "ABSENT") {
    return FailAt(statement.line,
                  "A reset line setting in the file is malformed.");
  }
  return true;
}

bool Player::MoveTo(TapState target) {
  const std::vector<bool> path = PathBetween(current_, target);
  if (path.empty()) {
    current_ = target;
    return true;
  }

  std::vector<uint8_t> tms(BytesForBits(path.size()), 0);
  const std::vector<uint8_t> tdi(BytesForBits(path.size()), 0);
  for (size_t index = 0; index < path.size(); ++index) {
    SetBitAt(tms, index, path[index]);
  }

  if (!cable_.Shift(tms, tdi, path.size(), nullptr)) {
    return Fail("The cable stopped answering.");
  }
  current_ = target;
  return true;
}

// Shift `bit_count` bits in the current shift state, leaving the TAP in the
// matching Exit1 state: the last bit is clocked with TMS high, which is how
// JTAG shifts the final bit and leaves at the same time.
bool Player::Shift(const std::vector<uint8_t>& tdi, size_t bit_count,
                   std::vector<uint8_t>* tdo) {
  std::vector<uint8_t> tms(BytesForBits(bit_count), 0);
  SetBitAt(tms, bit_count - 1, true);

  if (!cable_.Shift(tms, tdi, bit_count, tdo)) {
    return Fail("The cable stopped answering.");
  }
  result_.shifted_bits += bit_count;
  return true;
}

bool Player::Fail(const std::string& problem) { return FailAt(line_, problem); }

bool Player::FailAt(size_t line, const std::string& problem) {
  if (result_.problem.empty()) {
    result_.problem = problem;
    result_.line = line;
    result_.statement_keyword = statement_keyword_;
    if (logger_ != nullptr) {
      logger_->Error("JTAG programming failed at line " + std::to_string(line) +
                     ": " + problem);
    }
  }
  return false;
}

std::optional<double> Player::Number(const Token& token) {
  if (token.bracketed || token.text.empty()) {
    return std::nullopt;
  }
  char* end = nullptr;
  const double value = std::strtod(token.text.c_str(), &end);
  if (end == nullptr || *end != '\0') {
    return std::nullopt;
  }
  return value;
}

}  // namespace

const char* TapStateName(TapState state) {
  switch (state) {
    case TapState::kReset:
      return "RESET";
    case TapState::kIdle:
      return "IDLE";
    case TapState::kDrSelect:
      return "DRSELECT";
    case TapState::kDrCapture:
      return "DRCAPTURE";
    case TapState::kDrShift:
      return "DRSHIFT";
    case TapState::kDrExit1:
      return "DREXIT1";
    case TapState::kDrPause:
      return "DRPAUSE";
    case TapState::kDrExit2:
      return "DREXIT2";
    case TapState::kDrUpdate:
      return "DRUPDATE";
    case TapState::kIrSelect:
      return "IRSELECT";
    case TapState::kIrCapture:
      return "IRCAPTURE";
    case TapState::kIrShift:
      return "IRSHIFT";
    case TapState::kIrExit1:
      return "IREXIT1";
    case TapState::kIrPause:
      return "IRPAUSE";
    case TapState::kIrExit2:
      return "IREXIT2";
    case TapState::kIrUpdate:
      return "IRUPDATE";
  }
  return "RESET";
}

SvfPlayer::SvfPlayer(IJtagCable& cable, ILogger* logger)
    : cable_(cable), logger_(logger) {}

SvfPlayResult SvfPlayer::Play(std::string_view text) {
  Player player(cable_, logger_, progress_, stop_, device_attached_);
  return player.Play(text);
}

}  // namespace ddd::capture
