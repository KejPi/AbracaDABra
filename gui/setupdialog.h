#ifndef SETUPDIALOG_H
#define SETUPDIALOG_H

#include <QDialog>
#include <QWidget>
#include <QList>
#include <QAbstractButton>
#include "config.h"
#include "inputdevice.h"
#include "rawfileinput.h"

QT_BEGIN_NAMESPACE
namespace Ui { class SetupDialog; }
QT_END_NAMESPACE

class SetupDialog : public QDialog
{
    Q_OBJECT
public:
    // this is to store active state
    struct Settings
    {
        InputDeviceId inputDevice;
        struct
        {
            QString file;
            RawFileInputFormat format;
            bool loopEna;
        } rawfile;
        struct
        {
            int gainIdx;
            QString tcpAddress;
            int tcpPort;
        } rtltcp;
#ifdef HAVE_RARTTCP
        struct
        {
            QString tcpAddress;
            int tcpPort;
        } rarttcp;
#endif
        struct
        {
            int gainIdx;
            int bandwidth;
            bool biasT;
        } rtlsdr;
    };

    SetupDialog(QWidget *parent = nullptr);
    Settings settings() const;
    void setGainValues(const QList<int> * pList = nullptr);
    void resetInputDevice();
    void onExpertMode(bool ena);
    void setSettings(const Settings &settings);

signals:
    void inputDeviceChanged(const InputDeviceId & inputDevice);
    void newSettings();

protected:
    void showEvent(QShowEvent *event);

private:
    Ui::SetupDialog *ui;
    Settings m_settings;
    QString rawfilename;

    void onButtonClicked(QAbstractButton *button);
    void onInputChanged(int index);
    void onOpenFileButtonClicked();
    void applySettings();
    void applyEnable();
};

#endif // SETUPDIALOG_H
