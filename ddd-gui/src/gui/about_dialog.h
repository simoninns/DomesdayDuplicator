/************************************************************************

    about_dialog.h

    The About box: what this is, which build, and under what terms
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QDialog>

class QLabel;

namespace ddd::gui {

// A dialog of its own rather than QMessageBox::about().
//
// Two things the message box could not do. It has no way to set a picture — its
// icon is one of a fixed set of system symbols — and no way to set a width, so
// the paragraphs of licence text wrapped into a tall narrow column. Both are
// fixable only by reaching into its internal layout by object name, which is a
// private detail of Qt's own widget and not somewhere to keep a fix.
//
// Thread-safety: NOT thread-safe. GUI thread only.
class AboutDialog : public QDialog {
  Q_OBJECT

 public:
  explicit AboutDialog(QWidget* parent = nullptr);

  // Wide enough that the licence paragraphs read as paragraphs. Not a guess at
  // a proportion of the screen: this is a fixed amount of text, and this is
  // what it needs.
  static constexpr int kTextWidthPixels = 520;

  static constexpr const char* kLogoLabelName = "about_logo";
  static constexpr const char* kTextLabelName = "about_text";
  static constexpr const char* kScrollAreaName = "about_scroll";

 private:
  QLabel* logo_ = nullptr;
  QLabel* text_ = nullptr;
};

}  // namespace ddd::gui
