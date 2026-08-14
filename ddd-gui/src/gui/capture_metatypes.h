/************************************************************************

    capture_metatypes.h

    Engine value types, declared once so queued signals can carry them
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QMetaType>
#include <cstdint>
#include <vector>

#include "device_updater.h"
#include "monitor_tap.h"
#include "usb_device_info.h"

// Every type that crosses a signal in this application, in one place.
//
// It has to be one place. Q_DECLARE_METATYPE is a template specialisation, and
// a specialisation that appears after the template has already been
// instantiated is an error rather than a warning — so a second header declaring
// the same type breaks the build, and a header that uses the type in a signal
// without seeing the declaration breaks the build of whatever includes it next.
// Both failures name a generated file rather than the header that caused them,
// which is a morning nobody needs twice.
//
// Declared in the GUI layer rather than beside the types themselves because the
// types live in the engine, and the engine may not include Qt (AGENTS.md §2).
// This is the seam: the engine defines the values, and this says they may
// travel through the meta-object system.
Q_DECLARE_METATYPE(ddd::capture::CaptureStats)
Q_DECLARE_METATYPE(std::vector<ddd::capture::DeviceInfo>)

// What the device said it was running once an update had finished with it.
// Carried whole rather than as the two strings a summary line needs, because
// what the update page has to decide afterwards — whether the FPGA came back
// in its factory image — is not in either of them.
Q_DECLARE_METATYPE(ddd::capture::DeviceIdentity)

// The analysis frames. Plain vectors rather than named types because that is
// what they are — a run of converter codes and a run of decibel figures — and
// wrapping them in a struct for the sake of the type system would be a class
// with one member and no behaviour.
Q_DECLARE_METATYPE(std::vector<uint16_t>)
Q_DECLARE_METATYPE(std::vector<double>)
