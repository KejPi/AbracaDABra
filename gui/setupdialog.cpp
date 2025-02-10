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

#include "setupdialog.h"

#include <QColorDialog>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QGeoCoordinate>
#include <QMovie>
#include <QRandomGenerator>
#include <QStandardItemModel>

#include "./ui_setupdialog.h"
#include "audiodecoder.h"
#include "rtlsdrinput.h"
#include "rtltcpinput.h"
#if HAVE_SOAPYSDR
#include "soapysdrinput.h"
#endif
#include "txdataloader.h"

SetupDialog::SetupDialog(QWidget *parent) : QDialog(parent), ui(new Ui::SetupDialog)
{
    ui->setupUi(this);

    // remove question mark from titlebar
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    ui->tabWidget->setTabText(SetupDialogTabs::Device, tr("Device"));
    ui->tabWidget->setTabText(SetupDialogTabs::Announcement, tr("Announcements"));
    ui->tabWidget->setTabText(SetupDialogTabs::Audio, tr("Audio"));
    ui->tabWidget->setTabText(SetupDialogTabs::UserApps, tr("User applications"));
    ui->tabWidget->setTabText(SetupDialogTabs::Tii, tr("TII"));
    ui->tabWidget->setTabText(SetupDialogTabs::Other, tr("Others"));
    ui->tabWidget->setCurrentIndex(SetupDialogTabs::Device);

    ui->inputCombo->addItem("RTL SDR", QVariant(int(InputDevice::Id::RTLSDR)));
    ui->inputCombo->addItem("RTL TCP", QVariant(int(InputDevice::Id::RTLTCP)));
#if HAVE_AIRSPY
    ui->inputCombo->addItem("Airspy", QVariant(int(InputDevice::Id::AIRSPY)));
#endif
#if HAVE_SOAPYSDR
    ui->inputCombo->addItem("SDRplay", QVariant(int(InputDevice::Id::SDRPLAY)));
    ui->inputCombo->addItem("Soapy SDR", QVariant(int(InputDevice::Id::SOAPYSDR)));
#endif
#if HAVE_RARTTCP
    ui->inputCombo->addItem("RaRT TCP", QVariant(int(InputDevice::Id::RARTTCP)));
#endif
    ui->inputCombo->addItem(tr("Raw file"), QVariant(int(InputDevice::Id::RAWFILE)));
    ui->inputCombo->setCurrentIndex(-1);  // undefined

    ui->fileNameLabel->setElideMode(Qt::ElideLeft);
    ui->fileNameLabel->setText(m_noFileString);
    m_rawfilename = "";

    ui->fileFormatCombo->insertItem(int(RawFileInputFormat::SAMPLE_FORMAT_U8), tr("Unsigned 8 bits"));
    ui->fileFormatCombo->insertItem(int(RawFileInputFormat::SAMPLE_FORMAT_S16), tr("Signed 16 bits"));

    // this has to be aligned with mainwindow
    ui->loopCheckbox->setChecked(false);

    ui->statusLabel->setText("<span style=\"color:red\">" + tr("No device connected") + "</span>");

    QString ipRange = "(?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])";
    QRegularExpression ipRegex("^" + ipRange + +"(\\." + ipRange + ")" + +"(\\." + ipRange + ")" + +"(\\." + ipRange + ")$");
    QRegularExpressionValidator *ipValidator = new QRegularExpressionValidator(ipRegex, this);
    ui->rtltcpIpAddressEdit->setValidator(ipValidator);
    ui->rarttcpIpAddressEdit->setValidator(ipValidator);
    ui->rtltcpSwAgcMaxLevel->setToolTip(ui->rtlsdrSwAgcMaxLevel->toolTip());
    ui->rtltcpSwAgcMaxLevel->setMinimumWidth(ui->rtlsdrSwAgcMaxLevel->width());

    ui->rtlsdrBiasTCombo->addItem(tr("Off"), false);
    ui->rtlsdrBiasTCombo->addItem(tr("On"), true);
    ui->rtlsdrBiasTCombo->setToolTip(
        QString(tr("Enable/disable bias tee.<br><br>"
                   "<b>WARNING:</b> Before using the bias tee please ensure that you understand "
                   "that you should not use this option when the dongle is connected directly "
                   "to a DC short circuited antenna unless you are using an LNA.")));

    ui->rtlsdrBandwidth->setToolTip(
        QString(tr("Input signal bandwidth in kHz. Value '0' means default bandwidth %1 kHz.")).arg(INPUTDEVICE_BANDWIDTH / 1000));
    ui->rtlsdrPPM->setToolTip(QString(tr("Input device XTAL frequency correction in PPM.")));
    ui->rtltcpPPM->setToolTip(ui->rtlsdrPPM->toolTip());
    ui->soapysdrPPM->setToolTip(ui->rtlsdrPPM->toolTip());
    ui->airspyBiasTCombo->addItem(tr("Off"), false);
    ui->airspyBiasTCombo->addItem(tr("On"), true);
    ui->airspyBiasTCombo->setToolTip(ui->rtlsdrBiasTCombo->toolTip());
    ui->soapysdrBandwidth->setToolTip(ui->rtlsdrBandwidth->toolTip());
    ui->soapysdrBandwidth->setMinimumWidth(ui->rtlsdrSwAgcMaxLevel->width());
    ui->sdrplayBiasTCombo->addItem(tr("Off"), false);
    ui->sdrplayBiasTCombo->addItem(tr("On"), true);
    ui->sdrplayBiasTCombo->setToolTip(ui->rtlsdrBiasTCombo->toolTip());

    ui->rtltcpControlSocketCheckbox->setText(tr("Connect to control socket if available"));
    ui->rtltcpControlSocketCheckbox->setToolTip(
        tr("Enable connection to control socket.<br>This option requires rtl-tcp server implemented by old-dab."));

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

    QLabel *label = new QLabel(tr("<br>Note: Alarm announcement cannot be disabled."));
    gridLayout->addWidget(label, 4, 0, 1, 2);
    QGroupBox *groupBox = new QGroupBox(tr("Alarm announcements"));
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
    for (int ann = static_cast<int>(DabAnnouncement::Alarm) + 1; ann < static_cast<int>(DabAnnouncement::AlarmTest); ++ann)
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
    groupBox = new QGroupBox(tr("Regular announcements"));
    groupBox->setLayout(gridLayout);
    vLayout->addWidget(groupBox);

    QSpacerItem *verticalSpacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
    vLayout->addItem(verticalSpacer);
    ui->tabAnnouncement->setLayout(vLayout);

    // gridLayout_4->addWidget(xmlHeaderWidget, 3, 0, 1, 3);
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
    label = new QLabel(tr("Model:"));
    m_xmlHeaderLabel[SetupDialogXmlHeader::XMLModel] = new QLabel;
    gridLayout->addWidget(label, SetupDialogXmlHeader::XMLModel, 0);
    gridLayout->addWidget(m_xmlHeaderLabel[SetupDialogXmlHeader::XMLModel], SetupDialogXmlHeader::XMLModel, 1);
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

    // RTL SDR device info
    row = 0;
    label = new QLabel(tr("Connected device:"), ui->rtlsdrInfoWidget);
    ui->rtlsdrInfoWidgetLayout->addWidget(label, row, 0);
    label = new QLabel(ui->rtlsdrInfoWidget);
    ui->rtlsdrInfoWidgetLayout->addWidget(label, row++, 1, 1, 2);
    m_rtlSdrLabel.append(label);
    label = new QLabel(tr("Serial number:"), ui->rtlsdrInfoWidget);
    ui->rtlsdrInfoWidgetLayout->addWidget(label, row, 0);
    label = new QLabel(ui->rtlsdrInfoWidget);
    ui->rtlsdrInfoWidgetLayout->addWidget(label, row++, 1, 1, 2);
    m_rtlSdrLabel.append(label);
    label = new QLabel(tr("Tuner:"), ui->rtlsdrInfoWidget);
    ui->rtlsdrInfoWidgetLayout->addWidget(label, row, 0);
    label = new QLabel(ui->rtlsdrInfoWidget);
    ui->rtlsdrInfoWidgetLayout->addWidget(label, row++, 1, 1, 2);
    m_rtlSdrLabel.append(label);
    label = new QLabel(tr("Sample format:"), ui->rtlsdrInfoWidget);
    ui->rtlsdrInfoWidgetLayout->addWidget(label, row, 0);
    label = new QLabel(ui->rtlsdrInfoWidget);
    ui->rtlsdrInfoWidgetLayout->addWidget(label, row++, 1, 1, 2);
    m_rtlSdrLabel.append(label);
    ui->rtlsdrInfoWidget->setVisible(false);

    row = 0;
    label = new QLabel(tr("Connected device:"), ui->rtltcpInfoWidget);
    ui->rtltcpInfoWidgetLayout->addWidget(label, row, 0);
    label = new QLabel(ui->rtltcpInfoWidget);
    ui->rtltcpInfoWidgetLayout->addWidget(label, row++, 1, 1, 2);
    m_rtlTcpLabel.append(label);
    label = new QLabel(tr("Tuner:"), ui->rtltcpInfoWidget);
    ui->rtltcpInfoWidgetLayout->addWidget(label, row, 0);
    label = new QLabel(ui->rtltcpInfoWidget);
    ui->rtltcpInfoWidgetLayout->addWidget(label, row++, 1, 1, 2);
    m_rtlTcpLabel.append(label);
    label = new QLabel(tr("Sample format:"), ui->rtltcpInfoWidget);
    ui->rtltcpInfoWidgetLayout->addWidget(label, row, 0);
    label = new QLabel(ui->rtltcpInfoWidget);
    ui->rtltcpInfoWidgetLayout->addWidget(label, row++, 1, 1, 2);
    m_rtlTcpLabel.append(label);
    ui->rtltcpInfoWidget->setVisible(false);
#if HAVE_AIRSPY
    // Airspy device info
    row = 0;
    label = new QLabel(tr("Connected device:"), ui->airspyInfoWidget);
    ui->airspyInfoWidgetLayout->addWidget(label, row, 0);
    label = new QLabel(ui->airspyInfoWidget);
    ui->airspyInfoWidgetLayout->addWidget(label, row++, 1, 1, 2);
    m_airspyLabel.append(label);
    label = new QLabel(tr("Serial number:"), ui->airspyInfoWidget);
    ui->airspyInfoWidgetLayout->addWidget(label, row, 0);
    label = new QLabel(ui->airspyInfoWidget);
    ui->airspyInfoWidgetLayout->addWidget(label, row++, 1, 1, 2);
    m_airspyLabel.append(label);
    ui->airspyInfoWidget->setVisible(false);
#endif
#if HAVE_SOAPYSDR
    // Soapy SDR device info
    row = 0;
    label = new QLabel(tr("Connected device:"), ui->soapysdrInfoWidget);
    ui->soapysdrInfoWidgetLayout->addWidget(label, row, 0);
    label = new QLabel(ui->soapysdrInfoWidget);
    ui->soapysdrInfoWidgetLayout->addWidget(label, row++, 1, 1, 2);
    m_soapySdrLabel.append(label);
    ui->soapysdrInfoWidget->setVisible(false);
    ui->soapysdrGainWidget->setLayout(new QGridLayout(ui->soapysdrGainWidget));
    ui->soapysdrGainWidget->setVisible(false);

    // SDRplay device info
    row = 0;
    label = new QLabel(tr("Connected device:"), ui->sdrplayInfoWidget);
    ui->sdrplayInfoWidgetLayout->addWidget(label, row, 0);
    label = new QLabel(ui->sdrplayInfoWidget);
    ui->sdrplayInfoWidgetLayout->addWidget(label, row++, 1, 1, 2);
    m_sdrPlayLabel.append(label);
    label = new QLabel(tr("Serial number:"), ui->sdrplayInfoWidget);
    ui->sdrplayInfoWidgetLayout->addWidget(label, row, 0);
    label = new QLabel(ui->sdrplayInfoWidget);
    ui->sdrplayInfoWidgetLayout->addWidget(label, row++, 1, 1, 2);
    m_sdrPlayLabel.append(label);
    ui->sdrplayInfoWidget->setVisible(false);
#endif
    connect(ui->openFileButton, &QPushButton::clicked, this, &SetupDialog::onOpenFileButtonClicked);

    connect(ui->inputCombo, &QComboBox::currentIndexChanged, this, &SetupDialog::onInputChanged);
    connect(ui->connectButton, &QPushButton::clicked, this, &SetupDialog::onConnectDeviceClicked);

    // device selection
    connect(ui->rtlsdrReloadButton, &QPushButton::clicked, this, [this]() { reloadDeviceList(InputDevice::Id::RTLSDR, ui->rtlsdrDeviceListCombo); });
    connect(ui->rtlsdrDeviceListCombo, &QComboBox::activated, this, [this]() { setConnectButton(ConnectButtonAuto); });
#ifdef HAVE_AIRSPY
    connect(ui->airspyReloadButton, &QPushButton::clicked, this, [this]() { reloadDeviceList(InputDevice::Id::AIRSPY, ui->airspyDeviceListCombo); });
    connect(ui->airspyDeviceListCombo, &QComboBox::activated, this, [this]() { setConnectButton(ConnectButtonAuto); });
#endif
#if HAVE_SOAPYSDR
    // SDRplay
    ui->sdrplayRFGainLabel->setFixedWidth(ui->sdrplayRFGainLabel->fontMetrics().boundingRect("-100 dB  ").width());
    connect(ui->sdrplayReloadButton, &QPushButton::clicked, this, &SetupDialog::onSdrplayReloadButtonClicked);
    connect(ui->sdrplayDeviceListCombo, &QComboBox::activated, this, [this]() { setConnectButton(ConnectButtonAuto); });
    connect(ui->sdrplayDeviceListCombo, &QComboBox::currentIndexChanged, this, &SetupDialog::onSdrplayDeviceChanged);
    connect(ui->sdrplayChannelCombo, &QComboBox::currentIndexChanged, this, &SetupDialog::onSdrplayChannelChanged);
#endif

    ui->defaultStyleRadioButton->setText(tr("Default style (OS dependent)"));
    ui->lightStyleRadioButton->setText(tr("Light style (Fusion with light colors)"));
    ui->darkStyleRadioButton->setText(tr("Dark style (Fusion with dark colors)"));

    // create combo box with language selection
    ui->langComboBox->addItem("<" + tr("System language") + ">", QVariant(QLocale::AnyLanguage));
    ui->langComboBox->addItem("English", QVariant(QLocale::English));  // QLocale::English native name returns "American English" :-(
    for (auto l : m_supportedLocalization)
    {
        ui->langComboBox->addItem(QLocale(l).nativeLanguageName(), QVariant(l));
    }
    ui->langComboBox->model()->sort(0);
    ui->langComboBox->setToolTip(tr("User interface language, the change will take effect after application restart."));
    ui->langWarningLabel->setText("<span style=\"color:red\">" + tr("Language change will take effect after application restart.") + "</span>");
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
    connect(ui->trayIconCheckBox, &QCheckBox::clicked, this, &SetupDialog::onTrayIconChecked);
    connect(ui->dlPlusCheckBox, &QCheckBox::clicked, this, &SetupDialog::onDLPlusChecked);
    connect(ui->xmlHeaderCheckBox, &QCheckBox::clicked, this, &SetupDialog::onXmlHeaderChecked);
    connect(ui->spiAppCheckBox, &QCheckBox::clicked, this, &SetupDialog::onSpiAppChecked);
    connect(ui->internetCheckBox, &QCheckBox::clicked, this, &SetupDialog::onUseInternetChecked);
    connect(ui->radioDNSCheckBox, &QCheckBox::clicked, this, &SetupDialog::onRadioDnsChecked);
    connect(ui->audioRecordingFolderButton, &QPushButton::clicked, this, &SetupDialog::onAudioRecordingFolderButtonClicked);
    connect(ui->audioInRecordingRadioButton, &QRadioButton::clicked, this, &SetupDialog::onAudioRecordingChecked);
    connect(ui->audioOutRecordingRadioButton, &QRadioButton::clicked, this, &SetupDialog::onAudioRecordingChecked);
    connect(ui->dlRecordCheckBox, &QCheckBox::clicked, this, &SetupDialog::onDlRecordingChecked);
    connect(ui->dlAbsTimeCheckBox, &QCheckBox::clicked, this, &SetupDialog::onDlAbsTimeChecked);

    ui->dataDumpFolderLabel->setElideMode(Qt::ElideLeft);
    ui->dumpSlsPatternEdit->setToolTip(
        tr("Storage path template for SLS application. Following tokens are supported:\n"
           "{serviceId, ensId, contentName, contentNameWithExt, transportId}"));
    ui->dumpSpiPatternEdit->setToolTip(
        tr("Storage path template for SPI application. Following tokens are supported:\n"
           "{serviceId, ensId, scId, contentName, directoryId, transportId}"));
    connect(ui->dataDumpFolderButton, &QPushButton::clicked, this, &SetupDialog::onDataDumpFolderButtonClicked);
    connect(ui->dumpSlsCheckBox, &QCheckBox::toggled, this, &SetupDialog::onDataDumpCheckboxToggled);
    connect(ui->dumpSpiCheckBox, &QCheckBox::toggled, this, &SetupDialog::onDataDumpCheckboxToggled);
    connect(ui->dumpOverwriteCheckbox, &QCheckBox::toggled, this, &SetupDialog::onDataDumpCheckboxToggled);
    connect(ui->dumpSlsPatternReset, &QPushButton::clicked, this, &SetupDialog::onDataDumpResetClicked);
    connect(ui->dumpSpiPatternReset, &QPushButton::clicked, this, &SetupDialog::onDataDumpResetClicked);
    connect(ui->dumpSlsPatternEdit, &QLineEdit::editingFinished, this, &SetupDialog::onDataDumpPatternEditingFinished);
    connect(ui->dumpSpiPatternEdit, &QLineEdit::editingFinished, this, &SetupDialog::onDataDumpPatternEditingFinished);
    connect(ui->rawFileSlider, &QSlider::valueChanged, this, &SetupDialog::onRawFileProgressChanged);
    connect(ui->rawFileSlider, &QSlider::sliderReleased, this, [this]() { emit rawFileSeek(ui->rawFileSlider->value()); });

    // reset UI
    onFileLength(0);

    ui->defaultStyleRadioButton->setChecked(true);

    ui->noiseConcealmentCombo->addItem("-20 dBFS", QVariant(20));
    ui->noiseConcealmentCombo->addItem("-25 dBFS", QVariant(25));
    ui->noiseConcealmentCombo->addItem("-30 dBFS", QVariant(30));
    ui->noiseConcealmentCombo->addItem("-35 dBFS", QVariant(35));
    ui->noiseConcealmentCombo->addItem("-40 dBFS", QVariant(40));
    ui->noiseConcealmentCombo->addItem(tr("Disabled"), QVariant(0));
    ui->noiseConcealmentCombo->setToolTip(
        tr("Select noise level that is generated during audio interruptions.<br>"
           "This may help to improve listening experience and make the audio interruptions less annoying."));
#if AUDIO_DECODER_NOISE_CONCEALMENT
    connect(ui->noiseConcealmentCombo, &QComboBox::currentIndexChanged, this, &SetupDialog::onNoiseLevelChanged);
#else
    ui->audioDecoderGroupBox->setVisible(false);
#endif

    ui->locationSourceCombo->addItem(tr("System"), QVariant::fromValue(Settings::GeolocationSource::System));
    ui->locationSourceCombo->addItem(tr("Manual"), QVariant::fromValue(Settings::GeolocationSource::Manual));
    ui->locationSourceCombo->addItem(tr("NMEA Serial Port"), QVariant::fromValue(Settings::GeolocationSource::SerialPort));
    connect(ui->locationSourceCombo, &QComboBox::currentIndexChanged, this, &SetupDialog::onGeolocationSourceChanged);
    ui->locationSrcWidget->setEnabled(false);
    ui->coordinatesEdit->setText("0.0, 0.0");
    ui->tiiModeCombo->addItem(tr("Default"), DABSDR_TII_MODE_DEFAULT);
    ui->tiiModeCombo->addItem(tr("Conservative"), DABSDR_TII_MODE_CONSERVATIVE);
    ui->tiiModeCombo->setToolTip(
        tr("TII detector configuration defines its sensitivity and reliability.<br>"
           "Conservative configuration is less sensitive but more reliable."));
    connect(ui->tiiModeCombo, &QComboBox::currentIndexChanged, this, &SetupDialog::onTiiModeChanged);
#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
    connect(ui->tiiSpectPlotCheckBox, &QCheckBox::clicked, this, &SetupDialog::onTiiSpectPlotClicked);
#else
    ui->tiiHline->setVisible(false);
    ui->tiiSpectPlotCheckBox->setVisible(false);
#endif

    static const QRegularExpression coordRe("[+-]?[0-9]+(\\.[0-9]+)?\\s*,\\s*[+-]?[0-9]+(\\.[0-9]+)?");
    QRegularExpressionValidator *coordValidator = new QRegularExpressionValidator(coordRe, this);
    ui->coordinatesEdit->setValidator(coordValidator);
    connect(ui->coordinatesEdit, &QLineEdit::editingFinished, this, &SetupDialog::onCoordinateEditFinished);
    connect(ui->serialPortEdit, &QLineEdit::editingFinished, this, &SetupDialog::onSerialPortEditFinished);

    ui->spinnerLabel->setVisible(false);
#if HAVE_FMLIST_INTERFACE
    m_spinner = new QMovie(this);
    m_spinner->setScaledSize(QSize(18, 18));
    ui->spinnerLabel->setMovie(m_spinner);
    ui->spinnerLabel->setFixedSize(QSize(18, 18));
    connect(ui->updateDbButton, &QPushButton::clicked, this, &SetupDialog::onTiiUpdateDbClicked);
#else
    ui->updateDbButton->setEnabled(false);
#endif
    ui->tiiLogFolderLabel->setElideMode(Qt::ElideLeft);
    connect(ui->tiiLogFolderButton, &QPushButton::clicked, this, &SetupDialog::onTiiLogFolderButtonClicked);

    ui->proxyConfigCombo->addItem(tr("No proxy"), QVariant::fromValue(Settings::ProxyConfig::NoProxy));
    ui->proxyConfigCombo->addItem(tr("System"), QVariant::fromValue(Settings::ProxyConfig::System));
    ui->proxyConfigCombo->addItem(tr("Manual"), QVariant::fromValue(Settings::ProxyConfig::Manual));
    ui->proxyApplyButton->setEnabled(false);
    ui->proxyPortEdit->setMaximumWidth(ui->proxyPortEdit->fontMetrics().boundingRect("00000000").width());
    ui->proxyPortEdit->setAlignment(Qt::AlignHCenter);
    connect(ui->proxyServerEdit, &QLineEdit::textEdited, this, &SetupDialog::onProxyConfigEdit);
    connect(ui->proxyPortEdit, &QLineEdit::textEdited, this, &SetupDialog::onProxyConfigEdit);
    connect(ui->proxyUserEdit, &QLineEdit::textEdited, this, &SetupDialog::onProxyConfigEdit);
    connect(ui->proxyPassEdit, &QLineEdit::textEdited, this, &SetupDialog::onProxyConfigEdit);
    connect(ui->proxyConfigCombo, &QComboBox::currentIndexChanged, this, &SetupDialog::onProxyConfigChanged);
    connect(ui->proxyApplyButton, &QPushButton::clicked, this, &SetupDialog::onProxyApplyButtonClicked);

    connect(ui->slsBgButton, &QPushButton::clicked, this, &SetupDialog::onSlsBgButtonClicked);
    ui->slsBgButton->setToolTip(tr("Select slideshow background color"));

#if (QT_VERSION >= QT_VERSION_CHECK(6, 7, 0))
    connect(ui->fmlistUploadCheckBox, &QCheckBox::checkStateChanged, this, &SetupDialog::setFmlistUploadInfoText);
#else
    connect(ui->fmlistUploadCheckBox, &QCheckBox::stateChanged, this, &SetupDialog::setFmlistUploadInfoText);
#endif

    QTimer::singleShot(10, this, [this]() { resize(minimumSizeHint()); });
}

