/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2024 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#include <QFileDialog>
#include <QFile>
#include <QDebug>
#include <QStandardItemModel>
#include <QGeoCoordinate>

#include "setupdialog.h"
#include "./ui_setupdialog.h"
#include "audiodecoder.h"

SetupDialog::SetupDialog(QWidget *parent) : QDialog(parent), ui(new Ui::SetupDialog)
{
    ui->setupUi(this);

    // remove question mark from titlebar
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    ui->tabWidget->setTabText(SetupDialogTabs::Device, tr("Device"));
    ui->tabWidget->setTabText(SetupDialogTabs::Announcement, tr("Announcements"));
    ui->tabWidget->setTabText(SetupDialogTabs::Audio, tr("Audio"));
    ui->tabWidget->setTabText(SetupDialogTabs::UserApps, tr("User Applications"));
    ui->tabWidget->setTabText(SetupDialogTabs::Other, tr("Others"));
    ui->tabWidget->setCurrentIndex(SetupDialogTabs::Device);

    ui->inputCombo->addItem("RTL SDR", QVariant(int(InputDeviceId::RTLSDR)));
#if HAVE_AIRSPY
    ui->inputCombo->addItem("Airspy", QVariant(int(InputDeviceId::AIRSPY)));
#endif
#if HAVE_SOAPYSDR
    ui->inputCombo->addItem("Soapy SDR", QVariant(int(InputDeviceId::SOAPYSDR)));
#endif
    ui->inputCombo->addItem("RTL TCP", QVariant(int(InputDeviceId::RTLTCP)));
    ui->inputCombo->addItem(tr("Raw file"), QVariant(int(InputDeviceId::RAWFILE)));
    ui->inputCombo->setCurrentIndex(-1);  // undefined

    ui->fileNameLabel->setElideMode(Qt::ElideLeft);
    ui->fileNameLabel->setText(m_noFileString);
    m_rawfilename = "";

    ui->fileFormatCombo->insertItem(int(RawFileInputFormat::SAMPLE_FORMAT_U8), tr("Unsigned 8 bits"));
    ui->fileFormatCombo->insertItem(int(RawFileInputFormat::SAMPLE_FORMAT_S16), tr("Signed 16 bits"));

    // this has to be aligned with mainwindow
    ui->loopCheckbox->setChecked(false);

    ui->statusLabel->setText("<span style=\"color:red\">"+tr("No device connected")+"</span>");

    QString ipRange = "(?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])";
    QRegularExpression ipRegex ("^" + ipRange
                               +                                + "(\\." + ipRange + ")"
                               +                                + "(\\." + ipRange + ")"
                               +                                + "(\\." + ipRange + ")$");
    QRegularExpressionValidator *ipValidator = new QRegularExpressionValidator(ipRegex, this);
    ui->rtltcpIpAddressEdit->setValidator(ipValidator);
    ui->rtltcpSwAgcMaxLevel->setToolTip(ui->rtlsdrSwAgcMaxLevel->toolTip());
    ui->rtltcpSwAgcMaxLevel->setMinimumWidth(ui->rtlsdrSwAgcMaxLevel->width());

    ui->rtlsdrBiasTCombo->addItem(tr("Off"), false);
    ui->rtlsdrBiasTCombo->addItem(tr("On"), true);
    ui->rtlsdrBiasTCombo->setToolTip(QString(tr("Enable/disable bias tee.<br><br>"
                                                   "<b>WARNING:</b> Before using the bias tee please ensure that you understand "
                                                   "that you should not use this option when the dongle is connected directly "
                                                   "to a DC short circuited antenna unless you are using an LNA.")));

    ui->rtlsdrBandwidth->setToolTip(QString(tr("Input signal bandwidth in kHz. Value '0' means default bandwidth %1 kHz.")).arg(INPUTDEVICE_BANDWIDTH/1000));
    ui->rtlsdrPPM->setToolTip(QString(tr("Input device XTAL frequency correction in PPM.")));
    ui->rtltcpPPM->setToolTip(ui->rtlsdrPPM->toolTip());
    ui->airspyBiasTCombo->addItem(tr("Off"), false);
    ui->airspyBiasTCombo->addItem(tr("On"), true);
    ui->airspyBiasTCombo->setToolTip(ui->rtlsdrBiasTCombo->toolTip());
    ui->soapysdrBandwidth->setToolTip(ui->rtlsdrBandwidth->toolTip());
    ui->soapysdrBandwidth->setMinimumWidth(ui->rtlsdrSwAgcMaxLevel->width());

    // set announcement combos
    QGridLayout *gridLayout = new QGridLayout;
    // alarm announcements
    int ann = static_cast<int>(DabAnnouncement::Alarm);
    m_announcementCheckBox[ann] = new QCheckBox();
    m_announcementCheckBox[ann]->setText(DabTables::getAnnouncementName(static_cast<DabAnnouncement>(ann)));
    m_announcementCheckBox[ann]->setChecked(true);
    m_announcementCheckBox[ann]->setDisabled(true);
    gridLayout->addWidget(m_announcementCheckBox[ann], 0, 0);

    ann = static_cast<int>(DabAnnouncement::AlarmTest);
    m_announcementCheckBox[ann] = new QCheckBox();
    m_announcementCheckBox[ann]->setText(DabTables::getAnnouncementName(static_cast<DabAnnouncement>(ann)));
    connect(m_announcementCheckBox[ann], &QCheckBox::clicked, this, &SetupDialog::onAnnouncementClicked);
    gridLayout->addWidget(m_announcementCheckBox[ann], 0, 1);

    QLabel * label = new QLabel(tr("<br>Note: Alarm announcement cannot be disabled."));
    gridLayout->addWidget(label, 4, 0, 1, 2);
    QGroupBox * groupBox = new QGroupBox(tr("Alarm Announcements"));
    groupBox->setLayout(gridLayout);
    QVBoxLayout *vLayout = new QVBoxLayout;
    vLayout->addWidget(groupBox);

    m_bringWindowToForegroundCheckbox = new QCheckBox();
    m_bringWindowToForegroundCheckbox->setText(tr("Bring window to foreground"));
    m_bringWindowToForegroundCheckbox->setToolTip(tr("Check to bring window to foreground when (test) alarm announcement starts"));
    m_bringWindowToForegroundCheckbox->setChecked(true);
    connect(m_bringWindowToForegroundCheckbox, &QCheckBox::clicked, this, &SetupDialog::onBringWindowToForegroundClicked);
    gridLayout->addWidget(m_bringWindowToForegroundCheckbox, 5, 0, 1, 2);

    // regular announcements
    int row = 0;
    int column = 0;
    gridLayout = new QGridLayout;
    for (int ann = static_cast<int>(DabAnnouncement::Alarm)+1; ann < static_cast<int>(DabAnnouncement::AlarmTest); ++ann)
    {
        m_announcementCheckBox[ann] = new QCheckBox();
        m_announcementCheckBox[ann]->setText(DabTables::getAnnouncementName(static_cast<DabAnnouncement>(ann)));
        connect(m_announcementCheckBox[ann], &QCheckBox::clicked, this, &SetupDialog::onAnnouncementClicked);
        gridLayout->addWidget(m_announcementCheckBox[ann], row++, column);
        if (row > 4)
        {
            row = 0;
            column = 1;
        }
    }
    groupBox = new QGroupBox(tr("Regular Announcements"));
    groupBox->setLayout(gridLayout);
    vLayout->addWidget(groupBox);

    QSpacerItem * verticalSpacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
    vLayout->addItem(verticalSpacer);
    ui->tabAnnouncement->setLayout(vLayout);


    //gridLayout_4->addWidget(xmlHeaderWidget, 3, 0, 1, 3);
    gridLayout = new QGridLayout;
    label = new QLabel(tr("Recording date:"));
    m_xmlHeaderLabel[SetupDialogXmlHeader::XMLDate] = new QLabel;
    gridLayout->addWidget(label, SetupDialogXmlHeader::XMLDate, 0);
    gridLayout->addWidget(m_xmlHeaderLabel[SetupDialogXmlHeader::XMLDate], SetupDialogXmlHeader::XMLDate, 1);
    label = new QLabel(tr("Recorder:"));
    m_xmlHeaderLabel[SetupDialogXmlHeader::XMLRecorder] = new QLabel;
    gridLayout->addWidget(label, SetupDialogXmlHeader::XMLRecorder, 0);
    gridLayout->addWidget(m_xmlHeaderLabel[SetupDialogXmlHeader::XMLRecorder], SetupDialogXmlHeader::XMLRecorder, 1);
    label = new QLabel(tr("Device:"));
    m_xmlHeaderLabel[SetupDialogXmlHeader::XMLDevice] = new QLabel;
    gridLayout->addWidget(label, SetupDialogXmlHeader::XMLDevice, 0);
    gridLayout->addWidget(m_xmlHeaderLabel[SetupDialogXmlHeader::XMLDevice], SetupDialogXmlHeader::XMLDevice, 1);
    label = new QLabel(tr("Sample rate [Hz]:"));
    m_xmlHeaderLabel[SetupDialogXmlHeader::XMLSampleRate] = new QLabel;
    gridLayout->addWidget(label, SetupDialogXmlHeader::XMLSampleRate, 0);
    gridLayout->addWidget(m_xmlHeaderLabel[SetupDialogXmlHeader::XMLSampleRate], SetupDialogXmlHeader::XMLSampleRate, 1);
    label = new QLabel(tr("Frequency [kHz]:"));
    m_xmlHeaderLabel[SetupDialogXmlHeader::XMLFreq] = new QLabel;
    gridLayout->addWidget(label, SetupDialogXmlHeader::XMLFreq, 0);
    gridLayout->addWidget(m_xmlHeaderLabel[SetupDialogXmlHeader::XMLFreq], SetupDialogXmlHeader::XMLFreq, 1);
    label = new QLabel(tr("Recording length [sec]:"));
    m_xmlHeaderLabel[SetupDialogXmlHeader::XMLLength] = new QLabel;
    gridLayout->addWidget(label, SetupDialogXmlHeader::XMLLength, 0);
    gridLayout->addWidget(m_xmlHeaderLabel[SetupDialogXmlHeader::XMLLength], SetupDialogXmlHeader::XMLLength, 1);
    label = new QLabel(tr("Sample format:"));
    m_xmlHeaderLabel[SetupDialogXmlHeader::XMLFormat] = new QLabel;
    gridLayout->addWidget(label, SetupDialogXmlHeader::XMLFormat, 0);
    gridLayout->addWidget(m_xmlHeaderLabel[SetupDialogXmlHeader::XMLFormat], SetupDialogXmlHeader::XMLFormat, 1);
    gridLayout->setContentsMargins(0, 12, 0, 0);
    ui->xmlHeaderWidget->setLayout(gridLayout);

    ui->xmlHeaderWidget->setVisible(false);

    connect(ui->inputCombo, &QComboBox::currentIndexChanged, this, &SetupDialog::onInputChanged);
    connect(ui->openFileButton, &QPushButton::clicked, this, &SetupDialog::onOpenFileButtonClicked);

    connect(ui->connectButton, &QPushButton::clicked, this, &SetupDialog::onConnectDeviceClicked);

    connect(ui->rtlsdrGainSlider, &QSlider::valueChanged, this, &SetupDialog::onRtlSdrGainSliderChanged);
    connect(ui->rtlsdrGainModeHw, &QRadioButton::toggled, this, &SetupDialog::onRtlSdrGainModeToggled);
    connect(ui->rtlsdrGainModeSw, &QRadioButton::toggled, this, &SetupDialog::onRtlSdrGainModeToggled);
    connect(ui->rtlsdrGainModeManual, &QRadioButton::toggled, this, &SetupDialog::onRtlSdrGainModeToggled);
    connect(ui->rtlsdrBandwidth, &QSpinBox::valueChanged, this, &SetupDialog::onRtlSdrBandwidthChanged);
    connect(ui->rtlsdrBandwidthDefault, &QPushButton::clicked, this, [this]() { ui->rtlsdrBandwidth->setValue(0); } );
    connect(ui->rtlsdrSwAgcMaxLevel, &QSpinBox::valueChanged, this, &SetupDialog::onRtlSdrSwAgcMaxLevelChanged);
    connect(ui->rtlsdrSwAgcMaxLevelDefault, &QPushButton::clicked, this, [this]() { ui->rtlsdrSwAgcMaxLevel->setValue(0); } );
    connect(ui->rtlsdrBiasTCombo, &QComboBox::currentIndexChanged, this, &SetupDialog::onRtlSdrBiasTCurrentIdxChanged);
    connect(ui->rtlsdrPPM, &QSpinBox::valueChanged, this, &SetupDialog::onRtlSdrPPMChanged);

    connect(ui->rtltcpGainSlider, &QSlider::valueChanged, this, &SetupDialog::onRtlTcpGainSliderChanged);
    connect(ui->rtltcpGainModeHw, &QRadioButton::toggled, this, &SetupDialog::onTcpGainModeToggled);
    connect(ui->rtltcpGainModeSw, &QRadioButton::toggled, this, &SetupDialog::onTcpGainModeToggled);
    connect(ui->rtltcpGainModeManual, &QRadioButton::toggled, this, &SetupDialog::onTcpGainModeToggled);
    connect(ui->rtltcpIpAddressEdit, &QLineEdit::editingFinished, this, &SetupDialog::onRtlTcpIpAddrEditFinished);
    connect(ui->rtltcpIpPortSpinBox, &QSpinBox::valueChanged, this, &SetupDialog::onRtlTcpPortValueChanged);
    connect(ui->rtltcpSwAgcMaxLevel, &QSpinBox::valueChanged, this, &SetupDialog::onRtlTcpSwAgcMaxLevelChanged);
    connect(ui->rtltcpSwAgcMaxLevelDefault, &QPushButton::clicked, this, [this]() { ui->rtltcpSwAgcMaxLevel->setValue(0); } );
    connect(ui->rtltcpPPM, &QSpinBox::valueChanged, this, &SetupDialog::onRtlTcpPPMChanged);


    connect(ui->loopCheckbox, &QCheckBox::stateChanged, this, [=](int val) { m_settings.rawfile.loopEna = (Qt::Unchecked != val); });
    connect(ui->fileFormatCombo, &QComboBox::currentIndexChanged, this, &SetupDialog::onRawFileFormatChanged);

#if HAVE_AIRSPY
    connect(ui->airspySensitivityGainSlider, &QSlider::valueChanged, this, &SetupDialog::onAirspySensitivityGainSliderChanged);
    connect(ui->airspyIFGainSlider, &QSlider::valueChanged, this, &SetupDialog::onAirspyIFGainSliderChanged);
    connect(ui->airspyLNAGainSlider, &QSlider::valueChanged, this, &SetupDialog::onAirspyLNAGainSliderChanged);
    connect(ui->airspyMixerGainSlider, &QSlider::valueChanged, this, &SetupDialog::onAirspyMixerGainSliderChanged);
    connect(ui->airspyLNAAGCCheckbox, &QCheckBox::stateChanged, this, &SetupDialog::onAirspyLNAAGCstateChanged);
    connect(ui->airspyMixerAGCCheckbox, &QCheckBox::stateChanged, this, &SetupDialog::onAirspyMixerAGCstateChanged);
    connect(ui->airspyGainModeHybrid, &QRadioButton::toggled, this, &SetupDialog::onAirspyModeToggled);
    connect(ui->airspyGainModeSw, &QRadioButton::toggled, this, &SetupDialog::onAirspyModeToggled);
    connect(ui->airspyGainModeManual, &QRadioButton::toggled, this, &SetupDialog::onAirspyModeToggled);
    connect(ui->airspyGainModeSensitivity, &QRadioButton::toggled, this, &SetupDialog::onAirspyModeToggled);
    connect(ui->airspyBiasTCombo, &QComboBox::currentIndexChanged, this, &SetupDialog::onAirspyBiasTCurrentIdxChanged);
#endif

#if HAVE_SOAPYSDR
    connect(ui->soapysdrGainSlider, &QSlider::valueChanged, this, &SetupDialog::onSoapySdrGainSliderChanged);
    connect(ui->soapysdrDevArgs, &QLineEdit::editingFinished, this, &SetupDialog::onSoapySdrDevArgsEditFinished);
    connect(ui->soapySdrAntenna, &QLineEdit::editingFinished, this, &SetupDialog::onSoapySdrAntennaEditFinished);
    connect(ui->soapysdrChannelNum, &QSpinBox::editingFinished, this, &SetupDialog::onSoapySdrChannelEditFinished);
    connect(ui->soapysdrGainModeHw, &QRadioButton::toggled, this, &SetupDialog::onSoapySdrGainModeToggled);
    connect(ui->soapysdrGainModeSw, &QRadioButton::toggled, this, &SetupDialog::onSoapySdrGainModeToggled);
    connect(ui->soapysdrGainModeManual, &QRadioButton::toggled, this, &SetupDialog::onSoapySdrGainModeToggled);
    connect(ui->soapysdrBandwidth, &QSpinBox::valueChanged, this, &SetupDialog::onSoapySdrBandwidthChanged);
    connect(ui->soapysdrBandwidthDefault, &QPushButton::clicked, this, [this]() { ui->soapysdrBandwidth->setValue(0); } );
#endif

    ui->defaultStyleRadioButton->setText(tr("Default style (OS dependent)"));
    ui->lightStyleRadioButton->setText(tr("Light style (Fusion with light colors)"));
    ui->darkStyleRadioButton->setText(tr("Dark style (Fusion with dark colors)"));

    // create combo box with language selection
    ui->langComboBox->addItem("<" + tr("System language") + ">", QVariant(QLocale::AnyLanguage));
    ui->langComboBox->addItem("English", QVariant(QLocale::English));   // QLocale::English native name returns "American English" :-(
    for (auto l : m_supportedLocalization)
    {
        ui->langComboBox->addItem(QLocale(l).nativeLanguageName(), QVariant(l));
    }
    ui->langComboBox->model()->sort(0);
    ui->langComboBox->setToolTip(tr("User interface language, the change will take effect after application restart."));
    ui->langWarningLabel->setText("<span style=\"color:red\">"+tr("Language change will take effect after application restart.")+"</span>");
    ui->langWarningLabel->setVisible(false);
    connect(ui->langComboBox, &QComboBox::currentIndexChanged, this, &SetupDialog::onLanguageChanged);

    ui->defaultStyleRadioButton->setToolTip(tr("Set default OS style."));
    ui->lightStyleRadioButton->setToolTip(tr("Force application light style."));
    ui->darkStyleRadioButton->setToolTip(tr("Force application dark style."));

    ui->expertCheckBox->setToolTip(tr("User interface in expert mode"));
    ui->dlPlusCheckBox->setToolTip(tr("Show Dynamic Label Plus (DL+) tags like artist, song name, etc."));
    ui->xmlHeaderCheckBox->setToolTip(tr("Include raw file XML header in IQ recording"));

    ui->audioRecordingFolderLabel->setElideMode(Qt::ElideLeft);

    connect(ui->defaultStyleRadioButton, &QRadioButton::clicked, this, &SetupDialog::onStyleChecked);
    connect(ui->lightStyleRadioButton, &QRadioButton::clicked, this, &SetupDialog::onStyleChecked);
    connect(ui->darkStyleRadioButton, &QRadioButton::clicked, this, &SetupDialog::onStyleChecked);
    connect(ui->expertCheckBox, &QCheckBox::clicked, this, &SetupDialog::onExpertModeChecked);
    connect(ui->dlPlusCheckBox, &QCheckBox::clicked, this, &SetupDialog::onDLPlusChecked);
    connect(ui->xmlHeaderCheckBox, &QCheckBox::clicked, this, &SetupDialog::onXmlHeaderChecked);
    connect(ui->spiAppCheckBox, &QCheckBox::clicked, this, &SetupDialog::onSpiAppChecked);
    connect(ui->internetCheckBox, &QCheckBox::clicked, this, &SetupDialog::onUseInternetChecked);
    connect(ui->radioDNSCheckBox, &QCheckBox::clicked, this, &SetupDialog::onRadioDnsChecked);
    connect(ui->audioRecordingFolderButton, &QPushButton::clicked, this, &SetupDialog::onAudioRecordingFolderButtonClicked);
    connect(ui->audioInRecordingRadioButton, &QRadioButton::clicked, this, &SetupDialog::onAudioRecordingChecked);
    connect(ui->audioOutRecordingRadioButton, &QRadioButton::clicked, this, &SetupDialog::onAudioRecordingChecked);
    connect(ui->autoStopRecordingCheckBox, &QCheckBox::toggled, this, [this](bool checked) { m_settings.audioRecAutoStopEna = checked; });

    ui->dataDumpFolderLabel->setElideMode(Qt::ElideLeft);
    ui->dumpSlsPatternEdit->setToolTip(tr("Storage path template for SLS application. Following tokens are supported:\n"
                                          "{serviceId, ensId, contentName, contentNameWithExt, transportId}"));
    ui->dumpSpiPatternEdit->setToolTip(tr("Storage path template for SPI application. Following tokens are supported:\n"
                                          "{serviceId, ensId, scId, contentName, directoryId, transportId}"));
    connect(ui->dataDumpFolderButton, &QPushButton::clicked, this, &SetupDialog::onDataDumpFolderButtonClicked);
    connect(ui->dumpSlsCheckBox, &QCheckBox::toggled, this, &SetupDialog::onDataDumpCheckboxToggled);
    connect(ui->dumpSpiCheckBox, &QCheckBox::toggled, this, &SetupDialog::onDataDumpCheckboxToggled);
    connect(ui->dumpOverwriteCheckbox, &QCheckBox::toggled, this, &SetupDialog::onDataDumpCheckboxToggled);
    connect(ui->dumpSlsPatternReset, &QPushButton::clicked, this, &SetupDialog::onDataDumpResetClicked);
    connect(ui->dumpSpiPatternReset, &QPushButton::clicked, this, &SetupDialog::onDataDumpResetClicked);
    connect(ui->dumpSlsPatternEdit, &QLineEdit::editingFinished, this, &SetupDialog::onDataDumpPatternEditingFinished);
    connect(ui->dumpSpiPatternEdit, &QLineEdit::editingFinished, this, &SetupDialog::onDataDumpPatternEditingFinished);
    connect(ui->rawFileProgressBar, &QProgressBar::valueChanged, this, &SetupDialog::onRawFileProgressChanged);            
    // reset UI
    onFileLength(0);

    ui->defaultStyleRadioButton->setChecked(true);

    ui->noiseConcealmentCombo->addItem("-20 dBFS", QVariant(20));
    ui->noiseConcealmentCombo->addItem("-25 dBFS", QVariant(25));
    ui->noiseConcealmentCombo->addItem("-30 dBFS", QVariant(30));
    ui->noiseConcealmentCombo->addItem("-35 dBFS", QVariant(35));
    ui->noiseConcealmentCombo->addItem("-40 dBFS", QVariant(40));
    ui->noiseConcealmentCombo->addItem(tr("Disabled"), QVariant(0));
    ui->noiseConcealmentCombo->setToolTip(tr("Select noise level that is generated during audio interruptions.<br>"
                                             "This may help to improve listening experience and make the audio interruptions less annoying."));
#if AUDIO_DECODER_NOISE_CONCEALMENT
    connect(ui->noiseConcealmentCombo, &QComboBox::currentIndexChanged, this, &SetupDialog::onNoiseLevelChanged);
#else
    ui->audioDecoderGroupBox->setVisible(false);
#endif

    ui->locationSourceCombo->addItem(tr("System"), QVariant::fromValue(GeolocationSource::System));
    ui->locationSourceCombo->addItem(tr("Manual"), QVariant::fromValue(GeolocationSource::Manual));
    ui->locationSourceCombo->addItem(tr("NMEA Serial Port"), QVariant::fromValue(GeolocationSource::SerialPort));
    connect(ui->locationSourceCombo, &QComboBox::currentIndexChanged, this, &SetupDialog::onGeolocationSourceChanged);
    ui->locationSrcWidget->setEnabled(false);
    ui->coordinatesEdit->setText("0.0, 0.0");

    static const QRegularExpression coordRe("[+-]?[0-9]+(\\.[0-9]+)?\\s*,\\s*[+-]?[0-9]+(\\.[0-9]+)?");
    QRegularExpressionValidator *coordValidator = new QRegularExpressionValidator(coordRe, this);
    ui->coordinatesEdit->setValidator(coordValidator);
    connect(ui->coordinatesEdit, &QLineEdit::editingFinished, this, &SetupDialog::onCoordinateEditFinished);
    connect(ui->serialPortEdit, &QLineEdit::editingFinished, this, &SetupDialog::onSerialPortEditFinished);

    QTimer::singleShot(10, this, [this](){ resize(minimumSizeHint()); } );
}

