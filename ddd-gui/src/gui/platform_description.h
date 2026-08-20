/************************************************************************

    platform_description.h

    What this build is running on, for the log
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>

namespace ddd::gui {

// One line naming the operating system, the kernel, the processor
// architecture and the Qt the application is running against.
//
// It is in the log because the first question asked of a fault report is which
// of the three platforms it happened on, and the second is which version — and
// neither is answerable from a log that does not say. A user pasting a log has
// then already answered both without being asked.
//
// The Qt version is there for a fourth question that only comes up in packaged
// builds: a bundle that ships one Qt and loads another is a class of fault that
// looks like an application bug and is not, and the two versions side by side
// are what makes it visible.
QString PlatformDescription();

// The wording, separated from where the facts come from so that it can be
// tested against facts a test chooses rather than against whatever machine the
// test is running on.
//
// @param product      The operating system as it names itself, or empty
// @param kernel_type  "linux", "darwin", "winnt"
// @param kernel       The kernel's version, or empty
// @param architecture The processor architecture this build runs on
// @param running_qt   The Qt this binary has loaded
// @param built_qt     The Qt this binary was compiled against
QString DescribePlatform(const QString& product, const QString& kernel_type,
                         const QString& kernel, const QString& architecture,
                         const QString& running_qt, const QString& built_qt);

}  // namespace ddd::gui