void SetupDialog::showEvent(QShowEvent *event)
{
    QDateTime lastModified = TxDataLoader::lastUpdateTime();
    if (lastModified.isValid())
    {
        ui->tiiDbLabel->setText(tr("Last update: ") + lastModified.toString("dd.MM.yyyy"));
    }
    else
    {
        ui->tiiDbLabel->setText(tr("Data not available"));
    }
#if HAVE_FMLIST_INTERFACE
    ui->updateDbButton->setEnabled(!lastModified.isValid() || lastModified.msecsTo(QDateTime::currentDateTime()) > 10 * 60 * 1000);
#endif
    ui->tabWidget->setFocus();
    QDialog::showEvent(event);

    // QTimer::singleShot(10, this, [this](){ adjustSize(); } );
    QTimer::singleShot(10, this, [this]() { resize(minimumSizeHint()); });
}

void SetupDialog::setSpiDumpPaternDefault(const QString &newSpiDumpPaternDefault)
{
    m_spiDumpPaternDefault = newSpiDumpPaternDefault;
}

void SetupDialog::setSlsDumpPaternDefault(const QString &newSlsDumpPaternDefault)
{
    m_slsDumpPaternDefault = newSlsDumpPaternDefault;
}

void SetupDialog::setGainValues(const QList<float> &gainList)
{
    switch (m_inputDeviceId)
    {
        case InputDevice::Id::RTLSDR:
            m_rtlsdrGainList.clear();
            m_rtlsdrGainList = gainList;
            ui->rtlsdrGainSlider->setMinimum(0);
            ui->rtlsdrGainSlider->setMaximum(m_rtlsdrGainList.size() - 1);
            ui->rtlsdrGainSlider->setValue((m_settings->rtlsdr.gainIdx >= 0) ? m_settings->rtlsdr.gainIdx : 0);
            ui->rtlsdrGainSlider->setDisabled(m_rtlsdrGainList.empty());
            ui->rtlsdrGainModeManual->setDisabled(m_rtlsdrGainList.empty());
            break;
        case InputDevice::Id::RTLTCP:
            m_rtltcpGainList.clear();
            m_rtltcpGainList = gainList;
            ui->rtltcpGainSlider->setMinimum(0);
            ui->rtltcpGainSlider->setMaximum(m_rtltcpGainList.size() - 1);
            ui->rtltcpGainSlider->setValue((m_settings->rtltcp.gainIdx >= 0) ? m_settings->rtltcp.gainIdx : 0);
            ui->rtltcpGainSlider->setDisabled(m_rtltcpGainList.empty());
            ui->rtltcpGainModeManual->setDisabled(m_rtltcpGainList.empty());
            break;
        case InputDevice::Id::SDRPLAY:
#if HAVE_SOAPYSDR
            m_sdrplayGainList.clear();
            m_sdrplayGainList = gainList;
            ui->sdrplayRFGainSlider->setMinimum(0);
            ui->sdrplayRFGainSlider->setMaximum(m_sdrplayGainList.size() - 1);
            if (m_settings->sdrplay.gain.rfGain >= 0)
            {
                ui->sdrplayRFGainSlider->setValue(m_settings->sdrplay.gain.rfGain);
                onSdrplayRFGainSliderChanged(m_settings->sdrplay.gain.rfGain);
            }
            else
            {
                ui->sdrplayRFGainSlider->setValue(m_sdrplayGainList.size() - 1);
            }
            if (m_settings->sdrplay.gain.ifGain != 0)
            {
                ui->sdrplayIFGainSlider->setValue(m_settings->sdrplay.gain.ifGain);
                onSdrplayIFGainSliderChanged(m_settings->sdrplay.gain.ifGain);
            }
            else
            {
                ui->sdrplayIFGainSlider->setValue(-40);
            }
            ui->sdrplayIFAGCCheckbox->setChecked(m_settings->sdrplay.gain.ifAgcEna);
#endif
            break;
        case InputDevice::Id::SOAPYSDR:
        case InputDevice::Id::UNDEFINED:
        case InputDevice::Id::RAWFILE:
        case InputDevice::Id::AIRSPY:
        case InputDevice::Id::RARTTCP:
            return;
    }
}