void SetupDialog::showEvent(QShowEvent *event)
{
    ui->tabWidget->setFocus();

    QDialog::showEvent(event);

    //QTimer::singleShot(10, this, [this](){ adjustSize(); } );
    QTimer::singleShot(10, this, [this](){ resize(minimumSizeHint()); } );
}

void SetupDialog::setSpiDumpPaternDefault(const QString &newSpiDumpPaternDefault)
{
    m_spiDumpPaternDefault = newSpiDumpPaternDefault;
}

void SetupDialog::setSlsDumpPaternDefault(const QString &newSlsDumpPaternDefault)
{
    m_slsDumpPaternDefault = newSlsDumpPaternDefault;
}

const SetupDialog::Settings & SetupDialog::settings() const
{
    return m_settings;
}

void SetupDialog::setGainValues(const QList<float> &gainList)
{

    switch (m_settings.inputDevice)
    {
    case InputDeviceId::RTLSDR:
        m_rtlsdrGainList.clear();
        m_rtlsdrGainList = gainList;
        ui->rtlsdrGainSlider->setMinimum(0);
        ui->rtlsdrGainSlider->setMaximum(m_rtlsdrGainList.size()-1);
        ui->rtlsdrGainSlider->setValue((m_settings.rtlsdr.gainIdx >= 0) ? m_settings.rtlsdr.gainIdx : 0);
        ui->rtlsdrGainSlider->setDisabled(m_rtlsdrGainList.empty());
        ui->rtlsdrGainModeManual->setDisabled(m_rtlsdrGainList.empty());
        break;
    case InputDeviceId::RTLTCP:
        m_rtltcpGainList.clear();
        m_rtltcpGainList = gainList;
        ui->rtltcpGainSlider->setMinimum(0);
        ui->rtltcpGainSlider->setMaximum(m_rtltcpGainList.size()-1);
        ui->rtltcpGainSlider->setValue((m_settings.rtltcp.gainIdx >= 0) ? m_settings.rtltcp.gainIdx : 0);
        ui->rtltcpGainSlider->setDisabled(m_rtltcpGainList.empty());
        ui->rtltcpGainModeManual->setDisabled(m_rtltcpGainList.empty());
        break;
    case InputDeviceId::SOAPYSDR:
#if HAVE_SOAPYSDR
        m_soapysdrGainList.clear();
        m_soapysdrGainList = gainList;
        ui->soapysdrGainSlider->setMinimum(0);
        ui->soapysdrGainSlider->setMaximum(m_soapysdrGainList.size()-1);
        ui->soapysdrGainSlider->setValue((m_settings.soapysdr.gainIdx >= 0) ? m_settings.soapysdr.gainIdx : 0);
        ui->soapysdrGainSlider->setDisabled(m_soapysdrGainList.empty());
        ui->soapysdrGainModeManual->setDisabled(m_soapysdrGainList.empty());
#endif // HAVE_SOAPYSDR
        break;
    case InputDeviceId::UNDEFINED:
    case InputDeviceId::RAWFILE:
    case InputDeviceId::AIRSPY:
        return;
    }
}

