#ifndef SETUPDIALOG_H
#define SETUPDIALOG_H

#include <QDialog>
#include <QWidget>
#include <QList>
#include "inputdevice.h"
#include "rawfileinput.h"

QT_BEGIN_NAMESPACE
namespace Ui { class SetupDialog; }
QT_END_NAMESPACE

class SetupDialog : public QDialog
{
    Q_OBJECT
public:
    SetupDialog(QWidget *parent = nullptr);    

    QString getInputFileName() const;
    RawFileInputFormat getInputFileFormat() const;
    bool isFileLoopActive() const;
    void setInputFile(const QString &value, const RawFileInputFormat &format, bool loop);
    bool getDAGCState() const;
    void setDAGCState(bool ena);

public slots:
    void setGainValues(const QList<int> * pList);
    void enableRtlSdrInput(bool ena);
    void setInputDevice(const InputDeviceId & inputDevice);
    void resetFilename();
    void enableFileSelection(bool ena);

signals:
    void setGainMode(GainMode mode);
    void setGain(int gain);
    void setDAGC(bool ena);
    void inputDeviceChanged(const InputDeviceId & inputDevice);
    void sampleFormat(const RawFileInputFormat &format);
    void rawFileSelected(const QString & filename, const RawFileInputFormat &format);
    void rawFileStop();
    void fileLoopingEnabled(bool ena);

private slots:
    void on_gainCombo_currentIndexChanged(int index);
    void on_inputCombo_currentIndexChanged(int index);
    void on_openFileButton_clicked();
    void on_fileFormatCombo_currentIndexChanged(int index);
    void on_loopCheckbox_stateChanged(int state);
    void on_dagcCheckBox_stateChanged(int state);

private:
    Ui::SetupDialog *ui;
    bool openFileButton = false;
    QString inputFileName;
};

#endif // SETUPDIALOG_H
