/************************************************************************

    log_panel.h

    Log panel contents
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>
#include <QWidget>

class QAction;
class QCheckBox;
class QComboBox;
class QListView;
class QPoint;

namespace ddd::gui {

class ApplicationLogger;
class LogMessageModel;

// The Log dock's contents: the record list, a minimum-level chooser, a
// follow-the-tail toggle and a clear button. Records can be selected — one, a
// range, or all of them — and copied to the clipboard as the lines they are
// shown as, through Ctrl+C or the list's context menu.
//
// Does not own the model — the main window does, because the model outlives
// any particular view of it — and does not own the logger either.
//
// The level chooser is the GUI's equivalent of --log-level, and does exactly
// what that switch does: it moves the logger's threshold, so it governs the
// console and the log file as well as this panel. It is not a view filter, and
// raising it does not hide records already listed — what is on screen was
// logged under the old threshold and stays there.
//
// Thread-safety: NOT thread-safe. GUI thread only.
class LogPanel : public QWidget {
  Q_OBJECT

 public:
  // Object names, for tests and for stylesheets.
  static constexpr const char* kLevelComboName = "log_level_combo";
  static constexpr const char* kCopyActionName = "log_copy_action";
  static constexpr const char* kSelectAllActionName = "log_select_all_action";

  // `logger` may be null, which leaves the level chooser disabled: a panel with
  // no logger behind it has no threshold to move, and a control that silently
  // did nothing would be worse than one that says it cannot.
  LogPanel(LogMessageModel* model, ApplicationLogger* logger,
           QWidget* parent = nullptr);

 private:
  void ScrollToLatest();

  // The selected records, oldest first, as the lines the view shows. Empty
  // when nothing is selected.
  QString SelectedText() const;

  void CopySelection();
  void ShowContextMenu(const QPoint& position);

  LogMessageModel* model_;
  ApplicationLogger* logger_;
  QListView* view_;
  QComboBox* level_;
  QCheckBox* follow_tail_;
  QAction* copy_action_;
  QAction* select_all_action_;
};

}  // namespace ddd::gui