void SetupDialog::setSettings(const Settings &settings)
{
    m_settings = settings;
    setUiState();
    onExpertModeChecked(m_settings.expertModeEna);
    setStatusLabel();    
    emit newAnnouncementSettings();
    emit noiseConcealmentLevelChanged(m_settings.noiseConcealmentLevel);
    emit xmlHeaderToggled(m_settings.xmlHeaderEna);
    emit audioRecordingSettings(m_settings.audioRecFolder, m_settings.audioRecCaptureOutput);
    emit uaDumpSettings(m_settings.uaDump);
    emit tiiSettingsChanged();
    onUseInternetChecked(m_settings.useInternet);    
    onSpiAppChecked(m_settings.spiAppEna);    
}

void SetupDialog::setXmlHeader(const InputDeviceDescription &desc)
{
    if (desc.rawFile.hasXmlHeader)
    {
        m_xmlHeaderLabel[SetupDialogXmlHeader::XMLDate]->setText(desc.rawFile.time);
        m_xmlHeaderLabel[SetupDialogXmlHeader::XMLRecorder]->setText(desc.rawFile.recorder);
        m_xmlHeaderLabel[SetupDialogXmlHeader::XMLDevice]->setText(QString("%1 [ %2 ]").arg(desc.device.name, desc.device.model));
        m_xmlHeaderLabel[SetupDialogXmlHeader::XMLSampleRate]->setText(QString::number(desc.sample.sampleRate));
        m_xmlHeaderLabel[SetupDialogXmlHeader::XMLFreq]->setText(QString::number(desc.rawFile.frequency_kHz));
        m_xmlHeaderLabel[SetupDialogXmlHeader::XMLLength]->setText(QString::number(desc.rawFile.numSamples * 1.0 / desc.sample.sampleRate));
        m_xmlHeaderLabel[SetupDialogXmlHeader::XMLFormat]->setText(desc.sample.channelContainer);

        switch (desc.sample.containerBits)
        {
        case 8:
            m_settings.rawfile.format = RawFileInputFormat::SAMPLE_FORMAT_U8;
            break;
        case 16:
            m_settings.rawfile.format = RawFileInputFormat::SAMPLE_FORMAT_S16;
            break;
        }
        ui->fileFormatCombo->setCurrentIndex(static_cast<int>(m_settings.rawfile.format));
        ui->fileFormatCombo->setEnabled(false);
        ui->xmlHeaderWidget->setVisible(true);
    }
    else
    {
        ui->fileFormatCombo->setEnabled(true);
        ui->xmlHeaderWidget->setVisible(false);
    }
}

