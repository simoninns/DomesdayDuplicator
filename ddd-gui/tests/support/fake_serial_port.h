/************************************************************************

    fake_serial_port.h

    A serial port with no serial in it
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "serial_port.h"

namespace ddd::player {

// A port that answers whatever the test says a player would.
//
// This is the piece that makes the protocol testable at all. With it, "a player
// is listening at 1200 baud", "something that is not a player is on this port",
// "the player answers a quarter of a second too late" and "the cable is pulled
// mid-command" are all things a test can simply state, on a machine with
// nothing attached, in microseconds.
//
// It carries the clock as well as the bytes. A read that finds nothing pending
// advances the clock by exactly the timeout it was asked to wait, so a session
// driven by this clock times out deterministically and instantly — the timeout
// logic is genuinely exercised rather than waited out or stubbed away.
//
// Thread-safety: every method, including the clock. The protocol's own tests
// drive it from one thread, but the Qt layer's tests drive it from the player
// worker's thread while the test inspects it from the main one — which is what
// the real thing does, so it is what the fake has to allow.
class FakeSerialPort : public ISerialPort {
 public:
  // A player at `baud_rate` that answers `request` — terminator included — with
  // `reply`.
  void AddResponse(uint32_t baud_rate, std::string request, std::string reply) {
    const std::lock_guard<std::mutex> guard(mutex_);
    responses_[{baud_rate, std::move(request)}] = std::move(reply);
  }

  // The Pioneer model request, answered with `model_reply` (without its
  // terminator) at one rate. The shorthand almost every test wants.
  void AddPioneerPlayer(uint32_t baud_rate, const std::string& model_reply) {
    AddResponse(baud_rate, "?X\r", model_reply + "\r");
  }

  // Answers to the three queries a status poll makes, so a test that wants a
  // connected player watching a disc does not have to spell out the protocol.
  void AddStatusResponses(uint32_t baud_rate, const std::string& active_mode,
                          const std::string& disc_status,
                          const std::string& address) {
    AddResponse(baud_rate, "?P\r", active_mode + "\r");
    AddResponse(baud_rate, "?D\r", disc_status + "\r");
    AddResponse(baud_rate, "?F\r", address + "\r");
  }

  // Every open fails, as a busy or absent port does.
  void set_open_fails(bool fails) {
    const std::lock_guard<std::mutex> guard(mutex_);
    open_fails_ = fails;
  }

  // The one path that exists. Any other fails to open, as a port that is not
  // there does — which is what makes "the user pointed the application at a
  // port with nothing on it" a thing a test can state.
  void set_only_path(std::string path) {
    const std::lock_guard<std::mutex> guard(mutex_);
    only_path_ = std::move(path);
  }

  // The link dies on the nth write, counting from one. Zero never fails.
  void set_failing_write(int write_number) {
    const std::lock_guard<std::mutex> guard(mutex_);
    failing_write_ = write_number;
  }

  // The link dies on the nth read, counting from one. Zero never fails.
  void set_failing_read(int read_number) {
    const std::lock_guard<std::mutex> guard(mutex_);
    failing_read_ = read_number;
  }

  // Every write from now on fails, which is what a cable pulled out of a
  // connected player looks like.
  void set_link_broken(bool broken) {
    const std::lock_guard<std::mutex> guard(mutex_);
    link_broken_ = broken;
  }

  // The player answers, but this many reads too late — so with the session's
  // one-read-per-deadline budget, one is enough to miss it.
  void set_late_by_reads(int reads) {
    const std::lock_guard<std::mutex> guard(mutex_);
    late_by_reads_ = reads;
  }

  // Deliver replies this many bytes at a time, so a session that has to read
  // more than once to see a complete reply is exercised.
  void set_chunk_size(size_t bytes) {
    const std::lock_guard<std::mutex> guard(mutex_);
    chunk_size_ = bytes;
  }

  // Copies rather than references, because the thread that asks is not the
  // thread that fills these in.
  std::vector<std::string> writes() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return writes_;
  }

  std::vector<uint32_t> opened_rates() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return opened_rates_;
  }

  uint32_t baud_rate() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return settings_.baud_rate;
  }

  int open_count() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return open_count_;
  }

  int close_count() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return close_count_;
  }

  std::chrono::steady_clock::time_point now() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return now_;
  }

  // The clock a session should be given to drive this port.
  auto clock() {
    return [this] { return now(); };
  }

  // --- ISerialPort --------------------------------------------------------

  bool Open(const std::string& path, const SerialSettings& settings) override {
    const std::lock_guard<std::mutex> guard(mutex_);
    if (open_fails_ || (!only_path_.empty() && path != only_path_)) {
      return false;
    }

    path_ = path;
    settings_ = settings;
    pending_.clear();
    is_open_ = true;
    ++open_count_;
    opened_rates_.push_back(settings.baud_rate);
    return true;
  }

  void Close() override {
    const std::lock_guard<std::mutex> guard(mutex_);
    if (is_open_) {
      ++close_count_;
    }
    is_open_ = false;
    pending_.clear();
  }

  bool IsOpen() const override {
    const std::lock_guard<std::mutex> guard(mutex_);
    return is_open_;
  }

  void DiscardBuffers() override {
    const std::lock_guard<std::mutex> guard(mutex_);
    pending_.clear();
  }

  bool Write(std::string_view bytes) override {
    const std::lock_guard<std::mutex> guard(mutex_);
    ++write_count_;
    if (link_broken_ ||
        (failing_write_ != 0 && write_count_ >= failing_write_)) {
      return false;
    }

    writes_.emplace_back(bytes);

    const auto found =
        responses_.find({settings_.baud_rate, std::string(bytes)});
    if (found != responses_.end()) {
      pending_ += found->second;
    }

    return true;
  }

  bool Read(std::string& into, std::chrono::milliseconds timeout) override {
    const std::lock_guard<std::mutex> guard(mutex_);
    ++read_count_;
    if (aborted_ || link_broken_ ||
        (failing_read_ != 0 && read_count_ >= failing_read_)) {
      return false;
    }

    // Nothing to give, or the answer is deliberately arriving too late: the
    // caller waits out its whole timeout, which is what the interface's
    // contract says a read with nothing to deliver does.
    if (pending_.empty() || late_by_reads_ > 0) {
      if (late_by_reads_ > 0) {
        --late_by_reads_;
      }
      now_ += timeout;
      return true;
    }

    const size_t taken = std::min(chunk_size_, pending_.size());
    into.append(pending_, 0, taken);
    pending_.erase(0, taken);
    return true;
  }

  void RequestAbort() override {
    const std::lock_guard<std::mutex> guard(mutex_);
    aborted_ = true;
  }

 private:
  mutable std::mutex mutex_;

  std::map<std::pair<uint32_t, std::string>, std::string> responses_;

  std::string path_;
  std::string only_path_;
  SerialSettings settings_;
  bool is_open_ = false;
  bool open_fails_ = false;
  bool link_broken_ = false;
  bool aborted_ = false;

  std::string pending_;
  size_t chunk_size_ = std::string::npos;

  int write_count_ = 0;
  int read_count_ = 0;
  int failing_write_ = 0;
  int failing_read_ = 0;
  int late_by_reads_ = 0;

  int open_count_ = 0;
  int close_count_ = 0;
  std::vector<uint32_t> opened_rates_;
  std::vector<std::string> writes_;

  std::chrono::steady_clock::time_point now_ =
      std::chrono::steady_clock::time_point{};
};

// An ISerialPort that forwards to one somebody else owns.
//
// The player worker builds its own port from a factory and owns it, which is
// right — but a test needs to keep hold of the fake to script it and inspect
// it. This is the adapter that lets both be true: the factory hands the worker
// one of these, and the test keeps the FakeSerialPort.
class BorrowedSerialPort : public ISerialPort {
 public:
  explicit BorrowedSerialPort(ISerialPort* port) : port_(port) {}

  bool Open(const std::string& path, const SerialSettings& settings) override {
    return port_->Open(path, settings);
  }
  void Close() override { port_->Close(); }
  bool IsOpen() const override { return port_->IsOpen(); }
  void DiscardBuffers() override { port_->DiscardBuffers(); }
  bool Write(std::string_view bytes) override { return port_->Write(bytes); }
  bool Read(std::string& into, std::chrono::milliseconds timeout) override {
    return port_->Read(into, timeout);
  }
  void RequestAbort() override { port_->RequestAbort(); }

 private:
  ISerialPort* port_ = nullptr;
};

}  // namespace ddd::player
