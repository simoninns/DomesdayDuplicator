/************************************************************************

    memory_lock.cpp

    Pinning capture buffers into physical memory
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "memory_lock.h"

#include <mutex>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <memoryapi.h>
#else
#include <sys/mman.h>

#include <cerrno>
#include <cstring>
#endif

namespace ddd::capture {
namespace {

#ifdef _WIN32

// Windows will not let a process lock more than its working set allows, so the
// working set has to be grown first. That is a process-wide setting, which
// makes this bookkeeping process-wide too: each lock adds to the total and each
// unlock takes it back off, and the mutex is what keeps two concurrent locks
// from computing the new total from the same stale figure.
//
// Only touched at capture start and stop. It is never on the transfer path.
std::mutex& WorkingSetMutex() {
  static std::mutex mutex;
  return mutex;
}

size_t g_locked_bytes = 0;
size_t g_locked_regions = 0;
bool g_have_baseline = false;
size_t g_baseline_minimum = 0;
size_t g_baseline_maximum = 0;

size_t SystemPageSize() {
  SYSTEM_INFO system_info;
  GetSystemInfo(&system_info);
  return static_cast<size_t>(system_info.dwPageSize);
}

// Memory allocations may straddle page boundaries, so each locked region is
// allowed two extra pages of headroom in the working set.
bool ApplyWorkingSetSize(size_t locked_bytes, size_t locked_regions,
                         std::string& message) {
  const size_t increase =
      locked_bytes + (SystemPageSize() * 2 * (locked_regions + 1));
  if (SetProcessWorkingSetSize(GetCurrentProcess(),
                               g_baseline_minimum + increase,
                               g_baseline_maximum + increase) == 0) {
    message = "SetProcessWorkingSetSize failed with error code " +
              std::to_string(GetLastError());
    return false;
  }
  return true;
}

#endif

}  // namespace

MemoryLockResult LockMemoryRegion(void* base_address, size_t size_in_bytes) {
  MemoryLockResult result;

  if (base_address == nullptr || size_in_bytes == 0) {
    result.message = "Nothing to lock";
    return result;
  }

#ifdef _WIN32
  const std::lock_guard<std::mutex> guard(WorkingSetMutex());

  if (!g_have_baseline) {
    if (GetProcessWorkingSetSize(GetCurrentProcess(), &g_baseline_minimum,
                                 &g_baseline_maximum) == 0) {
      result.message = "GetProcessWorkingSetSize failed with error code " +
                       std::to_string(GetLastError());
      return result;
    }
    g_have_baseline = true;
  }

  if (!ApplyWorkingSetSize(g_locked_bytes + size_in_bytes, g_locked_regions + 1,
                           result.message)) {
    return result;
  }

  if (VirtualLock(base_address, size_in_bytes) == 0) {
    result.message =
        "VirtualLock failed with error code " + std::to_string(GetLastError());
    return result;
  }

  g_locked_bytes += size_in_bytes;
  ++g_locked_regions;
#else
  if (mlock(base_address, size_in_bytes) != 0) {
    // ENOMEM here is almost always RLIMIT_MEMLOCK rather than a machine out of
    // memory, and saying so saves the reader a diagnostic step: the fix is a
    // limits change, not more RAM.
    const int error_number = errno;
    result.message =
        std::string("mlock failed: ") + std::strerror(error_number);
    if (error_number == ENOMEM) {
      result.message +=
          " (the locked-memory limit, RLIMIT_MEMLOCK, is most likely too low "
          "for the configured queue size)";
    }
    return result;
  }
#endif

  result.locked = true;
  return result;
}

void UnlockMemoryRegion(void* base_address, size_t size_in_bytes) {
  if (base_address == nullptr || size_in_bytes == 0) {
    return;
  }

#ifdef _WIN32
  const std::lock_guard<std::mutex> guard(WorkingSetMutex());

  VirtualUnlock(base_address, size_in_bytes);

  if (g_locked_bytes >= size_in_bytes && g_locked_regions > 0) {
    g_locked_bytes -= size_in_bytes;
    --g_locked_regions;
  }

  std::string ignored;
  ApplyWorkingSetSize(g_locked_bytes, g_locked_regions, ignored);
#else
  munlock(base_address, size_in_bytes);
#endif
}

}  // namespace ddd::capture
