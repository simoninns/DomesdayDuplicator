/************************************************************************

    thread_priority.cpp

    Raising the capture threads above ordinary scheduling
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "thread_priority.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <pthread.h>
#include <sched.h>
#endif

namespace ddd::capture {

#ifdef _WIN32

struct ScopedThreadPriority::Saved {
  int original_priority = 0;
  bool valid = false;
};

ScopedThreadPriority::ScopedThreadPriority()
    : saved_(std::make_unique<Saved>()) {
  const HANDLE thread = GetCurrentThread();

  const int original = GetThreadPriority(thread);
  if (original == THREAD_PRIORITY_ERROR_RETURN) {
    message_ = "GetThreadPriority failed with error code " +
               std::to_string(GetLastError());
    return;
  }

  if (SetThreadPriority(thread, THREAD_PRIORITY_TIME_CRITICAL) == 0) {
    message_ = "SetThreadPriority failed with error code " +
               std::to_string(GetLastError());
    return;
  }

  saved_->original_priority = original;
  saved_->valid = true;
  raised_ = true;
  message_ = "Thread priority raised to time-critical (was " +
             std::to_string(original) + ")";
}

ScopedThreadPriority::~ScopedThreadPriority() {
  if (saved_->valid) {
    SetThreadPriority(GetCurrentThread(), saved_->original_priority);
  }
}

#else

struct ScopedThreadPriority::Saved {
  int policy = 0;
  sched_param parameters{};
  bool valid = false;
};

ScopedThreadPriority::ScopedThreadPriority()
    : saved_(std::make_unique<Saved>()) {
  if (pthread_getschedparam(pthread_self(), &saved_->policy,
                            &saved_->parameters) != 0) {
    message_ = "pthread_getschedparam failed; leaving thread priority alone";
    return;
  }

  const int target_policy = SCHED_RR;
  const int minimum = sched_get_priority_min(target_policy);
  const int maximum = sched_get_priority_max(target_policy);

  sched_param requested{};
  if (minimum == -1 || maximum == -1) {
    requested.sched_priority = 0;
  } else {
    // About three quarters of the way through the range: high enough to
    // pre-empt ordinary work, low enough to leave headroom above for anything
    // the system genuinely needs to run first.
    requested.sched_priority = (minimum + (3 * maximum)) / 4;
  }

  if (pthread_setschedparam(pthread_self(), target_policy, &requested) != 0) {
    // The ordinary case on a desktop Linux: real-time scheduling needs
    // CAP_SYS_NICE or a raised RLIMIT_RTPRIO, and neither is the default.
    message_ =
        "Unable to obtain SCHED_RR; the capture runs at ordinary priority and "
        "is more exposed to other load on this machine";
    return;
  }

  saved_->valid = true;
  raised_ = true;
  message_ = "Thread priority set to SCHED_RR at " +
             std::to_string(requested.sched_priority);
}

ScopedThreadPriority::~ScopedThreadPriority() {
  if (saved_->valid) {
    pthread_setschedparam(pthread_self(), saved_->policy, &saved_->parameters);
  }
}

#endif

}  // namespace ddd::capture
