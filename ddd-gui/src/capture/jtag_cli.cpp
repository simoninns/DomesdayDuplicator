/************************************************************************

    jtag_cli.cpp

    ddd-jtag: playing a programming file through the cable from a shell
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "jtag_cli.h"

#include <chrono>
#include <fstream>
#include <memory>
#include <ostream>
#include <string>

#include "jtag_cable.h"
#include "logger.h"
#include "svf_player.h"

namespace ddd::capture {
namespace {

// Read a whole file as text. Returns false for anything that is not a
// readable regular file, which is as much detail as a caller needs: the next
// thing it says is "that file could not be read".
bool ReadWholeFile(const std::string& path, std::string& out) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) {
    return false;
  }

  const std::streamoff size = file.tellg();
  if (size < 0) {
    return false;
  }
  file.seekg(0, std::ios::beg);

  out.resize(static_cast<size_t>(size));
  if (size > 0 && !file.read(out.data(), static_cast<std::streamsize>(size))) {
    return false;
  }
  return true;
}

// A logger that writes to a stream, so the tool says what the engine says
// rather than swallowing it.
class StreamLogger : public ILogger {
 public:
  explicit StreamLogger(std::ostream& out) : out_(out) {}

  void Log(LogLevel level, std::string_view message) override {
    if (level == LogLevel::kDebug) {
      return;
    }
    out_ << message << "\n";
  }

 private:
  std::ostream& out_;
};

std::string Rounded(double value, int places) {
  std::string text = std::to_string(value);
  const size_t point = text.find('.');
  if (point == std::string::npos) {
    return text;
  }
  text.resize(places > 0 ? point + 1 + static_cast<size_t>(places) : point);
  return text;
}

}  // namespace

std::string JtagCliUsage() {
  return "ddd-jtag — play a JTAG programming file through the DE0-Nano's "
         "on-board cable\n"
         "\n"
         "Usage:\n"
         "  ddd-jtag [options] <file.svf>\n"
         "\n"
         "Options:\n"
         "  --dry-run             Read the file and work out every vector it "
         "asks for,\n"
         "                        without a cable and without touching any "
         "hardware\n"
         "  --help                Show this text\n"
         "\n"
         "Without --dry-run this writes the FPGA's configuration flash, which "
         "is a\n"
         "deliberate manual act: it is never run automatically, and it needs "
         "the\n"
         "DE0-Nano's mini-USB connector cabled to this machine. The file comes "
         "from the\n"
         "bitstream build, which emits one beside the .jic it has always "
         "produced.\n"
         "\n"
         "Exit codes: 0 success, 2 usage, 3 file, 4 no cable, 5 programming "
         "failed.\n";
}

JtagCliOptions ParseJtagCliOptions(const std::vector<std::string>& args) {
  JtagCliOptions options;

  for (const std::string& argument : args) {
    if (argument == "--help" || argument == "-h") {
      options.show_help = true;
      return options;
    }

    if (argument == "--dry-run") {
      options.dry_run = true;
      continue;
    }

    if (argument.rfind("--", 0) == 0) {
      options.problem = "Unknown option: " + argument;
      return options;
    }

    if (!options.svf_path.empty()) {
      options.problem = "Only one programming file can be played at a time.";
      return options;
    }
    options.svf_path = argument;
  }

  if (options.svf_path.empty()) {
    options.problem = "No programming file was given.";
  }
  return options;
}

int RunJtagCli(const std::vector<std::string>& args, std::ostream& out,
               std::ostream& error) {
  const JtagCliOptions options = ParseJtagCliOptions(args);

  if (options.show_help) {
    out << JtagCliUsage();
    return kJtagCliSuccess;
  }

  if (!options.problem.empty()) {
    error << options.problem << "\n\n" << JtagCliUsage();
    return kJtagCliUsage;
  }

  std::string text;
  if (!ReadWholeFile(options.svf_path, text)) {
    error << "Could not read " << options.svf_path << "\n";
    return kJtagCliFile;
  }

  StreamLogger logger(out);

  // The cable, or the one that goes nowhere. Chosen here and nowhere else:
  // everything below this line is identical in both modes, which is what
  // makes a dry run worth trusting as a check of the real one.
  std::unique_ptr<IJtagCable> cable;
  NullJtagCable nothing;
  IJtagCable* target = &nothing;
  if (!options.dry_run) {
    cable = MakeUsbBlasterCable(&logger);
    if (cable == nullptr) {
      return kJtagCliNoCable;
    }
    target = cable.get();
  }

  out << (options.dry_run ? "Reading " : "Programming from ")
      << options.svf_path << " (" << text.size() << " bytes)\n";

  SvfPlayer player(*target, &logger);

  // A dry run has nothing on the far end: no answers to compare against, and
  // no flash to wait for. What is exercised is everything this application
  // is responsible for.
  player.SetDeviceAttached(!options.dry_run);

  int last_percent = -1;
  player.SetProgressCallback([&out, &last_percent](size_t done, size_t total) {
    if (total == 0) {
      return;
    }
    const int percent = static_cast<int>((done * 100) / total);
    if (percent == last_percent) {
      return;
    }
    last_percent = percent;
    out << "  " << percent << "%\r" << std::flush;
  });

  const auto started = std::chrono::steady_clock::now();
  const SvfPlayResult result = player.Play(text);
  const double seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - started)
          .count();

  out << "\n";

  if (!result.succeeded) {
    error << (result.stopped ? "Stopped" : "Failed") << " at line "
          << result.line << ": " << result.problem << "\n";
    return result.statements == 0 && !result.stopped ? kJtagCliFile
                                                     : kJtagCliFailed;
  }

  // The numbers a bench run exists to produce: what the file asked for, and
  // how long it took to ask for it.
  out << "Played " << result.statements
      << " statements: " << result.shifted_bits << " bits shifted, "
      << result.run_clocks << " idle clocks";
  if (result.frequency_hz > 0) {
    out << ", file asks for at most " << Rounded(result.frequency_hz / 1e6, 2)
        << " MHz";
  }
  out << "\n";
  out << "Took " << Rounded(seconds, 1) << " seconds through the "
      << target->Name() << " cable\n";

  return kJtagCliSuccess;
}

}  // namespace ddd::capture
