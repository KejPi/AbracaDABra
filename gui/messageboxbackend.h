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

#ifndef MESSAGEBOXBACKEND_H
#define MESSAGEBOXBACKEND_H

#include <QFlags>
#include <QObject>
#include <QVariant>
#include <QtQmlIntegration>
#include <functional>

class MessageBoxBackend : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString message READ message WRITE setMessage NOTIFY messageChanged FINAL)
    Q_PROPERTY(QString informativeText READ informativeText NOTIFY informativeTextChanged FINAL)
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged FINAL)
    Q_PROPERTY(QVariantList buttons READ buttons NOTIFY buttonsChanged FINAL)
    Q_PROPERTY(ButtonOrientation buttonOrientation READ buttonOrientation NOTIFY buttonOrientationChanged FINAL)
    Q_PROPERTY(int defaultButtonIndex READ defaultButtonIndex NOTIFY defaultButtonIndexChanged FINAL)

public:
    enum StandardButton
    {  // buttons are rendered in this order
        Ok = 0x0001,
        Yes = 0x0004,
        No = 0x0008,
        Cancel = 0x0002,
        Retry = 0x0020,
        Abort = 0x0010,
        Ignore = 0x0040
    };
    Q_ENUM(StandardButton)
    Q_DECLARE_FLAGS(StandardButtons, StandardButton)

    enum ButtonOrientation
    {
        Horizontal,
        Vertical
    };
    Q_ENUM(ButtonOrientation)

    enum ButtonRole
    {
        Neutral,
        Positive,
        Negative
    };
    Q_ENUM(ButtonRole)

    struct Button
    {
        MessageBoxBackend::StandardButton button;
        QString text;
        ButtonRole role;
    };

    explicit MessageBoxBackend(QObject *parent = nullptr);

    // Show dialog with callback
    void showQuestion(const QString &message, const QString &infoText, std::function<void(MessageBoxBackend::StandardButton)> callback,
                      const QList<Button> &buttons, MessageBoxBackend::StandardButton defaultButton = MessageBoxBackend::StandardButton::Ok,
                      MessageBoxBackend::ButtonOrientation orientation = MessageBoxBackend::ButtonOrientation::Horizontal,
                      bool usePlatformOrder = true);

    Q_INVOKABLE void handleButtonClicked(int button);

    // Property getters
    QString message() const { return m_message; }
    QString informativeText() const { return m_informativeText; }
    bool visible() const { return m_visible; }
    QVariantList buttons() const { return m_buttons; }
    ButtonOrientation buttonOrientation() const { return m_buttonOrientation; }
    int defaultButtonIndex() const { return m_defaultButtonIndex; }

    // Property setters
    void setMessage(const QString &message);

signals:
    void messageChanged();
    void visibleChanged();
    void buttonsChanged();
    void informativeTextChanged();
    void buttonOrientationChanged();
    void defaultButtonIndexChanged();

private:
    void setVisible(bool visible);
    void setupButtons(MessageBoxBackend::StandardButtons buttons, MessageBoxBackend::StandardButton defaultButton,
                      const QList<MessageBoxBackend::StandardButton> &customOrder = QList<MessageBoxBackend::StandardButton>());
    void setupButtons(MessageBoxBackend::StandardButtons buttons, const QMap<MessageBoxBackend::StandardButton, QString> &customTexts,
                      MessageBoxBackend::StandardButton defaultButton,
                      const QList<MessageBoxBackend::StandardButton> &customOrder = QList<MessageBoxBackend::StandardButton>());
    QString buttonText(MessageBoxBackend::StandardButton button);
    QList<MessageBoxBackend::StandardButton> getPlatformButtonOrder(MessageBoxBackend::StandardButtons buttons);

    QString m_message;
    bool m_visible;
    QVariantList m_buttons;
    std::function<void(MessageBoxBackend::StandardButton)> m_callback;
    QMap<MessageBoxBackend::StandardButton, QString> m_customTexts;
    QString m_informativeText;
    ButtonOrientation m_buttonOrientation = ButtonOrientation::Horizontal;
    int m_defaultButtonIndex = 0;
};

#endif  // MESSAGEBOXBACKEND_H
