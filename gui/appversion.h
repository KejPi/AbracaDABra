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

#ifndef APPVERSION_H
#define APPVERSION_H

#include <QDebug>
#include <QRegularExpression>
#include <QString>

class AppVersion
{
public:
    AppVersion() {}
    AppVersion(const QString& verString)
    {
        static const QRegularExpression verRe("v(\\d+)\\.(\\d+)\\.(\\d+)(-(\\d+))?");
        QRegularExpressionMatch verMatch = verRe.match(verString);
        if (verMatch.hasMatch())
        {
            m_major = verMatch.captured(1).toUInt();
            m_minor = verMatch.captured(2).toUInt();
            m_patch = verMatch.captured(3).toUInt();
            if (!verMatch.captured(5).isNull())
            {
                m_git = verMatch.captured(5).toUInt();
            }
            // qDebug() << m_major << m_minor << m_patch << m_git;
        }
    }
    bool operator==(const AppVersion& other) const { return toUInt64() == other.toUInt64(); }
    bool operator>(const AppVersion& other) const { return toUInt64() > other.toUInt64(); }
    bool operator>=(const AppVersion& other) const { return toUInt64() >= other.toUInt64(); }
    bool operator<(const AppVersion& other) const { return !(*this >= other); }
    bool operator<=(const AppVersion& other) const { return !(*this > other); }
    bool isValid() const { return m_major != 0; }

private:
    uint m_major = 0;
    uint m_minor = 0;
    uint m_patch = 0;
    uint m_git = 0;
    uint64_t toUInt64() const { return uint64_t((m_major << 30) + (m_minor << 20) + (m_patch << 10) + m_git); }
};

#endif  // APPVERSION_H