void SetupDialog::onFileLength(int msec)
{    
    ui->rawFileProgressBar->setMinimum(0);
    ui->rawFileProgressBar->setMaximum(msec);
    ui->rawFileProgressBar->setValue(0);
    onRawFileProgressChanged(0);

    ui->rawFileProgressBar->setVisible(0 != msec);
    ui->rawFileTime->setVisible(0 != msec);
}

void SetupDialog::onFileProgress(int msec)
{
    ui->rawFileProgressBar->setValue(msec);
}

void SetupDialog::setAudioRecAutoStop(bool ena)
{
    m_settings.audioRecAutoStopEna = ena;
    ui->autoStopRecordingCheckBox->setChecked(ena);
}

QLocale::Language SetupDialog::applicationLanguage() const
{
    if (m_supportedLocalization.contains(m_settings.lang) || (QLocale::English == m_settings.lang))
    {   // selected specific localization
        return m_settings.lang;
    }
    else
    {   // system default or not supported
        // find if system default is supported
        if (m_supportedLocalization.contains(QLocale::system().language()))
        {
            return QLocale::system().language();
        }
        else
        {
            return QLocale::English;
        }
    }
}

void SetupDialog::setUiState()
{
    // block signals while UI is set to be aligned with m_settings
    const QSignalBlocker blocker(this);

    int index = ui->inputCombo->findData(QVariant(static_cast<int>(m_settings.inputDevice)));
    if (index < 0)
    {  // not found -> show rtlsdr as default
        index = ui->inputCombo->findData(QVariant(static_cast<int>(InputDeviceId::RTLSDR)));
    }
    ui->inputCombo->setCurrentIndex(index);

    if (!m_rtlsdrGainList.isEmpty())
    {
        ui->rtlsdrGainSlider->setValue(m_settings.rtlsdr.gainIdx);
        onRtlSdrGainSliderChanged(m_settings.rtlsdr.gainIdx);
    }
    else
    {
        ui->rtlsdrGainSlider->setValue(0);
        ui->rtlsdrGainValueLabel->setText(tr("N/A"));
    }
    if (!m_rtltcpGainList.isEmpty())
    {
        ui->rtltcpGainSlider->setValue(m_settings.rtltcp.gainIdx);
        onRtlTcpGainSliderChanged(m_settings.rtltcp.gainIdx);
    }
    else
    {
        ui->rtltcpGainSlider->setValue(0);
        ui->rtltcpGainValueLabel->setText(tr("N/A"));
    }

    switch (m_settings.rtlsdr.gainMode) {
    case RtlGainMode::Software:
        ui->rtlsdrGainModeSw->setChecked(true);
        break;
    case RtlGainMode::Hardware:
        ui->rtlsdrGainModeHw->setChecked(true);
        break;
    case RtlGainMode::Manual:
        ui->rtlsdrGainModeManual->setChecked(true);
        break;
    default:
        break;
    }   
    ui->rtlsdrBandwidth->setValue(m_settings.rtlsdr.bandwidth / 1000);
    ui->rtlsdrBiasTCombo->setCurrentIndex(m_settings.rtlsdr.biasT ? 1 : 0);
    ui->rtlsdrSwAgcMaxLevel->setValue(m_settings.rtlsdr.agcLevelMax);
    ui->rtlsdrPPM->setValue(m_settings.rtlsdr.ppm);

    switch (m_settings.rtltcp.gainMode) {
    case RtlGainMode::Software:
        ui->rtltcpGainModeSw->setChecked(true);
        break;
    case RtlGainMode::Hardware:
        ui->rtltcpGainModeHw->setChecked(true);
        break;
    case RtlGainMode::Manual:
        ui->rtltcpGainModeManual->setChecked(true);
        break;
    default:
        break;
    }
    ui->rtltcpSwAgcMaxLevel->setValue(m_settings.rtltcp.agcLevelMax);
    ui->rtltcpPPM->setValue(m_settings.rtltcp.ppm);

#if HAVE_AIRSPY
    switch (m_settings.airspy.gain.mode) {
    case AirpyGainMode::Software:
        ui->airspyGainModeSw->setChecked(true);
        break;
    case AirpyGainMode::Hybrid:
        ui->airspyGainModeHybrid->setChecked(true);
        break;
    case AirpyGainMode::Manual:
        ui->airspyGainModeManual->setChecked(true);
        break;
    case AirpyGainMode::Sensitivity:
        ui->airspyGainModeSensitivity->setChecked(true);
        break;
    default:
        break;
    }

    ui->airspySensitivityGainSlider->setValue(m_settings.airspy.gain.sensitivityGainIdx);
    ui->airspyIFGainSlider->setValue(m_settings.airspy.gain.ifGainIdx);
    ui->airspyLNAGainSlider->setValue(m_settings.airspy.gain.lnaGainIdx);
    ui->airspyMixerGainSlider->setValue(m_settings.airspy.gain.mixerGainIdx);
    ui->airspyMixerAGCCheckbox->setChecked(m_settings.airspy.gain.mixerAgcEna);
    ui->airspyLNAAGCCheckbox->setChecked(m_settings.airspy.gain.lnaAgcEna);
    ui->airspyBiasTCombo->setCurrentIndex(m_settings.airspy.biasT ? 1 : 0);
#endif

#if HAVE_SOAPYSDR
    switch (m_settings.soapysdr.gainMode) {
    case SoapyGainMode::Software:
        ui->soapysdrGainModeSw->setChecked(true);
        break;
    case SoapyGainMode::Hardware:
        ui->soapysdrGainModeHw->setChecked(true);
        break;
    case SoapyGainMode::Manual:
        ui->soapysdrGainModeManual->setChecked(true);
        break;
    default:
        break;
    }

    if (!m_soapysdrGainList.isEmpty())
    {
        ui->soapysdrGainSlider->setValue(m_settings.soapysdr.gainIdx);
        onSoapySdrGainSliderChanged(m_settings.soapysdr.gainIdx);
    }
    else
    {
        ui->soapysdrGainSlider->setValue(0);
        ui->soapysdrGainValueLabel->setText(tr("N/A"));
    }

    ui->soapysdrDevArgs->setText(m_settings.soapysdr.devArgs);
    ui->soapysdrChannelNum->setValue(m_settings.soapysdr.channel);
    ui->soapySdrAntenna->setText(m_settings.soapysdr.antenna);

    ui->soapysdrBandwidth->setValue(m_settings.soapysdr.bandwidth / 1000);
#endif

    if (m_settings.rawfile.file.isEmpty())
    {
        ui->fileNameLabel->setText(m_noFileString);
        m_rawfilename = "";
        ui->fileNameLabel->setToolTip("");
    }
    else
    {
        ui->fileNameLabel->setText(m_settings.rawfile.file);
        m_rawfilename = m_settings.rawfile.file;
        ui->fileNameLabel->setToolTip(m_settings.rawfile.file);
    }
    ui->loopCheckbox->setChecked(m_settings.rawfile.loopEna);
    ui->fileFormatCombo->setCurrentIndex(static_cast<int>(m_settings.rawfile.format));
    ui->rtltcpIpAddressEdit->setText(m_settings.rtltcp.tcpAddress);
    ui->rtltcpIpPortSpinBox->setValue(m_settings.rtltcp.tcpPort);

    setStatusLabel();

    // announcements
    uint16_t announcementEna = m_settings.announcementEna | (1 << static_cast<int>(DabAnnouncement::Alarm));  // enable alarm
    for (int a = 0; a < static_cast<int>(DabAnnouncement::Undefined); ++a)
    {
        m_announcementCheckBox[a]->setChecked(announcementEna & 0x1);
        announcementEna >>= 1;
    }
    m_bringWindowToForegroundCheckbox->setChecked(m_settings.bringWindowToForeground);

    // other settings
    switch (m_settings.applicationStyle)
    {
    case ApplicationStyle::Default:
        ui->defaultStyleRadioButton->setChecked(true);
        break;
    case ApplicationStyle::Light:
        ui->lightStyleRadioButton->setChecked(true);
        break;
    case ApplicationStyle::Dark:
        ui->darkStyleRadioButton->setChecked(true);
        break;
    }
    ui->expertCheckBox->setChecked(m_settings.expertModeEna);
    ui->dlPlusCheckBox->setChecked(m_settings.dlPlusEna);

    index = ui->noiseConcealmentCombo->findData(QVariant(m_settings.noiseConcealmentLevel));
    if (index < 0)
    {   // not found
        index = 0;
    }
    ui->noiseConcealmentCombo->setCurrentIndex(index);
    ui->xmlHeaderCheckBox->setChecked(m_settings.xmlHeaderEna);
    ui->spiAppCheckBox->setChecked(m_settings.spiAppEna);
    ui->internetCheckBox->setChecked(m_settings.useInternet);
    ui->radioDNSCheckBox->setChecked(m_settings.radioDnsEna);

    index = ui->langComboBox->findData(QVariant(m_settings.lang));
    if (index < 0)
    {   // not found
        index = 0;
    }
    ui->langComboBox->setCurrentIndex(index);

    ui->audioRecordingFolderLabel->setText(m_settings.audioRecFolder);
    if (m_settings.audioRecCaptureOutput)
    {
        ui->audioOutRecordingRadioButton->setChecked(true);
    }
    else
    {
        ui->audioInRecordingRadioButton->setChecked(true);
    }
    ui->autoStopRecordingCheckBox->setChecked(m_settings.audioRecAutoStopEna);

    ui->dataDumpFolderLabel->setText(m_settings.uaDump.folder);
    ui->dumpSlsPatternEdit->setText(m_settings.uaDump.slsPattern);
    ui->dumpSpiPatternEdit->setText(m_settings.uaDump.spiPattern);
    // blocker is required to avoid triggering unwanted signal
    // we need to set UI state first
    {
        const QSignalBlocker blocker(ui->dumpSlsCheckBox);
        // no signals here
        ui->dumpSlsCheckBox->setChecked(m_settings.uaDump.slsEna);
    }
    {
        const QSignalBlocker blocker(ui->dumpSpiCheckBox);
        // no signals here
        ui->dumpSpiCheckBox->setChecked(m_settings.uaDump.spiEna);
    }
    // this may trigger the signal (no problem if it happens)
    ui->dumpOverwriteCheckbox->setChecked(m_settings.uaDump.overwriteEna);
    ui->coordinatesEdit->setText(QString("%1, %2")
                                     .arg(m_settings.tii.coordinates.latitude(), 0, 'g', QLocale::FloatingPointShortest)
                                     .arg(m_settings.tii.coordinates.longitude(), 0, 'g', QLocale::FloatingPointShortest));
    ui->serialPortEdit->setText(m_settings.tii.serialPort);
    ui->locationSourceCombo->setCurrentIndex(static_cast<int>(m_settings.tii.locationSource));
}

