/************************************************************************

    bundled_provisioning.h

    Finding the provisioning set a packaged build carries
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>
#include <QStringList>

namespace ddd::gui {

// Where the bring-up wizard's provisioning set comes from when nobody has
// downloaded one.
//
// **Why a file at all, when the release key is compiled in.** The key is a
// build input because a key read from disk is a key an attacker can replace.
// A provisioning set is the opposite kind of thing: it is *data the signature
// covers*, so where it came from does not matter — it is verified before it is
// used, bundled or picked, by the same reader with the same key policy. What
// bundling buys is the offline case, which is the one that matters here: a
// board being brought up cannot be updated over USB, and the machine in front
// of it may be the one that has just been built.
//
// It is also why nothing in this file trusts what it finds. It returns a path.
// Everything that decides whether that path is usable happens in
// OpenUpdateBundleForPolicy, exactly as it does for a file a user chose.
//
// **The layouts.** A packaged build installs one file, under one fixed name,
// in whichever place its platform keeps read-only application data:
//
//   Windows    beside ddd-gui.exe, which is what the MSI harvests
//   macOS      Contents/Resources, inside the .app
//   Linux      <prefix>/share/<app-id>/, which is where CMake installs it and
//              where a Flatpak's /app/share ends up
//
// A build that bundles nothing finds nothing and says so: the wizard then
// opens with no preselection and its file picker, which is the honest state
// and the same convention the unpinned release key follows.

// The name the installed set carries, whatever platform installed it. Fixed
// rather than versioned: a build carries one, and a name that changed with the
// release would mean the search had to know which release it was looking for.
inline constexpr const char* kBundledProvisioningName = "provisioning.dddfw";

// Every place a bundled set might be, in the order they are tried, as absolute
// file paths. Pure: it looks at no filesystem and asks Qt nothing, so a test
// can state the layout it is checking rather than arranging one.
//
// `application_dir` is QCoreApplication::applicationDirPath(), and
// `generic_data_dirs` is QStandardPaths::standardLocations(GenericDataLocation)
// — the XDG data path on Linux, which is what carries /app/share inside a
// Flatpak. Either may be empty, and the candidates that depend on it are then
// simply not offered.
QStringList BundledProvisioningSearchPaths(
    const QString& application_dir, const QStringList& generic_data_dirs);

// The first of those that exists, or an empty string when this build carries
// no set. Needs a QCoreApplication, like anything that asks where it is
// installed.
QString BundledProvisioningPath();

}  // namespace ddd::gui