void SetupDialog::setInputDeviceEnabled(bool ena)
{
    ui->inputCombo->setEnabled(ena);
}

void SetupDialog::setSettings(Settings *settings)
{
    m_settings = settings;
#if (QT_VERSION >= QT_VERSION_CHECK(6, 7, 0))
    connect(ui->loopCheckbox, &QCheckBox::checkStateChanged, this, [=](int val) { m_settings->rawfile.loopEna = (Qt::Unchecked != val); });
#else
    connect(ui->loopCheckbox, &QCheckBox::stateChanged, this, [=](int val) { m_settings->rawfile.loopEna = (Qt::Unchecked != val); });
#endif
    connect(ui->autoStopRecordingCheckBox, &QCheckBox::toggled, this, [this](bool checked) { m_settings->audioRec.autoStopEna = checked; });
    connect(ui->checkForUpdates, &QCheckBox::toggled, this, &SetupDialog::setCheckUpdatesEna);

    setUiState();
    connectDeviceControlSignals();

    onExpertModeChecked(m_settings->expertModeEna);
    setStatusLabel();
    emit newAnnouncementSettings();
    emit noiseConcealmentLevelChanged(m_settings->noiseConcealmentLevel);
    emit xmlHeaderToggled(m_settings->xmlHeaderEna);
    emit audioRecordingSettings(m_settings->audioRec.folder, m_settings->audioRec.captureOutput);
    emit uaDumpSettings(m_settings->uaDump);
    emit tiiSettingsChanged();
    emit tiiModeChanged(m_settings->tii.mode);
    onUseInternetChecked(m_settings->useInternet);
    onSpiAppChecked(m_settings->spiAppEna);
    onDlRecordingChecked(m_settings->audioRec.dl);
    emit proxySettingsChanged();
    emit trayIconToggled(m_settings->trayIconEna);
    emit slsBgChanged(m_settings->slsBackground);
}

void SetupDialog::onFileLength(int msec)
{
    ui->rawFileSlider->setMinimum(0);
    ui->rawFileSlider->setMaximum(msec);
    ui->rawFileSlider->setValue(0);
    onRawFileProgressChanged(0);

    ui->rawFileSlider->setVisible(0 != msec);
    ui->rawFileSlider->setEnabled(false);

    onRawFileProgressChanged(0);

    int labelWidth = ui->rawFileTime->fontMetrics().boundingRect(QString("%1 / %1 " + tr("sec")).arg(msec / 1000.0, 0, 'f', 1)).width() + 10;
    ui->rawFileTime->setMinimumWidth(labelWidth);
    ui->rawFileTime->setVisible(0 != msec);
}

void SetupDialog::onFileProgress(int msec)
{
    if (!ui->rawFileSlider->isSliderDown())
    {
        ui->rawFileSlider->setValue(msec);
    }
    ui->rawFileSlider->setEnabled(msec != 0);
}

void SetupDialog::setAudioRecAutoStop(bool ena)
{
    m_settings->audioRec.autoStopEna = ena;
    ui->autoStopRecordingCheckBox->setChecked(ena);
}

void SetupDialog::setCheckUpdatesEna(bool ena)
{
    m_settings->updateCheckEna = ena;
    ui->checkForUpdates->setChecked(ena);
}

QLocale::Language SetupDialog::applicationLanguage() const
{
    if (m_supportedLocalization.contains(m_settings->lang) || (QLocale::English == m_settings->lang))
    {  // selected specific localization
        return m_settings->lang;
    }
    else
    {  // system default or not supported
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

    int index = ui->inputCombo->findData(QVariant(static_cast<int>(m_settings->inputDevice)));
    if (index < 0)
    {  // not found -> show rtlsdr as default
        index = ui->inputCombo->findData(QVariant(static_cast<int>(InputDevice::Id::RTLSDR)));
    }
    ui->inputCombo->setCurrentIndex(index);

    if (!m_rtlsdrGainList.isEmpty())
    {
        ui->rtlsdrGainSlider->setValue(m_settings->rtlsdr.gainIdx);
        onRtlSdrGainSliderChanged(m_settings->rtlsdr.gainIdx);
    }
    else
    {
        ui->rtlsdrGainSlider->setValue(0);
        ui->rtlsdrGainValueLabel->setText(tr("N/A"));
    }
    if (!m_rtltcpGainList.isEmpty())
    {
        ui->rtltcpGainSlider->setValue(m_settings->rtltcp.gainIdx);
        onRtlTcpGainSliderChanged(m_settings->rtltcp.gainIdx);
    }
    else
    {
        ui->rtltcpGainSlider->setValue(0);
        ui->rtltcpGainValueLabel->setText(tr("N/A"));
    }

    switch (m_settings->rtlsdr.gainMode)
    {
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
    ui->rtltcpIpAddressEdit->setText(m_settings->rtltcp.tcpAddress);
    ui->rtltcpIpPortSpinBox->setValue(m_settings->rtltcp.tcpPort);
    ui->rtltcpControlSocketCheckbox->setChecked(m_settings->rtltcp.controlSocketEna);
    ui->rtlsdrBandwidth->setValue(m_settings->rtlsdr.bandwidth / 1000);
    ui->rtlsdrBandwidthDefault->setEnabled(m_settings->rtlsdr.bandwidth != 0);
    ui->rtlsdrBiasTCombo->setCurrentIndex(m_settings->rtlsdr.biasT ? 1 : 0);
    ui->rtlsdrSwAgcMaxLevel->setValue(m_settings->rtlsdr.agcLevelMax);
    ui->rtlsdrSwAgcMaxLevelDefault->setEnabled(m_settings->rtlsdr.agcLevelMax != 0);
    ui->rtlsdrPPM->setValue(m_settings->rtlsdr.ppm);

    switch (m_settings->rtltcp.gainMode)
    {
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
    ui->rtltcpSwAgcMaxLevel->setValue(m_settings->rtltcp.agcLevelMax);
    ui->rtltcpSwAgcMaxLevelDefault->setEnabled(m_settings->rtltcp.agcLevelMax != 0);
    ui->rtltcpPPM->setValue(m_settings->rtltcp.ppm);

#if HAVE_AIRSPY
    switch (m_settings->airspy.gain.mode)
    {
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

    ui->airspySensitivityGainSlider->setValue(m_settings->airspy.gain.sensitivityGainIdx);
    ui->airspyIFGainSlider->setValue(m_settings->airspy.gain.ifGainIdx);
    ui->airspyLNAGainSlider->setValue(m_settings->airspy.gain.lnaGainIdx);
    ui->airspyMixerGainSlider->setValue(m_settings->airspy.gain.mixerGainIdx);
    ui->airspyMixerAGCCheckbox->setChecked(m_settings->airspy.gain.mixerAgcEna);
    ui->airspyLNAAGCCheckbox->setChecked(m_settings->airspy.gain.lnaAgcEna);
    ui->airspyBiasTCombo->setCurrentIndex(m_settings->airspy.biasT ? 1 : 0);
    onAirspySensitivityGainSliderChanged(m_settings->airspy.gain.sensitivityGainIdx);
    onAirspyIFGainSliderChanged(m_settings->airspy.gain.ifGainIdx);
    onAirspyLNAGainSliderChanged(m_settings->airspy.gain.lnaGainIdx);
    onAirspyMixerGainSliderChanged(m_settings->airspy.gain.mixerGainIdx);
#endif

#if HAVE_SOAPYSDR
    if (m_settings->soapysdr.driver.isEmpty() || !m_settings->soapysdr.gainMap.contains(m_settings->soapysdr.driver))
    {  // no last driver -> using defaults
        ui->soapysdrGainModeHw->setChecked(true);
    }
    else
    {
        switch (m_settings->soapysdr.gainMap[m_settings->soapysdr.driver].mode)
        {
            case SoapyGainMode::Hardware:
                ui->soapysdrGainModeHw->setChecked(true);
                break;
            case SoapyGainMode::Manual:
                ui->soapysdrGainModeManual->setChecked(true);
                break;
            default:
                break;
        }
    }
    ui->soapysdrGainWidget->setVisible(false);

    ui->soapysdrDevArgs->setText(m_settings->soapysdr.devArgs);
    ui->soapysdrChannelNum->setValue(m_settings->soapysdr.channel);
    ui->soapySdrAntenna->setText(m_settings->soapysdr.antenna);
    ui->soapysdrBandwidth->setValue(m_settings->soapysdr.bandwidth / 1000);
    ui->soapysdrBandwidthDefault->setEnabled(m_settings->soapysdr.bandwidth != 0);
    ui->soapysdrPPM->setValue(m_settings->soapysdr.ppm);

    switch (m_settings->sdrplay.gain.mode)
    {
        case SdrPlayGainMode::Software:
            ui->sdrplayGainModeSw->setChecked(true);
            break;
        case SdrPlayGainMode::Manual:
            ui->sdrplayGainModeManual->setChecked(true);
            break;
        default:
            break;
    }
    if (!m_sdrplayGainList.isEmpty())
    {
        ui->sdrplayRFGainSlider->setValue(m_settings->sdrplay.gain.rfGain);
        onSdrplayRFGainSliderChanged(m_settings->sdrplay.gain.rfGain);
    }
    else
    {
        ui->sdrplayRFGainSlider->setValue(0);
        ui->sdrplayRFGainLabel->setText(tr("N/A  "));
    }
    ui->sdrplayIFGainSlider->setValue(0);
    ui->sdrplayIFGainLabel->setText(tr("N/A  "));
    ui->sdrplayIFAGCCheckbox->setChecked(m_settings->sdrplay.gain.ifAgcEna);
    ui->sdrplayPPM->setValue(m_settings->sdrplay.ppm);
    ui->sdrplayBiasTCombo->setCurrentIndex(m_settings->sdrplay.biasT ? 1 : 0);
#endif

    if (m_settings->rawfile.file.isEmpty())
    {
        ui->fileNameLabel->setText(m_noFileString);
        m_rawfilename = "";
        ui->fileNameLabel->setToolTip("");
    }
    else
    {
        ui->fileNameLabel->setText(m_settings->rawfile.file);
        m_rawfilename = m_settings->rawfile.file;
        ui->fileNameLabel->setToolTip(m_settings->rawfile.file);
    }
    ui->loopCheckbox->setChecked(m_settings->rawfile.loopEna);
    ui->fileFormatCombo->setCurrentIndex(static_cast<int>(m_settings->rawfile.format));
#if HAVE_RARTTCP
    ui->rarttcpIpAddressEdit->setText(m_settings->rarttcp.tcpAddress);
    ui->rarttcpIpPortSpinBox->setValue(m_settings->rarttcp.tcpPort);
#endif

    setStatusLabel();

    // announcements
    uint16_t announcementEna = m_settings->announcementEna | (1 << static_cast<int>(DabAnnouncement::Alarm));  // enable alarm
    for (int a = 0; a < static_cast<int>(DabAnnouncement::Undefined); ++a)
    {
        m_announcementCheckBox[a]->setChecked(announcementEna & 0x1);
        announcementEna >>= 1;
    }
    m_bringWindowToForegroundCheckbox->setChecked(m_settings->bringWindowToForeground);

    // other settings
    switch (m_settings->applicationStyle)
    {
        case Settings::ApplicationStyle::Default:
            ui->defaultStyleRadioButton->setChecked(true);
            break;
        case Settings::ApplicationStyle::Light:
            ui->lightStyleRadioButton->setChecked(true);
            break;
        case Settings::ApplicationStyle::Dark:
            ui->darkStyleRadioButton->setChecked(true);
            break;
    }
    ui->expertCheckBox->setChecked(m_settings->expertModeEna);
    ui->trayIconCheckBox->setChecked(m_settings->trayIconEna);
    ui->dlPlusCheckBox->setChecked(m_settings->dlPlusEna);

    index = ui->noiseConcealmentCombo->findData(QVariant(m_settings->noiseConcealmentLevel));
    if (index < 0)
    {  // not found
        index = 0;
    }
    ui->noiseConcealmentCombo->setCurrentIndex(index);
    ui->xmlHeaderCheckBox->setChecked(m_settings->xmlHeaderEna);
    ui->spiAppCheckBox->setChecked(m_settings->spiAppEna);
    ui->internetCheckBox->setChecked(m_settings->useInternet);
    ui->radioDNSCheckBox->setChecked(m_settings->radioDnsEna);

    index = ui->langComboBox->findData(QVariant(m_settings->lang));
    if (index < 0)
    {  // not found
        index = 0;
    }
    ui->langComboBox->setCurrentIndex(index);

    ui->audioRecordingFolderLabel->setText(m_settings->audioRec.folder);
    if (m_settings->audioRec.captureOutput)
    {
        ui->audioOutRecordingRadioButton->setChecked(true);
    }
    else
    {
        ui->audioInRecordingRadioButton->setChecked(true);
    }
    ui->autoStopRecordingCheckBox->setChecked(m_settings->audioRec.autoStopEna);
    ui->dlAbsTimeCheckBox->setChecked(m_settings->audioRec.dlAbsTime);
    ui->dlRecordCheckBox->setChecked(m_settings->audioRec.dl);

    ui->dataDumpFolderLabel->setText(m_settings->uaDump.folder);
    ui->dumpSlsPatternEdit->setText(m_settings->uaDump.slsPattern);
    ui->dumpSpiPatternEdit->setText(m_settings->uaDump.spiPattern);
    // blocker is required to avoid triggering unwanted signal
    // we need to set UI state first
    {
        const QSignalBlocker blocker(ui->dumpSlsCheckBox);
        // no signals here
        ui->dumpSlsCheckBox->setChecked(m_settings->uaDump.slsEna);
    }
    {
        const QSignalBlocker blocker(ui->dumpSpiCheckBox);
        // no signals here
        ui->dumpSpiCheckBox->setChecked(m_settings->uaDump.spiEna);
    }
    // this may trigger the signal (no problem if it happens)
    ui->dumpOverwriteCheckbox->setChecked(m_settings->uaDump.overwriteEna);
    ui->coordinatesEdit->setText(QString("%1, %2")
                                     .arg(m_settings->tii.coordinates.latitude(), 0, 'g', QLocale::FloatingPointShortest)
                                     .arg(m_settings->tii.coordinates.longitude(), 0, 'g', QLocale::FloatingPointShortest));
    ui->serialPortEdit->setText(m_settings->tii.serialPort);
    ui->locationSourceCombo->setCurrentIndex(static_cast<int>(m_settings->tii.locationSource));
    ui->tiiSpectPlotCheckBox->setChecked(m_settings->tii.showSpectumPlot);
    ui->tiiLogFolderLabel->setText(m_settings->tii.logFolder);
    ui->tiiModeCombo->setCurrentIndex(static_cast<int>(m_settings->tii.mode));

    ui->checkForUpdates->setChecked(m_settings->updateCheckEna);
    ui->proxyConfigCombo->setCurrentIndex(static_cast<int>(m_settings->proxy.config));
    ui->proxyServerEdit->setText(m_settings->proxy.server);
    ui->proxyPortEdit->setText(QString::number(m_settings->proxy.port));
    ui->proxyUserEdit->setText(m_settings->proxy.user);
    ui->proxyPassEdit->setText(m_settings->proxy.pass);
    onProxyConfigChanged(static_cast<int>(m_settings->proxy.config));

    QString qss = QString("QPushButton {background-color: %1; border: 1px solid black; border-radius: 3px}")
                      .arg(m_settings->slsBackground.name(QColor::HexArgb));
    ui->slsBgButton->setStyleSheet(qss);
    ui->slsBgButton->setMaximumWidth(ui->slsBgButton->height() * 2);

    ui->fmlistUploadCheckBox->setChecked(m_settings->uploadEnsembleInfo);
    setFmlistUploadInfoText();
}

void SetupDialog::connectDeviceControlSignals()
{
    connect(ui->rtlsdrGainSlider, &QSlider::valueChanged, this, &SetupDialog::onRtlSdrGainSliderChanged);
    connect(ui->rtlsdrGainModeHw, &QRadioButton::toggled, this, &SetupDialog::onRtlSdrGainModeToggled);
    connect(ui->rtlsdrGainModeSw, &QRadioButton::toggled, this, &SetupDialog::onRtlSdrGainModeToggled);
    connect(ui->rtlsdrGainModeManual, &QRadioButton::toggled, this, &SetupDialog::onRtlSdrGainModeToggled);
    connect(ui->rtlsdrBandwidth, &QSpinBox::valueChanged, this, &SetupDialog::onBandwidthChanged);
    connect(ui->rtlsdrBandwidthDefault, &QPushButton::clicked, this, [this]() { ui->rtlsdrBandwidth->setValue(0); });
    connect(ui->rtlsdrSwAgcMaxLevel, &QSpinBox::valueChanged, this, &SetupDialog::onRtlSdrSwAgcMaxLevelChanged);
    connect(ui->rtlsdrSwAgcMaxLevelDefault, &QPushButton::clicked, this, [this]() { ui->rtlsdrSwAgcMaxLevel->setValue(0); });
    connect(ui->rtlsdrBiasTCombo, &QComboBox::currentIndexChanged, this, &SetupDialog::onBiasTChanged);
    connect(ui->rtlsdrPPM, &QSpinBox::valueChanged, this, &SetupDialog::onPPMChanged);

    connect(ui->rtltcpGainSlider, &QSlider::valueChanged, this, &SetupDialog::onRtlTcpGainSliderChanged);
    connect(ui->rtltcpGainModeHw, &QRadioButton::toggled, this, &SetupDialog::onTcpGainModeToggled);
    connect(ui->rtltcpGainModeSw, &QRadioButton::toggled, this, &SetupDialog::onTcpGainModeToggled);
    connect(ui->rtltcpGainModeManual, &QRadioButton::toggled, this, &SetupDialog::onTcpGainModeToggled);
    connect(ui->rtltcpIpAddressEdit, &QLineEdit::editingFinished, this, &SetupDialog::onRtlTcpIpAddrEditFinished);
    connect(ui->rtltcpIpPortSpinBox, &QSpinBox::valueChanged, this, &SetupDialog::onRtlTcpPortValueChanged);
    connect(ui->rtltcpSwAgcMaxLevel, &QSpinBox::valueChanged, this, &SetupDialog::onRtlTcpSwAgcMaxLevelChanged);
    connect(ui->rtltcpSwAgcMaxLevelDefault, &QPushButton::clicked, this, [this]() { ui->rtltcpSwAgcMaxLevel->setValue(0); });
    connect(ui->rtltcpPPM, &QSpinBox::valueChanged, this, &SetupDialog::onPPMChanged);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 7, 0))
    connect(ui->rtltcpControlSocketCheckbox, &QCheckBox::checkStateChanged, this, &SetupDialog::onRtlTcpControlSocketChecked);
#else
    connect(ui->rtltcpControlSocketCheckbox, &QCheckBox::stateChanged, this, &SetupDialog::onRtlTcpControlSocketChecked);
#endif

    connect(ui->fileFormatCombo, &QComboBox::currentIndexChanged, this, &SetupDialog::onRawFileFormatChanged);

#if HAVE_AIRSPY
    connect(ui->airspySensitivityGainSlider, &QSlider::valueChanged, this, &SetupDialog::onAirspySensitivityGainSliderChanged);
    connect(ui->airspyIFGainSlider, &QSlider::valueChanged, this, &SetupDialog::onAirspyIFGainSliderChanged);
    connect(ui->airspyLNAGainSlider, &QSlider::valueChanged, this, &SetupDialog::onAirspyLNAGainSliderChanged);
    connect(ui->airspyMixerGainSlider, &QSlider::valueChanged, this, &SetupDialog::onAirspyMixerGainSliderChanged);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 7, 0))
    connect(ui->airspyLNAAGCCheckbox, &QCheckBox::checkStateChanged, this, &SetupDialog::onAirspyLNAAGCstateChanged);
    connect(ui->airspyMixerAGCCheckbox, &QCheckBox::checkStateChanged, this, &SetupDialog::onAirspyMixerAGCstateChanged);
#else
    connect(ui->airspyLNAAGCCheckbox, &QCheckBox::stateChanged, this, &SetupDialog::onAirspyLNAAGCstateChanged);
    connect(ui->airspyMixerAGCCheckbox, &QCheckBox::stateChanged, this, &SetupDialog::onAirspyMixerAGCstateChanged);
#endif
    connect(ui->airspyGainModeHybrid, &QRadioButton::toggled, this, &SetupDialog::onAirspyModeToggled);
    connect(ui->airspyGainModeSw, &QRadioButton::toggled, this, &SetupDialog::onAirspyModeToggled);
    connect(ui->airspyGainModeManual, &QRadioButton::toggled, this, &SetupDialog::onAirspyModeToggled);
    connect(ui->airspyGainModeSensitivity, &QRadioButton::toggled, this, &SetupDialog::onAirspyModeToggled);
    connect(ui->airspyBiasTCombo, &QComboBox::currentIndexChanged, this, &SetupDialog::onBiasTChanged);
#endif

#if HAVE_SOAPYSDR
    connect(ui->soapysdrDevArgs, &QLineEdit::textEdited, this, [this]() { setConnectButton(ConnectButtonAuto); });
    connect(ui->soapySdrAntenna, &QLineEdit::textEdited, this, [this]() { setConnectButton(ConnectButtonAuto); });
    connect(ui->soapysdrChannelNum, &QSpinBox::valueChanged, this, [this]() { setConnectButton(ConnectButtonAuto); });
    connect(ui->soapysdrGainModeManual, &QRadioButton::toggled, this, &SetupDialog::onSoapySdrGainModeToggled);
    connect(ui->soapysdrGainModeHw, &QRadioButton::toggled, this, &SetupDialog::onSoapySdrGainModeToggled);
    connect(ui->soapysdrBandwidth, &QSpinBox::valueChanged, this, &SetupDialog::onBandwidthChanged);
    connect(ui->soapysdrBandwidthDefault, &QPushButton::clicked, this, [this]() { ui->soapysdrBandwidth->setValue(0); });
    connect(ui->soapysdrPPM, &QSpinBox::valueChanged, this, &SetupDialog::onPPMChanged);

    connect(ui->sdrplayGainModeSw, &QRadioButton::toggled, this, &SetupDialog::onSdrplayModeToggled);
    connect(ui->sdrplayGainModeManual, &QRadioButton::toggled, this, &SetupDialog::onSdrplayModeToggled);
    connect(ui->sdrplayRFGainSlider, &QSlider::valueChanged, this, &SetupDialog::onSdrplayRFGainSliderChanged);
    connect(ui->sdrplayIFGainSlider, &QSlider::valueChanged, this, &SetupDialog::onSdrplayIFGainSliderChanged);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 7, 0))
    connect(ui->sdrplayIFAGCCheckbox, &QCheckBox::checkStateChanged, this, &SetupDialog::onSdrplayAGCstateChanged);
