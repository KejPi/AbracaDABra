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

#include <QApplication>
#include <QByteArrayView>
#include <QCommandLineParser>
#include <QLibraryInfo>
#include <QTranslator>

#include "config.h"
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName("AbracaDABra");
    QCoreApplication::setApplicationVersion(PROJECT_VER);
    int currentExitCode = 0;
    do
    {
        QApplication a(argc, argv);

        // this is required for correct deployment of SDRplay
#ifdef Q_OS_MACOS
        qputenv("SOAPY_SDR_PLUGIN_PATH", QByteArrayView(QString(QCoreApplication::applicationDirPath() + "/../Plugins/SoapySDR").toUtf8()));
#endif
#ifdef Q_OS_WIN
        qputenv("SOAPY_SDR_PLUGIN_PATH", QByteArrayView(QString(QCoreApplication::applicationDirPath()).toUtf8()));
#endif
        // qDebug() << qgetenv("SOAPY_SDR_PLUGIN_PATH");

        QCommandLineParser parser;
        parser.setApplicationDescription(QObject::tr("Abraca DAB radio: DAB/DAB+ Software Defined Radio (SDR)"));
        parser.addHelpOption();
        parser.addVersionOption();

        // An option with a value
        QCommandLineOption iniFileOption(QStringList() << "i" << "ini",
                                         QObject::tr("Optional INI file. If not specified AbracaDABra.ini in system directory will be used."), "ini");
        parser.addOption(iniFileOption);

        QCommandLineOption slFileOption(
            QStringList() << "s" << "service-list",
            QObject::tr("Optional service list JSON file. If not specified ServiceList.json in system directory will be used."), "json");

        parser.addOption(slFileOption);

        // Process the actual command line arguments given by the user
        parser.process(a);

        QString iniFile = parser.value(iniFileOption);
        QString slFile = parser.value(slFileOption);

#ifdef Q_OS_LINUX
        // Set icon
        a.setWindowIcon(QIcon(":/resources/appIcon-linux.png"));
#endif

        // loading of translation
        // use system default or user selected
        QSettings *settings;
        if (iniFile.isEmpty())
        {
            settings =
                new QSettings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::applicationName(), QCoreApplication::applicationName());
        }
        else
        {
            settings = new QSettings(iniFile, QSettings::IniFormat);
        }

        QLocale::Language lang = QLocale::codeToLanguage(settings->value("language", QString("")).toString());

        delete settings;

        // QString filename = QLatin1String(":/i18n") + "/"  + QString("AbracaDABra_%1").arg(QLocale::languageToCode(lang))+".qm";
        // QDirIterator it(":", QDirIterator::Subdirectories);
        // while (it.hasNext()) {
        //     qDebug() << it.next();
        // }
        QTranslator translator;
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0) && QT_VERSION < QT_VERSION_CHECK(6, 7, 2)
        if (QLocale::AnyLanguage == lang)
        {  // system default
            if (translator.load(QLocale(), QLatin1String("AbracaDABra"), QLatin1String("_"), QLatin1String(":/i18n/gui")))
            {
                a.installTranslator(&translator);
            }
        }
        else if (QLocale::English != lang)
        {  // user selected translation
            if (translator.load(QString("AbracaDABra_%1").arg(QLocale::languageToCode(lang)), QLatin1String(":/i18n/gui")))
            {
                a.installTranslator(&translator);
            }
        }
#else
        if (QLocale::AnyLanguage == lang)
        {  // system default
            if (translator.load(QLocale(), QLatin1String("AbracaDABra"), QLatin1String("_"), QLatin1String(":/i18n")))
            {
                a.installTranslator(&translator);
            }
        }
        else if (QLocale::English != lang)
        {  // user selected translation
            if (translator.load(QString("AbracaDABra_%1").arg(QLocale::languageToCode(lang)), QLatin1String(":/i18n")))
            {
                a.installTranslator(&translator);
            }
        }
#endif
        MainWindow w(iniFile, slFile);
        w.show();
        currentExitCode = a.exec();
    } while (currentExitCode == MainWindow::EXIT_CODE_RESTART);

    return currentExitCode;
}
