#include "aboutdialog.h"
#include "ui_aboutdialog.h"

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    ui->appName->setText("<b>Abraca DAB radio</b>");
    //ui->author->setText("Developed by <a href=\"mailto:xkejpi@gmail.com\">Petr Kopecky</a>");
    ui->author->setText("Developed by Petr Kopecky (<a href=\"mailto:xkejpi@gmail.com\">xkejpi@gmail.com</a>)");
    ui->libraries->setText("AbracaDABra links following libraries (special thanks to):"
                           "<ul>"
                           "<li><a href=\"https://github.com/anthonix/ffts\">FFTS</a> by Anthony Blake</li>"
                           "<li><a href=\"https://github.com/anthonix/ffts\">KISS FFT</a> by Mark Borgerding</li>"
                           "<li><a href=\"https://github.com/Opendigitalradio/ka9q-fec\">FEC</a> by Phil Karn, KA9Q</li>"
                           "<li><a href=\"https://osmocom.org/projects/rtl-sdr/wiki/Rtl-sdr\">rtl-sdr</a> by Steve Markgraf, Dimitri Stolnikov, and Hoernchen, with contributions by Kyle Keen, Christian Vogel and Harald Welte.</li>"
                           "<li><a href=\"https://www.mpg123.dec\">mpg123</a> by Michael Hipp, Thomas Orgis and others</li>"
                           "<li><a href=\"https://github.com/knik0/faad2\">FAAD2</a> Copyright (C) 2003-2005 M. Bakker, Nero AG</li>"
                           "<li><a href=\"http://www.portaudio.com\">PortAudio</a> Copyright (c) 1999-2011 Ross Bencina and Phil Burk</li>"
                           "</ul>");
    ui->disclaimer->setText("<p>Copyright © 2019-2022 Petr Kopecký</p>"
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
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