#else
    connect(ui->sdrplayIFAGCCheckbox, &QCheckBox::stateChanged, this, &SetupDialog::onSdrplayAGCstateChanged);
#endif
    connect(ui->sdrplayPPM, &QSpinBox::valueChanged, this, &SetupDialog::onPPMChanged);
    connect(ui->sdrplayBiasTCombo, &QComboBox::currentIndexChanged, this, &SetupDialog::onBiasTChanged);
#endif
}

void SetupDialog::setupDarkMode(bool darkModeEna)
{
#if HAVE_FMLIST_INTERFACE
    bool isRunning = m_spinner->state() == QMovie::Running;
    m_spinner->stop();
    if (darkModeEna)
    {
        m_spinner->setFileName(":/resources/spinner_dark.gif");
    }
    else
    {
        m_spinner->setFileName(":/resources/spinner.gif");
    }
    ui->spinnerLabel->setMovie(m_spinner);
    if (isRunning)
    {
        m_spinner->start();
    }
#endif
}

void SetupDialog::onConnectDeviceClicked()
{
    setConnectButton(ConnectButtonOff);
    ui->rtlsdrInfoWidget->setVisible(false);
    ui->rtltcpInfoWidget->setVisible(false);
    ui->airspyInfoWidget->setVisible(false);
    ui->soapysdrInfoWidget->setVisible(false);
    ui->sdrplayInfoWidget->setVisible(false);
    setSoapySdrGainWidget(false);
    m_inputDeviceId = InputDevice::Id::UNDEFINED;
    setStatusLabel(true);  // clear label
    m_settings->inputDevice = static_cast<InputDevice::Id>(ui->inputCombo->itemData(ui->inputCombo->currentIndex()).toInt());
    QVariant id;
    switch (m_settings->inputDevice)
    {
        case InputDevice::Id::RTLSDR:
            id = ui->rtlsdrDeviceListCombo->currentData();
            ui->rtlsdrDeviceListCombo->setEnabled(false);
            break;
        case InputDevice::Id::RTLTCP:
            m_settings->rtltcp.tcpAddress = ui->rtltcpIpAddressEdit->text();
            m_settings->rtltcp.tcpPort = ui->rtltcpIpPortSpinBox->value();
            m_settings->rtltcp.controlSocketEna = ui->rtltcpControlSocketCheckbox->isChecked();
            break;
        case InputDevice::Id::RARTTCP:
            break;
        case InputDevice::Id::UNDEFINED:
            break;
        case InputDevice::Id::RAWFILE:
            m_settings->rawfile.file = m_rawfilename;
            m_settings->rawfile.loopEna = ui->loopCheckbox->isChecked();
            m_settings->rawfile.format = static_cast<RawFileInputFormat>(ui->fileFormatCombo->currentIndex());
            break;
        case InputDevice::Id::AIRSPY:
#if HAVE_AIRSPY
            id = ui->airspyDeviceListCombo->currentData();
            ui->airspyDeviceListCombo->setEnabled(false);
#endif
            break;
        case InputDevice::Id::SOAPYSDR:
#if HAVE_SOAPYSDR
            m_settings->soapysdr.devArgs = ui->soapysdrDevArgs->text().trimmed();
            m_settings->soapysdr.channel = ui->soapysdrChannelNum->text().toInt();
            m_settings->soapysdr.antenna = ui->soapySdrAntenna->text().trimmed();
#endif
            break;
        case InputDevice::Id::SDRPLAY:
#if HAVE_SOAPYSDR
            id = ui->sdrplayDeviceListCombo->currentData();
            m_settings->sdrplay.channel = ui->sdrplayChannelCombo->currentData().toInt();
            m_settings->sdrplay.antenna = ui->sdrplayAntennaCombo->currentData().toString();
            ui->sdrplayReloadButton->setEnabled(false);
            ui->sdrplayDeviceListCombo->setEnabled(false);
            ui->sdrplayChannelCombo->setEnabled(false);
            ui->sdrplayAntennaCombo->setEnabled(false);
#endif
            break;
    }
    emit inputDeviceChanged(m_settings->inputDevice, id);
}

void SetupDialog::onBandwidthChanged(int val)
{
    switch (m_inputDeviceId)
    {
        case InputDevice::Id::RTLSDR:
            m_settings->rtlsdr.bandwidth = val * 1000;
            ui->rtlsdrBandwidthDefault->setEnabled(val > 0);
            if (m_device)
            {
                m_device->setBW(m_settings->rtlsdr.bandwidth);
            }
            break;
        case InputDevice::Id::SOAPYSDR:
#if HAVE_SOAPYSDR
            m_settings->soapysdr.bandwidth = val * 1000;
            ui->soapysdrBandwidthDefault->setEnabled(val > 0);
            if (m_device)
            {
                m_device->setBW(m_settings->soapysdr.bandwidth);
            }
#endif
            break;
        case InputDevice::Id::RTLTCP:
        case InputDevice::Id::AIRSPY:
        case InputDevice::Id::RARTTCP:
        default:
            break;
    }
}

