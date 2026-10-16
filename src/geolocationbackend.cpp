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

#include "geolocationbackend.h"

#if QT_CONFIG(permissions)
#include <QPermissions>
#endif
#include <QCoreApplication>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(geolocation, "geolocation", QtInfoMsg)

GeolocationBackend::GeolocationBackend(Settings *settings, QObject *parent) : QObject{parent}, m_settings(settings)
{}

void GeolocationBackend::doLocationUpdate(bool ena)
{
    m_locUpdateCounter += ena ? 1 : -1;
    // qDebug() << "GeolocationBackend::doLocationUpdate" << ena << "counter:" << m_locUpdateCounter;

    if (ena && m_locUpdateCounter == 1)
    {  // start location update
        initGeolocationSource();
    }
    else if (m_locUpdateCounter <= 0)
    {  // stop location update
        m_locUpdateCounter = 0;
        if (m_geopositionSource != nullptr)
        {
            m_geopositionSource->stopUpdates();
        }
    }
}

void GeolocationBackend::onSettingsChanged()
{
    if (m_geopositionSource != nullptr)
    {
        delete m_geopositionSource;
        m_geopositionSource = nullptr;
    }
    if (m_locUpdateCounter > 0)
    {
        initGeolocationSource();
    }
}

void GeolocationBackend::initGeolocationSource()
{
    switch (m_settings->tii.locationSource)
    {
        case Settings::GeolocationSource::System:
        {
            // ask for permission
#if QT_CONFIG(permissions)
            QLocationPermission locationsPermission;
            locationsPermission.setAccuracy(QLocationPermission::Precise);
            locationsPermission.setAvailability(QLocationPermission::WhenInUse);
            switch (qApp->checkPermission(locationsPermission))
            {
                case Qt::PermissionStatus::Undetermined:
                    qApp->requestPermission(locationsPermission, this, [this]() { initGeolocationSource(); });
                    qCDebug(geolocation) << "LocationPermission Undetermined";
                    return;
                case Qt::PermissionStatus::Denied:
                {
                    qCInfo(geolocation) << "LocationPermission Denied";
                }
                    return;
                case Qt::PermissionStatus::Granted:
                    qCInfo(geolocation) << "LocationPermission Granted";
                    break;  // Proceed
            }
#endif
            // start location update
            if (m_geopositionSource == nullptr)
            {
                m_geopositionSource = QGeoPositionInfoSource::createDefaultSource(this);
            }
            if (m_geopositionSource != nullptr)
            {
                qCDebug(geolocation) << "Start position update";
                connect(m_geopositionSource, &QGeoPositionInfoSource::positionUpdated, this, &GeolocationBackend::onSourcePositionUpdated);
                m_geopositionSource->startUpdates();
                qCInfo(geolocation) << "Location source system";
            }
            else
            {
                qCWarning(geolocation) << "Failed to create location source: system";
            }
        }
        break;
        case Settings::GeolocationSource::Manual:
            if (m_geopositionSource != nullptr)
            {
                delete m_geopositionSource;
                m_geopositionSource = nullptr;
            }
            m_currentPosition = m_settings->tii.coordinates;
            m_currentPosition.setAltitude(m_settings->tii.altitude);
            emit positionUpdated(QGeoPositionInfo(m_currentPosition, QDateTime::currentDateTime()));
            qCInfo(geolocation) << "Location source manual: latitude" << m_currentPosition.latitude() << "| longitude"
                                << m_currentPosition.longitude();
            break;
        case Settings::GeolocationSource::SerialPort:
        {
            // serial port
            QVariantMap params;
            params["nmea.source"] = "serial:" + m_settings->tii.serialPort;
            params["nmea.baudrate"] = m_settings->tii.serialPortBaudrate;
            m_geopositionSource = QGeoPositionInfoSource::createSource("nmea", params, this);
            if (m_geopositionSource != nullptr)
            {
                qCDebug(geolocation) << "Start position update";
                connect(m_geopositionSource, &QGeoPositionInfoSource::positionUpdated, this, &GeolocationBackend::onSourcePositionUpdated);
                m_geopositionSource->startUpdates();
                qCInfo(geolocation) << "Location source serial port" << m_settings->tii.serialPort << "@" << m_settings->tii.serialPortBaudrate;
            }
            else
            {
                qCWarning(geolocation) << "Failed to create location source: serial port" << m_settings->tii.serialPort << "@"
                                       << m_settings->tii.serialPortBaudrate;
            }
        }
        break;
    }
}

void GeolocationBackend::onSourcePositionUpdated(const QGeoPositionInfo &info)
{
    m_currentPosition = info.coordinate();
    if (m_settings->tii.manualAltitude)
    {  // modify altitude to the one set in settings
        m_currentPosition.setAltitude(m_settings->tii.altitude);
        QGeoPositionInfo infoWithManualAltitude = info;
        infoWithManualAltitude.setCoordinate(m_currentPosition);
        emit positionUpdated(infoWithManualAltitude);
        return;
    }
    emit positionUpdated(info);
}
