/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2025 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#ifndef ELIDEDLABEL_H
#define ELIDEDLABEL_H

#include <QLabel>
#include <QPainter>

class ElidedLabel : public QLabel
{
    Q_OBJECT

public:
    explicit ElidedLabel(QWidget *parent = nullptr);

    void setText(const QString &text);
    const QString &text() const { return content; }

    void setElideMode(Qt::TextElideMode elideMode);
    Qt::TextElideMode elideMode() const { return m_elideMode; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Qt::TextElideMode m_elideMode = Qt::ElideRight;
    QString content;
};

#endif  // ELIDEDLABEL_H