void SetupDialog::onPPMChanged(int val)
{
    switch (m_inputDeviceId)
    {
        case InputDevice::Id::RTLSDR:
            m_settings->rtlsdr.ppm = val;
            if (m_device)
            {
                m_device->setPPM(m_settings->rtlsdr.ppm);
            }
            break;
        case InputDevice::Id::RTLTCP:
            m_settings->rtltcp.ppm = val;
            if (m_device)
            {
                m_device->setPPM(m_settings->rtltcp.ppm);
            }
            break;
        case InputDevice::Id::SOAPYSDR:
#if HAVE_SOAPYSDR
            m_settings->soapysdr.ppm = val;
            if (m_device)
            {
                m_device->setPPM(m_settings->soapysdr.ppm);
            }
#endif
            break;
        case InputDevice::Id::SDRPLAY:
            m_settings->sdrplay.ppm = val;
            if (m_device)
            {
                m_device->setPPM(m_settings->sdrplay.ppm);
            }
            break;
        case InputDevice::Id::AIRSPY:
        case InputDevice::Id::RARTTCP:
        case InputDevice::Id::UNDEFINED:
        case InputDevice::Id::RAWFILE:
            break;
    }
}

void SetupDialog::onBiasTChanged(int)
{
    switch (m_inputDeviceId)
    {
        case InputDevice::Id::RTLSDR:
            m_settings->rtlsdr.biasT = ui->rtlsdrBiasTCombo->currentData().toBool();
            if (m_device)
            {
                m_device->setBiasT(m_settings->rtlsdr.biasT);
            }
            break;
        case InputDevice::Id::AIRSPY:
#if HAVE_AIRSPY
            m_settings->airspy.biasT = ui->airspyBiasTCombo->currentData().toBool();
            if (m_device)
            {
                m_device->setBiasT(m_settings->airspy.biasT);
            }
#endif
            break;
        case InputDevice::Id::SDRPLAY:
#if HAVE_SOAPYSDR
            m_settings->sdrplay.biasT = ui->sdrplayBiasTCombo->currentData().toBool();
            if (m_device)
            {
                m_device->setBiasT(m_settings->sdrplay.biasT);
            }
#endif
            break;
        case InputDevice::Id::RTLTCP:
        case InputDevice::Id::SOAPYSDR:
        case InputDevice::Id::RARTTCP:
        case InputDevice::Id::UNDEFINED:
        case InputDevice::Id::RAWFILE:
            break;
    }
}

void SetupDialog::onRtlSdrGainSliderChanged(int val)
{
    if (!m_rtlsdrGainList.empty())
    {
        ui->rtlsdrGainValueLabel->setText(QString("%1 dB").arg(m_rtlsdrGainList.at(val)));
        m_settings->rtlsdr.gainIdx = val;
        dynamic_cast<RtlSdrInput *>(m_device)->setGainMode(m_settings->rtlsdr.gainMode, m_settings->rtlsdr.gainIdx);
    }
    else
    { /* empy gain list => do nothing */
    }
}

void SetupDialog::onRtlSdrSwAgcMaxLevelChanged(int val)
{
    m_settings->rtlsdr.agcLevelMax = val;
    ui->rtlsdrSwAgcMaxLevelDefault->setEnabled(val > 0);
    dynamic_cast<RtlSdrInput *>(m_device)->setAgcLevelMax(m_settings->rtlsdr.agcLevelMax);
}

void SetupDialog::onRtlSdrGainModeToggled(bool checked)
{
    if (checked)
    {
        if (ui->rtlsdrGainModeHw->isChecked())
        {
            m_settings->rtlsdr.gainMode = RtlGainMode::Hardware;
        }
        else if (ui->rtlsdrGainModeSw->isChecked())
        {
            m_settings->rtlsdr.gainMode = RtlGainMode::Software;
        }
        else if (ui->rtlsdrGainModeManual->isChecked())
        {
            m_settings->rtlsdr.gainMode = RtlGainMode::Manual;
        }
        activateRtlSdrControls(true);
        dynamic_cast<RtlSdrInput *>(m_device)->setGainMode(m_settings->rtlsdr.gainMode, m_settings->rtlsdr.gainIdx);
    }
}

void SetupDialog::activateRtlSdrControls(bool en)
{
    ui->rtlsdrDeviceListCombo->setEnabled(true);
    ui->rtlsdrGainModeGroup->setEnabled(en);
    ui->rtlsdrGainWidget->setEnabled(en && (RtlGainMode::Manual == m_settings->rtlsdr.gainMode));
    ui->rtlsdrExpertGroup->setEnabled(en);
}

void SetupDialog::onRtlTcpGainSliderChanged(int val)
{
    if (!m_rtltcpGainList.empty())
    {
        ui->rtltcpGainValueLabel->setText(QString("%1 dB").arg(m_rtltcpGainList.at(val)));
        m_settings->rtltcp.gainIdx = val;
        dynamic_cast<RtlTcpInput *>(m_device)->setGainMode(m_settings->rtltcp.gainMode, m_settings->rtltcp.gainIdx);
    }
    else
    { /* empy gain list => do nothing */
    }
}

void SetupDialog::onRtlTcpIpAddrEditFinished()
{
    if (ui->rtltcpIpAddressEdit->text() != m_settings->rtltcp.tcpAddress)
    {
        setConnectButton(ConnectButtonOn);
    }
}

void SetupDialog::onRtlTcpControlSocketChecked(Qt::CheckState state)
{
    if ((state == Qt::Checked) != m_settings->rtltcp.controlSocketEna)
    {
        setConnectButton(ConnectButtonOn);
    }
}

void SetupDialog::onRtlTcpPortValueChanged(int val)
{
    if (val != m_settings->rtltcp.tcpPort)
    {
        setConnectButton(ConnectButtonOn);
    }
}

void SetupDialog::onTcpGainModeToggled(bool checked)
{
    if (checked)
    {
        if (ui->rtltcpGainModeHw->isChecked())
        {
            m_settings->rtltcp.gainMode = RtlGainMode::Hardware;
        }
        else if (ui->rtltcpGainModeSw->isChecked())
        {
            m_settings->rtltcp.gainMode = RtlGainMode::Software;
        }
        else if (ui->rtltcpGainModeManual->isChecked())
        {
            m_settings->rtltcp.gainMode = RtlGainMode::Manual;
        }
        activateRtlTcpControls(true);
        dynamic_cast<RtlTcpInput *>(m_device)->setGainMode(m_settings->rtltcp.gainMode, m_settings->rtltcp.gainIdx);
    }
}

void SetupDialog::onRtlTcpSwAgcMaxLevelChanged(int val)
{
    m_settings->rtltcp.agcLevelMax = val;
    ui->rtltcpSwAgcMaxLevelDefault->setEnabled(val > 0);
    dynamic_cast<RtlTcpInput *>(m_device)->setAgcLevelMax(m_settings->rtltcp.agcLevelMax);
}

void SetupDialog::activateRtlTcpControls(bool en)
{
    ui->rtltcpGainModeGroup->setEnabled(en);
    ui->rtltcpGainWidget->setEnabled(en && (RtlGainMode::Manual == m_settings->rtltcp.gainMode));
    ui->rtltcpExpertGroup->setEnabled(en);
}

void SetupDialog::onRawFileFormatChanged(int idx)
{
    if (static_cast<RawFileInputFormat>(idx) != m_settings->rawfile.format)
    {
        setConnectButton(ConnectButtonOn);
    }
}

#if HAVE_AIRSPY
void SetupDialog::activateAirspyControls(bool en)
{
    ui->airspyDeviceListCombo->setEnabled(true);
    ui->airspyGainModeGroup->setEnabled(en);
    bool sensEna = en && (AirpyGainMode::Sensitivity == m_settings->airspy.gain.mode);
    bool manualEna = en && (AirpyGainMode::Manual == m_settings->airspy.gain.mode);

    // sensitivity controls
    ui->airspySensitivityGain->setEnabled(sensEna);
    ui->airspySensitivityGainLabel->setEnabled(sensEna);
    ui->airspySensitivityGainSlider->setEnabled(sensEna);

    // manual gain controls
    ui->airspyIFGain->setEnabled(manualEna);
    ui->airspyIFGainLabel->setEnabled(manualEna);
    ui->airspyIFGainSlider->setEnabled(manualEna);

    ui->airspyLNAAGCCheckbox->setEnabled(manualEna);
    ui->airspyLNAGain->setEnabled(manualEna && !m_settings->airspy.gain.lnaAgcEna);
    ui->airspyLNAGainLabel->setEnabled(manualEna && !m_settings->airspy.gain.lnaAgcEna);
    ui->airspyLNAGainSlider->setEnabled(manualEna && !m_settings->airspy.gain.lnaAgcEna);

    ui->airspyMixerAGCCheckbox->setEnabled(manualEna);
    ui->airspyMixerGain->setEnabled(manualEna && !m_settings->airspy.gain.mixerAgcEna);
    ui->airspyMixerGainLabel->setEnabled(manualEna && !m_settings->airspy.gain.mixerAgcEna);
    ui->airspyMixerGainSlider->setEnabled(manualEna && !m_settings->airspy.gain.mixerAgcEna);

    ui->airspyExpertGroup->setEnabled(en);
}

void SetupDialog::onAirspyModeToggled(bool checked)
{
    if (checked)
    {
        if (ui->airspyGainModeHybrid->isChecked())
        {
            m_settings->airspy.gain.mode = AirpyGainMode::Hybrid;
        }
        else if (ui->airspyGainModeSw->isChecked())
        {
            m_settings->airspy.gain.mode = AirpyGainMode::Software;
        }
        else if (ui->airspyGainModeManual->isChecked())
        {
            m_settings->airspy.gain.mode = AirpyGainMode::Manual;
        }
        else if (ui->airspyGainModeSensitivity->isChecked())
        {
            m_settings->airspy.gain.mode = AirpyGainMode::Sensitivity;
        }
        activateAirspyControls(true);
        dynamic_cast<AirspyInput *>(m_device)->setGainMode(m_settings->airspy.gain);
    }
}

void SetupDialog::onAirspySensitivityGainSliderChanged(int val)
{
    ui->airspySensitivityGainLabel->setText(QString::number(val));
    m_settings->airspy.gain.sensitivityGainIdx = val;
    if (dynamic_cast<AirspyInput *>(m_device))
    {
        dynamic_cast<AirspyInput *>(m_device)->setGainMode(m_settings->airspy.gain);
    }
}

void SetupDialog::onAirspyIFGainSliderChanged(int val)
{
    ui->airspyIFGainLabel->setText(QString::number(val));
    m_settings->airspy.gain.ifGainIdx = val;
    if (dynamic_cast<AirspyInput *>(m_device))
    {
        dynamic_cast<AirspyInput *>(m_device)->setGainMode(m_settings->airspy.gain);
    }
}

void SetupDialog::onAirspyLNAGainSliderChanged(int val)
{
    ui->airspyLNAGainLabel->setText(QString::number(val));
    m_settings->airspy.gain.lnaGainIdx = val;
    if (dynamic_cast<AirspyInput *>(m_device))
    {
        dynamic_cast<AirspyInput *>(m_device)->setGainMode(m_settings->airspy.gain);
    }
}

void SetupDialog::onAirspyMixerGainSliderChanged(int val)
{
    ui->airspyMixerGainLabel->setText(QString::number(val));
    m_settings->airspy.gain.mixerGainIdx = val;
    if (dynamic_cast<AirspyInput *>(m_device))
    {
        dynamic_cast<AirspyInput *>(m_device)->setGainMode(m_settings->airspy.gain);
    }
}

void SetupDialog::onAirspyLNAAGCstateChanged(int state)
{
    bool ena = (Qt::Unchecked == state);
    ui->airspyLNAGain->setEnabled(ena);
    ui->airspyLNAGainLabel->setEnabled(ena);
    ui->airspyLNAGainSlider->setEnabled(ena);

    m_settings->airspy.gain.lnaAgcEna = !ena;
    dynamic_cast<AirspyInput *>(m_device)->setGainMode(m_settings->airspy.gain);
}

void SetupDialog::onAirspyMixerAGCstateChanged(int state)
{
    bool ena = (Qt::Unchecked == state);
    ui->airspyMixerGain->setEnabled(ena);
    ui->airspyMixerGainLabel->setEnabled(ena);
    ui->airspyMixerGainSlider->setEnabled(ena);

    m_settings->airspy.gain.mixerAgcEna = !ena;
    dynamic_cast<AirspyInput *>(m_device)->setGainMode(m_settings->airspy.gain);
}
#endif  // HAVE_AIRSPY

#if HAVE_SOAPYSDR

