/************************************************************************

    floating_dock_drag.h

    Moving and resizing a popped-out panel under Wayland
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QtCore/qnamespace.h>

#include <QPoint>

class QDockWidget;

namespace ddd::gui {

// What a left button press at a given point in a dock widget should start.
enum class DockDrag {
  kNone,    // Nothing: leave the press to Qt's own handling.
  kMove,    // Ask the compositor to move the window.
  kResize,  // Ask the compositor to resize it by the named edges.
};

// A press, classified. edges is meaningful only for kResize.
struct DockDragRequest {
  DockDrag drag = DockDrag::kNone;
  Qt::Edges edges;
};

// Classifies a left button press at pos, in dock coordinates.
//
// A docked panel is always kNone: Qt drags one from its dock into another with
// a platform drag, which works. Only a floating panel — a window of its own —
// is this function's business, and there the title bar means move and the four
// edges mean resize.
//
// Separate from the event filter below so that the decision can be tested
// without a compositor to drag against.
DockDragRequest DockDragAt(const QDockWidget* dock, QPoint pos);

// Makes a floating dock widget movable and resizable under Wayland. Does
// nothing on any other platform.
//
// Qt draws its own title bar for a floating dock here — the platform's window
// manager is not asked to decorate it — and drags that title bar by grabbing
// the mouse and calling QWidget::move(). A Wayland client is allowed to do
// neither. The compositor refuses the grab, and says so on the console once per
// attempt:
//
//   This plugin supports grabbing the mouse only for popup windows
//
// and it ignores a client that tries to place its own top-level window. So a
// popped-out panel cannot be moved at all, its edges cannot be dragged, and
// every attempt at either prints a line of the plugin's diagnostics into a
// terminal a user is watching for this application's own messages.
//
// The way a Wayland client moves or resizes its window is to hand the drag to
// the compositor, which is what QWindow::startSystemMove() and
// startSystemResize() are for. This installs an event filter that takes the
// press which would otherwise start Qt's own drag and asks for the compositor's
// instead. Nothing else about the dock changes: the title bar Qt draws, its
// float and close buttons, and double-clicking it to put the panel back in the
// window all go on working.
void EnableFloatingDockDrag(QDockWidget* dock);

}  // namespace ddd::gui
