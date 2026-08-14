/************************************************************************

    fpga_version.h

    What the gateware reports about the build it came from
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ddd::capture {

// The FPGA carries a read-only identity block — a signature, the version of
// the register map it implements, some build flags and the commit it was built
// from — which the FX3 relays to the host through vendor request 0xB7. This
// turns those eleven bytes into something the application can show a user.
//
// It is the gateware's counterpart to firmware_version.h, and the two report
// the same kind of thing about different halves of the device: the FX3 stamps
// its commit into the USB product string, the FPGA into these registers. A
// release builds both from one commit, so a difference between them is worth
// mentioning and never worth refusing to work over.
struct FpgaVersion {
  // The identity block was read and carries the expected signature.
  //
  // False covers every way of not knowing: no device, an FPGA that has not
  // finished loading its configuration, gateware predating this interface, or
  // a device whose firmware stalled the request. None of them are
  // distinguishable from here and none of them need to be.
  bool present = false;

  // The register map version the gateware implements. Zero when not present.
  uint8_t map_version = 0;

  // The gateware was built from a tree with uncommitted changes.
  bool dirty = false;

  // The commit the gateware was built from, empty when it names none. Seven or
  // eight hex characters — the length varies with which build system produced
  // the gateware, which is why the register holds text — see the register map
  // on the "FPGA register interface" documentation page.
  std::string commit;

  // Does this gateware implement a register map this build understands?
  bool MapVersionIsKnown() const;
};

// Parse an identity block as returned by the register-read request.
//
// Returns a default-constructed FpgaVersion — present false — for anything
// that is not a well-formed block, including the all-zero and all-ones
// readings that an absent or unconfigured FPGA produces.
FpgaVersion ParseFpgaIdentity(const std::vector<uint8_t>& identity);

}  // namespace ddd::capture
