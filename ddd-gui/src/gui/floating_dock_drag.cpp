/************************************************************************

    floating_dock_drag.cpp

    Moving and resizing a popped-out panel under Wayland
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "floating_dock_drag.h"

#include <QApplication>
#include <QDockWidget>
#include <QEvent>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QObject>
#include <QRect>
#include <QString>
#include <QWidget>
#include <QWindow>
#include <optional>

namespace ddd::gui {
namespace {

// How near an edge a press has to be to mean "resize" rather than "move", in
// device-independent pixels. Four is what Qt's own QWidgetResizeHandler uses
// for the frame it draws around a floating dock, so the band this reacts to is
// the band the cursor already changes shape over.
constexpr int kResizeBand = 4;

bool RunningOnWayland() {
  // Cached: the platform cannot change while the application runs, and this is
  // asked once per dock.
  static const bool wayland = QGuiApplication::platformName().startsWith(
      QStringLiteral("wayland"), Qt::CaseInsensitive);
  return wayland;
}

// Takes the press that would start Qt's own drag and asks the compositor for
// one instead. See the header for why.
class FloatingDockDrag : public QObject {
 public:
  explicit FloatingDockDrag(QDockWidget* dock) : QObject(dock) {}

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    // The dock itself, not one of its children: the title bar's float and close
    // buttons are widgets of their own and are sent their presses directly, so
    // a press that arrives here is on the bar or the frame around it.
    auto* const dock = qobject_cast<QDockWidget*>(watched);
    if (dock == nullptr) {
      return false;
    }

    switch (event->type()) {
      case QEvent::MouseButtonPress:
        return HandlePress(dock, static_cast<QMouseEvent*>(event));
      case QEvent::MouseMove:
        return HandleMove(dock, static_cast<QMouseEvent*>(event));
      case QEvent::MouseButtonRelease:
        pressed_at_.reset();
        return false;
      default:
        return false;
    }
  }

 private:
  bool HandlePress(QDockWidget* dock, QMouseEvent* mouse) {
    pressed_at_.reset();
    if (mouse->button() != Qt::LeftButton) {
      return false;
    }

    const QPoint pos = mouse->position().toPoint();
    const DockDragRequest request = DockDragAt(dock, pos);

    if (request.drag == DockDrag::kResize) {
      // Straight away, as a window frame does: a press on an edge means a
      // resize whether or not the pointer then moves.
      QWindow* const window = dock->windowHandle();
      return window != nullptr && window->startSystemResize(request.edges);
    }

    if (request.drag != DockDrag::kMove) {
      return false;
    }

    // Remembered rather than acted on, and this is the whole reason the move
    // is in two parts. Handing the press to the compositor immediately would
    // hand it the second click of a double click as well, and double-clicking
    // the title bar is how a panel is put back in the window. Waiting for the
    // pointer to actually move leaves that click where it belongs.
    //
    // The press is still swallowed, so that Qt does not start the drag of its
    // own that neither the compositor nor this code wants.
    pressed_at_ = pos;
    return true;
  }

  bool HandleMove(QDockWidget* dock, QMouseEvent* mouse) {
    if (!pressed_at_.has_value()) {
      return false;
    }
    if ((mouse->buttons() & Qt::LeftButton) == 0) {
      pressed_at_.reset();
      return false;
    }
    if ((mouse->position().toPoint() - *pressed_at_).manhattanLength() <
        QApplication::startDragDistance()) {
      return true;
    }

    pressed_at_.reset();

    // From here the compositor owns the drag and this application sees no more
    // of it. If it will not take it there is nothing to be gained by hiding the
    // event, so Qt is left to do whatever it would have done.
    QWindow* const window = dock->windowHandle();
    return window != nullptr && window->startSystemMove();
  }

  std::optional<QPoint> pressed_at_;
};

}  // namespace

DockDragRequest DockDragAt(const QDockWidget* dock, QPoint pos) {
  if (dock == nullptr || !dock->isFloating()) {
    return {};
  }

  const QRect rect = dock->rect();
  if (!rect.contains(pos)) {
    return {};
  }

  Qt::Edges edges;
  if (pos.x() <= rect.left() + kResizeBand) {
    edges |= Qt::LeftEdge;
  } else if (pos.x() >= rect.right() - kResizeBand) {
    edges |= Qt::RightEdge;
  }
  if (pos.y() <= rect.top() + kResizeBand) {
    edges |= Qt::TopEdge;
  } else if (pos.y() >= rect.bottom() - kResizeBand) {
    edges |= Qt::BottomEdge;
  }
  if (edges != Qt::Edges()) {
    return {DockDrag::kResize, edges};
  }

  // Everything inside the frame that is not the panel is the title bar — asked
  // this way rather than by measuring a title height, so a dock with its title
  // bar down the side is described as correctly as one with it on top.
  const QWidget* const content = dock->widget();
  if (content != nullptr && content->geometry().contains(pos)) {
    return {};
  }
  return {DockDrag::kMove, {}};
}

void EnableFloatingDockDrag(QDockWidget* dock) {
  if (dock == nullptr || !RunningOnWayland()) {
    return;
  }

  auto* const filter = new FloatingDockDrag(dock);
  dock->installEventFilter(filter);

  // Installed again every time the dock floats, and that is the whole reason
  // this connection exists. The first time a dock floats Qt gives it a
  // QWidgetResizeHandler — an event filter of its own, for the frame it draws —
  // and the filter installed last is the filter called first. Ours has to be
  // the later one, or Qt's takes the press on an edge and resizes the window by
  // moving its corners, which a Wayland client cannot do either.
  //
  // Queued, because topLevelChanged() is emitted before Qt installs that
  // handler; running this once the event loop gets back to it puts the two in
  // the right order.
  QObject::connect(
      dock, &QDockWidget::topLevelChanged, filter,
      [dock, filter](bool floating) {
        if (!floating) {
          return;
        }
        dock->removeEventFilter(filter);
        dock->installEventFilter(filter);
      },
      Qt::QueuedConnection);
}

}  // namespace ddd::gui
