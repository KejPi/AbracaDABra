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

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QAbstractItemModel>
#include <QColor>
#include <QDateTime>
#include <QGeoCoordinate>
#include <QLocale>

#include "config.h"
#include "inputdevice.h"
#include "rawfileinput.h"

#define ENSEMBLE_DIR_NAME "ensemble"
#define SCANNER_DIR_NAME "scanner"
#define TII_DIR_NAME "tii"
#define SLIDES_DIR_NAME "slides"
#define UA_DIR_NAME "userApps"
#define AUDIO_DIR_NAME "audio"
#define RAW_DIR_NAME "raw"

#if HAVE_AIRSPY
#include "airspyinput.h"
#endif
#if HAVE_SOAPYSDR
#include "sdrplayinput.h"
#include "soapysdrinput.h"
#endif

class Settings
{
public:
    enum class ApplicationStyle
    {
        Default = 0,
        Light,
        Dark
    };
    enum class GeolocationSource
    {
        System = 0,
        Manual,
        SerialPort
    };
    enum class ProxyConfig
    {
        NoProxy = 0,
        System,
        Manual
    };
    enum AudioFramework
    {
        Pa = 0,
        Qt = 1
    };
    enum AudioDecoder
    {
        FAAD = 0,
        FDKAAC = 1
    };

    Settings() {};

    QString filePath;
    InputDevice::Id inputDevice;
    struct
    {
        QString file;
        RawFileInputFormat format;
        bool loopEna;
    } rawfile;
    struct
    {
        QVariant hwId;
        bool fallbackConnection;
        RtlGainMode gainMode;
        int gainIdx;
        uint32_t bandwidth;
        bool biasT;
        int agcLevelMax;
        int ppm;
        float rfLevelOffset;
        bool rfLevelEna;
    } rtlsdr;
    struct
    {
        RtlGainMode gainMode;
        int gainIdx;
        QString tcpAddress;
        int tcpPort;
        bool controlSocketEna;
        int agcLevelMax;
        int ppm;
        float rfLevelOffset;
    } rtltcp;
#if HAVE_AIRSPY
    struct
    {
        QVariant hwId;
        bool fallbackConnection;
        AirspyGainStruct gain;
        bool biasT;
        bool dataPacking;
        bool prefer4096kHz;
    } airspy;
#endif
#if HAVE_SOAPYSDR
    struct
    {
        QString devArgs;
        QString antenna;
        int channel;
        uint32_t bandwidth;
        int ppm;
        QString driver;
        QHash<QString, SoapyGainStruct> gainMap;
    } soapysdr;
    struct
    {
        QVariant hwId;
        bool fallbackConnection;
        QString antenna;
        int channel;
        bool biasT;
        int ppm;
        SdrPlayGainStruct gain;
    } sdrplay;
#endif
#if HAVE_RARTTCP
    struct
    {
        QString tcpAddress;
        int tcpPort;
    } rarttcp;
#endif
    uint16_t announcementEna;
    bool bringWindowToForeground;
    ApplicationStyle applicationStyle;
    QLocale::Language lang;
    AudioFramework audioFramework;
    AudioDecoder audioDecoder;
    bool keepServiceListOnScan;
    bool dlPlusEna;
    int noiseConcealmentLevel;
    bool xmlHeaderEna;
    bool spiAppEna;
    bool spiProgressEna;
    bool spiProgressHideComplete;
    bool useInternet;
    bool radioDnsEna;
    bool trayIconEna;
    bool restoreWindows;
    QColor slsBackground = Qt::red;
    bool updateCheckEna;
    QDateTime updateCheckTime;
    bool uploadEnsembleInfo;
    bool showSystemTime;
    bool showEnsFlag;
    bool showServiceFlag;
    bool compactUi;
    bool cableChannelsEna;
    QString dataStoragePath;
    bool keepScreenOn;  // Keep screen on (Android only)

    struct
    {
        int x;
        int y;
        int width;
        int height;
        bool fullscreen;
    } appWindow;

    // audio recording settings
    struct AudioRec
    {
        bool captureOutput;
        bool autoStopEna;
        bool dl;
        bool dlAbsTime;
    } audioRec;

    // this is settings for UA data dumping (storage)
    struct UADumpSettings
    {
        QString dataStoragePath;  // duplicates Settings::dataStoragePath
        bool overwriteEna;
        bool slsEna;
        bool spiEna;
        QString slsPattern;
        QString spiPattern;
    } uaDump;

    struct TIISettings
    {
        GeolocationSource locationSource;
        QGeoCoordinate coordinates;
        QString serialPort;
        int serialPortBaudrate;
        bool showSpectumPlot;
        bool restore;
        QVariant splitterState;
        int mode;
        bool showInactiveTx;
        bool inactiveTxTimeoutEna;
        int inactiveTxTimeout;
        bool timestampInUTC;
        bool saveCoordinates;
        bool centerMapToCurrentPosition;
        QGeoCoordinate mapCenter;
        float mapZoom;
        struct TxTableSettings
        {
            struct TxTableCol
            {
                bool enabled;
                int order;
            };

            TxTableCol code;
            TxTableCol level;
            TxTableCol location;
            TxTableCol power;
            TxTableCol dist;
            TxTableCol azimuth;
        } txTable;
    } tii;
    struct SignalDialog
    {
        QVariant splitterState;
        bool restore;
        bool showSNR;
        int spectrumMode;
        int spectrumUpdate;
    } signal;
    struct EPGSettings
    {
        bool filterEmptyEpg;
        bool filterEnsemble;
        QPersistentModelIndex selectedItem;
        QVariant splitterState;
        bool restore;
    } epg;
    struct Proxy
    {
        ProxyConfig config;
        QString server;
        uint32_t port;
        QString user;
        QByteArray pass;
    } proxy;
    struct EnsembleInfo
    {
        bool restore;
        bool recordingTimeoutEna;
        int recordingTimeoutSec;
    } ensembleInfo;
    struct SetupDialog
    {
        bool restore;
    } setupDialog;
    struct Log
    {
        bool restore;
    } log;
    struct CatSLS
    {
        bool restore;
    } catSls;
    struct ScannerSettings
    {
        QVariant splitterState;
        bool restore;
        QMap<uint32_t, bool> channelSelection;
        int mode;
        int numCycles;
        int waitForSync;
        int waitForEnsemble;
        bool clearOnStart;
        bool hideLocalTx;
        bool autoSave;
        bool centerMapToCurrentPosition;
        QGeoCoordinate mapCenter;
        float mapZoom;
    } scanner;
};

#endif  // SETTINGS_H
