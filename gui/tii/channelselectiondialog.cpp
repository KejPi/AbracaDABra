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

#include "channelselectiondialog.h"

#include <QCheckBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QPushButton>

#include "dabtables.h"

ChannelSelectionDialog::ChannelSelectionDialog(const QMap<uint32_t, bool> &channelSelection, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Channel selection"));
    setSizeGripEnabled(false);

    m_checkCntr = 0;
    QGridLayout *mainLayout = new QGridLayout(this);
    int col = 0;
    int row = 0;
    for (auto it = DabTables::channelList.cbegin(); it != DabTables::channelList.cend(); ++it)
    {
        auto checkbox = new QCheckBox();
        checkbox->setChecked(channelSelection.value(it.key()));
        m_checkCntr += checkbox->isChecked();
        checkbox->setText(it.value());
        if (row == 4)
        {
            mainLayout->setColumnMinimumWidth(col, 50);
            col += 1;
            row = 0;
        }
        mainLayout->addWidget(checkbox, row++, col);
        connect(checkbox, &QCheckBox::clicked, this, &ChannelSelectionDialog::onCheckBoxClicked);
        m_chList.insert(it.key(), checkbox);
    }

    mainLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum), 4, 0);

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    m_acceptButton = buttonBox->button(QDialogButtonBox::Ok);
    m_acceptButton->setEnabled(m_checkCntr > 0);

    auto selectAll = new QPushButton(tr("Select all"), this);
    auto unselectAll = new QPushButton(tr("Unselect all"), this);
    connect(selectAll, &QPushButton::clicked, this, [this]() { checkAll(true); });
    connect(unselectAll, &QPushButton::clicked, this, [this]() { checkAll(false); });

    buttonBox->addButton(selectAll, QDialogButtonBox::ActionRole);
    buttonBox->addButton(unselectAll, QDialogButtonBox::ActionRole);

    mainLayout->addWidget(buttonBox, 5, 0, 2, (13 + 1 - 5 + 1));

    setFixedSize(sizeHint());
}

ChannelSelectionDialog::~ChannelSelectionDialog()
{
    qDeleteAll(m_chList);
}

void ChannelSelectionDialog::getChannelList(QMap<uint32_t, bool> &channelSelection) const
{
    for (auto it = m_chList.cbegin(); it != m_chList.cend(); ++it)
    {
        channelSelection[it.key()] = it.value()->isChecked();
    }
}

void ChannelSelectionDialog::checkAll(bool check)
{
    for (auto checkbox : m_chList)
    {
        checkbox->setChecked(check);
    }
    if (check)
    {
        m_checkCntr = DabTables::channelList.count();
        m_acceptButton->setEnabled(true);
    }
    else
    {
        m_checkCntr = 0;
        m_acceptButton->setEnabled(false);
    }
}

void ChannelSelectionDialog::onCheckBoxClicked(bool checked)
{
    if (checked)
    {
        m_checkCntr += 1;
    }
    else
    {
        m_checkCntr -= 1;
    }
    m_acceptButton->setEnabled(m_checkCntr > 0);
}
