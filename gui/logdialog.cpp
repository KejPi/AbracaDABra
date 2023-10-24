/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2023 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#include <QDebug>
#include <QString>
#include <QScrollBar>
#include <QFileDialog>
#include <QFile>
#include <QStandardPaths>
#include <QLoggingCategory>
#include <QClipboard>
#include "logdialog.h"
#include "ui_logdialog.h"

Q_DECLARE_LOGGING_CATEGORY(application)


LogDialog::LogDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LogDialog)
{
    ui->setupUi(this);

    m_dataModel = new LogModel(this);
    ui->logListView->setModel(m_dataModel);
    ui->logListView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    static const char kViewAtBottom[] = "viewAtBottom";
    auto *scrollBar = ui->logListView->verticalScrollBar();
    Q_ASSERT(scrollBar);
    auto rescroller = [scrollBar]() mutable {
        if (scrollBar->property(kViewAtBottom).isNull())
        {
            scrollBar->setProperty(kViewAtBottom, true);
        }
        auto const atBottom = scrollBar->property(kViewAtBottom).toBool();
        if (atBottom)
        {
            scrollBar->setValue(scrollBar->maximum());
        }
    };
    QObject::connect(scrollBar, &QAbstractSlider::rangeChanged, ui->logListView, rescroller, Qt::QueuedConnection);
    QObject::connect(scrollBar, &QAbstractSlider::valueChanged, ui->logListView, [scrollBar] {
        auto const atBottom = scrollBar->value() == scrollBar->maximum();
        scrollBar->setProperty(kViewAtBottom, atBottom);
    });

    QObject::connect(ui->clearButton, &QPushButton::clicked, this, [this] { m_dataModel->removeRows(0, m_dataModel->rowCount()); } );
    QObject::connect(ui->saveButton, &QPushButton::clicked, this, &LogDialog::saveLogToFile);
    QObject::connect(ui->copyButton, &QPushButton::clicked, this, &LogDialog::copyToClipboard);
}

LogDialog::~LogDialog()
{
    delete ui;
}

QAbstractItemModel *LogDialog::getModel() const
{
    return ui->logListView->model();
}

void LogDialog::setupDarkMode(bool darkModeEna)
{
    m_dataModel->setupDarkMode(darkModeEna);
}

void LogDialog::saveLogToFile()
{
    QString f = QString("%1/AbracaDABra_%2.log").arg(QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                            QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"));

    QString fileName = QFileDialog::getSaveFileName(this,
                                            tr("Save application log"),
                                            QDir::toNativeSeparators(f),
                                            tr("Log files")+" (*.log)");

    if (!fileName.isEmpty())
    {
        QFile logFile(fileName);
        if (!logFile.open(QIODevice::WriteOnly))
        {
            qCCritical(application) << "Unable to open file: " << fileName;
            return;
        }

        qCInfo(application) << "Writing log to:" << fileName;

        QTextStream stream(&logFile);
        for (int n = 0; n < m_dataModel->rowCount(); ++n)
        {
            stream << m_dataModel->data(m_dataModel->index(n, 0)).toString() << Qt::endl;
        }
        stream.flush();
        logFile.close();
    }
    else
    { /* no file selected */ }
}

void LogDialog::copyToClipboard()
{
    QString logText("");
    QTextStream stream(&logText);
    for (int n = 0; n < m_dataModel->rowCount(); ++n)
    {
        stream << m_dataModel->data(m_dataModel->index(n, 0)).toString() << Qt::endl;
    }
    stream.flush();

    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(logText);
}
