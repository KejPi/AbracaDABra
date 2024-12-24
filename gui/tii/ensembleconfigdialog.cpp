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

#include <QRegularExpression>
#include <QFileDialog>

#include "ensembleconfigdialog.h"
#include "ui_ensembleconfigdialog.h"

EnsembleConfigDialog::EnsembleConfigDialog(const TxTableModelItem & item, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::EnsembleConfigDialog)
    , m_txTableModelItem(item)
{
    ui->setupUi(this);
    ui->ensStructureTextEdit->setHtml(item.ensConfig());
    ui->closeButton->setDefault(true);
    connect(ui->exportButton, &QPushButton::clicked, this, &EnsembleConfigDialog::onExportCSV);
    connect(ui->closeButton, &QPushButton::clicked, this, [this]() { close(); });
}

EnsembleConfigDialog::~EnsembleConfigDialog()
{
    delete ui;
}

void EnsembleConfigDialog::onExportCSV()
{
    static const QRegularExpression regexp( "[" + QRegularExpression::escape("/:*?\"<>|") + "]");
    QString ensemblename = m_txTableModelItem.ensLabel();
    ensemblename.replace(regexp, "_");
    QString f = QString("%1/%2_%3_%4.csv").arg(m_exportPath,
                                               QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"),
                                               DabTables::channelList.value(m_txTableModelItem.ensId().freq()),
                                               ensemblename);

    QString fileName = QFileDialog::getSaveFileName(this,
                                                    tr("Export CSV file"),
                                                    QDir::toNativeSeparators(f),
                                                    tr("CSV Files")+" (*.csv)");

    if (!fileName.isEmpty())
    {
        m_exportPath = QFileInfo(fileName).path(); // store path for next time
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly))
        {
            QTextStream out(&file);
            out << m_txTableModelItem.ensConfigCSV();
            file.close();
        }
    }
}

QString EnsembleConfigDialog::exportPath() const
{
    return m_exportPath;
}

void EnsembleConfigDialog::setExportPath(const QString &newExportPath)
{
    m_exportPath = newExportPath;
}
