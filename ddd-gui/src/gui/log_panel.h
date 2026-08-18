/************************************************************************

    log_panel.h

    Log panel contents
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QWidget>

class QListView;
class QCheckBox;

namespace ddd::gui {

class LogMessageModel;

// The Log dock's contents: the record list, a follow-the-tail toggle and a
// clear button. Does not own the model — the main window does, because the
// model outlives any particular view of it.
//
// Thread-safety: NOT thread-safe. GUI thread only.
class LogPanel : public QWidget {
  Q_OBJECT

 public:
  explicit LogPanel(LogMessageModel* model, QWidget* parent = nullptr);

 private:
  void ScrollToLatest();

  LogMessageModel* model_;
  QListView* view_;
  QCheckBox* follow_tail_;
};

}  // namespace ddd::gui
