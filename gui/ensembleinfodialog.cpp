#include <QFileDialog>
#include <QScrollBar>
#include <QDateTime>
#include <QDebug>
#include <QMenu>

#include "ensembleinfodialog.h"
#include "ui_ensembleinfodialog.h"

EnsembleInfoDialog::EnsembleInfoDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EnsembleInfoDialog)
{
    ui->setupUi(this);

    // remove question mark from titlebar
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    connect(ui->recordButton, &QPushButton::clicked, this, &EnsembleInfoDialog::onRecordingButtonClicked);

    m_recordingPath = QDir::homePath();

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

    ui->recordButton->setToolTip("Record raw IQ stream to file");

    enableRecording(false);
}

EnsembleInfoDialog::~EnsembleInfoDialog()
{
    delete ui;
}

void EnsembleInfoDialog::refreshEnsembleConfiguration(const QString & txt)
{    
    if (isVisible())
    {
        ui->ensStructureTextEdit->setHtml(txt);
        if (txt.isEmpty())
        {   // empty ensemble configuration means tuning to new frequency
            clearSignalInfo();
            clearServiceInfo();

            resetFibStat();
            resetMscStat();

            ui->ensStructureTextEdit->setDocumentTitle("");
        }
        else
        {
            ui->ensStructureTextEdit->setDocumentTitle("Ensemble information");

            int minWidth = ui->ensStructureTextEdit->document()->idealWidth()
                           + ui->ensStructureTextEdit->contentsMargins().left()
                           + ui->ensStructureTextEdit->contentsMargins().right()
                           + ui->ensStructureTextEdit->verticalScrollBar()->width();

            if (minWidth > 1000)
            {
                minWidth = 1000;
            }
            ui->ensStructureTextEdit->setMinimumWidth(minWidth);
        }
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

void EnsembleInfoDialog::enableRecording(bool ena)
{
    ui->recordButton->setVisible(ena);
    ui->dumpSize->setText("");
    ui->dumpLength->setText("");
    showRecordingStat(false);
    if (!ena)
    {
        emit recordingStop();
    }
}

void EnsembleInfoDialog::onRecordingButtonClicked()
{
    ui->recordButton->setEnabled(false);
    if (!m_isRecordingActive)
    {
        ui->recordButton->setEnabled(false);
        emit recordingStart(this);
    }
    else
    {
        emit recordingStop();
    }
}

void EnsembleInfoDialog::onRecording(bool isActive)
{
    m_isRecordingActive = isActive;
    if (isActive)
    {
        ui->recordButton->setText("Stop recording");
    }
    else
    {
        ui->recordButton->setText("Record raw data");
    }
    ui->dumpSize->setText("");
    ui->dumpLength->setText("");

    showRecordingStat(m_isRecordingActive);
    ui->recordButton->setEnabled(true);
}

void EnsembleInfoDialog::updateRecordingStatus(uint64_t bytes, float ms)
{
    ui->dumpSize->setText(QString::number(double(bytes/(1024*1024.0)),'f', 1) + " MB");
    ui->dumpLength->setText(QString::number(double(ms * 0.001),'f', 1) + " sec");
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
    m_fibCounter += (fibCntr - fibErrCount);
    m_fibCounter &= 0x7FFFFFFF;       // wrapping
    m_fibErrorCounter += fibErrCount;
    m_fibErrorCounter &= 0x7FFFFFFF;  // wrapping
    ui->fibCount->setText(QString::number(m_fibCounter));
    ui->fibErrCount->setText(QString::number(m_fibErrorCounter));
    ui->fibErrRate->setText(QString::number(double(m_fibErrorCounter)/m_fibCounter,'e', 4));
}

void EnsembleInfoDialog::updateMSCstatus(int crcOkCount, int crcErrCount)
{
    m_crcCounter += (crcOkCount + crcErrCount);
    m_crcCounter &= 0x7FFFFFFF;       // wrapping
    m_crcErrorCounter += crcErrCount;
    m_crcErrorCounter &= 0x7FFFFFFF;  // wrapping
    ui->crcCount->setText(QString::number(m_crcCounter));
    ui->crcErrCount->setText(QString::number(m_crcErrorCounter));
    ui->crcErrRate->setText(QString::number(double(m_crcErrorCounter)/m_crcCounter,'e', 4));
}

void EnsembleInfoDialog::resetFibStat()
{
    m_fibCounter = 0;
    m_fibErrorCounter = 0;
    ui->fibCount->setText("0");
    ui->fibErrCount->setText("0");
    ui->fibErrRate->setText("N/A");
}

void EnsembleInfoDialog::resetMscStat()
{
    m_crcCounter = 0;
    m_crcErrorCounter = 0;
    ui->crcCount->setText("0");
    ui->crcErrCount->setText("0");
    ui->crcErrRate->setText("N/A");
}

void EnsembleInfoDialog::newFrequency(quint32 f)
{
    m_frequency = f;
    if (m_frequency)
    {
        ui->freq->setText(QString::number(m_frequency) + " kHz");
        ui->channel->setText(DabTables::channelList.value(m_frequency));
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
    if (m_isRecordingActive)
    {
        emit recordingStop();
    }
    event->accept();
}

const QString &EnsembleInfoDialog::getDumpPath() const
{
    return m_recordingPath;
}

void EnsembleInfoDialog::setDumpPath(const QString &newDumpPath)
{
    m_recordingPath = newDumpPath;
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

void EnsembleInfoDialog::showRecordingStat(bool ena)
{
    ui->dumpLengthLabel->setVisible(ena);
    ui->dumpSizeLabel->setVisible(ena);
    ui->dumpVLine->setVisible(ena);
}