void SetupDialog::onConnectDeviceClicked()
{
    ui->connectButton->setHidden(true);
    m_settings.inputDevice = static_cast<InputDeviceId>(ui->inputCombo->itemData(ui->inputCombo->currentIndex()).toInt());
    setStatusLabel();
    switch (m_settings.inputDevice)
    {
    case InputDeviceId::RTLSDR:
        activateRtlSdrControls(true);
        break;
    case InputDeviceId::RTLTCP:
        m_settings.rtltcp.tcpAddress = ui->rtltcpIpAddressEdit->text();
        m_settings.rtltcp.tcpPort = ui->rtltcpIpPortSpinBox->value();
        activateRtlTcpControls(true);
        break;
    case InputDeviceId::UNDEFINED:
        break;
    case InputDeviceId::RAWFILE:
        m_settings.rawfile.file = m_rawfilename;
        m_settings.rawfile.loopEna = ui->loopCheckbox->isChecked();
        m_settings.rawfile.format = static_cast<RawFileInputFormat>(ui->fileFormatCombo->currentIndex());
        break;
    case InputDeviceId::AIRSPY:
#if HAVE_AIRSPY
        activateAirspyControls(true);
#endif
        break;
    case InputDeviceId::SOAPYSDR:
#if HAVE_SOAPYSDR
        m_settings.soapysdr.devArgs = ui->soapysdrDevArgs->text();
        m_settings.soapysdr.channel = ui->soapysdrChannelNum->text().toInt();
        m_settings.soapysdr.antenna = ui->soapySdrAntenna->text();
        activateSoapySdrControls(true);
#endif
        break;
    }
    emit inputDeviceChanged(m_settings.inputDevice);
}

