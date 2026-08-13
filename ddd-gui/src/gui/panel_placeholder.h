/************************************************************************

    panel_placeholder.h

    Stand-in contents for panels whose displays are not yet built
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>
#include <QWidget>

namespace ddd::gui {

// Contents for a dock whose real display has not been built yet. It states what
// the panel is for and that it is not implemented, rather than showing an empty
// box: an empty panel reads as a broken panel.
//
// Every use of this class is a panel still to be written, so the set of
// remaining ones can be found by looking for its constructor.
//
// Thread-safety: NOT thread-safe. GUI thread only.
class PanelPlaceholder : public QWidget {
  Q_OBJECT

 public:
  PanelPlaceholder(const QString& title, const QString& description,
                   QWidget* parent = nullptr);
};

}  // namespace ddd::gui
