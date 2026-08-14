/************************************************************************

    update_key.cpp

    Which signatures this build accepts, and what each one proves
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "update_key.h"

#include "minisign_verify.h"

namespace ddd::capture {
namespace {

// tools/keys/development.pub, verbatim.
//
// Compiled in rather than read from a file, because a key the application
// loads from disk at run time is a key an attacker can replace, and then the
// whole chain proves nothing. The same reasoning applies to the release key
// below, which is why it is a build definition and not a path.
constexpr std::string_view kDevelopmentKeyText =
    "untrusted comment: Domesday Duplicator development signing key — public "
    "half. Proves format, never authenticity.\n"
    "RWR82Ay8IQPniaM+g2JAeVDIBxTGinceXiVzrjUfHL9Ki3MT2lj7S3QM\n";

// The release key, pinned at build time.
//
// Empty until the release pipeline exists to hold the secret half. That is a
// real state and not a placeholder: this build can install development
// bundles and honestly cannot verify a release one, and saying so is better
// than pretending to a trust it does not have.
#if defined(DDD_RELEASE_UPDATE_KEY)
constexpr std::string_view kReleaseKeyText = DDD_RELEASE_UPDATE_KEY;
#else
constexpr std::string_view kReleaseKeyText = "";
#endif

std::optional<MinisignPublicKey> ParseKey(std::string_view text) {
  if (text.empty()) {
    return std::nullopt;
  }
  return ParseMinisignPublicKey(text, nullptr);
}

// Does the channel the manifest claims match the key that actually verified
// it? The claim is written by whoever made the bundle and the proof is not,
// so a disagreement is decided in favour of the proof — by refusing.
bool ChannelMatchesKey(UpdateChannel channel, UpdateChannel key_channel) {
  return channel == key_channel;
}

}  // namespace

std::string_view DevelopmentUpdateKeyText() { return kDevelopmentKeyText; }

bool HasReleaseUpdateKey() { return !kReleaseKeyText.empty(); }

UpdateKeyPolicy DefaultUpdateKeyPolicy() {
  UpdateKeyPolicy policy;

  // A debug build implies the opt-in, because a debug build is already a
  // build nobody ships and requiring the flag there would only mean typing
  // it every time.
#if !defined(NDEBUG)
  policy.accept_development_key = true;
#endif

  // A build with no release key pinned can verify nothing else, so refusing
  // the development key as well would leave it unable to open any bundle at
  // all — an application that cannot be tested is not safer, it is untested.
  // This is temporary in the honest sense: it stops being true the moment a
  // release key is pinned, and the condition below says exactly that.
  if (!HasReleaseUpdateKey()) {
    policy.accept_development_key = true;
  }

  return policy;
}

std::optional<UpdateBundle> OpenUpdateBundleForPolicy(
    std::span<const uint8_t> archive, const UpdateKeyPolicy& policy,
    std::string* error) {
  const std::optional<MinisignPublicKey> release = ParseKey(kReleaseKeyText);
  const std::optional<MinisignPublicKey> development =
      ParseKey(kDevelopmentKeyText);

  // The release key first, always. A bundle that verifies against it is a
  // release bundle and the development key is never consulted for it.
  if (release.has_value()) {
    std::string release_error;
    std::optional<UpdateBundle> bundle =
        OpenUpdateBundle(archive, *release, &release_error);
    if (bundle.has_value()) {
      if (!ChannelMatchesKey(bundle->manifest.channel,
                             UpdateChannel::kRelease)) {
        if (error != nullptr) {
          *error =
              "This update file is signed with the release key but does not "
              "say it is a release. It has not been installed.";
        }
        return std::nullopt;
      }
      return bundle;
    }

    if (!policy.accept_development_key) {
      if (error != nullptr) {
        *error =
            "This update file is not signed by the Domesday Duplicator "
            "project. It has not been installed. (" +
            release_error + ")";
      }
      return std::nullopt;
    }
  }

  if (!policy.accept_development_key || !development.has_value()) {
    if (error != nullptr) {
      *error =
          "This update file is not signed by the Domesday Duplicator project. "
          "It has not been installed.";
    }
    return std::nullopt;
  }

  std::string development_error;
  std::optional<UpdateBundle> bundle =
      OpenUpdateBundle(archive, *development, &development_error);
  if (!bundle.has_value()) {
    if (error != nullptr) {
      *error = "This update file could not be verified: " + development_error;
    }
    return std::nullopt;
  }

  if (!ChannelMatchesKey(bundle->manifest.channel,
                         UpdateChannel::kDevelopment)) {
    if (error != nullptr) {
      *error =
          "This update file claims to be a release but is signed with the "
          "development key, whose secret half is public. It has not been "
          "installed.";
    }
    return std::nullopt;
  }

  return bundle;
}

}  // namespace ddd::capture