void SetupDialog::setSoapySdrGainWidget(bool activate)
{
    if (activate)
    {
        auto gains = dynamic_cast<SoapySdrInput *>(m_device)->getGains();
        if (!gains->empty())
        {
            QGridLayout *layout = dynamic_cast<QGridLayout *>(ui->soapysdrGainWidget->layout());
            for (int row = 0; row < gains->count(); ++row)
            {
                auto nameLabel = new QLabel(gains->at(row).first, ui->soapysdrGainWidget);
                auto valueLabel = new QLabel(QString::number(m_settings->soapysdr.gainMap[m_settings->soapysdr.driver].gainList[row], 'f', 1) + " dB",
                                             ui->soapysdrGainWidget);
                valueLabel->setFixedWidth(valueLabel->fontMetrics().boundingRect(" 88.8 dB").width());
                valueLabel->setAlignment(Qt::AlignRight);
                auto slider = new QSlider(Qt::Horizontal, ui->soapysdrGainWidget);
                slider->setMinimum(gains->at(row).second.minimum());
                slider->setMaximum(gains->at(row).second.maximum());
                int step = 1;
                if (gains->at(row).second.step() != 0)
                {
                    step = gains->at(row).second.step();
                }
                slider->setSingleStep(step);
                slider->setValue(m_settings->soapysdr.gainMap[m_settings->soapysdr.driver].gainList[row]);
                dynamic_cast<SoapySdrInput *>(m_device)->setGain(row, m_settings->soapysdr.gainMap[m_settings->soapysdr.driver].gainList[row]);
                connect(slider, &QSlider::valueChanged, this,
                        [=](int val)
                        {
                            valueLabel->setText(QString::number(val * step, '1', 1) + " dB");
                            m_settings->soapysdr.gainMap[m_settings->soapysdr.driver].gainList[row] = val * step;
                            dynamic_cast<SoapySdrInput *>(m_device)->setGain(row, val * step);
                        });

                layout->addWidget(nameLabel, row, 0);
                layout->addWidget(slider, row, 1);
                layout->addWidget(valueLabel, row, 2);
            }
        }
        ui->soapysdrGainWidget->setEnabled(SoapyGainMode::Manual == m_settings->soapysdr.gainMap[m_settings->soapysdr.driver].mode);
    }
    else
    {
        QLayout *layout = ui->soapysdrGainWidget->layout();
        QLayoutItem *child;
        while ((child = layout->takeAt(0)) != 0)
        {
            delete child->widget();
            delete child;
        }
    }
    ui->soapysdrGainWidget->setVisible(activate);
}

void SetupDialog::activateSoapySdrControls(bool en)
{
    ui->soapysdrGainModeGroup->setEnabled(en);
    ui->soapysdrGainWidget->setEnabled(en && (SoapyGainMode::Manual == m_settings->soapysdr.gainMap[m_settings->soapysdr.driver].mode));
    ui->soapysdrExpertGroup->setEnabled(en);
}

void SetupDialog::onSoapySdrGainModeToggled(bool checked)
{
    if (checked)
    {
        if (ui->soapysdrGainModeHw->isChecked())
        {
            m_settings->soapysdr.gainMap[m_settings->soapysdr.driver].mode = SoapyGainMode::Hardware;
        }
        else if (ui->soapysdrGainModeManual->isChecked())
        {
            m_settings->soapysdr.gainMap[m_settings->soapysdr.driver].mode = SoapyGainMode::Manual;
        }
        qDebug() << Q_FUNC_INFO << (m_settings->soapysdr.gainMap[m_settings->soapysdr.driver].mode == SoapyGainMode::Manual);
        activateSoapySdrControls(true);
        dynamic_cast<SoapySdrInput *>(m_device)->setGainMode(m_settings->soapysdr.gainMap[m_settings->soapysdr.driver]);
    }
}

void SetupDialog::onSdrplayDeviceChanged(int idx)
{
    ui->sdrplayDeviceListCombo->setEnabled(false);
    ui->sdrplayChannelCombo->setEnabled(false);
    ui->sdrplayAntennaCombo->setEnabled(false);
    ui->sdrplayChannelCombo->clear();
    if (idx >= 0)
    {  // timer is needed to avoid GUI blocking
        QTimer::singleShot(100, this,
                           [this]()
                           {
                               int numCh = SdrPlayInput::getNumRxChannels(ui->sdrplayDeviceListCombo->currentData());
                               for (int n = 0; n < numCh; ++n)
                               {
                                   ui->sdrplayChannelCombo->addItem(QString::number(n), QVariant(n));
                               }
                           });
    }
}

void SetupDialog::activateSdrplayControls(bool en)
{
    ui->sdrplayReloadButton->setText(en ? tr("Disconnect") : tr("Reload"));
    ui->sdrplayGainModeGroup->setEnabled(en);
    ui->sdrplayGainWidget->setEnabled(en && (SdrPlayGainMode::Manual == m_settings->sdrplay.gain.mode));
    ui->sdrplayExpertGroup->setEnabled(en);
    ui->sdrplayIFGainSlider->setEnabled(en && !m_settings->sdrplay.gain.ifAgcEna);
}

void SetupDialog::onSdrplayReloadButtonClicked()
{
    if (m_inputDeviceId != InputDevice::Id::SDRPLAY)
    {
        ui->sdrplayReloadButton->setEnabled(false);
        reloadDeviceList(InputDevice::Id::SDRPLAY, ui->sdrplayDeviceListCombo);
    }
    else
    {  // sdrplay is connected now -> need to disconnect
        resetInputDevice();
        m_inputDeviceId = InputDevice::Id::UNDEFINED;
        emit inputDeviceChanged(InputDevice::Id::UNDEFINED, m_settings->sdrplay.hwId);
        ui->sdrplayReloadButton->setEnabled(false);
    }
}

void SetupDialog::onSdrplayChannelChanged(int idx)
{
    ui->sdrplayAntennaCombo->clear();
    if (idx >= 0)
    {  // timer is needed to avoid GUI blocking
        QTimer::singleShot(100, this,
                           [this]()
                           {
                               QStringList ant = SdrPlayInput::getRxAntennas(ui->sdrplayDeviceListCombo->currentData(),
                                                                             ui->sdrplayChannelCombo->currentData().toInt());
                               for (const auto &a : ant)
                               {
                                   ui->sdrplayAntennaCombo->addItem(a, a);
                               }

                               ui->sdrplayDeviceListCombo->setEnabled(m_inputDeviceId != InputDevice::Id::SDRPLAY);
                               ui->sdrplayChannelCombo->setEnabled(m_inputDeviceId != InputDevice::Id::SDRPLAY);
                               ui->sdrplayAntennaCombo->setEnabled(m_inputDeviceId != InputDevice::Id::SDRPLAY);
                               ui->sdrplayReloadButton->setEnabled(true);
                               setConnectButton(ConnectButtonAuto);
                           });
    }
}

void SetupDialog::onSdrplayModeToggled(bool checked)
{
    if (checked)
    {
        if (ui->sdrplayGainModeSw->isChecked())
        {
            m_settings->sdrplay.gain.mode = SdrPlayGainMode::Software;
        }
        else if (ui->sdrplayGainModeManual->isChecked())
        {
            m_settings->sdrplay.gain.mode = SdrPlayGainMode::Manual;
        }
        activateSdrplayControls(true);
        dynamic_cast<SdrPlayInput *>(m_device)->setGainMode(m_settings->sdrplay.gain);
    }
}

void SetupDialog::onSdrplayRFGainSliderChanged(int val)
{
    ui->sdrplayRFGainLabel->setText(QString("%1 dB  ").arg(m_sdrplayGainList.at(val)));
    m_settings->sdrplay.gain.rfGain = val;
    if (dynamic_cast<SdrPlayInput *>(m_device))
    {
        dynamic_cast<SdrPlayInput *>(m_device)->setGainMode(m_settings->sdrplay.gain);
    }
}

void SetupDialog::onSdrplayIFGainSliderChanged(int val)
{
    ui->sdrplayIFGainLabel->setText(QString("%1 dB  ").arg(val));
    m_settings->sdrplay.gain.ifGain = val;
    if (dynamic_cast<SdrPlayInput *>(m_device))
    {
        dynamic_cast<SdrPlayInput *>(m_device)->setGainMode(m_settings->sdrplay.gain);
    }
}

void SetupDialog::onSdrplayAGCstateChanged(int state)
{
    bool ena = (Qt::Unchecked == state);
    ui->sdrplayIFGain->setEnabled(ena);
    ui->sdrplayIFGainSlider->setEnabled(ena);
    ui->sdrplayIFGainLabel->setEnabled(ena);

    m_settings->sdrplay.gain.ifAgcEna = !ena;
    dynamic_cast<SdrPlayInput *>(m_device)->setGainMode(m_settings->sdrplay.gain);
}

#endif  // HAVE_SOAPYSDR

void SetupDialog::setStatusLabel(bool clearLabel)
{
    if (clearLabel)
    {
        ui->statusLabel->setText("");
    }
    else
    {
        switch (m_inputDeviceId)
        {
            case InputDevice::Id::RTLSDR:
                ui->statusLabel->setText(tr("RTL SDR device connected"));
                break;
            case InputDevice::Id::RTLTCP:
                ui->statusLabel->setText(tr("RTL TCP device connected"));
                break;
            case InputDevice::Id::UNDEFINED:
                ui->statusLabel->setText("<span style=\"color:red\">" + tr("No device connected") + "</span>");
                break;
            case InputDevice::Id::RAWFILE:
                ui->statusLabel->setText(tr("Raw file connected"));
                break;
            case InputDevice::Id::AIRSPY:
                ui->statusLabel->setText(tr("Airspy device connected"));
                break;
            case InputDevice::Id::SOAPYSDR:
                ui->statusLabel->setText(tr("Soapy SDR device connected"));
                break;
            case InputDevice::Id::RARTTCP:
                ui->statusLabel->setText("RART TCP device connected");
                break;
            case InputDevice::Id::SDRPLAY:
                ui->statusLabel->setText("SDRplay device connected");
                break;
        }
    }
}

void SetupDialog::setFmlistUploadInfoText()
{
    m_settings->uploadEnsembleInfo = ui->fmlistUploadCheckBox->isChecked();
    if (ui->fmlistUploadCheckBox->isChecked())
    {
        ui->fmlistUploadInfotext->setText(
            tr("Application automatically uploads ensemble information to <a href=\"https://www.fmlist.org/\">FMLIST</a>.<br>"
               "Ensemble information is a small CSV file with list of services in the ensemble, it is anonymous and contains no personal data.<br>"
               "Thank you for supporting the community!"));
    }
    else
    {
        ui->fmlistUploadInfotext->setText(
            tr("Upload of ensemble information to <a href=\"https://www.fmlist.org/\">FMLIST</a> is currenly disabled.<br>"
               "Ensemble information is a small CSV file with list of services in the ensemble, it is anonymous and contains no personal data.<br>"
               "Please consider enabling this option to help the community."));
    }
}

void SetupDialog::onInputChanged(int index)
{
    int inputDeviceInt = ui->inputCombo->itemData(index).toInt();
    ui->deviceOptionsWidget->setCurrentIndex(inputDeviceInt - 1);
    switch (static_cast<InputDevice::Id>(inputDeviceInt))
    {
        case InputDevice::Id::RTLSDR:
        {
            reloadDeviceList(InputDevice::Id::RTLSDR, ui->rtlsdrDeviceListCombo);
            if (m_inputDeviceId != static_cast<InputDevice::Id>(inputDeviceInt))
            {
                activateRtlSdrControls(false);
            }
        }
        break;
        case InputDevice::Id::RTLTCP:
            if (m_inputDeviceId != static_cast<InputDevice::Id>(inputDeviceInt))
            {
                activateRtlTcpControls(false);
            }
            break;
        case InputDevice::Id::UNDEFINED:
            break;
        case InputDevice::Id::RAWFILE:
            break;
        case InputDevice::Id::AIRSPY:
        {
#if HAVE_AIRSPY
            reloadDeviceList(InputDevice::Id::AIRSPY, ui->airspyDeviceListCombo);
            if (m_inputDeviceId != static_cast<InputDevice::Id>(inputDeviceInt))
            {
                activateAirspyControls(false);
            }
#endif
        }
        break;
        case InputDevice::Id::SOAPYSDR:
#if HAVE_SOAPYSDR
            if (m_inputDeviceId != static_cast<InputDevice::Id>(inputDeviceInt))
            {
                activateSoapySdrControls(false);
            }
#endif
            break;
        case InputDevice::Id::SDRPLAY:
#if HAVE_SOAPYSDR
            QTimer::singleShot(200, this, [this]() { reloadDeviceList(InputDevice::Id::SDRPLAY, ui->sdrplayDeviceListCombo); });
            if (m_inputDeviceId != static_cast<InputDevice::Id>(inputDeviceInt))
            {
                activateSdrplayControls(false);
            }
#endif
            break;
        case InputDevice::Id::RARTTCP:
            break;
    }
    setConnectButton(ConnectButtonAuto);
}

void SetupDialog::onOpenFileButtonClicked()
{
    QString dir = QDir::homePath();
    if (!m_rawfilename.isEmpty() && QFileInfo::exists(m_rawfilename))
    {
        dir = QFileInfo(m_rawfilename).path();
    }
    QString fileName =
        QFileDialog::getOpenFileName(this, tr("Open IQ stream"), dir, tr("Binary files") + " (*.bin *.s16 *.u8 *.raw *.sdr *.uff *.wav)", nullptr,
                                     QFileDialog::Option::DontResolveSymlinks);
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
        { /* format cannot be guessed from extension - if XML header is recognized, then it will be set automatically */
        }

        setConnectButton(ConnectButtonOn);
        ui->fileFormatCombo->setEnabled(true);

        // we do not know the length yet
        onFileLength(0);

#ifdef Q_OS_MACOS          // bug in Ventura
        show();            // bring window to top on OSX
        raise();           // bring window from minimized state on OSX
        activateWindow();  // bring window to front/unminimize on windows
#endif
    }
}

