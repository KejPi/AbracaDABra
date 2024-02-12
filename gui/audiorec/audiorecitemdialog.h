#ifndef AUDIORECITEMDIALOG_H
#define AUDIORECITEMDIALOG_H

#include <QDialog>
#include "audiorecscheduleitem.h"

namespace Ui {
class AudioRecItemDialog;
}

class AudioRecItemDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AudioRecItemDialog(QLocale locale, QWidget *parent = nullptr);
    ~AudioRecItemDialog();

    const AudioRecScheduleItem & itemData() const;
    void setItemData(const AudioRecScheduleItem &newItemData);

private:
    Ui::AudioRecItemDialog *ui;
    QLocale m_locale;
    AudioRecScheduleItem m_itemData;

    void alignUiState();
    void onStartDateChanged();
    void onStartTimeChanged(const QTime & time);
    void onDurationChanged(const QDateTime &duration);
    void updateEndTime();
};

#endif // AUDIORECITEMDIALOG_H
