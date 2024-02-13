#ifndef AUDIORECITEMDIALOG_H
#define AUDIORECITEMDIALOG_H

#include <QDialog>
#include "slmodel.h"
#include "audiorecscheduleitem.h"

namespace Ui {
class AudioRecItemDialog;
}

class AudioRecItemDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AudioRecItemDialog(QLocale locale, SLModel * slModel, QWidget *parent = nullptr);
    ~AudioRecItemDialog();

    const AudioRecScheduleItem & itemData() const;
    void setItemData(const AudioRecScheduleItem &newItemData);

private:
    Ui::AudioRecItemDialog *ui;
    QLocale m_locale;
    AudioRecScheduleItem m_itemData;
    SLModel * m_slModel;

    void alignUiState();
    void onStartDateChanged();
    void onStartTimeChanged(const QTime & time);
    void onDurationChanged(const QDateTime &duration);
    void onServiceSelection(const QItemSelection &selection);
    void updateEndTime();
};

#endif // AUDIORECITEMDIALOG_H
