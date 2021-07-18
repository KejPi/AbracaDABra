#include <QFileDialog>
#include <QFile>
#include <QDebug>
#include <QStandardItemModel>

#include "setupdialog.h"
#include "./ui_setupdialog.h"

static const QString NO_FILE("No file selected");

SetupDialog::SetupDialog(QWidget *parent) : QDialog(parent), ui(new Ui::SetupDialog)
{
    ui->setupUi(this);

    inputFileName = "";

    // remove question mark from titlebar
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    ui->inputCombo->insertItem(int(InputDeviceId::UNDEFINED), "No device");
    ui->inputCombo->insertItem(int(InputDeviceId::RTLSDR), "RTL SDR");
    ui->inputCombo->insertItem(int(InputDeviceId::RAWFILE), "Raw file");
    ui->inputCombo->setCurrentIndex(-1);  // no selection
    resetFilename();

    ui->fileFormatCombo->insertItem(int(RawFileInputFormat::SAMPLE_FORMAT_U8), "Unsigned 8 bits");
    ui->fileFormatCombo->insertItem(int(RawFileInputFormat::SAMPLE_FORMAT_S16), "Signed 16 bits");

    // this has to be aligned with mainwindow
    ui->loopCheckbox->setChecked(false);
}

void SetupDialog::setGainValues(const QList<int> * pList)
{
    ui->gainCombo->clear();
    ui->gainCombo->addItem("Auto");
    ui->gainCombo->addItem("Software");
    for (int i=0; i<pList->size(); ++i)
    {
        ui->gainCombo->addItem(QString("%1").arg(pList->at(i)/10.0), pList->at(i));
    }

    ui->gainCombo->setCurrentIndex(1);
}

void SetupDialog::resetFilename()
{
    ui->fileNameLabel->setText(NO_FILE);
    adjustSize();
}

void SetupDialog::enableFileSelection(bool ena)
{
    if (ena)
    {
        ui->openFileButton->setText("Open file...");
    }
    else
    {
        ui->openFileButton->setText("Stop");
    }
    openFileButton = ena;
}

void SetupDialog::on_gainCombo_currentIndexChanged(int index)
{
    if (index == 0)
    {  // auto gain
        emit setGainMode(GainMode::Hardware);
    }
    else if (index == 1)
    {  // software gain
        emit setGainMode(GainMode::Software);
    }
    else
    {  // manual gain value
        emit setGain(ui->gainCombo->itemData(index).toInt());
    }
}

void SetupDialog::on_inputCombo_currentIndexChanged(int index)
{
    InputDeviceId id = InputDeviceId(index);
    switch (id)
    {
    case InputDeviceId::UNDEFINED:
        ui->rawFileFrame->setVisible(false);
        ui->rtlsdrFrame->setVisible(false);
        break;
    case InputDeviceId::RTLSDR:
        ui->rawFileFrame->setVisible(false);
        ui->rtlsdrFrame->setVisible(true);
        break;
    case InputDeviceId::RAWFILE:
        ui->rawFileFrame->setVisible(true);
        ui->rtlsdrFrame->setVisible(false);
        break;
    }

    adjustSize();
    emit inputDeviceChanged(id);
}

void SetupDialog::on_openFileButton_clicked()
{
    if (openFileButton)
    {
        QString dir = QDir::homePath();
        if (!inputFileName.isEmpty())
        {
            dir = QFileInfo(inputFileName).path();
        }
        QString fileName = QFileDialog::getOpenFileName(this, tr("Open IQ stream"), dir, tr("Binary files (*.bin *.s16 *.u8 *.raw)"));
        if (!fileName.isEmpty())
        {
            inputFileName = fileName;
            ui->fileNameLabel->setText(fileName);
            if (fileName.endsWith(".s16"))
            {
                ui->fileFormatCombo->setCurrentIndex(int(RawFileInputFormat::SAMPLE_FORMAT_S16));
                qDebug() << "Selected file :" << fileName << "SAMPLE_FORMAT_S16";
                //emit fileSelected(fileName, uint8_t(SAMPLE_FORMAT_S16));
            }
            else if (fileName.endsWith(".u8"))
            {
                ui->fileFormatCombo->setCurrentIndex(int(RawFileInputFormat::SAMPLE_FORMAT_U8));
                qDebug() << "Selected file :" << fileName << "SAMPLE_FORMAT_U8";
                //emit fileSelected(fileName, uint8_t(SAMPLE_FORMAT_S16));
            }
            else
            {
                qDebug() << "Selected file :" << fileName << "Sample format unknown";
                //emit fileSelected(fileName, uint8_t(SAMPLE_FORMAT_U8));
            }
            emit rawFileSelected(fileName, RawFileInputFormat(ui->fileFormatCombo->currentIndex()));
        }
    }
    else
    {
        emit rawFileStop();
    }
}

void SetupDialog::on_fileFormatCombo_currentIndexChanged(int index)
{
    qDebug() << Q_FUNC_INFO << "Sample format:" << index;
    emit sampleFormat(RawFileInputFormat(index));
}

void SetupDialog::setInputDevice(const InputDeviceId & inputDevice)
{
    ui->inputCombo->setCurrentIndex(int(inputDevice));
}

void SetupDialog::enableRtlSdrInput(bool ena)
{
    QStandardItemModel *model = qobject_cast<QStandardItemModel *>(ui->inputCombo->model());
    Q_ASSERT(model != nullptr);

    QStandardItem *item = model->item(int(InputDeviceId::RTLSDR));
    if (ena)
    {
        item->setFlags(item->flags() | Qt::ItemIsEnabled);
    }
    else
    {
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        int currentIdx = ui->inputCombo->currentIndex();
        if (int(InputDeviceId::RTLSDR) == currentIdx)
        {
            currentIdx = (currentIdx + 1) % ui->inputCombo->count();
            ui->inputCombo->setCurrentIndex(currentIdx);
        }
    }
}

void SetupDialog::on_loopCheckbox_stateChanged(int state)
{
    emit fileLoopingEnabled(Qt::Checked == Qt::CheckState(state));
}

QString SetupDialog::getInputFileName() const
{
    return inputFileName;
}

RawFileInputFormat SetupDialog::getInputFileFormat() const
{
    return RawFileInputFormat(ui->fileFormatCombo->currentIndex());
}

bool SetupDialog::isFileLoopActive() const
{
    return ui->loopCheckbox->isChecked();
}

void SetupDialog::setInputFile(const QString &value, const RawFileInputFormat &format, bool loop)
{
    inputFileName = value;
    if (value.isEmpty())
    {
        ui->fileNameLabel->setText(NO_FILE);
        // format has no meaning in this case
    }
    else
    {
        ui->fileNameLabel->setText(inputFileName);
        ui->fileFormatCombo->setCurrentIndex(int(format));
        emit rawFileSelected(inputFileName, RawFileInputFormat(format));
    }
    ui->loopCheckbox->setChecked(loop);
}

void SetupDialog::on_dagcCheckBox_stateChanged(int state)
{
    emit setDAGC(Qt::Checked == Qt::CheckState(state));
}

bool SetupDialog::getDAGCState() const
{
    return ui->dagcCheckBox->isChecked();
}

void SetupDialog::setDAGCState(bool ena)
{
    ui->dagcCheckBox->setChecked(ena);
}
