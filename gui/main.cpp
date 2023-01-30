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

#include "mainwindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include "config.h"

int main(int argc, char *argv[])
{

    QCoreApplication::setApplicationName("AbracaDABra");
    QCoreApplication::setApplicationVersion(PROJECT_VER);

    QApplication a(argc, argv);


    QCommandLineParser parser;
    parser.setApplicationDescription("Abraca DAB radio: DAB/DAB+ Software Defined Radio (SDR)");
    parser.addHelpOption();
    parser.addVersionOption();

    // An option with a value
    QCommandLineOption iniFileOption(QStringList() << "i" << "ini",
            "Optional INI file. If not specified AbracaDABra.ini in system directory will be used.", "ini");
    parser.addOption(iniFileOption);

    // Process the actual command line arguments given by the user
    parser.process(a);

    QString iniFile = parser.value(iniFileOption);

#ifdef Q_OS_LINUX
    // Set icon
    a.setWindowIcon(QIcon(":/resources/appIcon-linux.png"));
#endif

    MainWindow w(iniFile);
    w.show();        
    return a.exec();
}
