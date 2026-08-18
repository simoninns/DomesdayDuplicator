/************************************************************************

    sample_sink.cpp

    Where samples go
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "sample_sink.h"

namespace ddd::capture {

bool NullSink::Write(const uint8_t* /*wire_data*/, size_t sample_count) {
  samples_written_ += sample_count;
  return true;
}

}  // namespace ddd::capture
