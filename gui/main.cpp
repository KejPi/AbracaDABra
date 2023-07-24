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

#include <QApplication>
#include <QCommandLineParser>
#include <QTranslator>
#include <QLibraryInfo>
#include "mainwindow.h"
#include "version.h"

int main(int argc, char *argv[])
{

    QCoreApplication::setApplicationName("AbracaDABra");
    QCoreApplication::setApplicationVersion(ABRACADABRA_VERSION);

    QApplication a(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("Abraca DAB radio: DAB/DAB+ Software Defined Radio (SDR)"));
    parser.addHelpOption();
    parser.addVersionOption();

    // An option with a value
    QCommandLineOption iniFileOption(QStringList() << "i" << "ini",
                                     QObject::tr("Optional INI file. If not specified AbracaDABra.ini in system directory will be used."), "ini");
    parser.addOption(iniFileOption);

    // Process the actual command line arguments given by the user
    parser.process(a);

    QString iniFile = parser.value(iniFileOption);

#ifdef Q_OS_LINUX
    // Set icon
    a.setWindowIcon(QIcon(":/resources/appIcon-linux.png"));
#endif

    // loading of translation
    // use system default or user selected
    QSettings * settings;
    if (iniFile.isEmpty())
    {
        settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::applicationName(), QCoreApplication::applicationName());
    }
    else
    {
        settings = new QSettings(iniFile, QSettings::IniFormat);
    }

    QLocale::Language lang = QLocale::codeToLanguage(settings->value("language", QString("")).toString());

    delete settings;

    QTranslator translator;
    if (QLocale::AnyLanguage == lang)
    {   // system default
        if (translator.load(QLocale(), QLatin1String("AbracaDABra"), QLatin1String("_"), QLatin1String(":/i18n")))
        {
            a.installTranslator(&translator);
        }
    }
    else if (QLocale::English != lang)
    {   // user selected translation
        if (translator.load(QString("AbracaDABra_%1").arg(QLocale::languageToCode(lang)), QLatin1String(":/i18n")))
        {
            a.installTranslator(&translator);
        }
    }

    MainWindow w(iniFile);
    w.show();        
    return a.exec();
}
