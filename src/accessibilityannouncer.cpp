/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2026 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#include "accessibilityannouncer.h"

#include <QAccessible>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQuickItem>
#include <QQuickWindow>
#include <QWindow>

Q_LOGGING_CATEGORY(a11yLog, "application.a11y", QtInfoMsg)

namespace a11y
{
void announce(const QString &text, bool assertive)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    if (text.isEmpty())
    {
        return;
    }

    // pick first candidate that has accessible interface
    // QML items are preferred (same target type as Accessible.announce in QML)
    QList<QObject *> candidates{QGuiApplication::focusObject()};
    QList<QWindow *> windows{QGuiApplication::focusWindow()};
    windows.append(QGuiApplication::topLevelWindows());
    for (QWindow *w : std::as_const(windows))
    {
        if (auto *qw = qobject_cast<QQuickWindow *>(w))
        {
            candidates.append(qw->activeFocusItem());
            candidates.append(qw->contentItem());
        }
        candidates.append(w);
    }

    QObject *target = nullptr;
    for (QObject *c : std::as_const(candidates))
    {
        if (c != nullptr && QAccessible::queryAccessibleInterface(c) != nullptr)
        {
            target = c;
            break;
        }
    }

    qCInfo(a11yLog) << "announce:" << text << "active:" << QAccessible::isActive() << "target:" << target;
    if (nullptr == target)
    {
        return;
    }

    QAccessibleAnnouncementEvent event(target, text);
    event.setPoliteness(assertive ? QAccessible::AnnouncementPoliteness::Assertive : QAccessible::AnnouncementPoliteness::Polite);
    QAccessible::updateAccessibility(&event);
#else
    Q_UNUSED(text)
    Q_UNUSED(assertive)
#endif
}
}  // namespace a11y