void SetupDialog::setInputDevice(InputDevice::Id id, InputDevice *device)
{
    m_device = device;
    m_inputDeviceId = id;

    Q_ASSERT(m_inputDeviceId == m_settings->inputDevice);

    // apply current settings and populate dialog
    setStatusLabel();

    switch (id)
    {
        case InputDevice::Id::RTLSDR:
            setGainValues(dynamic_cast<RtlSdrInput *>(m_device)->getGainList());
            m_device->setBW(m_settings->rtlsdr.bandwidth);
            m_device->setBiasT(m_settings->rtlsdr.biasT);
            m_device->setPPM(m_settings->rtlsdr.ppm);
            dynamic_cast<RtlSdrInput *>(m_device)->setGainMode(m_settings->rtlsdr.gainMode, m_settings->rtlsdr.gainIdx);
            dynamic_cast<RtlSdrInput *>(m_device)->setAgcLevelMax(m_settings->rtlsdr.agcLevelMax);
            m_settings->rtlsdr.hwId = m_device->hwId();
            activateRtlSdrControls(true);
            break;
        case InputDevice::Id::RTLTCP:
            setGainValues(dynamic_cast<RtlTcpInput *>(m_device)->getGainList());
            m_device->setPPM(m_settings->rtltcp.ppm);
            dynamic_cast<RtlTcpInput *>(m_device)->setGainMode(m_settings->rtltcp.gainMode, m_settings->rtltcp.gainIdx);
            dynamic_cast<RtlTcpInput *>(m_device)->setAgcLevelMax(m_settings->rtltcp.agcLevelMax);
            activateRtlTcpControls(true);
            break;
        case InputDevice::Id::AIRSPY:
#if HAVE_AIRSPY
            m_device->setBiasT(m_settings->airspy.biasT);
            dynamic_cast<AirspyInput *>(m_device)->setGainMode(m_settings->airspy.gain);
            m_settings->airspy.hwId = m_device->hwId();
            activateAirspyControls(true);
#endif
            break;
        case InputDevice::Id::SOAPYSDR:
#if HAVE_SOAPYSDR
            // store last driver
            m_settings->soapysdr.driver = dynamic_cast<SoapySdrInput *>(m_device)->getDriver();
            if (!m_settings->soapysdr.gainMap.contains(m_settings->soapysdr.driver))
            {
                m_settings->soapysdr.gainMap[m_settings->soapysdr.driver] = dynamic_cast<SoapySdrInput *>(m_device)->getDefaultGainStruct();
            }
            dynamic_cast<SoapySdrInput *>(m_device)->setGainMode(m_settings->soapysdr.gainMap[m_settings->soapysdr.driver]);
            setSoapySdrGainWidget(true);
            m_device->setBW(m_settings->soapysdr.bandwidth);
            m_device->setPPM(m_settings->soapysdr.ppm);
            activateSoapySdrControls(true);
#endif
            break;
        case InputDevice::Id::SDRPLAY:
#if HAVE_SOAPYSDR
            setGainValues(dynamic_cast<SdrPlayInput *>(m_device)->getRFGainList());
            ui->sdrplayReloadButton->setEnabled(true);
            ui->sdrplayDeviceListCombo->setEnabled(false);
            ui->sdrplayChannelCombo->setEnabled(false);
            ui->sdrplayAntennaCombo->setEnabled(false);
            m_settings->sdrplay.hwId = m_device->hwId();
            dynamic_cast<SdrPlayInput *>(m_device)->setGainMode(m_settings->sdrplay.gain);
            m_device->setPPM(m_settings->sdrplay.ppm);
            m_device->setBiasT(m_settings->sdrplay.biasT);
            activateSdrplayControls(true);
#endif
            break;
        case InputDevice::Id::RAWFILE:
            break;
        case InputDevice::Id::RARTTCP:
        case InputDevice::Id::UNDEFINED:
            break;
    }
    setConnectButton(ConnectButtonOff);
    setDeviceDescription(m_device->deviceDescription());
}

void SetupDialog::resetInputDevice()
{
    // deactivate controls for all devices
    activateRtlSdrControls(false);
    activateRtlTcpControls(false);
#if HAVE_AIRSPY
    activateAirspyControls(false);
#endif
#if HAVE_SOAPYSDR
    setSoapySdrGainWidget(false);
    activateSoapySdrControls(false);
    activateSdrplayControls(false);
#endif
    int inputDeviceInt = ui->inputCombo->currentData().toInt();
    switch (static_cast<InputDevice::Id>(inputDeviceInt))
    {
        case InputDevice::Id::RTLSDR:
            reloadDeviceList(InputDevice::Id::RTLSDR, ui->rtlsdrDeviceListCombo);
            break;
        case InputDevice::Id::AIRSPY:
#if HAVE_AIRSPY
            reloadDeviceList(InputDevice::Id::AIRSPY, ui->airspyDeviceListCombo);
#endif
            break;
        case InputDevice::Id::SDRPLAY:
#if HAVE_SOAPYSDR
            QTimer::singleShot(200, this, [this]() { reloadDeviceList(InputDevice::Id::SDRPLAY, ui->sdrplayDeviceListCombo); });
#endif
            break;
        case InputDevice::Id::SOAPYSDR:
        case InputDevice::Id::RARTTCP:
        case InputDevice::Id::RTLTCP:
        case InputDevice::Id::UNDEFINED:
        case InputDevice::Id::RAWFILE:
            break;
    }

    m_settings->inputDevice = InputDevice::Id::UNDEFINED;
    m_inputDeviceId = InputDevice::Id::UNDEFINED;
    m_device = nullptr;
    setStatusLabel();
    ui->rtlsdrInfoWidget->setVisible(false);
    ui->rtltcpInfoWidget->setVisible(false);
    ui->airspyInfoWidget->setVisible(false);
    ui->soapysdrInfoWidget->setVisible(false);
    ui->sdrplayInfoWidget->setVisible(false);
    setConnectButton(ConnectButtonOn);
    ui->tabWidget->setCurrentIndex(SetupDialogTabs::Device);
}

void SetupDialog::onAnnouncementClicked()
{  // calculate ena flag
    uint16_t announcementEna = 0;
    for (int a = 0; a < static_cast<int>(DabAnnouncement::Undefined); ++a)
    {
        announcementEna |= (m_announcementCheckBox[a]->isChecked() << a);
    }
    if (m_settings->announcementEna != announcementEna)
    {
        m_settings->announcementEna = announcementEna;
        emit newAnnouncementSettings();
    }
}

void SetupDialog::onBringWindowToForegroundClicked(bool checked)
{
    m_settings->bringWindowToForeground = checked;
}

