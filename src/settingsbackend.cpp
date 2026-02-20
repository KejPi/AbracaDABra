/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2026 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#include "settingsbackend.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGeoCoordinate>
#include <QQmlContext>
#include <QRandomGenerator>
#include <QStandardPaths>

#ifdef Q_OS_ANDROID
#include "androidfilehelper.h"
#endif
#include "itemmodel.h"
#include "rtlsdrinput.h"
#include "rtltcpinput.h"
#if HAVE_SOAPYSDR
#include "soapysdrgainmodel.h"
#include "soapysdrinput.h"
#endif
#include "txdataloader.h"

const QString SettingsBackend::m_coordinatesHelp = QString(QT_TR_NOOP(R"(
        Enter coordinates in "latitude, longitude" format, for example: 1.234,-5.678</p>
        <p>Tip: <i>Go to <a href="https://www.google.com/maps">Google maps</a>,
        right click on your location, click on coordinates in popup menu to copy them
        and then insert the values here as they are.</i></p>
    )"));

SettingsBackend::SettingsBackend(QQmlApplicationEngine *qmlEngine, QObject *parent) : UIControlProvider(parent)
{
    QQmlContext *context = qmlEngine->rootContext();
    context->setContextProperty("settingsBackend", this);

    m_tabsModel = new ItemModel(this);
    m_tabsModel->addItem(tr("Device"), static_cast<int>(SettingsBackendTabs::Device));
    m_tabsModel->addItem(tr("Audio"), static_cast<int>(SettingsBackendTabs::Audio));
    m_tabsModel->addItem(tr("Announcements"), static_cast<int>(SettingsBackendTabs::Announcement));
    m_tabsModel->addItem(tr("User applications"), static_cast<int>(SettingsBackendTabs::UserApps));
    m_tabsModel->addItem(tr("TII"), static_cast<int>(SettingsBackendTabs::Tii));
    m_tabsModel->addItem(tr("Others"), static_cast<int>(SettingsBackendTabs::Other));
    m_tabsModel->setCurrentIndex(0);

    m_inputsModel = new ItemModel(this);
    m_inputsModel->addItem("RTL SDR", static_cast<int>(InputDevice::Id::RTLSDR));
    m_inputsModel->addItem("RTL TCP", static_cast<int>(InputDevice::Id::RTLTCP));
#if HAVE_AIRSPY
    m_inputsModel->addItem("Airspy", static_cast<int>(InputDevice::Id::AIRSPY));
#endif
#if HAVE_SOAPYSDR
    m_inputsModel->addItem("SDRplay", static_cast<int>(InputDevice::Id::SDRPLAY));
    m_inputsModel->addItem("Soapy SDR", static_cast<int>(InputDevice::Id::SOAPYSDR));
#endif
#if HAVE_RARTTCP
    m_inputsModel->addItem("RaRT TCP", static_cast<int>(InputDevice::Id::RARTTCP));
#endif
    m_inputsModel->addItem("Raw file", static_cast<int>(InputDevice::Id::RAWFILE));

    m_rawFileFormatModel = new ItemModel(this);
    m_rawFileFormatModel->addItem(tr("Unsigned 8 bits"), static_cast<int>(RawFileInputFormat::SAMPLE_FORMAT_U8));
    m_rawFileFormatModel->addItem(tr("Signed 16 bits"), static_cast<int>(RawFileInputFormat::SAMPLE_FORMAT_S16));

    m_rtlSdrDevicesModel = new ItemModel(this);
#if HAVE_AIRSPY
    m_airspyDevicesModel = new ItemModel(this);
#endif
#if HAVE_SOAPYSDR
    m_sdrplayDevicesModel = new ItemModel(this);
    m_sdrplayAntennaModel = new ItemModel(this);
    m_sdrplayChannelModel = new ItemModel(this);
    m_soapySdrGainModel = new SoapySdrGainModel(this);
    connect(m_soapySdrGainModel, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
            {
                Q_UNUSED(bottomRight)
                if (topLeft.isValid() && roles.contains(SoapySdrGainModel::GainValueRole))
                {
                    // valueLabel->setText(QString::number(val * step, '1', 1) + " dB");
                    int val = m_soapySdrGainModel->data(topLeft, SoapySdrGainModel::GainValueRole).toInt();
                    m_settings->soapysdr.gainMap[m_settings->soapysdr.driver].gainList[topLeft.row()] = val;
                    auto device = dynamic_cast<SoapySdrInput *>(m_device);
                    if (device)
                    {
                        device->setGain(topLeft.row(), val);
                    }
                }
            });
#endif
    connect(this, &SettingsBackend::rawFileProgressValueChanged, this, &SettingsBackend::updateRawFileProgressLabel);
    connect(this, &SettingsBackend::rawFileProgressMaxChanged, this, &SettingsBackend::updateRawFileProgressLabel);
    connect(m_inputsModel, &ItemModel::currentIndexChanged, this, &SettingsBackend::onCurrentDeviceChanged);
    connect(m_rawFileFormatModel, &ItemModel::currentIndexChanged, this,
            [this]()
            {
                if (static_cast<RawFileInputFormat>(m_rawFileFormatModel->currentData().toInt()) != m_settings->rawfile.format)
                {
                    setConnectButton(ConnectButtonOn);
                }
            });
    connect(m_rtlSdrDevicesModel, &ItemModel::currentIndexChanged, this, [this]() { setConnectButton(ConnectButtonAuto); });
    connect(this, &SettingsBackend::rtlSdrGainIndexChanged, this, &SettingsBackend::updateRtlSdrGainLabel);
    connect(this, &SettingsBackend::rtlSdrGainIndexMaxChanged, this, &SettingsBackend::updateRtlSdrGainLabel);
    connect(this, &SettingsBackend::rtlSdrBandWidthChanged, this, &SettingsBackend::onBandwidthChanged);
    connect(this, &SettingsBackend::rtlSdrFreqCorrectionChanged, this, &SettingsBackend::onFrequencyCorrectionChanged);
    connect(this, &SettingsBackend::rtlSdrRfLevelCorrectionChanged, this, &SettingsBackend::onRfLevelOffsetChanged);
    connect(this, &SettingsBackend::rtlSdrBiasTChanged, this, &SettingsBackend::onBiasTChanged);

    connect(this, &SettingsBackend::rtlTcpIpAddressChanged, this, [this]() { setConnectButton(ConnectButtonAuto); });
    connect(this, &SettingsBackend::rtlTcpPortChanged, this, [this]() { setConnectButton(ConnectButtonAuto); });
    connect(this, &SettingsBackend::isRtlTcpControlSocketCheckedChanged, this, [this]() { setConnectButton(ConnectButtonAuto); });

    connect(this, &SettingsBackend::rtlTcpGainIndexChanged, this, &SettingsBackend::updateRtlTcpGainLabel);
    connect(this, &SettingsBackend::rtlTcpGainIndexMaxChanged, this, &SettingsBackend::updateRtlTcpGainLabel);
    connect(this, &SettingsBackend::rtlTcpFreqCorrectionChanged, this, &SettingsBackend::onFrequencyCorrectionChanged);
    connect(this, &SettingsBackend::rtlTcpRfLevelCorrectionChanged, this, &SettingsBackend::onRfLevelOffsetChanged);

#if HAVE_AIRSPY
    connect(m_airspyDevicesModel, &ItemModel::currentIndexChanged, this, [this]() { setConnectButton(ConnectButtonAuto); });
    connect(this, &SettingsBackend::airspyBiasTChanged, this, &SettingsBackend::onBiasTChanged);
#endif
#if HAVE_SOAPYSDR
    connect(this, &SettingsBackend::soapySdrDevArgsChanged, this, [this]() { setConnectButton(ConnectButtonAuto); });
    connect(this, &SettingsBackend::soapySdrChannelNumChanged, this, [this]() { setConnectButton(ConnectButtonAuto); });
    connect(this, &SettingsBackend::soapySdrAntennaChanged, this, [this]() { setConnectButton(ConnectButtonAuto); });
    connect(this, &SettingsBackend::soapySdrBandWidthChanged, this, &SettingsBackend::onBandwidthChanged);
    connect(this, &SettingsBackend::soapySdrFreqCorrectionChanged, this, &SettingsBackend::onFrequencyCorrectionChanged);

    connect(m_sdrplayDevicesModel, &ItemModel::currentIndexChanged, this, [this]() { setConnectButton(ConnectButtonAuto); });
    connect(m_sdrplayDevicesModel, &ItemModel::currentIndexChanged, this, &SettingsBackend::onSdrplayDeviceChanged);
    connect(m_sdrplayChannelModel, &ItemModel::currentIndexChanged, this, &SettingsBackend::onSdrplayChannelChanged);
    connect(m_sdrplayAntennaModel, &ItemModel::currentIndexChanged, this, &SettingsBackend::onSdrplayAntennaChanged);
    connect(this, &SettingsBackend::sdrplayRfGainIndexChanged, this, &SettingsBackend::updateSdrplayRfGainLabel);
    connect(this, &SettingsBackend::sdrplayRfGainIndexMaxChanged, this, &SettingsBackend::updateSdrplayRfGainLabel);
    connect(this, &SettingsBackend::sdrplayFreqCorrectionChanged, this, &SettingsBackend::onFrequencyCorrectionChanged);
    connect(this, &SettingsBackend::sdrplayBiasTChanged, this, &SettingsBackend::onBiasTChanged);
#endif

    m_audioNoiseConcealModel = new ItemModel(this);
    m_audioNoiseConcealModel->addItem("-20 dBFS", QVariant(20));
    m_audioNoiseConcealModel->addItem("-25 dBFS", QVariant(25));
    m_audioNoiseConcealModel->addItem("-30 dBFS", QVariant(30));
    m_audioNoiseConcealModel->addItem("-35 dBFS", QVariant(35));
    m_audioNoiseConcealModel->addItem("-40 dBFS", QVariant(40));
    m_audioNoiseConcealModel->addItem(tr("Disabled"), QVariant(0));

    connect(m_audioNoiseConcealModel, &ItemModel::currentIndexChanged, this, &SettingsBackend::onNoiseLevelChanged);

    connect(this, &SettingsBackend::audioRecCaptureOutputChanged, this,
            [this]() { emit audioRecordingSettings(m_settings->dataStoragePath, m_settings->audioRec.captureOutput); });

    m_audioOutputModel = new ItemModel(this);
#if (HAVE_PORTAUDIO)
    m_audioOutputModel->addItem("PortAudio", Settings::AudioFramework::Pa);
#endif
    m_audioOutputModel->addItem("Qt", Settings::AudioFramework::Qt);
    // ui->audioOutWarnLabel->setText("<span style=\"color:red\">" + tr("Audio output change will take effect after application restart.") +
    // "</span>"); ui->audioOutWarnLabel->setVisible(false); ui->audioOutRestartButton->setVisible(false);
    connect(m_audioOutputModel, &ItemModel::currentIndexChanged, this, &SettingsBackend::onAudioOutChanged);
    // connect(ui->audioOutRestartButton, &QPushButton::clicked, this, [this]() { emit restartRequested(); });

    m_audioDecoderModel = new ItemModel(this);
#if (HAVE_FAAD)
    m_audioDecoderModel->addItem("FAAD2", Settings::AudioDecoder::FAAD);
#endif
#if (HAVE_FDKAAC)
    m_audioDecoderModel->addItem("FDK-AAC", Settings::AudioDecoder::FDKAAC);
#endif
    connect(m_audioDecoderModel, &ItemModel::currentIndexChanged, this, &SettingsBackend::onAudioDecChanged);

    // qmlRegisterType<AnnouncementsProxyModel>("abracaComponents", 1, 0, "AnnouncementsProxyModel");
    m_announcementModel = new ItemModel(this);

    connect(this, &SettingsBackend::isSpiAppEnabledChanged, this,
            [this]()
            {
                emit spiApplicationEnabled(m_settings->spiAppEna);
                emit spiApplicationSettingsChanged(m_settings->useInternet && m_settings->spiAppEna,
                                                   m_settings->radioDnsEna && m_settings->spiAppEna);
            });
    connect(this, &SettingsBackend::isSpiInternetEnabledChanged, this,
            [this]() { emit spiApplicationSettingsChanged(m_settings->useInternet, m_settings->radioDnsEna); });

    connect(this, &SettingsBackend::isRadioDnsEnabledChanged, this,
            [this]() { emit spiApplicationSettingsChanged(m_settings->useInternet, m_settings->radioDnsEna); });
    connect(this, &SettingsBackend::isSpiProgressEnabledChanged, this, &SettingsBackend::spiIconSettingsChanged);
    connect(this, &SettingsBackend::spiProgressHideCompleteChanged, this, &SettingsBackend::spiIconSettingsChanged);

    connect(this, &SettingsBackend::dataStoragePathChanged, this,
            [this]()
            {
                m_settings->uaDump.dataStoragePath = m_settings->dataStoragePath;
                dataDumpPath(getDisplayPath(m_settings->uaDump.dataStoragePath + '/' + UA_DIR_NAME));
                tiiLogPath(getDisplayPath(m_settings->uaDump.dataStoragePath + '/' + TII_DIR_NAME));
                audioRecPath(getDisplayPath(m_settings->uaDump.dataStoragePath + '/' + AUDIO_DIR_NAME));
                emit uaDumpSettings(m_settings->uaDump);
                emit audioRecordingSettings(m_settings->dataStoragePath, m_settings->audioRec.captureOutput);
            });
    connect(this, &SettingsBackend::isUaDumpOverwiteEnabledChanged, this, [this]() { emit uaDumpSettings(m_settings->uaDump); });
    connect(this, &SettingsBackend::isUaDumpSlsEnabledChanged, this, [this]() { emit uaDumpSettings(m_settings->uaDump); });
    connect(this, &SettingsBackend::isUaDumpSpiEnabledChanged, this, [this]() { emit uaDumpSettings(m_settings->uaDump); });
    connect(this, &SettingsBackend::uaDumpSlsPatternChanged, this, [this]() { emit uaDumpSettings(m_settings->uaDump); });
    connect(this, &SettingsBackend::uaDumpSpiPatternChanged, this, [this]() { emit uaDumpSettings(m_settings->uaDump); });

    m_locationSourceModel = new ItemModel(this);
    m_locationSourceModel->addItem(tr("System"), static_cast<int>(Settings::GeolocationSource::System));
    m_locationSourceModel->addItem(tr("Manual"), static_cast<int>(Settings::GeolocationSource::Manual));
#ifndef Q_OS_ANDROID
    m_locationSourceModel->addItem(tr("Serial port"), static_cast<int>(Settings::GeolocationSource::SerialPort));
#endif

    connect(m_locationSourceModel, &ItemModel::currentIndexChanged, this, &SettingsBackend::onGeolocationSourceChanged);

    m_serialPortBaudrateModel = new ItemModel(this);
    m_serialPortBaudrateModel->addItem("1200", 1200);
    m_serialPortBaudrateModel->addItem("2400", 2400);
    m_serialPortBaudrateModel->addItem("4800", 4800);
    m_serialPortBaudrateModel->addItem("9600", 9600);
    m_serialPortBaudrateModel->addItem("19200", 19200);
    m_serialPortBaudrateModel->addItem("38400", 38400);
    m_serialPortBaudrateModel->addItem("57600", 57600);
    m_serialPortBaudrateModel->addItem("115200", 115200);
    connect(m_serialPortBaudrateModel, &ItemModel::currentIndexChanged, this,
            [this]()
            {
                if (m_serialPortBaudrateModel->currentIndex() > 0)
                {
                    auto rate = m_serialPortBaudrateModel->currentData().toInt();
                    m_settings->tii.serialPortBaudrate = rate;
                    emit tiiSettingsChanged();
                }
            });
    connect(this, &SettingsBackend::tiiSerialPortChanged, this, &SettingsBackend::tiiSettingsChanged);
    connect(this, &SettingsBackend::tiiLogUtcTimestampChanged, this, &SettingsBackend::tiiSettingsChanged);
    connect(this, &SettingsBackend::tiiLogCoordinatesChanged, this, &SettingsBackend::tiiSettingsChanged);
    connect(this, &SettingsBackend::tiiModeValueChanged, this, [this]() { emit tiiModeChanged(m_settings->tii.mode); });
    connect(this, &SettingsBackend::tiiShowSpectrumPlotChanged, this, &SettingsBackend::tiiSettingsChanged);
    connect(this, &SettingsBackend::tiiShowInactiveChanged, this, &SettingsBackend::tiiSettingsChanged);
    connect(this, &SettingsBackend::isTiiInactiveTimeoutEnabledChanged, this, &SettingsBackend::tiiSettingsChanged);
    connect(this, &SettingsBackend::tiiInactiveTimeoutChanged, this, &SettingsBackend::tiiSettingsChanged);

    m_languageSelectionModel = new ItemModel(this);

    // create combo box with language selection
    m_languageSelectionModel->addItem("<" + tr("System language") + ">", QVariant(QLocale::AnyLanguage));
    m_languageSelectionModel->addItem("English", QVariant(QLocale::English));  // QLocale::English native name returns "American English" :-(
    for (auto l : m_supportedLocalization)
    {
        m_languageSelectionModel->addItem(QLocale(l).nativeLanguageName(), QVariant(l));
    }
    connect(m_languageSelectionModel, &ItemModel::currentIndexChanged, this, &SettingsBackend::onLanguageChanged);

    m_proxyConfigModel = new ItemModel(this);
    m_proxyConfigModel->addItem(tr("No proxy"), QVariant::fromValue(Settings::ProxyConfig::NoProxy));
    m_proxyConfigModel->addItem(tr("System"), QVariant::fromValue(Settings::ProxyConfig::System));
    m_proxyConfigModel->addItem(tr("Manual"), QVariant::fromValue(Settings::ProxyConfig::Manual));
    connect(m_proxyConfigModel, &ItemModel::currentIndexChanged, this, [this]()
            { isProxyApplyEnabled(static_cast<Settings::ProxyConfig>(m_proxyConfigModel->currentData().toInt()) != m_settings->proxy.config); });
    connect(this, &SettingsBackend::isXmlHeaderEnabledChanged, this, [this]() { emit xmlHeaderToggled(m_settings->xmlHeaderEna); });

    m_tiiTableColsModel = new TiiTableColsSettingsModel(this);
    connect(m_tiiTableColsModel, &TiiTableColsSettingsModel::colSettingsChanged, this, &SettingsBackend::tiiTableSettingsChanged);
}

QUrl SettingsBackend::rawFilePath() const
{
    QString dir = QDir::homePath();
    if (!m_rawFileName.isEmpty() && QFileInfo::exists(m_rawFileName))
    {
        dir = QFileInfo(m_rawFileName).path();
    }
    return QUrl::fromLocalFile(dir + '/');
}

void SettingsBackend::setSpiDumpPaternDefault(const QString &newSpiDumpPaternDefault)
{
    m_spiDumpPaternDefault = newSpiDumpPaternDefault;
}

void SettingsBackend::setSlsDumpPaternDefault(const QString &newSlsDumpPaternDefault)
{
    m_slsDumpPaternDefault = newSlsDumpPaternDefault;
}

void SettingsBackend::setGainValues(const QList<float> &gainList)
{
    switch (m_inputDeviceId)
    {
        case InputDevice::Id::RTLSDR:
            m_rtlsdrGainList.clear();
            m_rtlsdrGainList = gainList;
            if (m_rtlsdrGainList.size() > 0)
            {
                rtlSdrGainIndexMax(m_rtlsdrGainList.size() - 1);
                isRtlSdrGainEnabled(true);
            }
            else
            {
                rtlSdrGainIndexMax(0);
                isRtlSdrGainEnabled(false);
            }
            m_settings->rtlsdr.gainIdx = qMax(0, qMin(m_settings->rtlsdr.gainIdx, m_rtlSdrGainIndexMax));
            setRtlSdrGainIndex(m_settings->rtlsdr.gainIdx);
            break;
        case InputDevice::Id::RTLTCP:
            m_rtltcpGainList.clear();
            m_rtltcpGainList = gainList;
            if (m_rtltcpGainList.size() > 0)
            {
                rtlTcpGainIndexMax(m_rtltcpGainList.size() - 1);
                isRtlTcpGainEnabled(true);
            }
            else
            {
                rtlTcpGainIndexMax(0);
                isRtlTcpGainEnabled(false);
            }
            m_settings->rtltcp.gainIdx = qMax(0, qMin(m_settings->rtltcp.gainIdx, m_rtlTcpGainIndexMax));
            setRtlTcpGainIndex(m_settings->rtltcp.gainIdx);
            break;
        case InputDevice::Id::SDRPLAY:
#if HAVE_SOAPYSDR
            m_sdrplayGainList.clear();
            m_sdrplayGainList = gainList;
            sdrplayRfGainIndexMax(m_sdrplayGainList.size() - 1);
            setSdrplayRfGainIndex(m_settings->sdrplay.gain.rfGain >= 0 ? m_settings->sdrplay.gain.rfGain : 0);
            setSdrplayIfGain(m_settings->sdrplay.gain.ifGain != 0 ? m_settings->sdrplay.gain.ifGain : -40);
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

void SettingsBackend::setSettings(Settings *settings)
{
    m_settings = settings;

    if (false == m_inputsModel->setCurrentData(static_cast<int>(m_settings->inputDevice)))
    {  // enable RTL-SDR as fallback
        m_inputsModel->setCurrentData(static_cast<int>(InputDevice::Id::RTLSDR));
    }

    if (m_settings->rawfile.file.isEmpty())
    {
        rawFileName("");
    }
    else
    {
        rawFileName(m_settings->rawfile.file);
    }

    if (false == m_rawFileFormatModel->setCurrentData(static_cast<int>(m_settings->rawfile.format)))
    {  // U8 is fallback
        m_rawFileFormatModel->setCurrentData(static_cast<int>(RawFileInputFormat::SAMPLE_FORMAT_U8));
    }

    setRtlSdrGainIndex(m_rtlsdrGainList.isEmpty() ? 0 : m_settings->rtlsdr.gainIdx);
    updateRtlSdrGainLabel();
    switch (m_settings->rtlsdr.gainMode)
    {
        case RtlGainMode::Software:
            rtlSdrGainMode(0);
            break;
        case RtlGainMode::Hardware:
            rtlSdrGainMode(2);
            break;
        case RtlGainMode::Manual:
            rtlSdrGainMode(3);
            break;
        case RtlGainMode::Driver:
#ifdef RTLSDR_OLD_DAB
            rtlSdrGainMode(1);
#else
            rtlSdrGainMode(0);
            m_settings->rtlsdr.gainMode = RtlGainMode::Software;
#endif
            break;
        default:
            rtlSdrGainMode(0);
            m_settings->rtlsdr.gainMode = RtlGainMode::Software;
            break;
    }
    rtlSdrBandWidth(m_settings->rtlsdr.bandwidth / 1000);
    setRtlSdrAgcLevelThr(m_settings->rtlsdr.agcLevelMax);
    rtlSdrFreqCorrection(m_settings->rtlsdr.ppm);
    rtlSdrRfLevelCorrection(m_settings->rtlsdr.rfLevelOffset);
    rtlSdrBiasT(m_settings->rtlsdr.biasT);

    rtlTcpIpAddress(m_settings->rtltcp.tcpAddress);
    rtlTcpPort(m_settings->rtltcp.tcpPort);
    isRtlTcpControlSocketChecked(m_settings->rtltcp.controlSocketEna);
    setRtlTcpGainIndex(m_rtltcpGainList.isEmpty() ? 0 : m_settings->rtltcp.gainIdx);
    updateRtlTcpGainLabel();
    switch (m_settings->rtltcp.gainMode)
    {
        case RtlGainMode::Software:
            rtlTcpGainMode(0);
            break;
        case RtlGainMode::Hardware:
            rtlTcpGainMode(1);
            break;
        case RtlGainMode::Manual:
            rtlTcpGainMode(2);
            break;
        default:
            rtlSdrGainMode(0);
            m_settings->rtltcp.gainMode = RtlGainMode::Software;
            break;
    }
    setRtlTcpAgcLevelThr(m_settings->rtltcp.agcLevelMax);
    rtlTcpFreqCorrection(m_settings->rtltcp.ppm);
    rtlTcpRfLevelCorrection(m_settings->rtltcp.rfLevelOffset);

#if HAVE_AIRSPY
    switch (m_settings->airspy.gain.mode)
    {
        case AirpyGainMode::Software:
            airspyGainMode(0);
            break;
        case AirpyGainMode::Hybrid:
            airspyGainMode(1);
            break;
        case AirpyGainMode::Sensitivity:
            airspyGainMode(2);
            break;
        case AirpyGainMode::Manual:
            airspyGainMode(3);
            break;
        default:  // fallback is SW
            airspyGainMode(0);
            m_settings->airspy.gain.mode = AirpyGainMode::Software;
            break;
    }

    airspyBiasT(m_settings->airspy.biasT);
#endif

#if HAVE_SOAPYSDR
    if (m_settings->soapysdr.driver.isEmpty() || !m_settings->soapysdr.gainMap.contains(m_settings->soapysdr.driver))
    {  // no last driver -> using defaults
        soapySdrGainMode(0);
    }
    else
    {
        switch (m_settings->soapysdr.gainMap[m_settings->soapysdr.driver].mode)
        {
            case SoapyGainMode::Hardware:
                soapySdrGainMode(0);
                break;
            case SoapyGainMode::Manual:
                soapySdrGainMode(1);
                break;
            default:
                break;
        }
    }
    soapySdrDevArgs(m_settings->soapysdr.devArgs);
    soapySdrChannelNum(m_settings->soapysdr.channel);
    soapySdrAntenna(m_settings->soapysdr.antenna);
    soapySdrBandWidth(m_settings->soapysdr.bandwidth / 1000);
    soapySdrFreqCorrection(m_settings->soapysdr.ppm);

    switch (m_settings->sdrplay.gain.mode)
    {
        case SdrPlayGainMode::Software:
            sdrplayGainMode(0);
            break;
        case SdrPlayGainMode::Manual:
            sdrplayGainMode(1);
            break;
        default:
            break;
    }

    setSdrplayRfGainIndex(m_sdrplayGainList.isEmpty() ? 0 : m_settings->sdrplay.gain.rfGain);
    updateSdrplayRfGainLabel();
    sdrplayFreqCorrection(m_settings->sdrplay.ppm);
    sdrplayBiasT(m_settings->sdrplay.biasT);
#endif
#if HAVE_RARTTCP
    rartTcpIpAddress(m_settings->rarttcp.tcpAddress);
    rartTcpPort(m_settings->rarttcp.tcpPort);
#endif

    if (false == m_audioNoiseConcealModel->setCurrentData(QVariant(m_settings->noiseConcealmentLevel)))
    {  // first item as fallback
        m_audioNoiseConcealModel->setCurrentIndex(0);
    }

    if (false == m_audioDecoderModel->setCurrentData(QVariant(m_settings->audioDecoder)))
    {  // first item as fallback
        m_audioDecoderModel->setCurrentIndex(0);
    }

    if (false == m_audioOutputModel->setCurrentData(QVariant(m_settings->audioFramework)))
    {  // first item as fallback
        m_audioOutputModel->setCurrentIndex(0);
    }

    // announcements
    uint16_t announcementEna = m_settings->announcementEna | (1 << static_cast<int>(DabAnnouncement::Alarm));  // enable alarm
    for (int ann = static_cast<int>(DabAnnouncement::Alarm); ann <= static_cast<int>(DabAnnouncement::AlarmTest); ++ann)
    {
        m_announcementModel->addItem(DabTables::getAnnouncementName(static_cast<DabAnnouncement>(ann)), announcementEna & 0x1);
        announcementEna >>= 1;
    }
    connect(m_announcementModel, &ItemModel::dataChanged, this, &SettingsBackend::updateAnnouncementFlags);

    setTiiDbUpdate();

    if (false == m_locationSourceModel->setCurrentData(static_cast<int>(m_settings->tii.locationSource)))
    {  // first item as fallback
        m_locationSourceModel->setCurrentIndex(0);
    }

    if (false == m_serialPortBaudrateModel->setCurrentData(m_settings->tii.serialPortBaudrate))
    {  // first item as fallback
        m_serialPortBaudrateModel->setCurrentIndex(0);
    }

    setLocationCoordinates(QString("%1, %2")
                               .arg(m_settings->tii.coordinates.latitude(), 0, 'g', QLocale::FloatingPointShortest)
                               .arg(m_settings->tii.coordinates.longitude(), 0, 'g', QLocale::FloatingPointShortest));

    applicationTheme(static_cast<int>(m_settings->applicationStyle));

    if (false == m_languageSelectionModel->setCurrentData(QVariant(m_settings->lang)))
    {  // first item as fallback
        m_languageSelectionModel->setCurrentIndex(0);
    }

    if (false == m_proxyConfigModel->setCurrentData(static_cast<int>(m_settings->proxy.config)))
    {  // first item as fallback
        m_proxyConfigModel->setCurrentIndex(0);
    }
    proxyServer(m_settings->proxy.server);
    proxyPort(m_settings->proxy.port);
    proxyUser(m_settings->proxy.user);
    proxyPass(m_settings->proxy.pass);

    dataDumpPath(getDisplayPath(m_settings->uaDump.dataStoragePath + '/' + UA_DIR_NAME));
    tiiLogPath(getDisplayPath(m_settings->uaDump.dataStoragePath + '/' + TII_DIR_NAME));
    audioRecPath(getDisplayPath(m_settings->uaDump.dataStoragePath + '/' + AUDIO_DIR_NAME));

    m_tiiTableColsModel->init(m_settings);

    setStatusLabel();

    // this is to align UI with settings values
    emit newAnnouncementSettings();
    emit noiseConcealmentLevelChanged(m_settings->noiseConcealmentLevel);
    emit xmlHeaderToggled(m_settings->xmlHeaderEna);
    emit audioRecordingSettings(m_settings->dataStoragePath, m_settings->audioRec.captureOutput);
    emit uaDumpSettings(m_settings->uaDump);
    emit tiiSettingsChanged();
    emit tiiModeChanged(m_settings->tii.mode);
    emit proxySettingsChanged();
    emit showTrayIconChanged();
    emit slsBackgroundColorChanged();
    emit showSystemTimeChanged();
    emit showEnsembleCountryFlagChanged();
    emit spiIconSettingsChanged();
    emit spiApplicationEnabled(m_settings->spiAppEna);
    emit spiApplicationSettingsChanged(m_settings->useInternet && m_settings->spiAppEna, m_settings->radioDnsEna && m_settings->spiAppEna);
    emit fullscreenChanged();
    emit compactUiChanged();
    emit cableChannelsEnaChanged();
}

void SettingsBackend::setRawFileLength(int msec)
{
    setRawFileProgressValue(0);
    rawFileProgressMax(msec);

    isRawFileProgressVisible(0 != msec);
    isRawFileProgressEnabled(false);
}

QLocale::Language SettingsBackend::applicationLanguage() const
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

void SettingsBackend::requestConnectDevice()
{
    setConnectButton(ConnectButtonOff);
#if HAVE_SOAPYSDR
    setSoapySdrGainModel(false);
#endif
    m_inputDeviceId = InputDevice::Id::UNDEFINED;
    setStatusLabel(true);  // clear label
    m_settings->inputDevice = static_cast<InputDevice::Id>(m_inputsModel->currentData().toInt());
    QVariant id;
    switch (m_settings->inputDevice)
    {
        case InputDevice::Id::RTLSDR:
            id = m_rtlSdrDevicesModel->currentData();
            isRtlSdrDeviceSelectionEnabled(false);
            break;
        case InputDevice::Id::RTLTCP:
            m_settings->rtltcp.tcpAddress = m_rtlTcpIpAddress;
            m_settings->rtltcp.tcpPort = m_rtlTcpPort;
            m_settings->rtltcp.controlSocketEna = m_isRtlTcpControlSocketChecked;
            break;
        case InputDevice::Id::RARTTCP:
            break;
        case InputDevice::Id::UNDEFINED:
            break;
        case InputDevice::Id::RAWFILE:
            m_settings->rawfile.file = m_rawFileName;
            m_settings->rawfile.format = static_cast<RawFileInputFormat>(m_rawFileFormatModel->currentData().toInt());
            break;
        case InputDevice::Id::AIRSPY:
#if HAVE_AIRSPY
            id = m_airspyDevicesModel->currentData();
            isAirspyDeviceSelectionEnabled(false);
#endif
            break;
        case InputDevice::Id::SOAPYSDR:
#if HAVE_SOAPYSDR
            m_settings->soapysdr.devArgs = m_soapySdrDevArgs.trimmed();
            m_settings->soapysdr.channel = m_soapySdrChannelNum;
            m_settings->soapysdr.antenna = m_soapySdrAntenna.trimmed();
#endif
            break;
        case InputDevice::Id::SDRPLAY:
#if HAVE_SOAPYSDR
            id = m_sdrplayDevicesModel->currentData();
            m_settings->sdrplay.channel = m_sdrplayChannelModel->currentData().toInt();
            m_settings->sdrplay.antenna = m_sdrplayAntennaModel->currentData().toString();
#endif
            break;
    }
    emit inputDeviceChanged(m_settings->inputDevice, id);
}

void SettingsBackend::onBandwidthChanged()
{
    switch (m_inputDeviceId)
    {
        case InputDevice::Id::RTLSDR:
            m_settings->rtlsdr.bandwidth = m_rtlSdrBandWidth * 1000;
            if (m_device)
            {
                m_device->setBW(m_settings->rtlsdr.bandwidth);
            }
            break;
        case InputDevice::Id::SOAPYSDR:
#if HAVE_SOAPYSDR
            m_settings->soapysdr.bandwidth = m_soapySdrBandWidth * 1000;
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

void SettingsBackend::onFrequencyCorrectionChanged()
{
    switch (m_inputDeviceId)
    {
        case InputDevice::Id::RTLSDR:
            m_settings->rtlsdr.ppm = m_rtlSdrFreqCorrection;
            if (m_device)
            {
                m_device->setPPM(m_settings->rtlsdr.ppm);
            }
            break;
        case InputDevice::Id::RTLTCP:
            m_settings->rtltcp.ppm = m_rtlTcpFreqCorrection;
            if (m_device)
            {
                m_device->setPPM(m_settings->rtltcp.ppm);
            }
            break;
        case InputDevice::Id::SOAPYSDR:
#if HAVE_SOAPYSDR
            m_settings->soapysdr.ppm = m_soapySdrFreqCorrection;
            if (m_device)
            {
                m_device->setPPM(m_settings->soapysdr.ppm);
            }
#endif
            break;
        case InputDevice::Id::SDRPLAY:
#if HAVE_SOAPYSDR
            m_settings->sdrplay.ppm = m_sdrplayFreqCorrection;
            if (m_device)
            {
                m_device->setPPM(m_settings->sdrplay.ppm);
            }
#endif
            break;
        case InputDevice::Id::AIRSPY:
        case InputDevice::Id::RARTTCP:
        case InputDevice::Id::UNDEFINED:
        case InputDevice::Id::RAWFILE:
            break;
    }
}

void SettingsBackend::onBiasTChanged()
{
    switch (m_inputDeviceId)
    {
        case InputDevice::Id::RTLSDR:
            m_settings->rtlsdr.biasT = m_rtlSdrBiasT;
            if (m_device)
            {
                m_device->setBiasT(m_settings->rtlsdr.biasT);
            }
            break;
        case InputDevice::Id::AIRSPY:
#if HAVE_AIRSPY
            m_settings->airspy.biasT = m_airspyBiasT;
            if (m_device)
            {
                m_device->setBiasT(m_settings->airspy.biasT);
            }
#endif
            break;
        case InputDevice::Id::SDRPLAY:
#if HAVE_SOAPYSDR
            m_settings->sdrplay.biasT = m_sdrplayBiasT;
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

void SettingsBackend::onRfLevelOffsetChanged()
{
    switch (m_inputDeviceId)
    {
        case InputDevice::Id::RTLSDR:
            m_settings->rtlsdr.rfLevelOffset = m_rtlSdrRfLevelCorrection;
            if (m_device)
            {
                m_device->setRfLevelOffset(m_settings->rtlsdr.rfLevelOffset);
            }
            break;
        case InputDevice::Id::RTLTCP:
            m_settings->rtltcp.rfLevelOffset = m_rtlTcpRfLevelCorrection;
            if (m_device)
            {
                m_device->setRfLevelOffset(m_settings->rtltcp.rfLevelOffset);
            }
            break;
        case InputDevice::Id::AIRSPY:
        case InputDevice::Id::SDRPLAY:
        case InputDevice::Id::SOAPYSDR:
        case InputDevice::Id::RARTTCP:
        case InputDevice::Id::UNDEFINED:
        case InputDevice::Id::RAWFILE:
            break;
    }
}

void SettingsBackend::requestRtlSdrGainMode(int modeIdx)
{
    switch (modeIdx)
    {
        case 1:
            m_settings->rtlsdr.gainMode = RtlGainMode::Driver;
            break;
        case 2:
            m_settings->rtlsdr.gainMode = RtlGainMode::Hardware;
            break;
        case 3:
            m_settings->rtlsdr.gainMode = RtlGainMode::Manual;
            break;
        default:
            m_settings->rtlsdr.gainMode = RtlGainMode::Software;
            modeIdx = 0;
            break;
    }
    rtlSdrGainMode(modeIdx);
    activateRtlSdrControls(true);
    dynamic_cast<RtlSdrInput *>(m_device)->setGainMode(m_settings->rtlsdr.gainMode, m_settings->rtlsdr.gainIdx);
}

void SettingsBackend::activateRtlSdrControls(bool en)
{
    isRtlSdrDeviceSelectionEnabled(true);
    isRtlSdrControlEnabled(en);
    isRtlSdrGainEnabled(RtlGainMode::Manual == m_settings->rtlsdr.gainMode);
}

void SettingsBackend::updateRtlSdrGainLabel()
{
    if (m_rtlsdrGainList.size() > 0)
    {
        rtlSdrGainLabel(QString("%1 dB").arg(m_rtlsdrGainList.at(m_rtlSdrGainIndex)));
    }
    else
    {
        rtlSdrGainLabel(tr("N/A"));
    }
}

void SettingsBackend::requestRtlTcpGainMode(int modeIdx)
{
    switch (modeIdx)
    {
        case 1:
            m_settings->rtltcp.gainMode = RtlGainMode::Hardware;
            break;
        case 2:
            m_settings->rtltcp.gainMode = RtlGainMode::Manual;
            break;
        default:
            m_settings->rtltcp.gainMode = RtlGainMode::Software;
            modeIdx = 0;
            break;
    }
    rtlTcpGainMode(modeIdx);
    activateRtlTcpControls(true);
    dynamic_cast<RtlTcpInput *>(m_device)->setGainMode(m_settings->rtltcp.gainMode, m_settings->rtltcp.gainIdx);
}

void SettingsBackend::activateRtlTcpControls(bool en)
{
    isRtlTcpControlEnabled(en);
    isRtlTcpGainEnabled(RtlGainMode::Manual == m_settings->rtltcp.gainMode);
}

void SettingsBackend::updateRtlTcpGainLabel()
{
    if (m_rtltcpGainList.size() > 0)
    {
        rtlTcpGainLabel(QString("%1 dB").arg(m_rtltcpGainList.at(m_rtlTcpGainIndex)));
    }
    else
    {
        rtlTcpGainLabel(tr("N/A"));
    }
}

void SettingsBackend::setStatusLabel(bool clearLabel)
{
    if (clearLabel)
    {
        statusLabel("");
    }
    else
    {
        switch (m_inputDeviceId)
        {
            case InputDevice::Id::RTLSDR:
                statusLabel(tr("RTL SDR device connected"));
                break;
            case InputDevice::Id::RTLTCP:
                statusLabel(tr("RTL TCP device connected"));
                break;
            case InputDevice::Id::UNDEFINED:
                statusLabel("<span style=\"color:red\">" + tr("No device connected") + "</span>");
                break;
            case InputDevice::Id::RAWFILE:
                statusLabel(tr("Raw file connected"));
                break;
            case InputDevice::Id::AIRSPY:
                statusLabel(tr("Airspy device connected"));
                break;
            case InputDevice::Id::SOAPYSDR:
                statusLabel(tr("Soapy SDR device connected"));
                break;
            case InputDevice::Id::RARTTCP:
                statusLabel("RART TCP device connected");
                break;
            case InputDevice::Id::SDRPLAY:
                statusLabel("SDRplay device connected");
                break;
        }
    }
}

void SettingsBackend::onCurrentDeviceChanged()
{
    auto selectedInputDevice = static_cast<InputDevice::Id>(m_inputsModel->currentData().toInt());
    switch (selectedInputDevice)
    {
        case InputDevice::Id::RTLSDR:
        {
            reloadDeviceList(InputDevice::Id::RTLSDR, m_rtlSdrDevicesModel);
            if (m_inputDeviceId != selectedInputDevice)
            {
                activateRtlSdrControls(false);
            }
        }
        break;
        case InputDevice::Id::RTLTCP:
            if (m_inputDeviceId != selectedInputDevice)
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
            reloadDeviceList(InputDevice::Id::AIRSPY, m_airspyDevicesModel);
            if (m_inputDeviceId != selectedInputDevice)
            {
                activateAirspyControls(false);
            }
#endif
        }
        break;
        case InputDevice::Id::SOAPYSDR:
#if HAVE_SOAPYSDR
            if (m_inputDeviceId != selectedInputDevice)
            {
                activateSoapySdrControls(false);
            }
#endif
            break;
        case InputDevice::Id::SDRPLAY:
#if HAVE_SOAPYSDR
            QTimer::singleShot(200, this, [this]() { reloadDeviceList(InputDevice::Id::SDRPLAY, m_sdrplayDevicesModel); });
            if (m_inputDeviceId != selectedInputDevice)
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

void SettingsBackend::selectRawFile(const QUrl &fileUrl)
{
    if (fileUrl.isEmpty())
    {
        return;
    }

    QString filePath;
#ifdef Q_OS_ANDROID
    // On Android, content:// URIs are used instead of file:// paths
    // We need to handle both cases
    if (fileUrl.scheme() == "content" || fileUrl.isLocalFile())
    {
        // For content:// URIs, use the URL string directly
        // Qt's QFile can handle content:// URIs on Android
        filePath = fileUrl.toString();
    }
    else
    {
        return;
    }
#else
    if (!fileUrl.isLocalFile())
    {
        return;
    }
    filePath = fileUrl.toLocalFile();
#endif

    rawFileName(filePath);
    if (m_rawFileName.endsWith(".s16"))
    {
        m_rawFileFormatModel->setCurrentData(int(RawFileInputFormat::SAMPLE_FORMAT_S16));
    }
    else if (m_rawFileName.endsWith(".u8"))
    {
        m_rawFileFormatModel->setCurrentData(int(RawFileInputFormat::SAMPLE_FORMAT_U8));
    }
    else
    { /* format cannot be guessed from extension - if XML header is recognized, then it will be set automatically */
    }

    setConnectButton(ConnectButtonOn);
    isRawFileFormatSelectionEnabled(true);

    // we do not know the length yet
    setRawFileLength(0);
}

void SettingsBackend::setInputDevice(InputDevice::Id id, InputDevice *device)
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
            m_device->setRfLevelOffset(m_settings->rtlsdr.rfLevelOffset);
            dynamic_cast<RtlSdrInput *>(m_device)->setGainMode(m_settings->rtlsdr.gainMode, m_settings->rtlsdr.gainIdx);
            dynamic_cast<RtlSdrInput *>(m_device)->setAgcLevelMax(m_settings->rtlsdr.agcLevelMax);
            m_settings->rtlsdr.hwId = m_device->hwId();
            connect(m_device, &InputDevice::gainIdx, this, &SettingsBackend::setRtlSdrGainIndex);
            activateRtlSdrControls(true);
            break;
        case InputDevice::Id::RTLTCP:
            setGainValues(dynamic_cast<RtlTcpInput *>(m_device)->getGainList());
            m_device->setPPM(m_settings->rtltcp.ppm);
            m_device->setRfLevelOffset(m_settings->rtltcp.rfLevelOffset);
            dynamic_cast<RtlTcpInput *>(m_device)->setGainMode(m_settings->rtltcp.gainMode, m_settings->rtltcp.gainIdx);
            dynamic_cast<RtlTcpInput *>(m_device)->setAgcLevelMax(m_settings->rtltcp.agcLevelMax);
            connect(m_device, &InputDevice::gainIdx, this, &SettingsBackend::setRtlTcpGainIndex);
            activateRtlTcpControls(true);
            break;
        case InputDevice::Id::AIRSPY:
#if HAVE_AIRSPY
            m_device->setBiasT(m_settings->airspy.biasT);
            dynamic_cast<AirspyInput *>(m_device)->setGainMode(m_settings->airspy.gain);
            m_settings->airspy.hwId = m_device->hwId();
            connect(m_device, &InputDevice::gainIdx, this,
                    [this](int idx)
                    {
                        if (m_settings->airspy.gain.mode == AirpyGainMode::Software)
                        {
                            setAirspySensitivityGainIndex(idx);
                        }
                        else if (m_settings->airspy.gain.mode == AirpyGainMode::Hybrid)
                        {
                            setAirspyIfGainIndex(idx);
                        }
                    });
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
            setSoapySdrGainModel(true);
            m_device->setBW(m_settings->soapysdr.bandwidth);
            m_device->setPPM(m_settings->soapysdr.ppm);
            activateSoapySdrControls(true);
#endif
            break;
        case InputDevice::Id::SDRPLAY:
#if HAVE_SOAPYSDR
            setGainValues(dynamic_cast<SdrPlayInput *>(m_device)->getRFGainList());
            m_settings->sdrplay.hwId = m_device->hwId();
            dynamic_cast<SdrPlayInput *>(m_device)->setGainMode(m_settings->sdrplay.gain);
            m_device->setPPM(m_settings->sdrplay.ppm);
            m_device->setBiasT(m_settings->sdrplay.biasT);
            connect(m_device, &InputDevice::gainIdx, this, &SettingsBackend::setSdrplayRfGainIndex);
            connect(dynamic_cast<SdrPlayInput *>(m_device), &SdrPlayInput::ifGain, this, &SettingsBackend::setSdrplayIfGain);
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

void SettingsBackend::resetInputDevice()
{
    // deactivate controls for all devices
    activateRtlSdrControls(false);
    activateRtlTcpControls(false);
#if HAVE_AIRSPY
    activateAirspyControls(false);
#endif
#if HAVE_SOAPYSDR
    setSoapySdrGainModel(false);
    activateSoapySdrControls(false);
    activateSdrplayControls(false);
#endif
    auto selectedInputDevice = static_cast<InputDevice::Id>(m_inputsModel->currentData().toInt());
    switch (selectedInputDevice)
    {
        case InputDevice::Id::RTLSDR:
            reloadDeviceList(InputDevice::Id::RTLSDR, m_rtlSdrDevicesModel);
            break;
        case InputDevice::Id::AIRSPY:
#if HAVE_AIRSPY
            reloadDeviceList(InputDevice::Id::AIRSPY, m_airspyDevicesModel);
#endif
            break;
        case InputDevice::Id::SDRPLAY:
#if HAVE_SOAPYSDR
            QTimer::singleShot(200, this, [this]() { reloadDeviceList(InputDevice::Id::SDRPLAY, m_sdrplayDevicesModel); });
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
    rtlSdrDeviceDesc({});
    rtlTcpDeviceDesc({});
#if HAVE_AIRSPY
    airspyDeviceDesc({});
#endif
#if HAVE_SOAPYSDR
    soapySdrDeviceDesc({});
    sdrplayDeviceDesc({});
#endif
    setConnectButton(ConnectButtonOn);
    m_tabsModel->setCurrentData(SettingsBackendTabs::Device);
}

void SettingsBackend::updateAnnouncementFlags()
{  // calculate ena flag
    uint16_t announcementEna = 0;
    for (int a = 0; a < static_cast<int>(DabAnnouncement::Undefined); ++a)
    {
        auto index = m_announcementModel->index(a, 0);
        bool isChecked = m_announcementModel->data(index, ItemModel::DataRole).toBool();
        announcementEna |= (isChecked << a);
    }
    if (m_settings->announcementEna != announcementEna)
    {
        m_settings->announcementEna = announcementEna;
        emit newAnnouncementSettings();
    }
}

void SettingsBackend::requestVisualStyle(int index)
{
    switch (index)
    {
        case 1:
            m_settings->applicationStyle = Settings::ApplicationStyle::Light;
            break;
        case 2:
            m_settings->applicationStyle = Settings::ApplicationStyle::Dark;
            break;
        default:  // system is fallback
            m_settings->applicationStyle = Settings::ApplicationStyle::Default;
            break;
    }
    applicationTheme(static_cast<int>(m_settings->applicationStyle));
    emit applicationStyleChanged();
}

void SettingsBackend::onLanguageChanged()
{
    QLocale::Language lang = static_cast<QLocale::Language>(m_languageSelectionModel->currentData().toInt());
    if (lang != m_settings->lang)
    {
        m_settings->lang = lang;
        languageChanged(true);
    }
}

void SettingsBackend::onNoiseLevelChanged()
{
    int noiseLevel = m_audioNoiseConcealModel->currentData().toInt();
    if (noiseLevel != m_settings->noiseConcealmentLevel)
    {
        m_settings->noiseConcealmentLevel = noiseLevel;
        emit noiseConcealmentLevelChanged(noiseLevel);
    }
}

void SettingsBackend::onAudioOutChanged()
{
    Settings::AudioFramework framework = static_cast<Settings::AudioFramework>(m_audioOutputModel->currentData().toInt());
    if (framework != m_settings->audioFramework)
    {
        m_settings->audioFramework = framework;
        audioOutputChanged(true);
    }
}

void SettingsBackend::onAudioDecChanged()
{
    Settings::AudioDecoder decoder = static_cast<Settings::AudioDecoder>(m_audioDecoderModel->currentData().toInt());
    if (decoder != m_settings->audioDecoder)
    {
        m_settings->audioDecoder = decoder;
        audioDecoderChanged(true);
    }
}

QUrl SettingsBackend::dataStoragePathUrl() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (!m_settings->dataStoragePath.isEmpty() && QFileInfo::exists(m_settings->dataStoragePath))
    {
        dir = QFileInfo(m_settings->dataStoragePath + '/').path();
    }
    return QUrl::fromLocalFile(dir + '/');
}

QString SettingsBackend::getDisplayPath(const QString &uriPath) const
{
    // Convert Android content:// URIs to human-friendly display paths

    // app folder: /storage/emulated/0/Android/data/org.qtproject.abracadabra/files/Documents/AbracaDABra
    // internal:    content://com.android.externalstorage.documents/document/primary%3ADownload%2Fnrk1_20250311.raw
    // sdcard:     content://com.android.externalstorage.documents/document/696C-E146%3ADownload%2Fnrk1_20250311.raw

    // Example: "content://com.android.externalstorage.documents/tree/primary%3ADocuments%2FAbracaDABra"
    // becomes: "storage:/Documents/AbracaDABra"

#ifdef Q_OS_ANDROID
    if (uriPath.startsWith("/storage/"))
    {  // not a SAF URI, but a direct path
        int idx = uriPath.indexOf("/Android");
        if (idx > 0)
        {  // this is default path on Android (hopefully on all implementations)
            // qDebug() << "Display path for URI" << uriPath << "is" << "internal:" + uriPath.mid(idx, uriPath.length() - idx);
            return "internal:" + uriPath.mid(idx, uriPath.length() - idx);
        }
        return uriPath;
    }

    QString displayPath = uriPath;

    // Decode the URL-encoded path
    displayPath = QUrl::fromPercentEncoding(displayPath.toUtf8());

    // the start of tha path shall be 'name:, first we need to find whatever is before colon using regex
    static const QRegularExpression re(R"(content://.*/([a-zA-F0-9_-]+:)(.*))");
    QRegularExpressionMatch match = re.match(displayPath);
    if (match.hasMatch())
    {
        QString storageName = match.captured(1);  // e.g., primary: or 696C-E146:
        if (storageName == "primary:")
        {
            storageName = "storage:";
        }
        QString pathPart = match.captured(2);  // rest of the path

        displayPath = storageName + '/' + pathPart;
    }

    // qDebug() << "Display path for URI" << uriPath << "is" << displayPath;

    return displayPath;
#else
    return QDir::toNativeSeparators(uriPath);
#endif
}

void SettingsBackend::selectDataStoragePath(const QUrl &dirUrl)
{
#ifdef Q_OS_ANDROID
    if (!dirUrl.isEmpty() && dirUrl.scheme() == "content")
    {
        QString path = dirUrl.toString();

        // Take persistable permission with both read and write flags
        // Qt's FolderDialog may not request write permission by default
        if (!AndroidFileHelper::takePersistablePermission(path))
        {
            qWarning() << "Failed to take persistable permission for:" << path;
            // Continue anyway - the permission may already be granted
        }

        dataStoragePath(path);

        // Helper lambda to append subdirectory to content URI with proper URL encoding
        auto appendPath = [](const QString &basePath, const QString &subdir) -> QString
        {
            // Forward slashes in path must be encoded as %2F
            QString encoded = QUrl::toPercentEncoding(subdir, "", "/");
            return basePath.endsWith('%') ? basePath + "2F" + encoded : basePath + "%2F" + encoded;
        };
    }
#else
    if (!dirUrl.isEmpty() && dirUrl.isLocalFile())
    {
        QString path = dirUrl.toLocalFile();
        dataStoragePath(path);
    }
#endif
}

void SettingsBackend::onGeolocationSourceChanged()
{
    auto src = static_cast<Settings::GeolocationSource>(m_locationSourceModel->currentData().toInt());
    m_settings->tii.locationSource = src;
    emit tiiSettingsChanged();
}

void SettingsBackend::requestTiiDbUpdate()
{
    isTiiUpdateEnabled(false);
    isTiiDbUpdating(true);

    emit updateTxDb();
}

void SettingsBackend::onTiiUpdateFinished(QNetworkReply::NetworkError err)
{
    isTiiDbUpdating(false);

    QDateTime lastModified = TxDataLoader::lastUpdateTime();
    if (err != QNetworkReply::NoError)
    {
        tiiDbLabel(tr("Update failed"));
    }
    setTiiDbUpdate();
}

void SettingsBackend::setTiiDbUpdate()
{
    QDateTime lastModified = TxDataLoader::lastUpdateTime();
    if (lastModified.isValid())
    {
        tiiDbLabel(tr("Last update: ") + lastModified.toString("dd.MM.yyyy"));
    }
    else
    {
        tiiDbLabel(tr("Data not available"));
    }
#if HAVE_FMLIST_INTERFACE
    const int updateIntervalMs = 10 * 60 * 1000;  // 10 minutes
    if (!lastModified.isValid() || lastModified.msecsTo(QDateTime::currentDateTime()) > updateIntervalMs)
    {
        isTiiUpdateEnabled(true);
    }
    else
    {
        isTiiUpdateEnabled(false);
        QTimer::singleShot(updateIntervalMs - lastModified.msecsTo(QDateTime::currentDateTime()), this, [this]() { isTiiUpdateEnabled(true); });
    }
#endif
}

void SettingsBackend::setDeviceDescription(const InputDevice::Description &desc)
{
    switch (m_inputDeviceId)
    {
        case InputDevice::Id::RTLSDR:
            rtlSdrDeviceDesc({desc.device.model, desc.device.sn, desc.device.tuner, desc.sample.channelContainer});
            reloadDeviceList(InputDevice::Id::RTLSDR, m_rtlSdrDevicesModel);
            break;
        case InputDevice::Id::RTLTCP:
            rtlTcpDeviceDesc({desc.device.model, desc.device.tuner, desc.sample.channelContainer});
            break;
        case InputDevice::Id::RAWFILE:
            if (desc.rawFile.hasXmlHeader)
            {
                rawFileXmlHeader({desc.rawFile.time, desc.rawFile.recorder, desc.device.name, desc.device.model,
                                  QString::number(desc.sample.sampleRate), QString::number(desc.rawFile.frequency_kHz),
                                  QString::number(desc.rawFile.numSamples * 1.0 / desc.sample.sampleRate), desc.sample.channelContainer});

                switch (desc.sample.containerBits)
                {
                    case 8:
                        m_rawFileFormatModel->setCurrentData(int(RawFileInputFormat::SAMPLE_FORMAT_U8));
                        break;
                    case 16:
                        m_rawFileFormatModel->setCurrentData(int(RawFileInputFormat::SAMPLE_FORMAT_S16));
                        break;
                }
                isRawFileFormatSelectionEnabled(false);
                // ui->xmlHeaderWidget->setVisible(true);
            }
            else
            {
                rawFileXmlHeader({});
                isRawFileFormatSelectionEnabled(true);
                // ui->xmlHeaderWidget->setVisible(false);
            }
            break;
        case InputDevice::Id::AIRSPY:
#if HAVE_AIRSPY
            airspyDeviceDesc({"Airspy " + desc.device.model, desc.device.sn});
            reloadDeviceList(InputDevice::Id::AIRSPY, m_airspyDevicesModel);
#endif
            break;
        case InputDevice::Id::SDRPLAY:
#if HAVE_SOAPYSDR
            sdrplayDeviceDesc({desc.device.model, desc.device.sn});
#endif
            break;
        case InputDevice::Id::SOAPYSDR:
        {
#if HAVE_SOAPYSDR
            soapySdrDeviceDesc({desc.device.model});
#endif
        }
        break;
        case InputDevice::Id::UNDEFINED:
        case InputDevice::Id::RARTTCP:
            break;
    }
}

void SettingsBackend::reloadDeviceList(const InputDevice::Id inputDeviceId, ItemModel *model)
{
    InputDeviceList list;
    QVariant currentId;
    switch (inputDeviceId)
    {
        case InputDevice::Id::RTLSDR:
        {
            list = RtlSdrInput::getDeviceList();
            currentId = m_settings->rtlsdr.hwId;
#ifdef Q_OS_WIN
            if (m_inputDeviceId == InputDevice::Id::RTLSDR)
            {  // add current device to list
                list.prepend(m_device->deviceDesc());
            }
#endif
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
#ifdef Q_OS_WIN
            if (m_inputDeviceId == InputDevice::Id::AIRSPY)
            {  // add current device to list
                list.prepend(m_device->deviceDesc());
            }
#endif
#endif
        }
        break;
        case InputDevice::Id::SOAPYSDR:
#if HAVE_SOAPYSDR
#endif
            break;
        case InputDevice::Id::SDRPLAY:
#if HAVE_SOAPYSDR
            m_sdrplayChannelModel->clear();
            m_sdrplayAntennaModel->clear();
            list = SdrPlayInput::getDeviceList();
            currentId = m_settings->sdrplay.hwId;
            isSdrplayDeviceSelectionEnabled(list.isEmpty());  // this is to enable at least reload button
#endif
            break;
            break;
        case InputDevice::Id::RARTTCP:
            break;
    }

    model->clear();
    for (auto it = list.cbegin(); it != list.cend(); ++it)
    {
        model->addItem(it->diplayName, it->id);
    }

    if (currentId.isValid())
    {
        int idx = model->findItem(currentId);
        if (idx >= 0)
        {  // found ==> select item
            model->setCurrentIndex(idx);
        }
        else
        {
            model->setCurrentIndex(0);
        }
    }
    else
    {
        model->setCurrentIndex(0);
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

void SettingsBackend::setConnectButton(SettingsBackendConnectButtonState state)
{
    switch (state)
    {
        case ConnectButtonOn:
            isConnectButtonEnabled(true);
            break;
        case ConnectButtonOff:
            isConnectButtonEnabled(false);
            break;
        case ConnectButtonAuto:
        {
            auto selectedInputDevice = static_cast<InputDevice::Id>(m_inputsModel->currentData().toInt());
            bool showButton = m_settings->inputDevice != selectedInputDevice;
            switch (selectedInputDevice)
            {
                case InputDevice::Id::RTLSDR:
                    showButton =
                        (m_rtlSdrDevicesModel->rowCount() > 0) && (showButton || (m_rtlSdrDevicesModel->currentData() != m_settings->rtlsdr.hwId));
                    break;
                case InputDevice::Id::AIRSPY:
#if HAVE_AIRSPY
                    showButton =
                        (m_airspyDevicesModel->rowCount() > 0) && (showButton || (m_airspyDevicesModel->currentData() != m_settings->airspy.hwId));
#endif
                    break;
                case InputDevice::Id::SOAPYSDR:
#if HAVE_SOAPYSDR
                    showButton = !m_soapySdrDevArgs.isEmpty() && (showButton || m_soapySdrDevArgs.trimmed() != m_settings->soapysdr.devArgs ||
                                                                  m_soapySdrAntenna.trimmed() != m_settings->soapysdr.antenna ||
                                                                  m_soapySdrChannelNum != m_settings->soapysdr.channel);

#endif
                    break;
                case InputDevice::Id::SDRPLAY:
#if HAVE_SOAPYSDR
                    showButton =
                        (m_sdrplayDevicesModel->rowCount() > 0) && (showButton || (m_sdrplayDevicesModel->currentData() != m_settings->sdrplay.hwId));
#endif
                    break;
                case InputDevice::Id::RTLTCP:
                    showButton = !m_rtlTcpIpAddress.isEmpty() && (showButton || m_rtlTcpIpAddress.trimmed() != m_settings->rtltcp.tcpAddress ||
                                                                  m_rtlTcpPort != m_settings->rtltcp.tcpPort ||
                                                                  m_isRtlTcpControlSocketChecked != m_settings->rtltcp.controlSocketEna);
                    break;
                case InputDevice::Id::RAWFILE:
                case InputDevice::Id::RARTTCP:
                case InputDevice::Id::UNDEFINED:
                    break;
            }
            isConnectButtonEnabled(showButton);
        }

        break;
    }
}

void SettingsBackend::requestApplyProxyConfig()
{
    m_settings->proxy.config = static_cast<Settings::ProxyConfig>(m_proxyConfigModel->currentData().toInt());
    if (m_settings->proxy.config == Settings::ProxyConfig::Manual)
    {
        m_settings->proxy.server = m_proxyServer.trimmed();
        m_settings->proxy.port = static_cast<uint32_t>(m_proxyPort);
        m_settings->proxy.user = m_proxyUser.trimmed();

        if (m_settings->proxy.pass != m_proxyPass)
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
            ba.append(m_proxyPass.toUtf8());
            for (int n = 4; n < ba.length(); ++n)
            {
                ba[n] = ba[n] ^ key;
            }
            m_settings->proxy.pass = ba;
        }
    }
    isProxyApplyEnabled(false);

    emit proxySettingsChanged();
}

QString SettingsBackend::locationCoordinates() const
{
    return m_locationCoordinates;
}

void SettingsBackend::setLocationCoordinates(const QString &locationCoordinates)
{
    if (m_locationCoordinates == locationCoordinates)
    {
        return;
    }
    m_locationCoordinates = locationCoordinates;

    static const QRegularExpression splitRegex("\\s*,\\s*");
    QStringList coord = locationCoordinates.split(splitRegex);
    if (coord.size() != 2)
    {
        setLocationCoordinates("0.0, 0.0");
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
        setLocationCoordinates("0.0, 0.0");
        return;
    }

    QGeoCoordinate coordinate(latitude, longitude);
    if (!coordinate.isValid())
    {
        setLocationCoordinates("0.0, 0.0");
        return;
    }

    if (m_settings->tii.coordinates != QGeoCoordinate(latitude, longitude))
    {
        m_settings->tii.coordinates = QGeoCoordinate(latitude, longitude);
        emit tiiSettingsChanged();
    }

    emit locationCoordinatesChanged();
}

void SettingsBackend::setSlsBackground(const QColor &color)
{
    if (color.isValid())
    {
        slsBackgroundColor(color);
    }
}

int SettingsBackend::rawFileProgressValue() const
{
    return m_rawFileProgressValue;
}

void SettingsBackend::setRawFileProgressValue(int msec)
{
    if (m_rawFileProgressValue == msec)
    {
        return;
    }
    m_rawFileProgressValue = msec;
    emit rawFileProgressValueChanged();
}

void SettingsBackend::updateRawFileProgressLabel()
{
    rawFileProgressLabel(
        QString("%1 / %2 " + tr("sec")).arg(rawFileProgressValue() / 1000.0, 0, 'f', 1).arg(rawFileProgressMax() / 1000.0, 0, 'f', 1));
}

bool SettingsBackend::haveRtlSdrOldDabDriver() const
{
#ifdef RTLSDR_OLD_DAB
    return true;
#else
    return false;
#endif
}

bool SettingsBackend::haveAirspy() const
{
#if HAVE_AIRSPY
    return true;
#else
    return false;
#endif
}

bool SettingsBackend::haveSoapySdr() const
{
#if HAVE_SOAPYSDR
    return true;
#else
    return false;
#endif
}

int SettingsBackend::rtlSdrGainIndex() const
{
    return m_rtlSdrGainIndex;
}

void SettingsBackend::setRtlSdrGainIndex(int rtlSdrGainIndex)
{
    if (m_rtlSdrGainIndex == rtlSdrGainIndex)
    {
        return;
    }
    m_rtlSdrGainIndex = rtlSdrGainIndex;

    if (!m_rtlsdrGainList.empty())
    {
        m_settings->rtlsdr.gainIdx = rtlSdrGainIndex;
        if (m_isRtlSdrGainEnabled)
        {  // user interaction
            dynamic_cast<RtlSdrInput *>(m_device)->setGainMode(m_settings->rtlsdr.gainMode, m_settings->rtlsdr.gainIdx);
        }
    }
    else
    { /* empy gain list => do nothing */
    }

    emit rtlSdrGainIndexChanged();
}

int SettingsBackend::rtlSdrAgcLevelThr() const
{
    return m_settings->rtlsdr.agcLevelMax;
}

void SettingsBackend::setRtlSdrAgcLevelThr(int rtlSdrAgcLevelThr)
{
    if (m_settings->rtlsdr.agcLevelMax == rtlSdrAgcLevelThr)
    {
        return;
    }
    m_settings->rtlsdr.agcLevelMax = rtlSdrAgcLevelThr;
    auto device = dynamic_cast<RtlSdrInput *>(m_device);
    if (device)
    {
        device->setAgcLevelMax(m_settings->rtlsdr.agcLevelMax);
    }
    emit rtlSdrAgcLevelThrChanged();
}

int SettingsBackend::rtlTcpGainIndex() const
{
    return m_rtlTcpGainIndex;
}

void SettingsBackend::setRtlTcpGainIndex(int rtlTcpGainIndex)
{
    if (m_rtlTcpGainIndex == rtlTcpGainIndex)
    {
        return;
    }
    m_rtlTcpGainIndex = rtlTcpGainIndex;

    if (!m_rtltcpGainList.empty())
    {
        // ui->rtltcpGainValueLabel->setText(QString("%1 dB").arg(m_rtltcpGainList.at(val)));
        m_settings->rtltcp.gainIdx = rtlTcpGainIndex;
        if (m_isRtlTcpGainEnabled)
        {  // user interaction
            dynamic_cast<RtlTcpInput *>(m_device)->setGainMode(m_settings->rtltcp.gainMode, m_settings->rtltcp.gainIdx);
        }
    }
    else
    { /* empy gain list => do nothing */
    }

    emit rtlTcpGainIndexChanged();
}

int SettingsBackend::rtlTcpAgcLevelThr() const
{
    return m_rtlTcpAgcLevelThr;
}

void SettingsBackend::setRtlTcpAgcLevelThr(int rtlTcpAgcLevelThr)
{
    if (m_rtlTcpAgcLevelThr == rtlTcpAgcLevelThr)
    {
        return;
    }
    m_rtlTcpAgcLevelThr = rtlTcpAgcLevelThr;

    m_settings->rtltcp.agcLevelMax = rtlTcpAgcLevelThr;

    auto device = dynamic_cast<RtlTcpInput *>(m_device);
    if (device)
    {
        device->setAgcLevelMax(m_settings->rtltcp.agcLevelMax);
    }

    emit rtlTcpAgcLevelThrChanged();
}

#if HAVE_AIRSPY
void SettingsBackend::activateAirspyControls(bool en)
{
    isAirspyDeviceSelectionEnabled(true);
    isAirspyControlEnabled(en);

    bool sensEna = en && (AirpyGainMode::Sensitivity == m_settings->airspy.gain.mode);
    bool manualEna = en && (AirpyGainMode::Manual == m_settings->airspy.gain.mode);

    isAirspySensitivityGainEnabled(sensEna);
    isAirspyManualGainEnabled(manualEna);
}

void SettingsBackend::requestAirspyGainMode(int modeIdx)
{  // invokable function must be always defined
    switch (modeIdx)
    {
        case 1:
            m_settings->airspy.gain.mode = AirpyGainMode::Hybrid;
            break;
        case 2:
            m_settings->airspy.gain.mode = AirpyGainMode::Sensitivity;
            break;
        case 3:
            m_settings->airspy.gain.mode = AirpyGainMode::Manual;
            break;
        default:
            m_settings->airspy.gain.mode = AirpyGainMode::Software;
            modeIdx = 0;
            break;
    }
    airspyGainMode(modeIdx);
    activateAirspyControls(true);
    dynamic_cast<AirspyInput *>(m_device)->setGainMode(m_settings->airspy.gain);
}

int SettingsBackend::airspySensitivityGainIndex() const
{
    return m_settings->airspy.gain.sensitivityGainIdx;
}

void SettingsBackend::setAirspySensitivityGainIndex(int airspySensitivityGainIndex)
{
    if (m_settings->airspy.gain.sensitivityGainIdx == airspySensitivityGainIndex)
    {
        return;
    }

    m_settings->airspy.gain.sensitivityGainIdx = airspySensitivityGainIndex;
    if (dynamic_cast<AirspyInput *>(m_device) && m_isAirspySensitivityGainEnabled)
    {
        dynamic_cast<AirspyInput *>(m_device)->setGainMode(m_settings->airspy.gain);
    }

    emit airspySensitivityGainIndexChanged();
}

int SettingsBackend::airspyIfGainIndex() const
{
    return m_settings->airspy.gain.ifGainIdx;
}

void SettingsBackend::setAirspyIfGainIndex(int airspyIfGainIndex)
{
    if (m_settings->airspy.gain.ifGainIdx == airspyIfGainIndex)
    {
        return;
    }

    m_settings->airspy.gain.ifGainIdx = airspyIfGainIndex;
    if (dynamic_cast<AirspyInput *>(m_device) && m_isAirspyManualGainEnabled)
    {
        dynamic_cast<AirspyInput *>(m_device)->setGainMode(m_settings->airspy.gain);
    }

    emit airspyIfGainIndexChanged();
}

bool SettingsBackend::isAirspyMixerAgcChecked() const
{
    return m_settings->airspy.gain.mixerAgcEna;
}

void SettingsBackend::setIsAirspyMixerAgcChecked(bool isAirspyMixerAgcChecked)
{
    if (m_settings->airspy.gain.mixerAgcEna == isAirspyMixerAgcChecked)
    {
        return;
    }

    m_settings->airspy.gain.mixerAgcEna = isAirspyMixerAgcChecked;
    dynamic_cast<AirspyInput *>(m_device)->setGainMode(m_settings->airspy.gain);

    emit isAirspyMixerAgcCheckedChanged();
}

bool SettingsBackend::isAirspyLnaAgcChecked() const
{
    return m_settings->airspy.gain.lnaAgcEna;
}

void SettingsBackend::setIsAirspyLnaAgcChecked(bool isAirspyLnaAgcChecked)
{
    if (m_settings->airspy.gain.lnaAgcEna == isAirspyLnaAgcChecked)
    {
        return;
    }
    m_settings->airspy.gain.lnaAgcEna = isAirspyLnaAgcChecked;
    dynamic_cast<AirspyInput *>(m_device)->setGainMode(m_settings->airspy.gain);

    emit isAirspyLnaAgcCheckedChanged();
}

int SettingsBackend::airspyMixerGainIndex() const
{
    return m_settings->airspy.gain.mixerGainIdx;
}

void SettingsBackend::setAirspyMixerGainIndex(int airspyMixerGainIndex)
{
    if (m_settings->airspy.gain.mixerGainIdx == airspyMixerGainIndex)
    {
        return;
    }
    m_settings->airspy.gain.mixerGainIdx = airspyMixerGainIndex;
    if (dynamic_cast<AirspyInput *>(m_device))
    {
        dynamic_cast<AirspyInput *>(m_device)->setGainMode(m_settings->airspy.gain);
    }
    emit airspyMixerGainIndexChanged();
}

int SettingsBackend::airspyLnaGainIndex() const
{
    return m_settings->airspy.gain.lnaGainIdx;
}

void SettingsBackend::setAirspyLnaGainIndex(int airspyLnaGainIndex)
{
    if (m_settings->airspy.gain.lnaGainIdx == airspyLnaGainIndex)
    {
        return;
    }
    m_settings->airspy.gain.lnaGainIdx = airspyLnaGainIndex;
    if (dynamic_cast<AirspyInput *>(m_device))
    {
        dynamic_cast<AirspyInput *>(m_device)->setGainMode(m_settings->airspy.gain);
    }

    emit airspyLnaGainIndexChanged();
}
#endif  // HAVE_AIRSPY

#if HAVE_SOAPYSDR
void SettingsBackend::updateSdrplayRfGainLabel()
{
    if (m_sdrplayGainList.size() > 0)
    {
        sdrplayRfGainLabel(QString("%1 dB").arg(m_sdrplayGainList.at(m_settings->sdrplay.gain.rfGain)));
    }
    else
    {
        sdrplayRfGainLabel(tr("N/A"));
    }
}

void SettingsBackend::requestSdrplayGainMode(int modeIdx)
{
    if (modeIdx == 1)
    {
        m_settings->sdrplay.gain.mode = SdrPlayGainMode::Manual;
    }
    else
    {
        m_settings->sdrplay.gain.mode = SdrPlayGainMode::Software;
        modeIdx = 0;
    }
    sdrplayGainMode(modeIdx);
    activateSdrplayControls(true);
    dynamic_cast<SdrPlayInput *>(m_device)->setGainMode(m_settings->sdrplay.gain);
}

void SettingsBackend::requestSoapySdrGainMode(int modeIdx)
{
    if (modeIdx == 1)
    {
        m_settings->soapysdr.gainMap[m_settings->soapysdr.driver].mode = SoapyGainMode::Manual;
    }
    else
    {
        m_settings->soapysdr.gainMap[m_settings->soapysdr.driver].mode = SoapyGainMode::Hardware;
        modeIdx = 0;
    }
    soapySdrGainMode(modeIdx);
    activateSoapySdrControls(true);
    dynamic_cast<SoapySdrInput *>(m_device)->setGainMode(m_settings->soapysdr.gainMap[m_settings->soapysdr.driver]);
}

void SettingsBackend::setSoapySdrGainModel(bool activate)
{
    m_soapySdrGainModel->clear();
    if (activate)
    {
        auto gains = dynamic_cast<SoapySdrInput *>(m_device)->getGains();
        if (!gains->empty())
        {
            m_soapySdrGainModel->clear();
            for (int row = 0; row < gains->count(); ++row)
            {
                m_soapySdrGainModel->addItem(gains->at(row).first, m_settings->soapysdr.gainMap[m_settings->soapysdr.driver].gainList[row],
                                             gains->at(row).second.minimum(), gains->at(row).second.maximum(),
                                             gains->at(row).second.step() != 0 ? gains->at(row).second.step() : 1);

                dynamic_cast<SoapySdrInput *>(m_device)->setGain(row, m_settings->soapysdr.gainMap[m_settings->soapysdr.driver].gainList[row]);
            }
        }
    }
}

void SettingsBackend::activateSoapySdrControls(bool en)
{
    isSoapySdrControlEnabled(en);
    isSoapySdrGainEnabled(SoapyGainMode::Manual == m_settings->soapysdr.gainMap[m_settings->soapysdr.driver].mode);
}

void SettingsBackend::onSdrplayDeviceChanged()
{
    m_sdrplayChannelModel->clear();
    if (m_sdrplayDevicesModel->currentIndex() >= 0)
    {  // timer is needed to avoid GUI blocking
        QTimer::singleShot(100, this,
                           [this]()
                           {
                               int numCh = SdrPlayInput::getNumRxChannels(m_sdrplayDevicesModel->currentData());
                               for (int n = 0; n < numCh; ++n)
                               {
                                   m_sdrplayChannelModel->addItem(QString::number(n), QVariant(n));
                               }
                               m_sdrplayChannelModel->setCurrentIndex(numCh > 0 ? 0 : -1);
                               isSdrplayDeviceSelectionEnabled(numCh == 0);  // release reload button
                           });
    }
}

void SettingsBackend::activateSdrplayControls(bool en)
{
    isSdrplayDeviceConnected(en);
    isSdrplayControlEnabled(en);
}

void SettingsBackend::sdrplayReloadOrDisconnectRequest()
{
    isSdrplayDeviceSelectionEnabled(false);
    if (m_inputDeviceId != InputDevice::Id::SDRPLAY)
    {
        // ui->sdrplayReloadButton->setEnabled(false);
        reloadDeviceList(InputDevice::Id::SDRPLAY, m_sdrplayDevicesModel);
    }
    else
    {  // sdrplay is connected now -> need to disconnect
        resetInputDevice();
        m_inputDeviceId = InputDevice::Id::UNDEFINED;
        emit inputDeviceChanged(InputDevice::Id::UNDEFINED, m_settings->sdrplay.hwId);
        // ui->sdrplayReloadButton->setEnabled(false);
    }
}

void SettingsBackend::onSdrplayChannelChanged()
{
    m_sdrplayAntennaModel->clear();
    if (m_sdrplayChannelModel->currentIndex() >= 0)
    {  // timer is needed to avoid GUI blocking
        QTimer::singleShot(100, this,
                           [this]()
                           {
                               QStringList ant =
                                   SdrPlayInput::getRxAntennas(m_sdrplayDevicesModel->currentData(), m_sdrplayChannelModel->currentData().toInt());
                               for (const auto &a : std::as_const(ant))
                               {
                                   m_sdrplayAntennaModel->addItem(a, a);
                               }
                               m_sdrplayAntennaModel->setCurrentIndex(ant.size() > 0 ? 0 : -1);
                               isSdrplayDeviceSelectionEnabled(true);
                               setConnectButton(ConnectButtonAuto);
                           });
    }
}

void SettingsBackend::onSdrplayAntennaChanged()
{
    QString ant = m_sdrplayAntennaModel->currentData().toString();
    if (m_inputDeviceId == InputDevice::Id::SDRPLAY && m_settings->sdrplay.antenna != ant)
    {
        if (dynamic_cast<SdrPlayInput *>(m_device))
        {
            m_settings->sdrplay.antenna = ant;
            dynamic_cast<SdrPlayInput *>(m_device)->setAntenna(ant);
        }
    }
}

bool SettingsBackend::isSdrplayIfAgcChecked() const
{
    return m_settings->sdrplay.gain.ifAgcEna;
}

void SettingsBackend::setIsSdrplayIfAgcChecked(bool isSdrplayIfAgcChecked)
{
    if (m_settings->sdrplay.gain.ifAgcEna == isSdrplayIfAgcChecked)
    {
        return;
    }
    m_settings->sdrplay.gain.ifAgcEna = isSdrplayIfAgcChecked;
    dynamic_cast<SdrPlayInput *>(m_device)->setGainMode(m_settings->sdrplay.gain);

    emit isSdrplayIfAgcCheckedChanged();
}

int SettingsBackend::sdrplayRfGainIndex() const
{
    return m_settings->sdrplay.gain.rfGain;
}

void SettingsBackend::setSdrplayRfGainIndex(int sdrplayRfGainIndex)
{
    if (m_settings->sdrplay.gain.rfGain == sdrplayRfGainIndex)
    {
        return;
    }
    m_settings->sdrplay.gain.rfGain = sdrplayRfGainIndex;
    if (m_settings->sdrplay.gain.mode == SdrPlayGainMode::Manual && dynamic_cast<SdrPlayInput *>(m_device))
    {  // user interaction
        QTimer::singleShot(1, this, [this]() { dynamic_cast<SdrPlayInput *>(m_device)->setGainMode(m_settings->sdrplay.gain); });
    }
    emit sdrplayRfGainIndexChanged();
}

int SettingsBackend::sdrplayIfGain() const
{
    return m_settings->sdrplay.gain.ifGain;
}

void SettingsBackend::setSdrplayIfGain(int sdrplayIfGain)
{
    if (m_settings->sdrplay.gain.ifGain == sdrplayIfGain)
    {
        return;
    }
    m_settings->sdrplay.gain.ifGain = sdrplayIfGain;
    if (m_settings->sdrplay.gain.mode == SdrPlayGainMode::Manual && !isSdrplayIfAgcChecked() && dynamic_cast<SdrPlayInput *>(m_device))
    {  // user interaction
        QTimer::singleShot(1, this, [this]() { dynamic_cast<SdrPlayInput *>(m_device)->setGainMode(m_settings->sdrplay.gain); });
    }

    emit sdrplayIfGainChanged();
}
#endif  // HAVE_SOAPYSDR
