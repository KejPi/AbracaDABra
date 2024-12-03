/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2024 Petr Kopecký <xkejpi (at) gmail (dot) com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <QPushButton>
#include <QDesktopServices>
#include "config.h"
#include "updatedialog.h"
#include "ui_updatedialog.h"

UpdateDialog::UpdateDialog(const QString &version, const QString &releaseNotes, Qt::WindowFlags f, QWidget *parent)
    : QDialog(parent, f)
    , ui(new Ui::UpdateDialog)
{
    ui->setupUi(this);
    setModal(true);
    setWindowTitle(tr("Application update"));
    ui->title->setText(tr("AbracaDABra update available"));
    ui->currentLabel->setText(tr("Current version: %1").arg(PROJECT_VER));
#if 0
    ui->availableLabel->setText(tr("Available version: %1 (<a href=\"https://github.com/KejPi/AbracaDABra/releases/tag/%1\">link</a>)").arg(version));
    connect(
        ui->availableLabel, &QLabel::linkActivated,
        [=]( const QString & link ) { QDesktopServices::openUrl(QUrl::fromUserInput(link)); }
        );
#else
    ui->availableLabel->setText(tr("Available version: %1").arg(version));
#endif

    auto font = ui->title->font();
    font.setPointSize(font.pointSize() + 2);
    ui->title->setFont(font);
    ui->releaseNotes->setReadOnly(true);
    ui->releaseNotes->setMarkdown("**Changelog:**\n\n" + releaseNotes);
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Do not show again"));
    auto goTo = new QPushButton(tr("Go to release page"), this);
    connect(goTo, &QPushButton::clicked, this, [this, version]() {
        QDesktopServices::openUrl(QUrl::fromUserInput(QString("https://github.com/KejPi/AbracaDABra/releases/tag/%1").arg(version)));
    });
    ui->buttonBox->addButton(goTo, QDialogButtonBox::ActionRole);

    setFixedSize(size());
}

UpdateDialog::~UpdateDialog()
{
    delete ui;
}
