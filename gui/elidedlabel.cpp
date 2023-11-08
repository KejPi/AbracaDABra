// this sourcecode is from: https://wiki.qt.io/Elided_Label

#include "elidedlabel.h"

#include <QPaintEvent>
#include <QResizeEvent>

void ElidedLabel::setElideMode(Qt::TextElideMode elideMode)
{
    m_elideMode = elideMode;
    m_cachedText.clear();
    update();
}

void ElidedLabel::resizeEvent(QResizeEvent *e)
{
    QLabel::resizeEvent(e);
    m_cachedText.clear();
}

void ElidedLabel::paintEvent(QPaintEvent *e)
{
    if (m_elideMode == Qt::ElideNone)
        return QLabel::paintEvent(e);

    updateCachedTexts();
    QLabel::setText(m_cachedElidedText);
    QLabel::paintEvent(e);
    QLabel::setText(m_cachedText);
}

void ElidedLabel::updateCachedTexts()
{
    // setText() is not virtual ... :/
    const auto txt = text();
    if (m_cachedText == txt)
        return;
    m_cachedText = txt;
    const QFontMetrics fm(fontMetrics());
    m_cachedElidedText = fm.elidedText(text(),
                                       m_elideMode,
                                       width(),
                                       Qt::TextShowMnemonic);
    // make sure to show at least the first character
    if (!m_cachedText.isEmpty())
    {
        const QString showFirstCharacter = m_cachedText.at(0) + QStringLiteral("...");
        setMinimumWidth(fm.horizontalAdvance(showFirstCharacter) + 1);
    }
}
