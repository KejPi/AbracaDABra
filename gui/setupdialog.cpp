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

    // remove question mark from titlebar
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    ui->inputCombo->addItem("No device", QVariant(int(InputDeviceId::UNDEFINED)));
    ui->inputCombo->addItem("RTL SDR", QVariant(int(InputDeviceId::RTLSDR)));
    ui->inputCombo->addItem("RTL TCP", QVariant(int(InputDeviceId::RTLTCP)));
    //ui->inputCombo->addItem("RaRT TCP", QVariant(int(InputDeviceId::RARTTCP)));
    ui->inputCombo->addItem("Raw file", QVariant(int(InputDeviceId::RAWFILE)));
    ui->inputCombo->setCurrentIndex(-1);  // undefined

    ui->fileNameLabel->setText(NO_FILE);
    setGainValues();

    ui->fileFormatCombo->insertItem(int(RawFileInputFormat::SAMPLE_FORMAT_U8), "Unsigned 8 bits");
    ui->fileFormatCombo->insertItem(int(RawFileInputFormat::SAMPLE_FORMAT_S16), "Signed 16 bits");

    // this has to be aligned with mainwindow
    ui->loopCheckbox->setChecked(false);

    QString ipRange = "(?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])";
    QRegularExpression ipRegex ("^" + ipRange
                               +                                + "(\\." + ipRange + ")"
                               +                                + "(\\." + ipRange + ")"
                               +                                + "(\\." + ipRange + ")$");
    QRegularExpressionValidator *ipValidator = new QRegularExpressionValidator(ipRegex, this);
    ui->ipAddressEdit->setValidator(ipValidator);
    ui->tcpFrame->hide();

    connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &SetupDialog::onButtonClicked);
    connect(ui->inputCombo, &QComboBox::currentIndexChanged, this, &SetupDialog::onInputChanged);
    connect(ui->openFileButton, &QPushButton::clicked, this, &SetupDialog::onOpenFileButtonClicked);

    adjustSize();
}

SetupDialog::Settings SetupDialog::settings() const
{
    return m_settings;
}


void SetupDialog::onButtonClicked(QAbstractButton *button)
{
    switch (ui->buttonBox->buttonRole(button))
    {
    case QDialogButtonBox::ApplyRole:
        applySettings();
        break;
    case QDialogButtonBox::AcceptRole:
        applySettings();
        hide();
        break;
    case QDialogButtonBox::RejectRole:
        hide();
        break;
    default:
        // do nothing
        return;
    }

}

void SetupDialog::resetGainValues()
{
    ui->gainCombo->clear();
    ui->gainCombo->addItem("Device");
    ui->gainCombo->addItem("Software");
}

void SetupDialog::setGainValues(const QList<int> * pList)
{
    resetGainValues();
    if (nullptr != pList)
    {
        for (int i=0; i<pList->size(); ++i)
        {
            ui->gainCombo->addItem(QString("%1").arg(pList->at(i)/10.0), pList->at(i));
        }
    }
    ui->gainCombo->setCurrentIndex(1);
}

void SetupDialog::setSettings(const Settings &settings)
{
    qDebug() << Q_FUNC_INFO;
    m_settings = settings;

    // -2 == HW, -1 == SW, 0 .. N is gain index
    ui->gainCombo->setCurrentIndex(m_settings.gainIdx + 2);
    int index = ui->inputCombo->findData(QVariant(static_cast<int>(m_settings.inputDevice)));
    ui->inputCombo->setCurrentIndex(index);
    if (m_settings.inputFile.isEmpty())
    {
        ui->fileNameLabel->setText(NO_FILE);
    }
    else
    {
        ui->fileNameLabel->setText(m_settings.inputFile);
    }
    ui->loopCheckbox->setChecked(m_settings.inputFileLoopEna);
    ui->fileFormatCombo->setCurrentIndex(static_cast<int>(m_settings.inputFormat));   
    ui->ipAddressEdit->setText(m_settings.tcpAddress);
    ui->ipPortSpinBox->setValue(m_settings.tcpPort);
}