void SetupDialog::onRtlSdrGainSliderChanged(int val)
{
    if (!m_rtlsdrGainList.empty())
    {
        ui->rtlsdrGainValueLabel->setText(QString("%1 dB").arg(m_rtlsdrGainList.at(val)));
        m_settings.rtlsdr.gainIdx = val;
        emit newInputDeviceSettings();
    }
    else { /* empy gain list => do nothing */}
}

void SetupDialog::onRtlSdrBandwidthChanged(int val)
{
    m_settings.rtlsdr.bandwidth = val * 1000;
    ui->rtlsdrBandwidthDefault->setEnabled(val > 0);
    emit newInputDeviceSettings();
}

void SetupDialog::onRtlSdrPPMChanged(int val)
{
    m_settings.rtlsdr.ppm = val;
    emit newInputDeviceSettings();
}

void SetupDialog::onRtlSdrSwAgcMaxLevelChanged(int val)
{
    m_settings.rtlsdr.agcLevelMax = val;
    ui->rtlsdrSwAgcMaxLevelDefault->setEnabled(val > 0);
    emit newInputDeviceSettings();    
}

void SetupDialog::onRtlSdrBiasTCurrentIdxChanged(int)
{
    m_settings.rtlsdr.biasT = ui->rtlsdrBiasTCombo->currentData().toBool();
    emit newInputDeviceSettings();
}

void SetupDialog::onRtlSdrGainModeToggled(bool checked)
{
    if (checked)
    {
        if (ui->rtlsdrGainModeHw->isChecked())
        {
            m_settings.rtlsdr.gainMode = RtlGainMode::Hardware;
        }
        else if (ui->rtlsdrGainModeSw->isChecked())
        {
            m_settings.rtlsdr.gainMode = RtlGainMode::Software;
        }
        else if (ui->rtlsdrGainModeManual->isChecked())
        {
            m_settings.rtlsdr.gainMode = RtlGainMode::Manual;
        }
        activateRtlSdrControls(true);
        emit newInputDeviceSettings();
    }
}

void SetupDialog::activateRtlSdrControls(bool en)
{
    ui->rtlsdrGainModeGroup->setEnabled(en);
    ui->rtlsdrGainWidget->setEnabled(en && (RtlGainMode::Manual == m_settings.rtlsdr.gainMode));
    ui->rtlsdrExpertGroup->setEnabled(en);
}

void SetupDialog::onRtlTcpGainSliderChanged(int val)
{
    if (!m_rtltcpGainList.empty())
    {
        ui->rtltcpGainValueLabel->setText(QString("%1 dB").arg(m_rtltcpGainList.at(val)));
        m_settings.rtltcp.gainIdx = val;
        emit newInputDeviceSettings();
    }
    else { /* empy gain list => do nothing */}
}

void SetupDialog::onRtlTcpIpAddrEditFinished()
{
    if (ui->rtltcpIpAddressEdit->text() != m_settings.rtltcp.tcpAddress)
    {
        ui->connectButton->setVisible(true);
    }
}

void SetupDialog::onRtlTcpPortValueChanged(int val)
{
    if (val != m_settings.rtltcp.tcpPort)
    {
        ui->connectButton->setVisible(true);
    }

}

void SetupDialog::onTcpGainModeToggled(bool checked)
{
    if (checked)
    {
        if (ui->rtltcpGainModeHw->isChecked())
        {
            m_settings.rtltcp.gainMode = RtlGainMode::Hardware;
        }
        else if (ui->rtltcpGainModeSw->isChecked())
        {
            m_settings.rtltcp.gainMode = RtlGainMode::Software;
        }
        else if (ui->rtltcpGainModeManual->isChecked())
        {
            m_settings.rtltcp.gainMode = RtlGainMode::Manual;
        }
        activateRtlTcpControls(true);
        emit newInputDeviceSettings();
    }
}

void SetupDialog::onRtlTcpSwAgcMaxLevelChanged(int val)
{
    m_settings.rtltcp.agcLevelMax = val;
    ui->rtltcpSwAgcMaxLevelDefault->setEnabled(val > 0);
    emit newInputDeviceSettings();
}

void SetupDialog::onRtlTcpPPMChanged(int val)
{
    m_settings.rtltcp.ppm = val;
    emit newInputDeviceSettings();
}


void SetupDialog::activateRtlTcpControls(bool en)
{
    ui->rtltcpGainModeGroup->setEnabled(en);
    ui->rtltcpGainWidget->setEnabled(en && (RtlGainMode::Manual == m_settings.rtltcp.gainMode));
    ui->rtltcpExpertGroup->setEnabled(en);
}

void SetupDialog::onRawFileFormatChanged(int idx)
{
    if (static_cast<RawFileInputFormat>(idx) != m_settings.rawfile.format)
    {
        ui->connectButton->setVisible(true);               
    }
}

#if HAVE_AIRSPY
void SetupDialog::activateAirspyControls(bool en)
{
    ui->airspyGainModeGroup->setEnabled(en);
    bool sensEna = en && (AirpyGainMode::Sensitivity == m_settings.airspy.gain.mode);
    bool manualEna = en && (AirpyGainMode::Manual == m_settings.airspy.gain.mode);

    // sensitivity controls
    ui->airspySensitivityGain->setEnabled(sensEna);
    ui->airspySensitivityGainLabel->setEnabled(sensEna);
    ui->airspySensitivityGainSlider->setEnabled(sensEna);

    // manual gain controls
    ui->airspyIFGain->setEnabled(manualEna);
    ui->airspyIFGainLabel->setEnabled(manualEna);
    ui->airspyIFGainSlider->setEnabled(manualEna);

    ui->airspyLNAAGCCheckbox->setEnabled(manualEna);
    ui->airspyLNAGain->setEnabled(manualEna && !m_settings.airspy.gain.lnaAgcEna);
    ui->airspyLNAGainLabel->setEnabled(manualEna && !m_settings.airspy.gain.lnaAgcEna);
    ui->airspyLNAGainSlider->setEnabled(manualEna && !m_settings.airspy.gain.lnaAgcEna);

    ui->airspyMixerAGCCheckbox->setEnabled(manualEna);
    ui->airspyMixerGain->setEnabled(manualEna && !m_settings.airspy.gain.mixerAgcEna);
    ui->airspyMixerGainLabel->setEnabled(manualEna && !m_settings.airspy.gain.mixerAgcEna);
    ui->airspyMixerGainSlider->setEnabled(manualEna && !m_settings.airspy.gain.mixerAgcEna);

    ui->airspyExpertGroup->setEnabled(en);
}

void SetupDialog::onAirspyModeToggled(bool checked)
{
    if (checked)
    {
        if (ui->airspyGainModeHybrid->isChecked())
        {
            m_settings.airspy.gain.mode = AirpyGainMode::Hybrid;
        }
        else if (ui->airspyGainModeSw->isChecked())
        {
            m_settings.airspy.gain.mode = AirpyGainMode::Software;
        }
        else if (ui->airspyGainModeManual->isChecked())
        {
            m_settings.airspy.gain.mode = AirpyGainMode::Manual;
        }
        else if (ui->airspyGainModeSensitivity->isChecked())
        {
            m_settings.airspy.gain.mode = AirpyGainMode::Sensitivity;
        }
        activateAirspyControls(true);
        emit newInputDeviceSettings();
    }
}

void SetupDialog::onAirspySensitivityGainSliderChanged(int val)
{
    ui->airspySensitivityGainLabel->setText(QString::number(val));
    m_settings.airspy.gain.sensitivityGainIdx = val;
    emit newInputDeviceSettings();
}

void SetupDialog::onAirspyIFGainSliderChanged(int val)
{
    ui->airspyIFGainLabel->setText(QString::number(val));
    m_settings.airspy.gain.ifGainIdx = val;
    emit newInputDeviceSettings();
}

void SetupDialog::onAirspyLNAGainSliderChanged(int val)
{
    ui->airspyLNAGainLabel->setText(QString::number(val));
    m_settings.airspy.gain.lnaGainIdx = val;
    emit newInputDeviceSettings();
}

void SetupDialog::onAirspyMixerGainSliderChanged(int val)
{
    ui->airspyMixerGainLabel->setText(QString::number(val));
    m_settings.airspy.gain.mixerGainIdx = val;
    emit newInputDeviceSettings();
}

void SetupDialog::onAirspyLNAAGCstateChanged(int state)
{
    bool ena = (Qt::Unchecked == state);
    ui->airspyLNAGain->setEnabled(ena);
    ui->airspyLNAGainLabel->setEnabled(ena);
    ui->airspyLNAGainSlider->setEnabled(ena);

    m_settings.airspy.gain.lnaAgcEna = !ena;
    emit newInputDeviceSettings();
}

