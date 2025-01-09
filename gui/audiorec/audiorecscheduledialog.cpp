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

#include "audiorecscheduledialog.h"

#include <QDebug>

#include "audiorecitemdialog.h"
#include "ui_audiorecscheduledialog.h"

AudioRecScheduleDialog::AudioRecScheduleDialog(AudioRecScheduleModel *model, SLModel *slModel, QWidget *parent)
    : QDialog(parent), ui(new Ui::AudioRecScheduleDialog), m_model(model), m_slModel(slModel)
{
    ui->setupUi(this);
    m_locale = QLocale::C;

    ui->scheduleTableView->setModel(m_model);
    ui->scheduleTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->scheduleTableView->horizontalHeader()->setStretchLastSection(true);
    ui->scheduleTableView->verticalHeader()->hide();
    ui->scheduleTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->scheduleTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->scheduleTableView->setSortingEnabled(false);
    ui->scheduleTableView->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);

    connect(ui->addButton, &QPushButton::clicked, this, QOverload<>().of(&AudioRecScheduleDialog::addItem));

    connect(ui->editButton, &QPushButton::clicked, this, &AudioRecScheduleDialog::editItem);
    ui->editButton->setEnabled(false);

    connect(ui->deleteButton, &QPushButton::clicked, this, &AudioRecScheduleDialog::removeItem);
    ui->deleteButton->setEnabled(false);

    connect(ui->clearButton, &QPushButton::clicked, this, &AudioRecScheduleDialog::deleteAll);
    connect(ui->scheduleTableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &AudioRecScheduleDialog::updateActions);
    connect(m_model, &QAbstractItemModel::dataChanged, this, &AudioRecScheduleDialog::resizeTableColumns);
    connect(m_model, &QAbstractItemModel::modelReset, this, &AudioRecScheduleDialog::resizeTableColumns);
    connect(m_model, &QAbstractItemModel::rowsRemoved, this, &AudioRecScheduleDialog::resizeTableColumns);
    connect(m_model, &QAbstractItemModel::rowsInserted, this, &AudioRecScheduleDialog::resizeTableColumns);

    resizeTableColumns();
    adjustSize();
}

AudioRecScheduleDialog::~AudioRecScheduleDialog()
{
    delete ui;
}

void AudioRecScheduleDialog::addItem()
{
    AudioRecItemDialog dialog(m_locale, m_slModel);
    dialog.setWindowTitle(tr("New recording schedule"));

    if (dialog.exec())
    {
        m_model->insertItem(dialog.itemData());
    }
    resizeTableColumns();
    updateActions(ui->scheduleTableView->selectionModel()->selection());
}

void AudioRecScheduleDialog::addItem(const AudioRecScheduleItem &item)
{
    AudioRecItemDialog dialog(m_locale, m_slModel);
    dialog.setWindowTitle(tr("New recording schedule"));
    dialog.setItemData(item);
    if (dialog.exec())
    {
        m_model->insertItem(dialog.itemData());
    }
    resizeTableColumns();
    updateActions(ui->scheduleTableView->selectionModel()->selection());
}

void AudioRecScheduleDialog::editItem()
{
    QItemSelectionModel *selectionModel = ui->scheduleTableView->selectionModel();
    const QModelIndexList indexes = selectionModel->selectedRows();
    if (indexes.size() > 0)
    {
        QModelIndex index = indexes.at(0);
        AudioRecItemDialog dialog(m_locale, m_slModel);
        dialog.setWindowTitle(tr("Edit recording schedule"));
        dialog.setItemData(m_model->itemAtIndex(index));
        if (dialog.exec())
        {
            m_model->replaceItemAtIndex(index, dialog.itemData());
            resizeTableColumns();
        }
    }
    updateActions(ui->scheduleTableView->selectionModel()->selection());
}

void AudioRecScheduleDialog::removeItem()
{
    QItemSelectionModel *selectionModel = ui->scheduleTableView->selectionModel();
    const QModelIndexList indexes = selectionModel->selectedRows();

    for (QModelIndex index : indexes)
    {
        int row = index.row();
        m_model->removeRows(row, 1, QModelIndex());
    }
    updateActions(ui->scheduleTableView->selectionModel()->selection());
}

void AudioRecScheduleDialog::deleteAll()
{
    m_model->clear();
    updateActions(ui->scheduleTableView->selectionModel()->selection());
}

void AudioRecScheduleDialog::setLocale(const QLocale &newLocale)
{
    m_locale = newLocale;
}

void AudioRecScheduleDialog::setServiceListModel(SLModel *newSlModel)
{
    m_slModel = newSlModel;
}

void AudioRecScheduleDialog::updateActions(const QItemSelection &selection)
{
    QModelIndexList indexes = selection.indexes();

    if (!indexes.isEmpty())
    {
        ui->deleteButton->setEnabled(true);
        ui->editButton->setEnabled(true);
    }
    else
    {
        ui->deleteButton->setEnabled(false);
        ui->editButton->setEnabled(false);
    }
    ui->clearButton->setEnabled(!m_model->isEmpty());
}

void AudioRecScheduleDialog::resizeTableColumns()
{
    QAbstractItemModel *tableModel = ui->scheduleTableView->model();

    // Fit to size all but last item
    ui->scheduleTableView->horizontalHeader()->setStretchLastSection(true);
    for (int i = 0; i < tableModel->columnCount() - 1; ++i)
    {
        ui->scheduleTableView->resizeColumnToContents(i);
    }
}
