/************************************************************************

    advancednamingdialog.h

    Capture application for the Domesday Duplicator
    DomesdayDuplicator - LaserDisc RF sampler
    Copyright (C) 2018-2019 Simon Inns

    This file is part of Domesday Duplicator.

    Domesday Duplicator is free software: you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Email: simon.inns@gmail.com

************************************************************************/

#ifndef ADVANCEDNAMINGDIALOG_H
#define ADVANCEDNAMINGDIALOG_H

#include <QDialog>
#include <QRegularExpressionValidator>
#include <QDate>

namespace Ui {
class AdvancedNamingDialog;
}

class AdvancedNamingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AdvancedNamingDialog(QWidget *parent = nullptr);
    ~AdvancedNamingDialog();

    QString getFileName(bool isTestData);
    bool getDurationChecked();

    void setPerSideNotesEnabled(bool enabled);
    void setPerSideMintEnabled(bool enabled);

private slots:
    void on_discTitleCheckBox_clicked();
    void on_discTypeCheckBox_clicked();
    void on_formatCheckBox_clicked();
    void on_audioCheckBox_clicked();
    void on_discSideCheckBox_clicked();
    void on_notesCheckBox_clicked();
    void on_mintCheckBox_clicked();
    void on_durationCheckBox_clicked();
    void on_discSideSpinBox_valueChanged();

private:
    Ui::AdvancedNamingDialog *ui;

    void updateGui();
    void updateSideHoldings();

    int discSideSpinBoxPrevVal = 1;
    bool perSideNotesEnabled = false;
    QString notesHolding[100];
    bool perSideMintEnabled = false;
    QString mintHolding[100];
};

#endif // ADVANCEDNAMINGDIALOG_H
