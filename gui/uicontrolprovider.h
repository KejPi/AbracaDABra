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

#ifndef UICONTROLPROVIDER_H
#define UICONTROLPROVIDER_H

#include <QMetaProperty>
#include <QObject>
#include <QVariantMap>

#define UI_PROPERTY(type, name)                                     \
    Q_PROPERTY(type name READ name WRITE name NOTIFY name##Changed) \
public:                                                             \
    type name() const                                               \
    {                                                               \
        return m_##name;                                            \
    }                                                               \
    void name(const type &value)                                    \
    {                                                               \
        if (m_##name != value)                                      \
        {                                                           \
            m_##name = value;                                       \
            emit name##Changed();                                   \
        }                                                           \
    }                                                               \
Q_SIGNALS:                                                          \
    void name##Changed();                                           \
                                                                    \
protected:                                                          \
    type m_##name{};

#define UI_PROPERTY_DEFAULT(type, name, defaultValue)               \
    Q_PROPERTY(type name READ name WRITE name NOTIFY name##Changed) \
public:                                                             \
    type name() const                                               \
    {                                                               \
        return m_##name;                                            \
    }                                                               \
    void name(const type &value)                                    \
    {                                                               \
        if (m_##name != value)                                      \
        {                                                           \
            m_##name = value;                                       \
            emit name##Changed();                                   \
        }                                                           \
    }                                                               \
Q_SIGNALS:                                                          \
    void name##Changed();                                           \
                                                                    \
protected:                                                          \
    type m_##name = defaultValue;

#define UI_PROPERTY_SETTINGS(type, name, settings)                  \
    Q_PROPERTY(type name READ name WRITE name NOTIFY name##Changed) \
public:                                                             \
    type name() const                                               \
    {                                                               \
        return settings;                                            \
    }                                                               \
    void name(const type &value)                                    \
    {                                                               \
        if (settings != value)                                      \
        {                                                           \
            settings = value;                                       \
            emit name##Changed();                                   \
        }                                                           \
    }                                                               \
Q_SIGNALS:                                                          \
    void name##Changed();                                           \
                                                                    \
protected:

class UIControlProvider : public QObject
{
    Q_OBJECT
public:
    explicit UIControlProvider(QObject *parent = nullptr) : QObject(parent) {}

    // Generic runtime setter via reflection
    Q_INVOKABLE void setValue(const QString &propertyName, const QVariant &value)
    {
        const QByteArray propName = propertyName.toUtf8();
        const QMetaObject *meta = this->metaObject();
        int index = meta->indexOfProperty(propName);
        if (index >= 0)
        {
            QMetaProperty prop = meta->property(index);
            if (prop.isWritable())
            {
                prop.write(this, value);
            }
        }
    }

    // Generic runtime getter
    Q_INVOKABLE QVariant getValue(const QString &propertyName) const
    {
        const QByteArray propName = propertyName.toUtf8();
        const QMetaObject *meta = this->metaObject();
        int index = meta->indexOfProperty(propName);
        if (index >= 0)
        {
            return meta->property(index).read(this);
        }
        return {};
    }

    // Load multiple values from QVariantMap
    Q_INVOKABLE void loadFromMap(const QVariantMap &map)
    {
        const QMetaObject *meta = this->metaObject();
        for (auto it = map.begin(); it != map.end(); ++it)
        {
            int idx = meta->indexOfProperty(it.key().toUtf8());
            if (idx >= 0)
            {
                QMetaProperty prop = meta->property(idx);
                if (prop.isWritable())
                {
                    prop.write(this, it.value());
                }
            }
        }
    }
};

#endif  // UICONTROLPROVIDER_H