void SetupDialog::applySettings()
{
    qDebug() << Q_FUNC_INFO;

    Settings newSet;
    newSet.gainIdx = ui->gainCombo->currentIndex() - 2;
    newSet.inputDevice = static_cast<InputDeviceId>(ui->inputCombo->itemData(ui->inputCombo->currentIndex()).toInt());
    if (ui->fileNameLabel->text() != NO_FILE)
    {
        newSet.inputFile = ui->fileNameLabel->text();
    }
    else
    {
        newSet.inputFile.clear();
    }
    newSet.inputFileLoopEna = ui->loopCheckbox->isChecked();
    newSet.inputFormat = static_cast<RawFileInputFormat>(ui->fileFormatCombo->currentIndex());
    newSet.tcpAddress = ui->ipAddressEdit->text();
    newSet.tcpPort = ui->ipPortSpinBox->value();

    bool inputDeviceChangeNeeded = false;
    if (m_settings.inputDevice != newSet.inputDevice)
    {
        inputDeviceChangeNeeded = true;
    }
    else
    {   // device is the same
        switch (m_settings.inputDevice)
        {
        case InputDeviceId::UNDEFINED:
            break;
        case InputDeviceId::RTLSDR:
            break;
        case InputDeviceId::RARTTCP:
        case InputDeviceId::RTLTCP:
            if ((newSet.tcpAddress != m_settings.tcpAddress) || (newSet.tcpPort != m_settings.tcpPort))
            {
                inputDeviceChangeNeeded = true;
            }
            break;
        case InputDeviceId::RAWFILE:
            if ((newSet.inputFile != m_settings.inputFile) || (newSet.inputFormat != m_settings.inputFormat))
            {
                inputDeviceChangeNeeded = true;
            }
            break;
        }
    }

    if (inputDeviceChangeNeeded)
    {   // device will be chnaged -> dialog will be enabled after change
        m_settings = newSet;
        emit inputDeviceChanged(m_settings.inputDevice);
        return;
    }

    // check if new settings
    bool settingsChanged = false;
    settingsChanged = (m_settings.gainIdx != newSet.gainIdx)
                      || (m_settings.inputFile != newSet.inputFile)
                      || (m_settings.inputFileLoopEna != newSet.inputFileLoopEna)
                      || (m_settings.inputFormat != newSet.inputFormat)
                      || (m_settings.tcpAddress != newSet.tcpAddress)
                      || (m_settings.tcpPort != newSet.tcpPort);

    m_settings = newSet;
    if (settingsChanged)
    {
        emit newSettings();
    }
}

void SetupDialog::onExpertMode(bool ena)
{
    int idx = ui->inputCombo->findData(QVariant(static_cast<int>(InputDeviceId::RAWFILE)));
    if (idx >= 0)
    {
        if (!ena)
        {   // remove it
            ui->inputCombo->removeItem(idx);
        }
        return;

    }

    // not found
    if (ena)
    {   // add item
        ui->inputCombo->addItem("Raw file", QVariant(int(InputDeviceId::RAWFILE)));
    }
}

void SetupDialog::onInputChanged(int index)
{
    qDebug() << Q_FUNC_INFO;

    InputDeviceId id = InputDeviceId(ui->inputCombo->itemData(index).toInt());
    switch (id)
    {
    case InputDeviceId::UNDEFINED:
        ui->rawFileFrame->setVisible(false);
        ui->rtlsdrFrame->setVisible(false);
        ui->tcpFrame->setVisible(false);
        break;
    case InputDeviceId::RTLSDR:
        ui->rawFileFrame->setVisible(false);
        ui->rtlsdrFrame->setVisible(true);
        ui->tcpFrame->setVisible(false);
        //resetGainValues();
        break;
    case InputDeviceId::RTLTCP:
        ui->rawFileFrame->setVisible(false);
        ui->rtlsdrFrame->setVisible(true);
        ui->tcpFrame->setVisible(true);
        //resetGainValues();
        break;
    case InputDeviceId::RARTTCP:
        ui->rawFileFrame->setVisible(false);
        ui->rtlsdrFrame->setVisible(false);
        ui->tcpFrame->setVisible(true);
        break;
    case InputDeviceId::RAWFILE:
        ui->rawFileFrame->setVisible(true);
        ui->rtlsdrFrame->setVisible(false);
        ui->tcpFrame->setVisible(false);
        break;
    }

    adjustSize();
}

void SetupDialog::onOpenFileButtonClicked()
{
    QString inputFileName = ui->fileNameLabel->text();
    QString dir = QDir::homePath();
    if (NO_FILE == inputFileName)
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
        }
        else if (fileName.endsWith(".u8"))
        {
            ui->fileFormatCombo->setCurrentIndex(int(RawFileInputFormat::SAMPLE_FORMAT_U8));
            qDebug() << "Selected file :" << fileName << "SAMPLE_FORMAT_U8";
        }
        else
        {
            qDebug() << "Selected file :" << fileName << "Sample format unknown";
        }
    }
}

void SetupDialog::resetInputDevice()
{
    for (int idx = 0; idx < ui->inputCombo->count(); ++idx)
    {
        if (ui->inputCombo->itemData(idx).toInt() == static_cast<int>(InputDeviceId::UNDEFINED))
        {
            ui->inputCombo->setCurrentIndex(idx);
            return;
        }
    }
}
