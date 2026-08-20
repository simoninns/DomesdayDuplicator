/************************************************************************

    test_floating_dock_drag.cpp

    T1 tests for the floating panel drag classification
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QDockWidget>
#include <QLabel>
#include <QLayout>
#include <QMainWindow>
#include <QPoint>
#include <QRect>

#include "floating_dock_drag.h"
#include "qt_message_filter.h"

namespace ddd::gui {
namespace {

// A dock in a window, with a panel in it and a title bar above the panel.
//
// The title bar is a widget of the test's own rather than the one Qt draws.
// Whether Qt draws a title bar for a floating dock or leaves it to the window
// manager is decided by the platform plugin — it draws one under Wayland,
// which is what this code is for, and leaves it to the window manager under the
// offscreen plugin these tests run on. Supplying one puts the dock into the
// layout the real thing has: a title area above the panel, and the panel
// inset from the frame.
struct DockFixture {
  QMainWindow window;
  QDockWidget* dock = nullptr;

  DockFixture() {
    dock = new QDockWidget(QStringLiteral("Panel"), &window);
    dock->setObjectName(QStringLiteral("panel_dock"));
    dock->setTitleBarWidget(new QLabel(QStringLiteral("Panel"), dock));
    dock->setWidget(new QLabel(QStringLiteral("contents"), dock));
    window.addDockWidget(Qt::LeftDockWidgetArea, dock);
  }

  void Float() {
    dock->setFloating(true);
    dock->resize(400, 300);
    // Laying out is what gives the contained panel a geometry; without it the
    // whole dock reads as title bar.
    dock->layout()->activate();
  }
};

TEST(FloatingDockDrag, PressInTheTitleBarAsksToMoveTheWindow) {
  DockFixture fixture;
  fixture.Float();
  const QRect content = fixture.dock->widget()->geometry();
  ASSERT_GT(content.top(), 0) << "the dock has no title bar to press in";

  const DockDragRequest request =
      DockDragAt(fixture.dock, QPoint(content.center().x(), content.top() / 2));

  EXPECT_EQ(request.drag, DockDrag::kMove);
}

TEST(FloatingDockDrag, PressInThePanelIsLeftAlone) {
  DockFixture fixture;
  fixture.Float();

  const DockDragRequest request =
      DockDragAt(fixture.dock, fixture.dock->widget()->geometry().center());

  EXPECT_EQ(request.drag, DockDrag::kNone);
}

TEST(FloatingDockDrag, PressOnAnEdgeAsksToResizeByThatEdge) {
  DockFixture fixture;
  fixture.Float();
  const QRect rect = fixture.dock->rect();

  const DockDragRequest left =
      DockDragAt(fixture.dock, QPoint(rect.left(), rect.center().y()));
  EXPECT_EQ(left.drag, DockDrag::kResize);
  EXPECT_EQ(left.edges, Qt::Edges(Qt::LeftEdge));

  const DockDragRequest bottom =
      DockDragAt(fixture.dock, QPoint(rect.center().x(), rect.bottom()));
  EXPECT_EQ(bottom.drag, DockDrag::kResize);
  EXPECT_EQ(bottom.edges, Qt::Edges(Qt::BottomEdge));
}

TEST(FloatingDockDrag, PressInACornerAsksToResizeByBothItsEdges) {
  DockFixture fixture;
  fixture.Float();
  const QRect rect = fixture.dock->rect();

  const DockDragRequest request =
      DockDragAt(fixture.dock, QPoint(rect.right(), rect.bottom()));

  EXPECT_EQ(request.drag, DockDrag::kResize);
  EXPECT_EQ(request.edges, Qt::Edges(Qt::RightEdge | Qt::BottomEdge));
}

TEST(FloatingDockDrag, PressInADockedPanelIsLeftAlone) {
  DockFixture fixture;
  fixture.window.resize(400, 300);
  fixture.window.layout()->activate();
  ASSERT_FALSE(fixture.dock->isFloating());

  const QRect rect = fixture.dock->rect();
  EXPECT_EQ(DockDragAt(fixture.dock, QPoint(rect.center().x(), 2)).drag,
            DockDrag::kNone);
  EXPECT_EQ(DockDragAt(fixture.dock, rect.topLeft()).drag, DockDrag::kNone);
  EXPECT_EQ(DockDragAt(fixture.dock, rect.center()).drag, DockDrag::kNone);
}

TEST(FloatingDockDrag, NoDockIsLeftAlone) {
  EXPECT_EQ(DockDragAt(nullptr, QPoint(0, 0)).drag, DockDrag::kNone);
}

// The message the Wayland plugin prints when Qt asks it to grab the mouse for
// a dock widget. Matched by text because it has no logging category, so the
// text is what the filter has to keep working against.
TEST(QtMessageFilter, SilencesTheMouseGrabWarningAndNothingElse) {
  EXPECT_TRUE(IsSilencedPluginMessage(QStringLiteral(
      "This plugin supports grabbing the mouse only for popup windows")));

  EXPECT_FALSE(IsSilencedPluginMessage(QStringLiteral(
      "This plugin supports grabbing the mouse only for popup window")));
  EXPECT_FALSE(IsSilencedPluginMessage(
      QStringLiteral("Capture failed: the device stopped responding")));
  EXPECT_FALSE(IsSilencedPluginMessage(QString()));
}

}  // namespace
}  // namespace ddd::gui
