#include <QFileDialog>
#include <QDateTime>
#include <QDebug>
#include <QMenu>

#include "ensembleinfodialog.h"
#include "ui_ensembleinfodialog.h"

#include "inputdevice.h"   // needed for AGC gain

EnsembleInfoDialog::EnsembleInfoDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EnsembleInfoDialog)
{
    ui->setupUi(this);

    // remove question mark from titlebar
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    dumpPath = QDir::homePath();

    clearFreqInfo();
    clearSignalInfo();
    clearServiceInfo();
    resetFibStat();
    resetMscStat();

    ui->FIBframe->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->FIBframe, &QWidget::customContextMenuRequested, this, &EnsembleInfoDialog::fibFrameContextMenu);

    enableDumpToFile(false);
}

EnsembleInfoDialog::~EnsembleInfoDialog()
{
    delete ui;
}

void EnsembleInfoDialog::refreshEnsembleConfiguration(const QString & txt)
{
    ui->ensStructureTextEdit->setHtml(txt);

    if (txt.isEmpty())
    {  // empty ensemble configuration means tuning to new frequency
       clearSignalInfo();
       clearServiceInfo();

        resetFibStat();
        resetMscStat();
    }
}

void EnsembleInfoDialog::updateSnr(float snr)
{
    ui->snr->setText(QString("%1 dB").arg(snr, 0, 'f', 1));
}

void EnsembleInfoDialog::updateFreqOffset(float offset)
{
    ui->freqOffset->setText(QString("%1 Hz").arg(offset, 0, 'f', 1));
}

void EnsembleInfoDialog::enableDumpToFile(bool ena)
{
    ui->dumpButton->setVisible(ena);       
    ui->dumpSize->setText("");
    ui->dumpLength->setText("");
    showDumpingStat(false);
}

void EnsembleInfoDialog::on_dumpButton_clicked()
{
    ui->dumpButton->setEnabled(false);
    if (!isDumping)
    {
        QString f = QString("%1/%2_%3.raw")
                .arg(dumpPath)
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"))
                .arg(DabTables::channelList.value(frequency));

        QString fileName = QFileDialog::getSaveFileName(this,
                                                        tr("Dump IQ stream"),
                                                        QDir::toNativeSeparators(f),
                                                        tr("Binary files (*.raw)"));
        if (!fileName.isEmpty())
        {
            dumpPath = QFileInfo(fileName).path(); // store path for next time
            emit dumpToFileStart(fileName);
        }
        else
        {
            ui->dumpButton->setEnabled(true);
        }
    }
    else
    {
        emit dumpToFileStop();
    }
}

void EnsembleInfoDialog::dumpToFileStateToggle(bool dumping, int bytesPerSample)
{
    isDumping = dumping;
    if (dumping)
    {
        ui->dumpButton->setText("Stop dumping");
        bytesDumped = 0;

        // default is bytes/2048/2 => 2 bytes per sample, 2048 samples per milisecond => 2^-12
        bytesToTimeShiftFactor = 12 + (4 == bytesPerSample);
    }
    else
    {
        ui->dumpButton->setText("Dump raw data");
    }
    ui->dumpSize->setText("");
    ui->dumpLength->setText("");

    showDumpingStat(dumping);
    ui->dumpButton->setEnabled(true);
}

void EnsembleInfoDialog::updateDumpStatus(ssize_t bytes)
{
    bytesDumped += bytes;
    ui->dumpSize->setText(QString::number(double(bytesDumped/(1024*1024.0)),'f', 1) + " MB");
    int timeMs = bytesDumped >> bytesToTimeShiftFactor;
    ui->dumpLength->setText(QString::number(double(timeMs * 0.001),'f', 1) + " sec");
}

void EnsembleInfoDialog::updateAgcGain(int gain10)
{
    if (INPUTDEVICE_AGC_GAIN_NA == gain10)
    {   // gain is not available (input device in HW mode)
        ui->agcGain->setText("N/A");
        return;
    }
    ui->agcGain->setText(QString::number(double(gain10 * 0.1),'f', 1) + " dB");
}

void EnsembleInfoDialog::updateFIBstatus(int fibCntr, int fibErrCount)
{
    fibCounter += (fibCntr - fibErrCount);
    fibCounter &= 0x7FFFFFFF;       // wrapping
    fibErrorCounter += fibErrCount;
    fibErrorCounter &= 0x7FFFFFFF;  // wrapping
    ui->fibCount->setText(QString::number(fibCounter));
    ui->fibErrCount->setText(QString::number(fibErrorCounter));
    ui->fibErrRate->setText(QString::number(double(fibErrorCounter)/fibCounter,'e', 4));
}

