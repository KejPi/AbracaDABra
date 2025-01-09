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

#ifndef CLICKABLELABEL_H
#define CLICKABLELABEL_H

#include <QLabel>
#include <QMenu>
#include <QObject>

class ClickableLabel : public QLabel
{
    Q_OBJECT
public:
    ClickableLabel(QWidget *parent = 0);
    ~ClickableLabel();
    void setMenu(QMenu *menu) { m_menu = menu; }
    bool setIcon(const QString &file, bool checked = false);
    void setTooltip(const QString &text, bool checked = false);
    void setCheckable(bool checkable)
    {
        m_checkable = checkable;
        m_checked = false;
    }
    bool isCheckable() const { return m_checkable; }
    void setChecked(bool checked);
    bool isChecked() const { return m_checked; }
    void toggle();
signals:
    void clicked();
    void toggled(bool checked);

protected:
    void mouseReleaseEvent(QMouseEvent *);

private:
    QMenu *m_menu = nullptr;
    bool m_checkable = false;
    bool m_checked = false;

    QPixmap *m_pic = nullptr;
    QPixmap *m_picChecked = nullptr;

    QString *m_tooltip = nullptr;
    QString *m_tooltipChecked = nullptr;

    void update();
};

#endif  // CLICKABLELABEL_H
