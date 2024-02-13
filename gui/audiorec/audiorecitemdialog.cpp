#include <QPushButton>
#include "audiorecitemdialog.h"
#include "ui_audiorecitemdialog.h"
#include "epgtime.h"

AudioRecItemDialog::AudioRecItemDialog(QLocale locale, SLModel *slModel, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AudioRecItemDialog)
    , m_locale(locale)
    , m_slModel(slModel)
{
    ui->setupUi(this);

    // initialize item using EPGTime or current time if not valid
    QDateTime startTime = QDateTime::currentDateTime();
    if (EPGTime::getInstance()->isValid())
    {   // valid EPG time
        startTime = EPGTime::getInstance()->currentTime();
    }

    ui->startDateCalendar->setMinimumDate(QDate(startTime.date().year(), startTime.date().month(), startTime.date().day()));
    ui->startDateCalendar->setLocale(m_locale);
    ui->startDateCalendar->setFirstDayOfWeek(Qt::Monday);

    ui->servicelistView->setModel(slModel);
    ui->servicelistView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->servicelistView->setSelectionMode(QAbstractItemView::SingleSelection);


    // init item data
    m_itemData.setName(tr("New audio recording schedule"));
    m_itemData.setStartTime(startTime);
    m_itemData.setDurationSec(0);

    alignUiState();

    connect(ui->startTimeEdit, &QDateEdit::timeChanged, this, &AudioRecItemDialog::onStartTimeChanged);
    connect(ui->durationEdit, &QDateEdit::dateTimeChanged, this, &AudioRecItemDialog::onDurationChanged);
    connect(ui->startDateCalendar, &QCalendarWidget::selectionChanged, this, &AudioRecItemDialog::onStartDateChanged);
    connect(ui->servicelistView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &AudioRecItemDialog::onServiceSelection);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, [this]() { m_itemData.setName(ui->nameEdit->text()); } );

    QTimer::singleShot(10, this, [this](){ resize(minimumSizeHint()); } );
}

AudioRecItemDialog::~AudioRecItemDialog()
{
    delete ui;
}

const AudioRecScheduleItem &AudioRecItemDialog::itemData() const
{
    return m_itemData;
}

void AudioRecItemDialog::setItemData(const AudioRecScheduleItem &newItemData)
{
    m_itemData = newItemData;
    alignUiState();
}

void AudioRecItemDialog::alignUiState()
{
    // block signals while UI is set to be aligned with m_itemData
    const QSignalBlocker blocker(this);

    ui->nameEdit->setText(m_itemData.name());
    ui->startDateCalendar->setSelectedDate(m_itemData.startTime().date());

    ui->startTimeEdit->setTime(m_itemData.startTime().time());
    ui->durationEdit->setTime(m_itemData.duration());
    updateEndTime();

    bool found = false;
    QModelIndex index;
    for (int r = 0; r < m_slModel->rowCount(); ++r)
    {
        index = m_slModel->index(r, 0);
        if (m_slModel->id(index) == m_itemData.serviceId())
        {   // found
            ui->servicelistView->selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Current);
            found = true;
            break;
        }
    }
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(found && (m_itemData.durationSec() != 0));

}

void AudioRecItemDialog::onStartDateChanged()
{
    QDateTime start;
    start.setDate(ui->startDateCalendar->selectedDate());
    start.setTime(ui->startTimeEdit->time());

    m_itemData.setStartTime(start);
    updateEndTime();
}

void AudioRecItemDialog::onStartTimeChanged(const QTime & time)
{
    QDateTime start;
    start.setDate(ui->startDateCalendar->selectedDate());
    start.setTime(time);

    m_itemData.setStartTime(start);
    updateEndTime();
}

void AudioRecItemDialog::onDurationChanged(const QDateTime & duration)
{
    m_itemData.setDuration(duration.time());
    updateEndTime();

    bool serviceSelected = !ui->servicelistView->selectionModel()->selection().isEmpty();
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(serviceSelected && (m_itemData.durationSec() != 0));
}

void AudioRecItemDialog::onServiceSelection(const QItemSelection &selection)
{
    QModelIndexList indexes = selection.indexes();
    if (!indexes.isEmpty()) {
        m_itemData.setServiceId(ServiceListId(m_slModel->data(indexes.at(0), SLModelRole::IdRole).value<uint64_t>()));
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m_itemData.durationSec() != 0);
    }
}

void AudioRecItemDialog::updateEndTime()
{
    ui->endTime->setText(m_locale.toString(m_itemData.endTime(), QString("dd.MM, hh:mm")));
}