void SetupDialog::onAirspyMixerAGCstateChanged(int state)
{
    bool ena = (Qt::Unchecked == state);
    ui->airspyMixerGain->setEnabled(ena);
    ui->airspyMixerGainLabel->setEnabled(ena);
    ui->airspyMixerGainSlider->setEnabled(ena);

    m_settings.airspy.gain.mixerAgcEna = !ena;
    emit newInputDeviceSettings();
}

void SetupDialog::onAirspyBiasTCurrentIdxChanged(int)
{
    m_settings.airspy.biasT = ui->airspyBiasTCombo->currentData().toBool();
    emit newInputDeviceSettings();
}
#endif // HAVE_AIRSPY

#if HAVE_SOAPYSDR
void SetupDialog::onSoapySdrGainSliderChanged(int val)
{
    if (!m_soapysdrGainList.empty())
    {
        ui->soapysdrGainValueLabel->setText(QString("%1 dB").arg(m_soapysdrGainList.at(val)));
        m_settings.soapysdr.gainIdx = val;
        emit newInputDeviceSettings();
    }
    else { /* empy gain list => do nothing */}
}

void SetupDialog::activateSoapySdrControls(bool en)
{
    ui->soapysdrGainModeGroup->setEnabled(en);
    ui->soapysdrGainWidget->setEnabled(en && (SoapyGainMode::Manual == m_settings.soapysdr.gainMode));
    ui->soapysdrExpertGroup->setEnabled(en);
}

void SetupDialog::onSoapySdrDevArgsEditFinished()
{
    if (ui->soapysdrDevArgs->text().trimmed() != m_settings.soapysdr.devArgs.trimmed())
    {
        ui->connectButton->setVisible(true);
    }
}

void SetupDialog::onSoapySdrAntennaEditFinished()
{
    if (ui->soapySdrAntenna->text().trimmed() != m_settings.soapysdr.antenna.trimmed())
    {
        ui->connectButton->setVisible(true);
    }
}

void SetupDialog::onSoapySdrChannelEditFinished()
{
    if (ui->soapysdrChannelNum->value() != m_settings.soapysdr.channel)
    {
        ui->connectButton->setVisible(true);
    }
}

void SetupDialog::onSoapySdrGainModeToggled(bool checked)
{
    if (checked)
    {
        if (ui->soapysdrGainModeHw->isChecked())
        {
            m_settings.soapysdr.gainMode = SoapyGainMode::Hardware;
        }
        else if (ui->soapysdrGainModeSw->isChecked())
        {
            m_settings.soapysdr.gainMode = SoapyGainMode::Software;
        }
        else if (ui->soapysdrGainModeManual->isChecked())
        {
            m_settings.soapysdr.gainMode = SoapyGainMode::Manual;
        }
        activateSoapySdrControls(true);
        emit newInputDeviceSettings();
    }
}

void SetupDialog::onSoapySdrBandwidthChanged(int val)
{
    m_settings.soapysdr.bandwidth = val * 1000;
    ui->soapysdrBandwidthDefault->setEnabled(val > 0);
    emit newInputDeviceSettings();
}

#endif // HAVE_SOAPYSDR

void SetupDialog::setStatusLabel()
{
    switch (m_settings.inputDevice)
    {
    case InputDeviceId::RTLSDR:
        ui->statusLabel->setText(tr("RTL SDR device connected"));
        break;
    case InputDeviceId::RTLTCP:
        ui->statusLabel->setText(tr("RTL TCP device connected"));
        break;
    case InputDeviceId::UNDEFINED:
        ui->statusLabel->setText("<span style=\"color:red\">"+tr("No device connected")+"</span>");
        break;
    case InputDeviceId::RAWFILE:
        ui->statusLabel->setText(tr("Raw file connected"));
        break;
    case InputDeviceId::AIRSPY:
        ui->statusLabel->setText(tr("Airspy device connected"));
        break;
    case InputDeviceId::SOAPYSDR:
        ui->statusLabel->setText(tr("Soapy SDR device connected"));
        break;
    }
}

void SetupDialog::onInputChanged(int index)
{
    int inputDeviceInt = ui->inputCombo->itemData(index).toInt();
    ui->deviceOptionsWidget->setCurrentIndex(inputDeviceInt-1);

    ui->connectButton->setHidden(m_settings.inputDevice == static_cast<InputDeviceId>(inputDeviceInt));
    if (m_settings.inputDevice != static_cast<InputDeviceId>(inputDeviceInt))
    {   // selected input device does not match current input device
        switch (static_cast<InputDeviceId>(inputDeviceInt))
        {
        case InputDeviceId::RTLSDR:
            activateRtlSdrControls(false);
            break;            
        case InputDeviceId::RTLTCP:
            activateRtlTcpControls(false);
            break;
        case InputDeviceId::UNDEFINED:
            break;
        case InputDeviceId::RAWFILE:
            break;
        case InputDeviceId::AIRSPY:
#if HAVE_AIRSPY
            activateAirspyControls(false);
#endif
            return;
        case InputDeviceId::SOAPYSDR:
#if HAVE_SOAPYSDR
            activateSoapySdrControls(false);
#endif
            return;
        }
    }
}

void SetupDialog::onOpenFileButtonClicked()
{
    QString dir = QDir::homePath();
    if (!m_rawfilename.isEmpty() && QFileInfo::exists(m_rawfilename))
    {
        dir = QFileInfo(m_rawfilename).path();
    }
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open IQ stream"), dir, tr("Binary files")+" (*.bin *.s16 *.u8 *.raw *.sdr *.uff)");
    if (!fileName.isEmpty())
    {
        m_rawfilename = fileName;
        ui->fileNameLabel->setText(fileName);
        ui->fileNameLabel->setToolTip(fileName);
        if (fileName.endsWith(".s16"))
        {
            ui->fileFormatCombo->setCurrentIndex(int(RawFileInputFormat::SAMPLE_FORMAT_S16));
        }
        else if (fileName.endsWith(".u8"))
        {
            ui->fileFormatCombo->setCurrentIndex(int(RawFileInputFormat::SAMPLE_FORMAT_U8));
        }
        else
        {   /* format cannot be guessed from extension - if XML header is recognized, then it will be set automatically */ }

        ui->connectButton->setVisible(true);
        ui->fileFormatCombo->setEnabled(true);

        // we do not know the length yet
        onFileLength(0);

#ifdef Q_OS_MACOS // bug in Ventura
        show(); //bring window to top on OSX
        raise(); //bring window from minimized state on OSX
        activateWindow(); //bring window to front/unminimize on windows
#endif
    }
}

void SetupDialog::resetInputDevice()
{
    switch (m_settings.inputDevice)
    {
    case InputDeviceId::RTLSDR:
        activateRtlSdrControls(false);
        break;
    case InputDeviceId::RTLTCP:
        activateRtlTcpControls(false);
        break;
    case InputDeviceId::RAWFILE:
        break;
    case InputDeviceId::UNDEFINED:
    case InputDeviceId::AIRSPY:
#if HAVE_AIRSPY
        activateAirspyControls(false);
#endif
        break;
    case InputDeviceId::SOAPYSDR:
#if HAVE_SOAPYSDR
        activateSoapySdrControls(false);
#endif
        break;
    }

    m_settings.inputDevice = InputDeviceId::UNDEFINED;
    setStatusLabel();
    ui->connectButton->setVisible(true);
}

void SetupDialog::onAnnouncementClicked()
{   // calculate ena flag
    uint16_t announcementEna = 0;
    for (int a = 0; a < static_cast<int>(DabAnnouncement::Undefined); ++a)
    {
        announcementEna |= (m_announcementCheckBox[a]->isChecked() << a);
    }
    if (m_settings.announcementEna != announcementEna)
    {
        m_settings.announcementEna = announcementEna;
        emit newAnnouncementSettings();
    }
}

void SetupDialog::onBringWindowToForegroundClicked(bool checked)
{
    m_settings.bringWindowToForeground = checked;
}

void SetupDialog::onStyleChecked(bool checked)
{
    if (checked)
    {
        if (ui->defaultStyleRadioButton->isChecked())
        {
            m_settings.applicationStyle = ApplicationStyle::Default;
        }
        else if (ui->lightStyleRadioButton->isChecked())
        {
            m_settings.applicationStyle = ApplicationStyle::Light;
        }
        else if (ui->darkStyleRadioButton->isChecked())
        {
            m_settings.applicationStyle = ApplicationStyle::Dark;
        }
        emit applicationStyleChanged(m_settings.applicationStyle);
    }
}

