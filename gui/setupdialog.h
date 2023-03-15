/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2023 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#ifndef SETUPDIALOG_H
#define SETUPDIALOG_H

#include <QDialog>
#include <QWidget>
#include <QList>
#include <QAbstractButton>
#include <QLocale>
#include "QtWidgets/qcheckbox.h"
#include "QtWidgets/qlabel.h"
#include "config.h"
#include "inputdevice.h"
#if HAVE_AIRSPY
#include "airspyinput.h"
#endif
#include "rawfileinput.h"
#include "dabtables.h"

QT_BEGIN_NAMESPACE
namespace Ui { class SetupDialog; }
QT_END_NAMESPACE

enum class ApplicationStyle { Default = 0, Light, Dark};

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
            RtlGainMode gainMode;
            int gainIdx;            
            int bandwidth;
            bool biasT;
        } rtlsdr;
        struct
        {
            RtlGainMode gainMode;
            int gainIdx;
            QString tcpAddress;
            int tcpPort;
        } rtltcp;
#if HAVE_AIRSPY
        struct
        {
            AirspyGainStr gain;
            bool biasT;
            bool dataPacking;
            bool prefer4096kHz;
        } airspy;
#endif
#if HAVE_SOAPYSDR
        struct
        {
            SoapyGainMode gainMode;
            int gainIdx;
            QString devArgs;
            QString antenna;
            int channel;
            int bandwidth;
        } soapysdr;
#endif
        uint16_t announcementEna;
        bool bringWindowToForeground;
        ApplicationStyle applicationStyle;
        QLocale::Language lang;
        bool expertModeEna;
        bool dlPlusEna;
        int noiseConcealmentLevel;
        bool xmlHeaderEna;
    };

    SetupDialog(QWidget *parent = nullptr);
    Settings settings() const;
    void setGainValues(const QList<float> & gainList);
    void resetInputDevice();
    void onExpertMode(bool ena);    
    void setSettings(const Settings &settings);
    void setXmlHeader(const InputDeviceDescription & desc);
    QLocale::Language applicationLanguage() const;

signals:
    void inputDeviceChanged(const InputDeviceId & inputDevice);
    void newInputDeviceSettings();
    void newAnnouncementSettings(uint16_t enaFlags);
    void expertModeToggled(bool enabled);
    void applicationStyleChanged(ApplicationStyle style);
    void noiseConcealmentLevelChanged(int level);
    void xmlHeaderToggled(bool enabled);
protected:
    void showEvent(QShowEvent *event);

private:
    enum SetupDialogTabs { Device = 0, Announcement = 1, Other = 2 };
    enum SetupDialogXmlHeader { XMLDate = 0, XMLRecorder, XMLDevice,
                                XMLSampleRate, XMLFreq, XMLLength, XMLFormat,
                                XMLNumLabels};

    const QList<QLocale::Language> m_supportedLocalization = { QLocale::Czech, QLocale::German, QLocale::Polish };

    Ui::SetupDialog *ui;
    Settings m_settings;
    QString m_rawfilename;
    QList<float> m_rtlsdrGainList;
    QList<float> m_rtltcpGainList;
    QList<float> m_soapysdrGainList;
    QCheckBox * m_announcementCheckBox[static_cast<int>(DabAnnouncement::Undefined)];
    QCheckBox * m_bringWindowToForegroundCheckbox;
    QLabel * m_xmlHeaderLabel[SetupDialogXmlHeader::XMLNumLabels];

    void setStatusLabel();

    void onButtonClicked(QAbstractButton *button);
    void onInputChanged(int index);
    void onOpenFileButtonClicked();

    void onConnectDeviceClicked();

    void onRtlGainModeToggled(bool checked);
    void onRtlSdrGainSliderChanged(int val);
    void activateRtlSdrControls(bool en);

    void onTcpGainModeToggled(bool checked);
    void onRtlTcpGainSliderChanged(int val);
    void onRtlTcpIpAddrEditFinished();
    void onRtlTcpPortValueChanged(int val);
    void activateRtlTcpControls(bool en);

    void onRawFileFormatChanged(int idx);    
    void onAnnouncementClicked();
    void onBringWindowToForegroundClicked(bool checked);

    void onStyleChecked(bool checked);
    void onExpertModeChecked(bool checked);
    void onDLPlusChecked(bool checked);
    void onLanguageChanged(int index);
    void onNoiseLevelChanged(int index);    
    void onXmlHeaderChecked(bool checked);

#if HAVE_AIRSPY
    void onAirspyModeToggled(bool checked);
    void onAirspySensitivityGainSliderChanged(int val);
    void onAirspyIFGainSliderChanged(int val);
    void onAirspyLNAGainSliderChanged(int val);
    void onAirspyMixerGainSliderChanged(int val);
    void onAirspyLNAAGCstateChanged(int state);
    void onAirspyMixerAGCstateChanged(int state);
    void activateAirspyControls(bool en);
#endif
#if HAVE_SOAPYSDR
    void onSoapySdrGainSliderChanged(int val);
    void onSoapySdrDevArgsEditFinished();
    void onSoapySdrAntennaEditFinished();
    void onSoapySdrChannelEditFinished();
    void onSoapySdrGainModeToggled(bool checked);
    void activateSoapySdrControls(bool en);
#endif        
};

#endif // SETUPDIALOG_H
