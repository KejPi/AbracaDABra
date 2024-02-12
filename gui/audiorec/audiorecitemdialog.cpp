#include "audiorecitemdialog.h"
#include "ui_audiorecitemdialog.h"
#include "epgtime.h"

AudioRecItemDialog::AudioRecItemDialog(QLocale locale, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AudioRecItemDialog)
    , m_locale(locale)
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

    // init item data
    m_itemData.setName(tr("New audio recording schedule"));
    m_itemData.setStartTime(startTime);
    m_itemData.setDurationSec(60);

    alignUiState();

    connect(ui->startTimeEdit, &QDateEdit::timeChanged, this, &AudioRecItemDialog::onStartTimeChanged);
    connect(ui->durationEdit, &QDateEdit::dateTimeChanged, this, &AudioRecItemDialog::onDurationChanged);
    connect(ui->startDateCalendar, &QCalendarWidget::selectionChanged, this, &AudioRecItemDialog::onStartDateChanged);

    adjustSize();
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
}

void AudioRecItemDialog::updateEndTime()
{
    ui->endTime->setText(m_locale.toString(m_itemData.endTime(), QString("dd.MM, hh:mm")));
}
