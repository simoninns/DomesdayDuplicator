/************************************************************************

    memory_lock.h

    Pinning capture buffers into physical memory
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <string>

namespace ddd::capture {

// Locks a memory region so the operating system cannot page it out.
//
// A page fault taken while a USB transfer is completing is a stall of
// unbounded length in a pipeline whose entire slack is the host-side queue —
// the device has 64 KB of buffer and no way to wait. Locking the disk buffers
// removes that failure mode.
//
// Every one of these calls can legitimately fail: an unprivileged process has a
// bounded locked-memory allowance on both Linux (RLIMIT_MEMLOCK) and Windows
// (the working set), and a machine may simply not permit it. That is why the
// return value is a report rather than a precondition — a capture that runs
// unlocked is a capture that might glitch, not one that cannot be attempted,
// and the caller logs the degradation and continues.
//
// Thread-safety: safe to call from any thread. The Windows working-set
// accounting these calls share is serialised internally, and it is touched at
// capture start and stop only, never on the transfer path.

struct MemoryLockResult {
  bool locked = false;

  // Populated when locked is false. Says what the platform refused and why, in
  // terms a log line can carry straight through.
  std::string message;
};

// Pin a region. Sizes are rounded up to whole pages by the platform.
MemoryLockResult LockMemoryRegion(void* base_address, size_t size_in_bytes);

// Release a region previously passed to LockMemoryRegion with the same address
// and size. Failures are not reported: by the time this runs the capture is
// over, and there is nothing a caller could usefully do about it.
void UnlockMemoryRegion(void* base_address, size_t size_in_bytes);

}  // namespace ddd::capture
