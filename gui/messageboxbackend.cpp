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

#include "messageboxbackend.h"

#include <QVariantMap>

MessageBoxBackend::MessageBoxBackend(QObject *parent) : QObject(parent), m_visible(false)
{}

void MessageBoxBackend::showQuestion(const QString &message, const QString &infoText, std::function<void(MessageBoxBackend::StandardButton)> callback,
                                     const QList<Button> &buttons, MessageBoxBackend::StandardButton defaultButton,
                                     MessageBoxBackend::ButtonOrientation orientation, bool usePlatformOrder)
{
    m_message = message;
    emit messageChanged();

    m_informativeText = infoText;
    emit informativeTextChanged();

    m_callback = callback;
    m_customTexts.clear();

    m_buttonOrientation = orientation;
    emit buttonOrientationChanged();

    m_buttons.clear();
    m_defaultButtonIndex = -1;

    // Build list of buttons to process
    QList<Button> orderedButtons = buttons;
    if (usePlatformOrder)
    {
        // Create StandardButtons flag from the button list
        StandardButtons buttonFlags = StandardButtons();
        for (const Button &btn : buttons)
        {
            buttonFlags |= btn.button;
        }

        // Get platform order
        QList<StandardButton> platformOrder = getPlatformButtonOrder(buttonFlags);

        // Reorder buttons according to platform order
        orderedButtons.clear();
        for (StandardButton platformBtn : platformOrder)
        {
            // Find matching button in original list
            for (const Button &btn : buttons)
            {
                if (btn.button == platformBtn)
                {
                    orderedButtons.append(btn);
                    break;
                }
            }
        }
    }

    // Process buttons
    int index = 0;
    for (const Button &btn : orderedButtons)
    {
        QVariantMap buttonMap;
        buttonMap["value"] = btn.button;
        buttonMap["text"] = btn.text.isEmpty() ? buttonText(btn.button) : btn.text;
        buttonMap["role"] = btn.role;
        m_buttons.append(buttonMap);

        // Track default button index
        if (btn.button == defaultButton)
        {
            m_defaultButtonIndex = index;
        }
        index++;
    }

    // If default button not found, default to first button
    if (m_defaultButtonIndex == -1 && !m_buttons.isEmpty())
    {
        m_defaultButtonIndex = 0;
    }

    emit buttonsChanged();
    emit defaultButtonIndexChanged();
    setVisible(true);
}

void MessageBoxBackend::handleButtonClicked(int button)
{
    setVisible(false);

    if (m_callback)
    {
        m_callback(static_cast<MessageBoxBackend::StandardButton>(button));
        m_callback = nullptr;
    }
}

void MessageBoxBackend::setVisible(bool visible)
{
    if (m_visible != visible)
    {
        m_visible = visible;
        emit visibleChanged();
    }
}

void MessageBoxBackend::setupButtons(MessageBoxBackend::StandardButtons buttons, MessageBoxBackend::StandardButton defaultButton,
                                     const QList<MessageBoxBackend::StandardButton> &customOrder)
{
    setupButtons(buttons, QMap<MessageBoxBackend::StandardButton, QString>(), defaultButton, customOrder);
}

void MessageBoxBackend::setupButtons(StandardButtons buttons, const QMap<MessageBoxBackend::StandardButton, QString> &customTexts,
                                     MessageBoxBackend::StandardButton defaultButton, const QList<MessageBoxBackend::StandardButton> &customOrder)
{
    m_buttons.clear();
    m_defaultButtonIndex = -1;

    // Use custom order if provided, otherwise get platform-specific button order
    QList<MessageBoxBackend::StandardButton> orderedButtons = customOrder.isEmpty() ? getPlatformButtonOrder(buttons) : customOrder;

    int index = 0;
    for (MessageBoxBackend::StandardButton button : orderedButtons)
    {
        if (buttons & button)
        {
            QVariantMap btn;
            btn["value"] = button;
            btn["text"] = customTexts.contains(button) ? customTexts[button] : buttonText(button);
            m_buttons.append(btn);

            // Track default button index
            if (button == defaultButton)
            {
                m_defaultButtonIndex = index;
            }
            index++;
        }
    }

    // If default button not found, default to first button
    if (m_defaultButtonIndex == -1 && !m_buttons.isEmpty())
    {
        m_defaultButtonIndex = 0;
    }

    emit buttonsChanged();
    emit defaultButtonIndexChanged();
}

QString MessageBoxBackend::buttonText(MessageBoxBackend::StandardButton button)
{
    switch (button)
    {
        case MessageBoxBackend::StandardButton::Ok:
            return tr("OK");
        case MessageBoxBackend::StandardButton::Cancel:
            return tr("Cancel");
        case MessageBoxBackend::StandardButton::Yes:
            return tr("Yes");
        case MessageBoxBackend::StandardButton::No:
            return tr("No");
        case MessageBoxBackend::StandardButton::Abort:
            return tr("Abort");
        case MessageBoxBackend::StandardButton::Retry:
            return tr("Retry");
        case MessageBoxBackend::StandardButton::Ignore:
            return tr("Ignore");
        default:
            return "";
    }
}

QList<MessageBoxBackend::StandardButton> MessageBoxBackend::getPlatformButtonOrder(MessageBoxBackend::StandardButtons buttons)
{
    QList<MessageBoxBackend::StandardButton> order;

#if defined(Q_OS_MACOS)
    // macOS: Action buttons on right, Cancel on left
    // Order: Cancel, Abort, Retry, Ignore, No, Yes, Ok
    if (buttons & Cancel)
    {
        order.append(Cancel);
    }
    if (buttons & Abort)
    {
        order.append(Abort);
    }
    if (buttons & Retry)
    {
        order.append(Retry);
    }
    if (buttons & Ignore)
    {
        order.append(Ignore);
    }
    if (buttons & No)
    {
        order.append(No);
    }
    if (buttons & Yes)
    {
        order.append(Yes);
    }
    if (buttons & Ok)
    {
        order.append(Ok);
    }
#elif defined(Q_OS_ANDROID)
    // Android: Positive actions on right, negative on left
    // Similar to macOS
    if (buttons & Cancel)
    {
        order.append(Cancel);
    }
    if (buttons & No)
    {
        order.append(No);
    }
    if (buttons & Abort)
    {
        order.append(Abort);
    }
    if (buttons & Retry)
    {
        order.append(Retry);
    }
    if (buttons & Ignore)
    {
        order.append(Ignore);
    }
    if (buttons & Yes)
    {
        order.append(Yes);
    }
    if (buttons & Ok)
    {
        order.append(Ok);
    }
#else
    // Windows/Linux: Action buttons on left, Cancel on right
    // Order: Ok, Yes, No, Retry, Ignore, Abort, Cancel
    if (buttons & Ok)
    {
        order.append(Ok);
    }
    if (buttons & Yes)
    {
        order.append(Yes);
    }
    if (buttons & No)
    {
        order.append(No);
    }
    if (buttons & Retry)
    {
        order.append(Retry);
    }
    if (buttons & Ignore)
    {
        order.append(Ignore);
    }
    if (buttons & Abort)
    {
        order.append(Abort);
    }
    if (buttons & Cancel)
    {
        order.append(Cancel);
    }
#endif

    return order;
}

void MessageBoxBackend::setMessage(const QString &message)
{
    if (m_message == message)
    {
        return;
    }
    m_message = message;
    emit messageChanged();
}
