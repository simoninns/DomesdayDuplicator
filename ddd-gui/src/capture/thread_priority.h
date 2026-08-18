/************************************************************************

    thread_priority.h

    Raising the capture threads above ordinary scheduling
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <memory>
#include <string>

namespace ddd::capture {

// Raises the calling thread's scheduling priority for the duration of a capture
// and puts it back afterwards.
//
// The transfer and processing threads have a hard deadline: at 80 MB/s a 2 MB
// buffer arrives every 26 ms, and a thread descheduled behind an unrelated
// workload for longer than the queue holds loses samples that cannot be
// recovered. SCHED_RR on POSIX and THREAD_PRIORITY_TIME_CRITICAL on Windows are
// how that is asked for.
//
// Failure is expected and is not an error. An unprivileged process cannot
// obtain a real-time scheduling policy on most Linux systems, and a capture at
// ordinary priority usually works — it is more exposed to a busy machine, not
// broken. So this reports what it achieved and the caller logs it; nothing
// refuses to capture because of it.
//
// Thread-safety: an instance belongs to the thread that constructed it, which
// is the thread whose priority it changes. It must be destroyed on that same
// thread. Construction and destruction happen once per capture, so the pimpl
// allocation is not on any deadline.
class ScopedThreadPriority {
 public:
  // Raises the calling thread's priority.
  ScopedThreadPriority();

  // Restores whatever the thread had before.
  ~ScopedThreadPriority();

  ScopedThreadPriority(const ScopedThreadPriority&) = delete;
  ScopedThreadPriority& operator=(const ScopedThreadPriority&) = delete;
  ScopedThreadPriority(ScopedThreadPriority&&) = delete;
  ScopedThreadPriority& operator=(ScopedThreadPriority&&) = delete;

  // Whether the priority was actually raised
  bool raised() const { return raised_; }

  // What happened, in a form a log line can carry straight through. Populated
  // whether or not the attempt succeeded, because "SCHED_RR at priority 74"
  // is as worth recording as a refusal.
  const std::string& message() const { return message_; }

 private:
  // The platform's saved scheduling state, kept behind a pointer so that
  // <sched.h> and <Windows.h> stay out of this header.
  struct Saved;

  bool raised_ = false;
  std::string message_;
  std::unique_ptr<Saved> saved_;
};

}  // namespace ddd::capture