void SetupDialog::onStyleChecked(bool checked)
{
    if (checked)
    {
        if (ui->defaultStyleRadioButton->isChecked())
        {
            m_settings->applicationStyle = Settings::ApplicationStyle::Default;
        }
        else if (ui->lightStyleRadioButton->isChecked())
        {
            m_settings->applicationStyle = Settings::ApplicationStyle::Light;
        }
        else if (ui->darkStyleRadioButton->isChecked())
        {
            m_settings->applicationStyle = Settings::ApplicationStyle::Dark;
        }
        emit applicationStyleChanged(m_settings->applicationStyle);
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
    ui->tabWidget->setTabVisible(SetupDialogTabs::Tii, checked);
    if (!checked)
    {
        ui->dumpSlsCheckBox->setChecked(false);
        ui->dumpSpiCheckBox->setChecked(false);
    }
    emit uaDumpSettings(m_settings->uaDump);

    int idx = ui->inputCombo->findData(QVariant(static_cast<int>(InputDevice::Id::RAWFILE)));
    if (checked && (idx < 0))
    {  // add item
        ui->inputCombo->addItem(tr("Raw file"), QVariant(int(InputDevice::Id::RAWFILE)));
    }
    if (!checked && (idx >= 0))
    {  // remove it
        if (ui->inputCombo->currentIndex() == idx)
        {
            ui->inputCombo->removeItem(idx);
            ui->inputCombo->setCurrentIndex(0);
        }
        else
        {
            ui->inputCombo->removeItem(idx);
        }
    }

    m_settings->expertModeEna = checked;
    ui->tabWidget->adjustSize();

    emit expertModeToggled(checked);

    QTimer::singleShot(10, this, [this]() { resize(minimumSizeHint()); });
}

void SetupDialog::onTrayIconChecked(bool checked)
{
    m_settings->trayIconEna = checked;
    emit trayIconToggled(checked);
}

void SetupDialog::onDLPlusChecked(bool checked)
{
    m_settings->dlPlusEna = checked;
}

void SetupDialog::onLanguageChanged(int index)
{
    QLocale::Language lang = static_cast<QLocale::Language>(ui->langComboBox->itemData(index).toInt());
    if (lang != m_settings->lang)
    {
        m_settings->lang = lang;
        ui->langWarningLabel->setVisible(true);
    }
}

void SetupDialog::onNoiseLevelChanged(int index)
{
    int noiseLevel = ui->noiseConcealmentCombo->itemData(index).toInt();
    if (noiseLevel != m_settings->noiseConcealmentLevel)
    {
        m_settings->noiseConcealmentLevel = noiseLevel;
        emit noiseConcealmentLevelChanged(noiseLevel);
    }
}

void SetupDialog::onXmlHeaderChecked(bool checked)
{
    m_settings->xmlHeaderEna = checked;
    emit xmlHeaderToggled(checked);
}

void SetupDialog::onRawFileProgressChanged(int val)
{
    ui->rawFileTime->setText(QString("%1 / %2 " + tr("sec")).arg(val / 1000.0, 0, 'f', 1).arg(ui->rawFileSlider->maximum() / 1000.0, 0, 'f', 1));
}

void SetupDialog::onSpiAppChecked(bool checked)
{
    m_settings->spiAppEna = checked;
    ui->internetCheckBox->setEnabled(checked);
    ui->radioDNSCheckBox->setEnabled(checked && m_settings->useInternet);
    emit spiApplicationEnabled(m_settings->spiAppEna);
    emit spiApplicationSettingsChanged(m_settings->useInternet && m_settings->spiAppEna, m_settings->radioDnsEna && m_settings->spiAppEna);
}

void SetupDialog::onUseInternetChecked(bool checked)
{
    m_settings->useInternet = checked;
    ui->radioDNSCheckBox->setEnabled(checked);
    emit spiApplicationSettingsChanged(m_settings->useInternet, m_settings->radioDnsEna);
}

void SetupDialog::onRadioDnsChecked(bool checked)
{
    m_settings->radioDnsEna = checked;
    emit spiApplicationSettingsChanged(m_settings->useInternet, m_settings->radioDnsEna);
}

void SetupDialog::onAudioRecordingFolderButtonClicked()
{
    QString dir = QDir::homePath();
    if (!m_settings->audioRec.folder.isEmpty())
    {
        dir = QFileInfo(m_settings->audioRec.folder).path();
    }
    dir = QFileDialog::getExistingDirectory(this, tr("Audio recording folder"), dir);
    if (!dir.isEmpty())
    {
        m_settings->audioRec.folder = dir;
        ui->audioRecordingFolderLabel->setText(dir);
        emit audioRecordingSettings(m_settings->audioRec.folder, m_settings->audioRec.captureOutput);

#ifdef Q_OS_MACOS          // bug in Ventura
        show();            // bring window to top on OSX
        raise();           // bring window from minimized state on OSX
        activateWindow();  // bring window to front/unminimize on windows
#endif
    }
}

void SetupDialog::onAudioRecordingChecked(bool checked)
{
    m_settings->audioRec.captureOutput = ui->audioOutRecordingRadioButton->isChecked();
    emit audioRecordingSettings(m_settings->audioRec.folder, m_settings->audioRec.captureOutput);
}

void SetupDialog::onDataDumpFolderButtonClicked()
{
    QString dir = QDir::homePath();
    if (!m_settings->uaDump.folder.isEmpty())
    {
        dir = QFileInfo(m_settings->uaDump.folder).path();
    }
    dir = QFileDialog::getExistingDirectory(this, tr("Data storage folder"), dir);
    if (!dir.isEmpty())
    {
        m_settings->uaDump.folder = dir;
        ui->dataDumpFolderLabel->setText(dir);
        emit uaDumpSettings(m_settings->uaDump);

#ifdef Q_OS_MACOS          // bug in Ventura
        show();            // bring window to top on OSX
        raise();           // bring window from minimized state on OSX
        activateWindow();  // bring window to front/unminimize on windows
#endif
    }
}

void SetupDialog::onDataDumpCheckboxToggled(bool)
{
    m_settings->uaDump.overwriteEna = ui->dumpOverwriteCheckbox->isChecked();
    m_settings->uaDump.slsEna = ui->dumpSlsCheckBox->isChecked();
    m_settings->uaDump.spiEna = ui->dumpSpiCheckBox->isChecked();

    emit uaDumpSettings(m_settings->uaDump);
}

void SetupDialog::onDataDumpPatternEditingFinished()
{
    if (QObject::sender() == ui->dumpSlsPatternEdit)
    {
        m_settings->uaDump.slsPattern = ui->dumpSlsPatternEdit->text().trimmed();
    }
    else
    {
        m_settings->uaDump.spiPattern = ui->dumpSpiPatternEdit->text().trimmed();
    }
    emit uaDumpSettings(m_settings->uaDump);
}

void SetupDialog::onDataDumpResetClicked()
{
    if (QObject::sender() == ui->dumpSlsPatternReset)
    {
        m_settings->uaDump.slsPattern = m_slsDumpPaternDefault;
        ui->dumpSlsPatternEdit->setText(m_slsDumpPaternDefault);
    }
    else
    {
        m_settings->uaDump.spiPattern = m_spiDumpPaternDefault;
        ui->dumpSpiPatternEdit->setText(m_spiDumpPaternDefault);
    }
    emit uaDumpSettings(m_settings->uaDump);
}

void SetupDialog::onDlRecordingChecked(bool checked)
{
    m_settings->audioRec.dl = checked;
    ui->dlAbsTimeCheckBox->setEnabled(checked);
}

void SetupDialog::onDlAbsTimeChecked(bool checked)
{
    m_settings->audioRec.dlAbsTime = checked;
}

void SetupDialog::onGeolocationSourceChanged(int index)
{
    int srcInt = ui->locationSourceCombo->itemData(index).toInt();
    if (srcInt > 0)
    {
        ui->locationSrcWidget->setCurrentIndex(srcInt - 1);
    }
    else
    {
        ui->locationSrcWidget->setCurrentIndex(0);
    }

    ui->locationSrcWidget->setEnabled(srcInt > 0);
    m_settings->tii.locationSource = static_cast<Settings::GeolocationSource>(srcInt);

    emit tiiSettingsChanged();
}

void SetupDialog::onCoordinateEditFinished()
{
    static const QRegularExpression splitRegex("\\s*,\\s*");
    QStringList coord = ui->coordinatesEdit->text().split(splitRegex);
    if (coord.size() != 2)
    {
        ui->coordinatesEdit->setText("0.0, 0.0");
        m_settings->tii.coordinates = QGeoCoordinate(0.0, 0.0);
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
        m_settings->tii.coordinates = QGeoCoordinate(0.0, 0.0);
        emit tiiSettingsChanged();
        return;
    }

    QGeoCoordinate coordinate(latitude, longitude);
    if (!coordinate.isValid())
    {
        ui->coordinatesEdit->setText("0.0, 0.0");
        m_settings->tii.coordinates = QGeoCoordinate(0.0, 0.0);
        emit tiiSettingsChanged();
        return;
    }

    m_settings->tii.coordinates = QGeoCoordinate(latitude, longitude);
    emit tiiSettingsChanged();
}

void SetupDialog::onSerialPortEditFinished()
{
    m_settings->tii.serialPort = ui->serialPortEdit->text().trimmed();
    emit tiiSettingsChanged();
}

void SetupDialog::onTiiSpectPlotClicked(bool checked)
{
    m_settings->tii.showSpectumPlot = checked;
    emit tiiSettingsChanged();
}

void SetupDialog::onTiiUpdateDbClicked()
{
    ui->updateDbButton->setEnabled(false);
    ui->spinnerLabel->setVisible(true);
    m_spinner->start();
    emit updateTxDb();
}

void SetupDialog::onTiiLogFolderButtonClicked()
{
    QString dir = QDir::homePath();
    if (!m_settings->tii.logFolder.isEmpty())
    {
        dir = QFileInfo(m_settings->tii.logFolder).path();
    }
    dir = QFileDialog::getExistingDirectory(this, tr("TII log folder"), dir);
    if (!dir.isEmpty())
    {
        m_settings->tii.logFolder = dir;
        ui->tiiLogFolderLabel->setText(dir);

#ifdef Q_OS_MACOS          // bug in Ventura
        show();            // bring window to top on OSX
        raise();           // bring window from minimized state on OSX
        activateWindow();  // bring window to front/unminimize on windows
#endif
    }
}

void SetupDialog::onTiiModeChanged(int index)
{
    int mode = ui->tiiModeCombo->currentData().toInt();
    m_settings->tii.mode = mode;
    emit tiiModeChanged(mode);
}

void SetupDialog::onTiiUpdateFinished(QNetworkReply::NetworkError err)
{
    ui->spinnerLabel->setVisible(false);
    m_spinner->stop();

    QDateTime lastModified = TxDataLoader::lastUpdateTime();
    if (err == QNetworkReply::NoError)
    {
        if (lastModified.isValid())
        {
            ui->tiiDbLabel->setText(tr("Last update: ") + lastModified.toString("dd.MM.yyyy"));
        }
        else
        {
            ui->tiiDbLabel->setText(tr("Data not available"));
        }
    }
    else
    {
        ui->tiiDbLabel->setText(tr("Update failed"));
    }
    ui->updateDbButton->setEnabled(!lastModified.isValid() || lastModified.msecsTo(QDateTime::currentDateTime()) > 10 * 60 * 1000);
}

void SetupDialog::setDeviceDescription(const InputDevice::Description &desc)
{
    switch (m_inputDeviceId)
    {
        case InputDevice::Id::RTLSDR:
        {
            auto it = m_rtlSdrLabel.begin();
            (*it++)->setText(desc.device.model);
            (*it++)->setText(desc.device.sn);
            (*it++)->setText(desc.device.tuner);
            (*it++)->setText(desc.sample.channelContainer);
            ui->rtlsdrInfoWidget->setVisible(true);
            reloadDeviceList(InputDevice::Id::RTLSDR, ui->rtlsdrDeviceListCombo);
        }
        break;
        case InputDevice::Id::RTLTCP:
        {
            auto it = m_rtlTcpLabel.begin();
            (*it++)->setText(desc.device.model);
            (*it++)->setText(desc.device.tuner);
            (*it++)->setText(desc.sample.channelContainer);
            ui->rtltcpInfoWidget->setVisible(true);
        }
        break;
        case InputDevice::Id::AIRSPY:
        {
#if HAVE_AIRSPY
            auto it = m_airspyLabel.begin();
            (*it++)->setText("Airspy " + desc.device.model);
            (*it++)->setText(desc.device.sn);
            ui->airspyInfoWidget->setVisible(true);
            reloadDeviceList(InputDevice::Id::AIRSPY, ui->airspyDeviceListCombo);
#endif
        }
        break;
        case InputDevice::Id::SOAPYSDR:
        {
#if HAVE_SOAPYSDR
            auto it = m_soapySdrLabel.begin();
            (*it)->setText(desc.device.model);
            ui->soapysdrInfoWidget->setVisible(true);
#endif
        }
        break;
        case InputDevice::Id::RAWFILE:
            if (desc.rawFile.hasXmlHeader)
            {
                m_xmlHeaderLabel[SetupDialogXmlHeader::XMLDate]->setText(desc.rawFile.time);
                m_xmlHeaderLabel[SetupDialogXmlHeader::XMLRecorder]->setText(desc.rawFile.recorder);
                m_xmlHeaderLabel[SetupDialogXmlHeader::XMLDevice]->setText(desc.device.name);
                m_xmlHeaderLabel[SetupDialogXmlHeader::XMLModel]->setText(desc.device.model);
                m_xmlHeaderLabel[SetupDialogXmlHeader::XMLSampleRate]->setText(QString::number(desc.sample.sampleRate));
                m_xmlHeaderLabel[SetupDialogXmlHeader::XMLFreq]->setText(QString::number(desc.rawFile.frequency_kHz));
                m_xmlHeaderLabel[SetupDialogXmlHeader::XMLLength]->setText(QString::number(desc.rawFile.numSamples * 1.0 / desc.sample.sampleRate));
                m_xmlHeaderLabel[SetupDialogXmlHeader::XMLFormat]->setText(desc.sample.channelContainer);

                switch (desc.sample.containerBits)
                {
                    case 8:
                        m_settings->rawfile.format = RawFileInputFormat::SAMPLE_FORMAT_U8;
                        break;
                    case 16:
                        m_settings->rawfile.format = RawFileInputFormat::SAMPLE_FORMAT_S16;
                        break;
                }
                ui->fileFormatCombo->setCurrentIndex(static_cast<int>(m_settings->rawfile.format));
                ui->fileFormatCombo->setEnabled(false);
                ui->xmlHeaderWidget->setVisible(true);
            }
            else
            {
                ui->fileFormatCombo->setEnabled(true);
                ui->xmlHeaderWidget->setVisible(false);
            }
            break;
        case InputDevice::Id::SDRPLAY:
        {
#if HAVE_SOAPYSDR
            auto it = m_sdrPlayLabel.begin();
            (*it++)->setText(desc.device.model);
            (*it++)->setText(desc.device.sn);
            ui->sdrplayInfoWidget->setVisible(true);
#endif
        }
        break;
        case InputDevice::Id::UNDEFINED:
        case InputDevice::Id::RARTTCP:
            break;
    }
}

void SetupDialog::reloadDeviceList(const InputDevice::Id inputDeviceId, QComboBox *combo)
{
    InputDeviceList list;
    QVariant currentId;
    switch (inputDeviceId)
    {
        case InputDevice::Id::RTLSDR:
        {
            list = RtlSdrInput::getDeviceList();
            currentId = m_settings->rtlsdr.hwId;
        }
        break;
        case InputDevice::Id::RTLTCP:
            break;
        case InputDevice::Id::UNDEFINED:
            break;
        case InputDevice::Id::RAWFILE:
            break;
        case InputDevice::Id::AIRSPY:
        {
#if HAVE_AIRSPY
            list = AirspyInput::getDeviceList();
            currentId = m_settings->airspy.hwId;
#endif
        }
        break;
        case InputDevice::Id::SOAPYSDR:
#if HAVE_SOAPYSDR
#endif
            break;
        case InputDevice::Id::SDRPLAY:
#if HAVE_SOAPYSDR
            list = SdrPlayInput::getDeviceList();
            currentId = m_settings->sdrplay.hwId;
            ui->sdrplayReloadButton->setEnabled(list.isEmpty());
#endif
            break;
        case InputDevice::Id::RARTTCP:
            break;
    }

    combo->clear();
    for (auto it = list.cbegin(); it != list.cend(); ++it)
    {
        combo->addItem(it->diplayName, it->id);
    }

    if (currentId.isValid())
    {
        int idx = combo->findData(currentId);
        if (idx >= 0)
        {  // found ==> select item
            combo->setCurrentIndex(idx);
        }
    }
    if (inputDeviceId == InputDevice::Id::SDRPLAY)
    {
        setConnectButton(ConnectButtonOff);
    }
    else
    {
        setConnectButton(ConnectButtonAuto);
    }
}

void SetupDialog::setConnectButton(SetupDialogConnectButtonState state)
{
    switch (state)
    {
        case ConnectButtonOn:
            ui->connectButton->setEnabled(true);
            break;
        case ConnectButtonOff:
            ui->connectButton->setEnabled(false);
            break;
        case ConnectButtonAuto:
        {
            int inputDeviceInt = ui->inputCombo->currentData().toInt();
            bool showButton = m_settings->inputDevice != static_cast<InputDevice::Id>(inputDeviceInt);
            switch (static_cast<InputDevice::Id>(inputDeviceInt))
            {
                case InputDevice::Id::RTLSDR:
                    showButton = (ui->rtlsdrDeviceListCombo->count() > 0) &&
                                 (showButton || (ui->rtlsdrDeviceListCombo->currentData() != m_settings->rtlsdr.hwId));
                    break;
                case InputDevice::Id::AIRSPY:
#if HAVE_AIRSPY
                    showButton = (ui->airspyDeviceListCombo->count() > 0) &&
                                 (showButton || (ui->airspyDeviceListCombo->currentData() != m_settings->airspy.hwId));
#endif
                    break;
                case InputDevice::Id::SOAPYSDR:
#if HAVE_SOAPYSDR
                    showButton = !ui->soapysdrDevArgs->text().isEmpty() &&
                                 (showButton || ui->soapysdrDevArgs->text().trimmed() != m_settings->soapysdr.devArgs ||
                                  ui->soapySdrAntenna->text().trimmed() != m_settings->soapysdr.antenna ||
                                  ui->soapysdrChannelNum->value() != m_settings->soapysdr.channel);
#endif
                    break;
                case InputDevice::Id::SDRPLAY:
#if HAVE_SOAPYSDR
                    showButton = (ui->sdrplayDeviceListCombo->count() > 0) &&
                                 (showButton || (ui->sdrplayDeviceListCombo->currentData() != m_settings->sdrplay.hwId));
#endif
                    break;
                case InputDevice::Id::RTLTCP:
                case InputDevice::Id::RAWFILE:
                case InputDevice::Id::RARTTCP:
                case InputDevice::Id::UNDEFINED:
                    break;
            }
            ui->connectButton->setEnabled(showButton);
        }

        break;
    }
}

void SetupDialog::onProxyConfigChanged(int index)
{
    bool enaManual = static_cast<Settings::ProxyConfig>(ui->proxyConfigCombo->itemData(index).toInt()) == Settings::ProxyConfig::Manual;
    ui->proxyServerLabel->setEnabled(enaManual);
    ui->proxyServerEdit->setEnabled(enaManual);
    ui->proxyPortLabel->setEnabled(enaManual);
    ui->proxyPortEdit->setEnabled(enaManual);
    ui->proxyUserLabel->setEnabled(enaManual);
    ui->proxyUserEdit->setEnabled(enaManual);
    ui->proxyPassLabel->setEnabled(enaManual);
    ui->proxyPassEdit->setEnabled(enaManual);

    ui->proxyApplyButton->setEnabled(static_cast<Settings::ProxyConfig>(ui->proxyConfigCombo->itemData(index).toInt()) != m_settings->proxy.config);
}

void SetupDialog::onProxyConfigEdit()
{
    ui->proxyApplyButton->setEnabled(true);
}

void SetupDialog::onProxyApplyButtonClicked()
{
    m_settings->proxy.config = static_cast<Settings::ProxyConfig>(ui->proxyConfigCombo->itemData(ui->proxyConfigCombo->currentIndex()).toInt());
    if (m_settings->proxy.config == Settings::ProxyConfig::Manual)
    {
        m_settings->proxy.server = ui->proxyServerEdit->text().trimmed();
        m_settings->proxy.port = ui->proxyPortEdit->text().trimmed().toUInt();
        m_settings->proxy.user = ui->proxyUserEdit->text().trimmed();

        if (m_settings->proxy.pass != ui->proxyPassEdit->text())
        {  // this is very very weak protection of pass used only to avoid storing pass in plain text
            QByteArray ba;
            int key = 0;
            for (int n = 0; n < 4; ++n)
            {
                int val = QRandomGenerator::global()->generate() & 0xFF;
                ba.append(static_cast<char>(val));
                key += val;
            }
            key = key & 0x00FF;
            if (key == 0)
            {
                key = 0x5C;
            }
            ba.append(ui->proxyPassEdit->text().toUtf8());
            for (int n = 4; n < ba.length(); ++n)
            {
                ba[n] = ba[n] ^ key;
            }
            m_settings->proxy.pass = ba;
        }
    }
    ui->proxyApplyButton->setEnabled(false);

    emit proxySettingsChanged();
}

void SetupDialog::onSlsBgButtonClicked()
{
    const QColor color = QColorDialog::getColor(m_settings->slsBackground, this, tr("Select SLS Background Color"), QColorDialog::ShowAlphaChannel);

    if (color.isValid() && color != m_settings->slsBackground)
    {
        m_settings->slsBackground = color;

        QString qss = QString("QPushButton {background-color: %1; border: 1px solid black; border-radius: 3px}").arg(color.name(QColor::HexArgb));
        ui->slsBgButton->setStyleSheet(qss);

        emit slsBgChanged(color);
    }
}