void SetupDialog::onExpertModeChecked(bool checked)
{
    ui->rtlsdrExpertGroup->setVisible(checked);
    ui->rtltcpExpertGroup->setVisible(checked);
    ui->airspyExpertGroup->setVisible(checked);
    ui->soapysdrExpertGroup->setVisible(checked);
    ui->dataDumpGroup->setVisible(checked);
    ui->tiiGroup->setVisible(checked);
    if (!checked)
    {
        ui->dumpSlsCheckBox->setChecked(false);
        ui->dumpSpiCheckBox->setChecked(false);
    }
    emit uaDumpSettings(m_settings.uaDump);

    int idx = ui->inputCombo->findData(QVariant(static_cast<int>(InputDeviceId::RAWFILE)));
    if (checked && (idx < 0))
    {   // add item
        ui->inputCombo->addItem(tr("Raw file"), QVariant(int(InputDeviceId::RAWFILE)));
    }
    if (!checked && (idx >= 0))
    {   // remove it
        if (ui->inputCombo->currentIndex() == idx) {
            ui->inputCombo->removeItem(idx);
            ui->inputCombo->setCurrentIndex(0);
        }
        else
        {
            ui->inputCombo->removeItem(idx);
        }
    }

    m_settings.expertModeEna = checked;
    ui->tabWidget->adjustSize();

    emit expertModeToggled(checked);

    QTimer::singleShot(10, this, [this](){ resize(minimumSizeHint()); } );
}

void SetupDialog::onDLPlusChecked(bool checked)
{
    m_settings.dlPlusEna = checked;
}

void SetupDialog::onLanguageChanged(int index)
{
    QLocale::Language lang = static_cast<QLocale::Language>(ui->langComboBox->itemData(index).toInt());
    if (lang != m_settings.lang)
    {
        m_settings.lang = lang;
        ui->langWarningLabel->setVisible(true);
    }
}


void SetupDialog::onNoiseLevelChanged(int index)
{
    int noiseLevel = ui->noiseConcealmentCombo->itemData(index).toInt();
    if (noiseLevel != m_settings.noiseConcealmentLevel)
    {
        m_settings.noiseConcealmentLevel = noiseLevel;
        emit noiseConcealmentLevelChanged(noiseLevel);
    }
}

void SetupDialog::onXmlHeaderChecked(bool checked)
{
    m_settings.xmlHeaderEna = checked;
    emit xmlHeaderToggled(checked);
}

void SetupDialog::onRawFileProgressChanged(int val)
{
    ui->rawFileTime->setText(QString("%1 / %2 "+tr("sec")).arg(val/1000.0, 0, 'f', 1).arg(ui->rawFileProgressBar->maximum()/1000.0, 0, 'f', 1));
}

void SetupDialog::onSpiAppChecked(bool checked)
{
    m_settings.spiAppEna = checked;
    ui->internetCheckBox->setEnabled(checked);
    ui->radioDNSCheckBox->setEnabled(checked && m_settings.useInternet);
    emit spiApplicationEnabled(m_settings.spiAppEna);
    emit spiApplicationSettingsChanged(m_settings.useInternet && m_settings.spiAppEna, m_settings.radioDnsEna && m_settings.spiAppEna);
}

void SetupDialog::onUseInternetChecked(bool checked)
{
    m_settings.useInternet = checked;
    ui->radioDNSCheckBox->setEnabled(checked);
    emit spiApplicationSettingsChanged(m_settings.useInternet, m_settings.radioDnsEna);
}


void SetupDialog::onRadioDnsChecked(bool checked)
{
    m_settings.radioDnsEna = checked;
    emit spiApplicationSettingsChanged(m_settings.useInternet, m_settings.radioDnsEna);
}

void SetupDialog::onAudioRecordingFolderButtonClicked()
{
    QString dir = QDir::homePath();
    if (!m_settings.audioRecFolder.isEmpty())
    {
        dir = QFileInfo(m_settings.audioRecFolder).path();
    }
    dir = QFileDialog::getExistingDirectory(this, tr("Audio recording folder"), dir);
    if (!dir.isEmpty())
    {
        m_settings.audioRecFolder = dir;
        ui->audioRecordingFolderLabel->setText(dir);
        emit audioRecordingSettings(m_settings.audioRecFolder, m_settings.audioRecCaptureOutput);

#ifdef Q_OS_MACOS // bug in Ventura
        show(); //bring window to top on OSX
        raise(); //bring window from minimized state on OSX
        activateWindow(); //bring window to front/unminimize on windows
#endif
    }
}

void SetupDialog::onAudioRecordingChecked(bool checked)
{
    m_settings.audioRecCaptureOutput = ui->audioOutRecordingRadioButton->isChecked();
    emit audioRecordingSettings(m_settings.audioRecFolder, m_settings.audioRecCaptureOutput);
}

void SetupDialog::onDataDumpFolderButtonClicked()
{
    QString dir = QDir::homePath();
    if (!m_settings.uaDump.folder.isEmpty())
    {
        dir = QFileInfo(m_settings.uaDump.folder).path();
    }
    dir = QFileDialog::getExistingDirectory(this, tr("Data storage folder"), dir);
    if (!dir.isEmpty())
    {
        m_settings.uaDump.folder = dir;
        ui->dataDumpFolderLabel->setText(dir);
        emit uaDumpSettings(m_settings.uaDump);

#ifdef Q_OS_MACOS // bug in Ventura
        show(); //bring window to top on OSX
        raise(); //bring window from minimized state on OSX
        activateWindow(); //bring window to front/unminimize on windows
#endif
    }
}

void SetupDialog::onDataDumpCheckboxToggled(bool)
{
    m_settings.uaDump.overwriteEna = ui->dumpOverwriteCheckbox->isChecked();
    m_settings.uaDump.slsEna = ui->dumpSlsCheckBox->isChecked();
    m_settings.uaDump.spiEna = ui->dumpSpiCheckBox->isChecked();

    emit uaDumpSettings(m_settings.uaDump);
}


void SetupDialog::onDataDumpPatternEditingFinished()
{
    if (QObject::sender() == ui->dumpSlsPatternEdit)
    {
        m_settings.uaDump.slsPattern = ui->dumpSlsPatternEdit->text().trimmed();
    }
    else
    {
        m_settings.uaDump.spiPattern = ui->dumpSpiPatternEdit->text().trimmed();
    }
    emit uaDumpSettings(m_settings.uaDump);
}

void SetupDialog::onDataDumpResetClicked()
{
    if (QObject::sender() == ui->dumpSlsPatternReset)
    {
        m_settings.uaDump.slsPattern = m_slsDumpPaternDefault;
        ui->dumpSlsPatternEdit->setText(m_slsDumpPaternDefault);
    }
    else
    {
        m_settings.uaDump.spiPattern = m_spiDumpPaternDefault;
        ui->dumpSpiPatternEdit->setText(m_spiDumpPaternDefault);
    }
    emit uaDumpSettings(m_settings.uaDump);
}

void SetupDialog::onGeolocationSourceChanged(int index)
{
    int srcInt = ui->locationSourceCombo->itemData(index).toInt();
    if (srcInt > 0)
    {
        ui->locationSrcWidget->setCurrentIndex(srcInt-1);
    }

    ui->locationSrcWidget->setEnabled(srcInt > 0);
    m_settings.tii.locationSource = static_cast<GeolocationSource>(srcInt);

    emit tiiSettingsChanged();
}

void SetupDialog::onCoordinateEditFinished()
{
    static const QRegularExpression splitRegex("\\s*,\\s*");
    QStringList coord = ui->coordinatesEdit->text().split(splitRegex);
    if (coord.size() != 2)
    {
        ui->coordinatesEdit->setText("0.0, 0.0");
        m_settings.tii.coordinates = QGeoCoordinate(0.0,0.0);
        emit tiiSettingsChanged();
        return;
    }

    bool ok;
    double longitude;
    double latitude = coord.at(0).toDouble(&ok);
    if (ok)
    {
        longitude = coord.at(1).toDouble(&ok);
    }
    if (!ok)
    {
        ui->coordinatesEdit->setText("0.0, 0.0");
        m_settings.tii.coordinates = QGeoCoordinate(0.0,0.0);
        emit tiiSettingsChanged();
        return;
    }

    QGeoCoordinate coordinate(latitude, longitude);
    if (!coordinate.isValid())
    {
        ui->coordinatesEdit->setText("0.0, 0.0");
        m_settings.tii.coordinates = QGeoCoordinate(0.0,0.0);
        emit tiiSettingsChanged();
        return;
    }

    m_settings.tii.coordinates = QGeoCoordinate(latitude, longitude);
    emit tiiSettingsChanged();
}

void SetupDialog::onSerialPortEditFinished()
{
    m_settings.tii.serialPort = ui->serialPortEdit->text().trimmed();
    emit tiiSettingsChanged();
}
