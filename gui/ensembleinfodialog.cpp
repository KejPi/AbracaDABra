#include <QFileDialog>
#include <QScrollBar>
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

    // 16x '_' (max label size is 16 characters)
    int minWidth = 1.7 * ui->service->fontMetrics().boundingRect("________________").width();
    ui->FIBframe->setMinimumWidth(minWidth);
    ui->serviceFrame->setMinimumWidth(minWidth);
    ui->signalFrame->setMinimumWidth(minWidth);

    ui->FIBframe->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->FIBframe, &QWidget::customContextMenuRequested, this, &EnsembleInfoDialog::fibFrameContextMenu);

    // set tooltips
    ui->freqLabel->setToolTip("Tuned frequency");
    ui->freq->setToolTip("Tuned frequency");
    ui->channelLabel->setToolTip("Tuned DAB channel");
    ui->channel->setToolTip("Tuned DAB channel");
    ui->freqOffsetLabel->setToolTip("Estimated frequency offset");
    ui->freqOffset->setToolTip("Estimated frequency offset");
    ui->snrLabel->setToolTip("Estimated SNR");
    ui->snr->setToolTip("Estimated SNR");
    ui->agcGainLabel->setToolTip("Current AGC gain<br>(only in software mode)");
    ui->agcGain->setToolTip("Current AGC gain<br>(only in software mode)");

    ui->serviceLabel->setToolTip("Current service name");
    ui->service->setToolTip("Current service name");
    ui->serviceIdLabel->setToolTip("Current Service Identifier");
    ui->serviceId->setToolTip("Current Service Identifier");
    ui->scidsLabel->setToolTip("Service Component Identifier within the Service");
    ui->scids->setToolTip("Service Component Identifier within the Service");
    ui->numCULabel->setToolTip("Number of capacity units used by sub-channel");
    ui->numCU->setToolTip("Number of capacity units used by sub-channel");
    ui->startCULabel->setToolTip("First capacity unit used by sub-channel");
    ui->startCU->setToolTip("First capacity unit used by sub-channel");
    ui->subChannelLabel->setToolTip("Sub-channel Identifier");
    ui->subChannel->setToolTip("Sub-channel Identifier");

    ui->fibCountLabel->setToolTip("Total number of FIB's");
    ui->fibCount->setToolTip("Total number of FIB's");
    ui->fibErrCountLabel->setToolTip("Number of FIB's with CRC error");
    ui->fibErrCount->setToolTip("Number of FIB's with CRC error");
    ui->fibErrRateLabel->setToolTip("FIB error rate");
    ui->fibErrRate->setToolTip("FIB error rate");
    ui->crcCountLabel->setToolTip("Total number of audio frames (AU for DAB+)");
    ui->crcCount->setToolTip("Total number of audio frames (AU for DAB+)");
    ui->crcErrCountLabel->setToolTip("Total number of audio frames with CRC error (AU for DAB+)");
    ui->crcErrCount->setToolTip("Total number of audio frames with CRC error (AU for DAB+)");
    ui->crcErrRateLabel->setToolTip("Audio frame (AU for DAB+) error rate");
    ui->crcErrRate->setToolTip("Audio frame (AU for DAB+) error rate");

    ui->dumpButton->setToolTip("Dump raw IQ stream to file");

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
    {   // empty ensemble configuration means tuning to new frequency
        clearSignalInfo();
        clearServiceInfo();

        resetFibStat();
        resetMscStat();
    }
    else
    {

        ui->ensStructureTextEdit->setMinimumWidth(ui->ensStructureTextEdit->document()->idealWidth()
                        + ui->ensStructureTextEdit->contentsMargins().left()
                        + ui->ensStructureTextEdit->contentsMargins().right()
                        + ui->ensStructureTextEdit->verticalScrollBar()->width());
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
        //bytesToTimeShiftFactor = 12 + (4 == bytesPerSample);

        bytesToTimeShiftFactor = 11;
        int n = 1;
        do
        {
            n <<= 1;
            bytesToTimeShiftFactor+=1;
        } while (n < bytesPerSample);
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

void EnsembleInfoDialog::updateAgcGain(float gain)
{
    if (std::isnan(gain))
    {   // gain is not available (input device in HW mode)
        ui->agcGain->setText("N/A");
        return;
    }
    ui->agcGain->setText(QString::number(double(gain),'f', 1) + " dB");
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
    if (!s.SId.isValid())
    {   // service component not valid -> can happen during reconfig
        clearServiceInfo();
        return;
    }

    ui->service->setText(QString("%1").arg(s.label, 16));
    ui->serviceId->setText("0x"+QString("%1").arg(s.SId.countryServiceRef(), 4, 16, QChar('0')).toUpper());
    ui->scids->setText(QString::number(s.SCIdS));
    ui->subChannel->setText(QString::number(s.SubChId));
    ui->startCU->setText(QString::number(s.SubChAddr));
    ui->numCU->setText(QString::number(s.SubChSize));

    resetMscStat();
}

void EnsembleInfoDialog::showEvent(QShowEvent *event)
{
    // calculate width

    emit requestEnsembleConfiguration();
    event->accept();

    // set to minimum size
    QTimer::singleShot(1, this, [this](){ resize(minimumSizeHint()); } );
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
    QAction * fibResetAction = menu.addAction("Reset FIB statistics");
    QAction * mscResetAction = menu.addAction("Reset MSC statistics");
    QAction * allResetAction = menu.addAction("Reset all statistics");
    QAction * selectedItem = menu.exec(globalPos);
    if (nullptr == selectedItem)
    {  // nothing was chosen
       return;
    }

    if (selectedItem == mscResetAction)
    {   // msc reset
        resetMscStat();
    }
    else if (selectedItem == fibResetAction)
    {   // fib reset
        resetFibStat();
    }
    else
    {   // reset all
        resetFibStat();
        resetMscStat();
    }
}

void EnsembleInfoDialog::clearServiceInfo()
{
    ui->service->setText(QString("%1").arg("", 16));
    ui->serviceId->setText("");
    ui->scids->setText("");
    ui->subChannel->setText("");
    ui->startCU->setText("");
    ui->numCU->setText("");
}

void EnsembleInfoDialog::clearSignalInfo()
{
    ui->snr->setText("N/A");
    ui->freqOffset->setText("N/A");
    //ui->agcGain->setText("N/A");
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


