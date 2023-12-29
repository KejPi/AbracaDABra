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

#ifndef EPGDIALOG_H
#define EPGDIALOG_H

#include <QDialog>
#include <QQuickImageProvider>
#include "metadatamanager.h"
#include "slmodel.h"

namespace Ui {
class EPGDialog;
}

class EPGDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EPGDialog(SLModel *serviceListModel, MetadataManager *metadataManager, QWidget *parent = nullptr);
    ~EPGDialog();

private:
    Ui::EPGDialog *ui;

    MetadataManager * m_metadataManager;
    SLModel * m_serviceListModel;
};


// this class serves as simple image provider for EPG QML using MatedataManager as backend
class LogoProvider : public QQuickImageProvider
{
public:
    explicit LogoProvider(MetadataManager * metadataManager) : QQuickImageProvider(QQuickImageProvider::Pixmap)
        , m_metadataManager(metadataManager) {};

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override
    {
        QPixmap logo = m_metadataManager->data(ServiceListId(static_cast<uint64_t>(id.toUInt())),
                                               MetadataManager::MetadataRole::SmallLogo).value<QPixmap>();
        if (logo.isNull())
        {
            logo = QPixmap(requestedSize.width() > 0 ? requestedSize.width() : 32,
                           requestedSize.height() > 0 ? requestedSize.height() : 32);
            logo.fill(QColor(Qt::white).rgba());
        }

        if (size)
        {
            *size = QSize(logo.width(), logo.height());
        }

        return logo;
    }

private:
    MetadataManager * m_metadataManager;
};


#endif // EPGDIALOG_H
