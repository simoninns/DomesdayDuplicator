/************************************************************************

    utc_time_zone.h

    Pinning the clock so a name can be checked against a literal
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdlib>
#include <ctime>

namespace ddd::capture::test {

// Capture names and the DATE tag are in local time, deliberately: the person
// who took the capture is the person who will look for it, and they remember
// what time it was where they were standing.
//
// That leaves a test with a choice — assert against a literal and pin the zone,
// or compute the expected string the same way the code does. The second is no
// test at all, since it would agree with the implementation however wrong both
// were. So the zone is pinned, and the literals stay.
inline void UseUtc() {
#ifdef _WIN32
  _putenv_s("TZ", "UTC0");
  _tzset();
#else
  setenv("TZ", "UTC0", 1);
  tzset();
#endif
}

}  // namespace ddd::capture::test
