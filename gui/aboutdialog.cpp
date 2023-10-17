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

#include <QDesktopServices>
#include <rtl-sdr.h>
#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include "config.h"
#include "dabsdr.h"

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    ui->appName->setText("<b>Abraca DAB radio</b>");
    ui->author->setText(tr("Developed by")+" Petr Kopecký (<a href=\"mailto:xkejpi@gmail.com\">xkejpi@gmail.com</a>)");

    dabsdrVersion_t dabsdrVer = {0};
    dabsdrGetVersion(&dabsdrVer);

    ui->version->setText(QString("Version %1 (%2)").arg(PROJECT_VER,"<a href=\"https://github.com/KejPi/AbracaDABra\">GitHub</a>"));
    ui->qtVersion->setText(QString(tr("Based on Qt %1")).arg(QT_VERSION_STR));
    ui->dabsdrVersion->setText(QString(tr("DAB SDR version %1.%2.%3")).arg(dabsdrVer.major).arg(dabsdrVer.minor).arg(dabsdrVer.patch));

    ui->libraries->setText(tr("AbracaDABra & DAB SDR library use following libraries (special thanks to):")+
                           "<ul>"
                           "<li><a href=\"https://github.com/anthonix/ffts\">FFTS</a> by Anthony Blake</li>"
                           "<li><a href=\"https://github.com/mborgerding/kissfft\">KISS FFT</a> by Mark Borgerding</li>"
                           "<li><a href=\"https://github.com/Opendigitalradio/ka9q-fec\">FEC</a> by Phil Karn, KA9Q</li>"
#ifdef RTLSDR_OLD_DAB
                           "<li><a href=\"https://github.com/old-dab/rtlsdr\">rtlsdr</a> by Steve Markgraf, Dimitri Stolnikov, and Hoernchen, with contributions by Kyle Keen, Christian Vogel, Harald Welte and Hayati Ayguen.</li>"
#else
                           "<li><a href=\"https://osmocom.org/projects/rtl-sdr/wiki/rtl-sdr\">rtl-sdr</a> by Steve Markgraf, Dimitri Stolnikov, and Hoernchen, with contributions by Kyle Keen, Christian Vogel and Harald Welte.</li>"
#endif
#ifdef HAVE_AIRSPY
                           "<li><a href=\"https://github.com/airspy\">AirSpy</a> by Benjamin Vernoux and Youssef Touil.</li>"
#endif
#ifdef HAVE_SOAPYSDR
                           "<li><a href=\"https://github.com/pothosware/SoapySDR/wiki\">SoapySDR</a> by Pothos.</li>"
#endif

                           "<li><a href=\"https://www.mpg123.de\">mpg123</a> by Michael Hipp, Thomas Orgis and others</li>"
#if HAVE_FDKAAC
                           "<li><a href=\"https://github.com/mstorsjo/fdk-aac\">fdk-aac</a> Copyright © 1995 - 2018 Fraunhofer-Gesellschaft "
                           "zur Förderung der angewandten Forschung e.V.</li>"
#else
                           "<li><a href=\"https://github.com/knik0/faad2\">FAAD2</a> Copyright © 2003-2005 M. Bakker, Nero AG</li>"
#endif
#if HAVE_PORTAUDIO
                           "<li><a href=\"http://www.portaudio.com\">PortAudio</a> Copyright © 1999-2011 Ross Bencina and Phil Burk</li>"
#endif
                           "</ul>"+
                           tr("Contributors to the translation:")+
                           "<ul>"
                           "<li>"+tr("Czech") +": Petr Kopecký</li>"
                           "<li>"+tr("German")+": Andreas Mikula</li>"
                           "<li>"+tr("Polish")+": Marek Schirmer</li>"
                           "</ul>"+
                           tr("AbracaDABra uses some artwork created by")+" <a href=\"https://www.flaticon.com/authors/basic-miscellany/lineal-color\">Smashicons - Flaticon</a>");
    ui->disclaimer->setText("<p>Copyright © 2019-2023 Petr Kopecký</p>"
                            "<p>Permission is hereby granted, free of charge, to any person obtaining a copy of this software "
                            "and associated documentation files (the “Software”), to deal in the Software without restriction, "
                            "including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, "
                            "and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, "
                            "subject to the following conditions: </p>"
                            "The above copyright notice and this permission notice shall be included in all copies or substantial "
                            "portions of the Software.</p>"
                            "<p>THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING "
                            "BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. "
                            "IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, "
                            "WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH "
                            "THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.");

    QObject::connect(
        ui->version, &QLabel::linkActivated,
        [=]( const QString & link ) { QDesktopServices::openUrl(QUrl::fromUserInput(link)); }
        );

    QObject::connect(
                ui->author, &QLabel::linkActivated,
                [=]( const QString & link ) { QDesktopServices::openUrl(QUrl::fromUserInput(link)); }
            );
    QObject::connect(
                ui->libraries, &QLabel::linkActivated,
                [=]( const QString & link ) { QDesktopServices::openUrl(QUrl::fromUserInput(link)); }
            );

    //resize(minimumSizeHint());
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

