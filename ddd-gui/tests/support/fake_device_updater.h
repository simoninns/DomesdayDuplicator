/************************************************************************

    fake_device_updater.h

    A device that can be updated, with no device in it
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "device_updater.h"
#include "digest.h"
#include "wire_protocol.h"

namespace ddd::capture {

// An IDeviceUpdater that models the firmware's side of the update protocol
// closely enough to drive the whole flow, and can be told to fail at any
// point in it.
//
// This is what makes the update path testable at all. With it, "the transfer
// is corrupted", "the device cannot write its flash", "the readback does not
// match", "the device never comes back" and "the device comes back running
// the wrong build" are things a test can simply state, in milliseconds, on a
// machine with nothing attached. Without them, every one of those is a
// deliberate act of sabotage on real hardware — and half of them cannot be
// arranged on real hardware at all.
//
// It keeps what was written, so a test can check that the bytes that arrived
// are the bytes that were sent, which is the property the whole chain
// exists to preserve.
class FakeDeviceUpdater : public IDeviceUpdater {
 public:
  // Where the fake should stop behaving.
  enum class Fault {
    kNone,

    // The device is not reachable at all — no firmware with an update agent
    // in it, or an unplugged cable.
    kNoStatus,

    // UPDATE_BEGIN is refused. The reason is whatever failure_error says.
    kRefuseBegin,

    // A chunk is refused, at chunk index fail_at_chunk.
    kRefuseChunk,

    // UPDATE_FINISH is refused.
    kRefuseFinish,

    // The device reports a failure while writing or verifying, with
    // failure_error as the reason.
    kFailDuringWrite,

    // The device stops answering part way through writing.
    kVanishDuringWrite,

    // The device never re-enumerates after the reset.
    kNeverReturns,

    // The device comes back running something other than what the manifest
    // said it would.
    kWrongIdentityAfterUpdate,
  };

  FakeDeviceUpdater() {
    identity_.product_string = "Domesday Duplicator (0123abcd)";
    identity_.protocol_version = 1;
    identity_.gateware_present = true;
    identity_.register_map_version = kRegisterMapVersionWithImageRole;
    identity_.image_role = kImageRoleApplication;
    identity_.gateware_commit = "0123abcd";

    // The chunk size is answerable before anything has been started, because
    // that is how the host discovers it rather than assuming one.
    status_.maximum_chunk_bytes = maximum_chunk_bytes_;
  }

  // --- IDeviceUpdater -------------------------------------------------------

  std::optional<DeviceIdentity> ReadIdentity() override {
    if (fault_ == Fault::kNoStatus) {
      return std::nullopt;
    }
    return identity_;
  }

  std::optional<DeviceUpdateStatus> ReadStatus() override {
    ++status_reads_;

    if (fault_ == Fault::kNoStatus) {
      return std::nullopt;
    }
    if (fault_ == Fault::kVanishDuringWrite &&
        status_.phase == UpdatePhase::kWriting) {
      return std::nullopt;
    }

    // The phase this poll sees, and only then the work that moves it on. A
    // real device is slower; a test that had to wait for one would not be
    // run.
    //
    // The order matters. Advancing first would step straight past the
    // writing phase and no poll would ever observe it, so a host that failed
    // to render that phase would pass. The FX3 target happens not to have a
    // long one — it writes each chunk to its EEPROM as the chunk arrives —
    // but the EPCS target will, and this fake is the general device.
    const DeviceUpdateStatus seen = status_;
    Advance();

    return seen;
  }

  bool Begin(UpdateTarget target, uint64_t length,
             const Sha256Digest& digest) override {
    ++begin_count_;

    if (fault_ == Fault::kRefuseBegin) {
      status_.phase = UpdatePhase::kFailed;
      status_.error = failure_error_;
      return false;
    }

    target_ = target;
    expected_length_ = length;
    expected_digest_ = digest;
    received_.clear();
    next_chunk_ = 0;

    status_ = DeviceUpdateStatus{};
    status_.phase = UpdatePhase::kReceiving;
    status_.maximum_chunk_bytes = maximum_chunk_bytes_;
    return true;
  }

  bool SendChunk(UpdateTarget target, uint16_t index,
                 std::span<const uint8_t> data) override {
    if (fault_ == Fault::kRefuseChunk && index == fail_at_chunk_) {
      status_.phase = UpdatePhase::kFailed;
      status_.error = failure_error_;
      return false;
    }

    if (target != target_ || index != next_chunk_) {
      status_.phase = UpdatePhase::kFailed;
      status_.error = DeviceUpdateError::kSequence;
      return false;
    }

    // Every chunk but the last is a whole number of the target medium's
    // pages, which is the firmware's rule and therefore a rule this fake
    // enforces — a host that broke it would otherwise pass here and fail on
    // hardware. The EPCS's page is four times the EEPROM's, so a chunk that
    // is legal for one target is not automatically legal for the other.
    const uint64_t page = target == UpdateTarget::kGateware ? 256 : 64;
    const uint64_t remaining = expected_length_ - received_.size();
    if (data.size() < remaining && (data.size() % page) != 0) {
      status_.phase = UpdatePhase::kFailed;
      status_.error = DeviceUpdateError::kChunk;
      return false;
    }

    if (data.size() > remaining) {
      status_.phase = UpdatePhase::kFailed;
      status_.error = DeviceUpdateError::kOverrun;
      return false;
    }

    received_.insert(received_.end(), data.begin(), data.end());
    ++next_chunk_;
    status_.bytes_received = static_cast<uint32_t>(received_.size());
    ++chunk_count_;
    largest_chunk_ = std::max(largest_chunk_, data.size());
    return true;
  }

  bool Finish(UpdateTarget target) override {
    if (fault_ == Fault::kRefuseFinish) {
      status_.phase = UpdatePhase::kFailed;
      status_.error = failure_error_;
      return false;
    }

    if (target != target_ || received_.size() != expected_length_) {
      status_.phase = UpdatePhase::kFailed;
      status_.error = DeviceUpdateError::kShort;
      return false;
    }

    // Integrity link 5, modelled rather than mimed: what arrived is hashed
    // and compared against what UPDATE_BEGIN promised, so a test that
    // corrupts the stream fails here exactly as the device would.
    if (Sha256(std::span<const uint8_t>(received_)) != expected_digest_) {
      status_.phase = UpdatePhase::kFailed;
      status_.error = DeviceUpdateError::kStreamDigest;
      return false;
    }

    // Kept per target, because a bundle carrying both components opens a
    // second transfer that would otherwise overwrite the record of the
    // first — and "did the right bytes reach the right half of the device"
    // is the question these tests exist to answer.
    committed_[target] = received_;

    status_.phase = UpdatePhase::kWriting;
    return true;
  }

  bool Reset() override {
    ++reset_count_;
    return true;
  }

  bool ReconfigureFpga() override {
    ++reconfigure_count_;
    return reconfigure_succeeds_;
  }

  std::optional<DeviceIdentity> WaitForReturn(
      std::chrono::milliseconds) override {
    ++wait_count_;

    if (fault_ == Fault::kNeverReturns) {
      return std::nullopt;
    }

    if (fault_ == Fault::kWrongIdentityAfterUpdate) {
      DeviceIdentity wrong = identity_;
      wrong.product_string = "Domesday Duplicator (deadbeef)";
      wrong.gateware_commit = "deadbeef";
      return wrong;
    }

    return identity_after_update_.value_or(identity_);
  }

  // --- What the test decides ------------------------------------------------

  void SetFault(Fault fault) { fault_ = fault; }
  void SetFailureError(DeviceUpdateError error) { failure_error_ = error; }
  void SetFailAtChunk(uint16_t index) { fail_at_chunk_ = index; }
  void SetMaximumChunkBytes(uint16_t bytes) {
    maximum_chunk_bytes_ = bytes;
    status_.maximum_chunk_bytes = bytes;
  }
  void SetReconfigureSucceeds(bool succeeds) {
    reconfigure_succeeds_ = succeeds;
  }

  void SetIdentity(const DeviceIdentity& identity) { identity_ = identity; }

  // What the device will report once it has restarted, when that differs
  // from what it reports now — which is the ordinary case for an update
  // that worked.
  void SetIdentityAfterUpdate(const DeviceIdentity& identity) {
    identity_after_update_ = identity;
  }

  // --- What the test can check ----------------------------------------------

  const std::vector<uint8_t>& received() const { return received_; }

  // What was committed to one target, which survives a later transfer to
  // the other one. Empty if that target was never finished.
  const std::vector<uint8_t>& received(UpdateTarget target) const {
    static const std::vector<uint8_t> nothing;
    const auto found = committed_.find(target);
    return found == committed_.end() ? nothing : found->second;
  }
  uint64_t begin_count() const { return begin_count_; }
  uint64_t chunk_count() const { return chunk_count_; }
  uint64_t reset_count() const { return reset_count_; }
  uint64_t reconfigure_count() const { return reconfigure_count_; }
  uint64_t wait_count() const { return wait_count_; }
  uint64_t status_reads() const { return status_reads_; }
  size_t largest_chunk() const { return largest_chunk_; }
  UpdateTarget target() const { return target_; }

 private:
  // One poll's worth of the device getting on with it.
  void Advance() {
    if (status_.phase == UpdatePhase::kWriting) {
      if (fault_ == Fault::kFailDuringWrite) {
        status_.phase = UpdatePhase::kFailed;
        status_.error = failure_error_;
        return;
      }

      status_.bytes_written = static_cast<uint32_t>(expected_length_);
      status_.phase = UpdatePhase::kVerifying;
      return;
    }

    if (status_.phase == UpdatePhase::kVerifying) {
      status_.bytes_verified = static_cast<uint32_t>(expected_length_);
      status_.phase = UpdatePhase::kComplete;
    }
  }

  Fault fault_ = Fault::kNone;
  DeviceUpdateError failure_error_ = DeviceUpdateError::kWrite;
  uint16_t fail_at_chunk_ = 0;
  uint16_t maximum_chunk_bytes_ = 2048;
  bool reconfigure_succeeds_ = true;

  DeviceIdentity identity_;
  std::optional<DeviceIdentity> identity_after_update_;

  DeviceUpdateStatus status_{};
  UpdateTarget target_ = UpdateTarget::kFirmware;
  uint64_t expected_length_ = 0;
  Sha256Digest expected_digest_{};
  std::vector<uint8_t> received_;
  std::map<UpdateTarget, std::vector<uint8_t>> committed_;
  uint16_t next_chunk_ = 0;

  uint64_t begin_count_ = 0;
  uint64_t chunk_count_ = 0;
  uint64_t reset_count_ = 0;
  uint64_t reconfigure_count_ = 0;
  uint64_t wait_count_ = 0;
  uint64_t status_reads_ = 0;
  size_t largest_chunk_ = 0;
};

}  // namespace ddd::capture
