/************************************************************************

    update_manifest.h

    What a release bundle says about itself
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "digest.h"

namespace ddd::capture {

// The manifest at the head of an update bundle.
//
// The bundle is a plain uncompressed tar carrying this manifest, a detached
// signature over it, and one payload per component. The manifest holds the
// SHA-256 of every payload, so signing this one small file authenticates the
// whole bundle, and every later check — the download, the transfer to the
// device, the readback from flash — compares against the digests recorded here
// rather than recomputing a fresh opinion.
//
// The normative definition of the schema is the "Update bundle format" page of
// the documentation site. This header is the reader's half of it and says why
// each field exists; the page says what a producer must write.

// Which key signed the bundle, and therefore what the signature proves.
enum class UpdateChannel : uint8_t {
  // Signed with the release key. Authentic.
  kRelease,

  // Signed with the development keypair, whose secret half is committed to this
  // repository and is therefore public. A development signature proves the
  // bundle is well formed and proves nothing at all about where it came from,
  // which is why it is a channel and not merely a flag: a release build of the
  // application refuses it outright, and a build that accepts it says so in the
  // interface every time.
  kDevelopment,
};

// One flashable payload.
// The channel's name, for a log line. What the interface says about a
// development bundle is a paragraph with a banner around it; this is the one
// word that says which key signed the file.
const char* UpdateChannelName(UpdateChannel channel);

struct UpdateComponent {
  // The archive entry holding the bytes. A bare filename: bundle entries are
  // flat, so a name with a path separator in it is a malformed bundle.
  std::string file;

  // The payload's length in bytes, as a cross-check against the archive's own
  // idea of it. Two independent statements of one length is how a truncated
  // payload is caught before its digest is computed rather than after.
  uint64_t length = 0;

  Sha256Digest sha256{};

  // The commit the device will report once this payload is installed — the
  // product-string commit for the firmware, the identity registers for the
  // gateware. This is what the post-update confirmation compares against, so
  // that an update is proved by reading the device rather than by assuming the
  // write worked.
  std::string identity;

  // The version of the interface this payload will advertise once installed:
  // for the firmware, the USB protocol version in bcdDevice; for the gateware,
  // the register-map version in register 0x01.
  //
  // One field name for both because the compatibility gate does exactly one
  // thing with it — compare it against the range this application supports —
  // and two names would have invited two code paths for one rule.
  int64_t interface_version = 0;
};

// What a device and an application must already be for this bundle to be
// installable on them.
struct UpdateCompatibility {
  // The oldest release of the capture application that may install this
  // bundle, as a dotted numeric version.
  //
  // This is enforced rather than advisory: a user cannot drive an update that
  // takes the device past what the application doing the updating understands.
  // Dotted numbers rather than commit hashes because this is the one question
  // commits cannot answer — they identify a build and order nothing.
  std::string minimum_application_version;

  // The oldest gateware register map the firmware in this bundle can drive.
  // Checked against the map version the attached device reports, which matters
  // for a firmware-only bundle installed onto older gateware.
  int64_t minimum_register_map_version = 0;

  // The EPCS layout the gateware payload assumes, checked by the gate against
  // kEpcsBootBlockLayoutVersion. The factory image's boot logic is frozen at
  // provisioning time and reads one layout; a bundle built against a different
  // one must not be written, because the boot block it would leave behind is
  // the one thing a field update cannot repair.
  int64_t epcs_layout_version = 0;
};

struct UpdateManifest {
  // The schema version. An application refuses a manifest whose schema it does
  // not know rather than reading the fields it recognises: a bundle from the
  // future may mean something different by a field of the same name, and
  // guessing is how a device gets flashed with something nobody described.
  int64_t manifest_version = 0;

  UpdateChannel channel = UpdateChannel::kDevelopment;

  // The release this bundle belongs to, as a dotted numeric version, and the
  // commit every payload was built from.
  std::string version;
  std::string commit;

  // When the bundle was assembled, ISO 8601 in UTC. Recorded for the human
  // reading a bundle years later; nothing decides anything on it, because a
  // timestamp an attacker can set is not a version.
  std::string created;

  // One line, shown in the update dialog before the user confirms.
  std::string release_notes;

  // Components are individually optional. A firmware-only bundle is legal and
  // is what the development loop and the early phases produce; a bundle with no
  // components at all is not.
  std::optional<UpdateComponent> firmware;
  std::optional<UpdateComponent> gateware;

  // The provisioning gateware, as JTAG vectors.
  //
  // A third kind of payload rather than a second gateware one, and the
  // difference is what it is written *with*: the gateware component above goes
  // to the device over USB, through the flash bridge that the running gateware
  // provides. This one is played into the FPGA's JTAG port through a cable, and
  // exists precisely for the board that has no working gateware to be reached
  // through — a board that has never been programmed, or one holding gateware
  // from before the bridge existed.
  //
  // Never installed by the ordinary update path. UpdateOrchestrator does not
  // look at it, the compatibility gate does not gate on it, and a bundle
  // carrying nothing else is not an update bundle in any useful sense — it is
  // the bring-up wizard's input.
  //
  // The schema version is deliberately *not* bumped for this, and the reason
  // is what a build that predates the field does with such a bundle: it reads
  // the firmware component beside this one and offers an ordinary firmware
  // install, which is a true description of what that build can do with the
  // file and is exactly as safe as any other firmware bundle. Bumping the
  // version would instead have every older build refuse every bundle, which
  // buys nothing here — nothing about this component changes the meaning of
  // any other field.
  //
  // What did change is that the reader now refuses a component kind it does
  // not know, rather than reading the two it recognises and ignoring the rest.
  // A payload nobody described must never be silently skipped by the half of
  // the chain that would have to write it.
  std::optional<UpdateComponent> provisioning;

  // The factory image, as the raw EPCS bytes that go into the flash at
  // address 0 — the image a board falls back to.
  //
  // The fourth kind, and it exists because the vectors above turned out not
  // to be able to write flash at all: a quartus_cpf flash .svf drives
  // Altera's serial flash loader and carries no configuration of its own, so
  // the route that works is to configure *this* project's factory image over
  // JTAG and then write it from the firmware, over USB. So the two now come
  // as a pair — the vectors put a flash bridge on the FPGA, and this is what
  // that bridge writes.
  //
  // Never installed by the ordinary update path either, and for a stronger
  // reason than the vectors: the firmware refuses this destination unless the
  // request carries the factory-write word, so an update dialog that somehow
  // offered it would be refused by the device.
  std::optional<UpdateComponent> factory_gateware;

  UpdateCompatibility compatibility;
};

// The schema version this build writes and reads. A bundle declaring anything
// else is refused.
inline constexpr int64_t kUpdateManifestVersion = 1;

// The four component kinds, as the manifest names them. Every other member of
// "components" is refused — see UpdateManifest::provisioning.
inline constexpr std::string_view kFirmwareComponentName = "firmware";
inline constexpr std::string_view kGatewareComponentName = "gateware";
inline constexpr std::string_view kProvisioningComponentName =
    "gateware-provisioning-svf";
inline constexpr std::string_view kFactoryGatewareComponentName =
    "gateware-factory";

// Read a manifest from its JSON text.
//
// Returns nothing, and appends one line per problem to `errors` when it is not
// null, for anything that is not a complete and well-formed manifest of a known
// schema version. Every problem is reported rather than only the first, because
// the usual reader of these messages is whoever is building the release
// pipeline, and one round trip per mistake is a poor way to spend an afternoon.
std::optional<UpdateManifest> ParseUpdateManifest(
    std::string_view text, std::vector<std::string>* errors);

// Write a manifest back out, in the layout tools/make-update-bundle.sh
// produces. Used by the tests to build the input the reader is checked
// against; nothing in the application writes a manifest into a real bundle.
std::string SerialiseUpdateManifest(const UpdateManifest& manifest);

}  // namespace ddd::capture
