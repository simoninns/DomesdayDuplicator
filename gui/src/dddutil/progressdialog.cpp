/************************************************************************

    progressdialog.cpp

    Utilities for Domesday Duplicator
    DomesdayDuplicator - LaserDisc RF sampler
    Copyright (C) 2019 Simon Inns

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

#include "progressdialog.h"
#include "ui_progressdialog.h"

ProgressDialog::ProgressDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ProgressDialog)
{
    ui->setupUi(this);
}

ProgressDialog::~ProgressDialog()
{
    delete ui;
}

// Set the percentage value of the progress bar
void ProgressDialog::setPercentage(qint32 percentage)
{
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;

    ui->progressBar->setValue(percentage);
}

// Change the processing label information
void ProgressDialog::setText(QString message)
{
    ui->processingLabel->setText(message);
}

void ProgressDialog::closeEvent(QCloseEvent *event)
{
    qDebug() << "ProgressDialog::closeEvent(): User close progress dialogue - sending cancelled signal";
    emit cancelled();

    // Accept the close event
    event->accept();
}