void EnsembleInfoDialog::updateMSCstatus(int crcOkCount, int crcErrCount)
{
    crcCounter += (crcOkCount + crcErrCount);
    crcCounter &= 0x7FFFFFFF;       // wrapping
    crcErrorCounter += crcErrCount;
    crcErrorCounter &= 0x7FFFFFFF;  // wrapping
    ui->crcCount->setText(QString::number(crcCounter));
    ui->crcErrCount->setText(QString::number(crcErrorCounter));
    ui->crcErrRate->setText(QString::number(double(crcErrorCounter)/crcCounter,'e', 4));
}

void EnsembleInfoDialog::resetFibStat()
{
    fibCounter = 0;
    fibErrorCounter = 0;
    ui->fibCount->setText("0");
    ui->fibErrCount->setText("0");
    ui->fibErrRate->setText("N/A");
}

void EnsembleInfoDialog::resetMscStat()
{
    crcCounter = 0;
    crcErrorCounter = 0;
    ui->crcCount->setText("0");
    ui->crcErrCount->setText("0");
    ui->crcErrRate->setText("N/A");
}

void EnsembleInfoDialog::newFrequency(quint32 f)
{
    frequency = f;
    if (frequency)
    {
        ui->freq->setText(QString::number(frequency) + " kHz");
        ui->channel->setText(DabTables::channelList.value(frequency));
    }
    else
    {
        clearFreqInfo();
        clearServiceInfo();
        resetMscStat();
        resetFibStat();
    }
}

void EnsembleInfoDialog::serviceChanged(const RadioControlServiceComponent &s)
{
    if (s.label.isEmpty())
    {   // service component not valid -> shoudl not happen
        clearServiceInfo();
        return;
    }

    ui->service->setText(QString("%1").arg(s.label, 16));
    ui->serviceID->setText("0x"+QString("%1").arg(s.SId.prog.countryServiceRef, 4, 16, QChar('0')).toUpper());
    ui->scids->setText(QString::number(s.SCIdS));
    ui->subChannel->setText(QString::number(s.SubChId));
    ui->startCU->setText(QString::number(s.SubChAddr));
    ui->numCU->setText(QString::number(s.SubChSize));
}

void EnsembleInfoDialog::showEvent(QShowEvent *event)
{
    emit requestEnsembleConfiguration();
    event->accept();

    // set to minimum size
    QTimer::singleShot(10, this, [this](){ resize(minimumSizeHint()); } );
}

void EnsembleInfoDialog::closeEvent(QCloseEvent *event)
{
    if (isDumping)
    {
        emit dumpToFileStop();
    }
    event->accept();
}

const QString &EnsembleInfoDialog::getDumpPath() const
{
    return dumpPath;
}

void EnsembleInfoDialog::setDumpPath(const QString &newDumpPath)
{
    dumpPath = newDumpPath;
}

void EnsembleInfoDialog::fibFrameContextMenu(const QPoint& pos)
{
    QPoint globalPos = ui->FIBframe->mapToGlobal(pos);
    QMenu menu(this);
    menu.addAction("Reset FIB statistics");
    QAction * mscResetAction = menu.addAction("Reset MSC statistics");
    QAction * selectedItem = menu.exec(globalPos);
    if (nullptr == selectedItem)
    {  // nothing was chosen
       return;
    }

    if (selectedItem == mscResetAction)
    {   // msc reset
        resetMscStat();
    }
    else
    {   // fib reset
        resetFibStat();
    }
}

void EnsembleInfoDialog::clearServiceInfo()
{
    ui->service->setText(QString("%1").arg("", 16));
    ui->serviceID->setText("");
    ui->scids->setText("");
    ui->subChannel->setText("");
    ui->startCU->setText("");
    ui->numCU->setText("");
}

void EnsembleInfoDialog::clearSignalInfo()
{
    ui->snr->setText("N/A");
    ui->freqOffset->setText("N/A");
    ui->agcGain->setText("N/A");
}

void EnsembleInfoDialog::clearFreqInfo()
{
    ui->freq->setText("");
    ui->channel->setText("");
}

void EnsembleInfoDialog::showDumpingStat(bool ena)
{
    ui->dumpLengthLabel->setVisible(ena);
    ui->dumpSizeLabel->setVisible(ena);
    ui->dumpVLine->setVisible(ena);
}


