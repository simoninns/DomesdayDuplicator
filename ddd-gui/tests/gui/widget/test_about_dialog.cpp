/************************************************************************

    test_about_dialog.cpp

    T1 tests for the About dialog's artwork
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QApplication>
#include <QIcon>
#include <QLabel>
#include <QList>
#include <QPixmap>
#include <QScrollArea>
#include <QScrollBar>

#include "about_dialog.h"
#include "about_text.h"

namespace ddd::gui {
namespace {

// These need a QApplication rather than the QCoreApplication the rest of the
// About tests run under, which is why they are here and not beside them: a
// QPixmap cannot be constructed without a GUI application.
//
// What they are actually guarding is the resource plumbing. The graphics are
// compiled into a static library, and a resource registered by a static
// initialiser can be dropped by the linker when nothing references the object
// file it lives in — leaving a dialog with a blank space where the logo should
// be, in the real application only, because the test binaries link the library
// differently. A null pixmap here is that failure.

TEST(AboutDialogTest, TheLogoIsCompiledInAndLoads) {
  const QPixmap logo = AboutLogo();

  ASSERT_FALSE(logo.isNull())
      << "the logo resource did not load: it is missing from the binary";
  EXPECT_GT(logo.width(), 100);
  EXPECT_GT(logo.height(), 40);
}

TEST(AboutDialogTest, TheApplicationIconCarriesEverySize) {
  const QIcon icon = ApplicationIcon();

  ASSERT_FALSE(icon.isNull()) << "the application icon did not load";

  // Several sizes, so a window manager, a task switcher and a title bar each
  // get artwork drawn for them rather than one bitmap scaled to all three.
  EXPECT_GE(icon.availableSizes().size(), 4);

  for (const QSize& size : icon.availableSizes()) {
    EXPECT_FALSE(icon.pixmap(size).isNull())
        << size.width() << "x" << size.height() << " is empty";
  }
}

TEST(AboutDialogTest, TheLogoIsTheSameEveryTimeItIsAskedFor) {
  // It is fetched afresh on each opening of the dialog, so a second call has to
  // work as well as the first — the resource initialisation is not a one-shot
  // that leaves later calls empty.
  EXPECT_FALSE(AboutLogo().isNull());
  EXPECT_EQ(AboutLogo().size(), AboutLogo().size());
}

// The dialog itself. QMessageBox::about() could set neither the picture nor the
// width, and the licence paragraphs wrapped into a tall narrow column — which
// is the whole reason there is a dialog here to test.
TEST(AboutDialogTest, TheDialogIsWideEnoughForItsOwnText) {
  const AboutDialog dialog;

  auto* const text =
      dialog.findChild<QLabel*>(QLatin1String(AboutDialog::kTextLabelName));
  ASSERT_NE(text, nullptr);

  EXPECT_GE(text->minimumWidth(), AboutDialog::kTextWidthPixels);
  EXPECT_GE(dialog.sizeHint().width(), AboutDialog::kTextWidthPixels)
      << "the dialog is narrower than the text it contains";
}

TEST(AboutDialogTest, NoLineIsCutOffAtTheRightHandEdge) {
  // A scroll area does not adopt its widget's minimum width, and the horizontal
  // scrollbar is deliberately off — so a dialog allowed to shrink below the
  // text's width would silently lose the end of every line.
  AboutDialog dialog;
  dialog.show();
  QApplication::processEvents();

  auto* const scroll = dialog.findChild<QScrollArea*>(
      QLatin1String(AboutDialog::kScrollAreaName));
  auto* const text =
      dialog.findChild<QLabel*>(QLatin1String(AboutDialog::kTextLabelName));
  ASSERT_NE(scroll, nullptr);
  ASSERT_NE(text, nullptr);

  EXPECT_LE(text->width(), scroll->viewport()->width())
      << "the text is " << text->width() << " px wide in a "
      << scroll->viewport()->width() << " px viewport with no horizontal bar";
  EXPECT_GE(text->width(), AboutDialog::kTextWidthPixels);
}

TEST(AboutDialogTest, TextThatDoesNotFitCanStillBeScrolledTo) {
  // The failure this replaces: a word-wrapped label whose height the layout did
  // not honour, with the last paragraph off the bottom of the dialog and no way
  // to reach it. A licence notice a user cannot read is worse than an ugly
  // dialog.
  AboutDialog dialog;
  dialog.show();
  QApplication::processEvents();

  auto* const scroll = dialog.findChild<QScrollArea*>(
      QLatin1String(AboutDialog::kScrollAreaName));
  ASSERT_NE(scroll, nullptr);

  // Nothing to scroll at the size it opens at, which is the point of sizing it
  // to its own text.
  EXPECT_EQ(scroll->verticalScrollBar()->maximum(), 0)
      << "the dialog opens too small for its own text";

  // Squeezed to half the height, everything is still reachable.
  dialog.resize(dialog.width(), dialog.height() / 2);
  QApplication::processEvents();

  EXPECT_GT(scroll->verticalScrollBar()->maximum(), 0)
      << "the text does not fit and cannot be scrolled to";
}

TEST(AboutDialogTest, TheDialogShowsTheLogoAndTheNotices) {
  const AboutDialog dialog;

  auto* const logo =
      dialog.findChild<QLabel*>(QLatin1String(AboutDialog::kLogoLabelName));
  ASSERT_NE(logo, nullptr);
  EXPECT_FALSE(logo->pixmap().isNull()) << "no logo in the dialog";

  auto* const text =
      dialog.findChild<QLabel*>(QLatin1String(AboutDialog::kTextLabelName));
  ASSERT_NE(text, nullptr);
  EXPECT_TRUE(text->text().contains(QStringLiteral("Simon Inns")));
  EXPECT_TRUE(text->text().contains(QStringLiteral("General Public License")));
}

TEST(AboutDialogTest, TheLinkToTheSourceCanBeFollowed) {
  // The licence entitles a user to the source. A link that does nothing when
  // clicked is no better than no link.
  const AboutDialog dialog;

  auto* const text =
      dialog.findChild<QLabel*>(QLatin1String(AboutDialog::kTextLabelName));
  ASSERT_NE(text, nullptr);

  EXPECT_TRUE(text->openExternalLinks());
}

}  // namespace
}  // namespace ddd::gui
