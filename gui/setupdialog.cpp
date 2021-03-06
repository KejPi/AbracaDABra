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
#ifdef HAVE_AIRSPY
    ui->inputCombo->addItem("Airspy", QVariant(int(InputDeviceId::AIRSPY)));
#endif
    ui->inputCombo->addItem("RTL TCP", QVariant(int(InputDeviceId::RTLTCP)));
#ifdef HAVE_RARTTCP
    ui->inputCombo->addItem("RaRT TCP", QVariant(int(InputDeviceId::RARTTCP)));
#endif
    ui->inputCombo->addItem("Raw file", QVariant(int(InputDeviceId::RAWFILE)));
    ui->inputCombo->setCurrentIndex(-1);  // undefined

    ui->fileNameLabel->setText(NO_FILE);
    rawfilename = "";

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
    ui->rtltcpIpAddressEdit->setValidator(ipValidator);
    ui->rarttcpIpAddressEdit->setValidator(ipValidator);

    connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &SetupDialog::onButtonClicked);
    connect(ui->inputCombo, &QComboBox::currentIndexChanged, this, &SetupDialog::onInputChanged);
    connect(ui->openFileButton, &QPushButton::clicked, this, &SetupDialog::onOpenFileButtonClicked);

    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);

    connect(ui->inputCombo, &QComboBox::currentIndexChanged, this, [this](){ applyEnable(); });
    connect(ui->fileFormatCombo, &QComboBox::currentIndexChanged, this, [this](){ applyEnable(); });
    connect(ui->rtlsdrGainCombo, &QComboBox::currentIndexChanged, this, [this](){ applyEnable(); });
    connect(ui->rtltcpGainCombo, &QComboBox::currentIndexChanged, this, [this](){ applyEnable(); });
    connect(ui->rtltcpIpAddressEdit, &QLineEdit::textEdited, this, [this](){ applyEnable(); });
    connect(ui->rtltcpIpPortSpinBox, &QSpinBox::valueChanged, this, [this](){ applyEnable(); });
#ifdef HAVE_RARTTCP
    connect(ui->rarttcpIpAddressEdit, &QLineEdit::textEdited, this, [this](){ applyEnable(); });
    connect(ui->rarttcpIpPortSpinBox, &QSpinBox::valueChanged, this, [this](){ applyEnable(); });
#endif
    connect(ui->loopCheckbox, &QCheckBox::stateChanged, this, [this](){ applyEnable(); });

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

void SetupDialog::setGainValues(const QList<int> * pList)
{
    QComboBox * comboPtr;

    switch (m_settings.inputDevice)
    {
    case InputDeviceId::RTLSDR:
        comboPtr = ui->rtlsdrGainCombo;
        break;
    case InputDeviceId::RTLTCP:
        comboPtr = ui->rtltcpGainCombo;
        break;
    case InputDeviceId::UNDEFINED:
    case InputDeviceId::RARTTCP:
    case InputDeviceId::RAWFILE:
    case InputDeviceId::AIRSPY:
        return;
    }

    comboPtr->clear();
    comboPtr->addItem("Device");
    comboPtr->addItem("Software");

    if (nullptr != pList)
    {
        for (int i=0; i<pList->size(); ++i)
        {
            comboPtr->addItem(QString("%1").arg(pList->at(i)/10.0), pList->at(i));
        }
    }
    comboPtr->setCurrentIndex(1);

    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
}

void SetupDialog::setSettings(const Settings &settings)
{
    //qDebug() << Q_FUNC_INFO;
    m_settings = settings;
}

void SetupDialog::showEvent(QShowEvent *event)
{
    // -2 == HW, -1 == SW, 0 .. N is gain index
    int index = ui->inputCombo->findData(QVariant(static_cast<int>(m_settings.inputDevice)));
    ui->inputCombo->setCurrentIndex(index);
    ui->rtlsdrGainCombo->setCurrentIndex(m_settings.rtlsdr.gainIdx + 2);
    ui->rtltcpGainCombo->setCurrentIndex(m_settings.rtltcp.gainIdx + 2);

    if (m_settings.rawfile.file.isEmpty())
    {
        ui->fileNameLabel->setText(NO_FILE);
        rawfilename = "";
        ui->fileNameLabel->setToolTip("");
    }
    else
    {
        ui->fileNameLabel->setText(QFileInfo(m_settings.rawfile.file).fileName());
        rawfilename = m_settings.rawfile.file;
        ui->fileNameLabel->setToolTip(m_settings.rawfile.file);
    }
    ui->loopCheckbox->setChecked(m_settings.rawfile.loopEna);
    ui->fileFormatCombo->setCurrentIndex(static_cast<int>(m_settings.rawfile.format));
    ui->rtltcpIpAddressEdit->setText(m_settings.rtltcp.tcpAddress);
    ui->rtltcpIpPortSpinBox->setValue(m_settings.rtltcp.tcpPort);
#ifdef HAVE_RARTTCP
    ui->rarttcpIpAddressEdit->setText(m_settings.rarttcp.tcpAddress);
    ui->rarttcpIpPortSpinBox->setValue(m_settings.rarttcp.tcpPort);
#endif
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
}

void SetupDialog::applySettings()
{
    //qDebug() << Q_FUNC_INFO;

    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);

    Settings newSet;
    newSet.rtlsdr.bandwidth = m_settings.rtlsdr.bandwidth;
    newSet.rtlsdr.biasT = m_settings.rtlsdr.biasT;
    newSet.inputDevice = static_cast<InputDeviceId>(ui->inputCombo->itemData(ui->inputCombo->currentIndex()).toInt());

    newSet.rtlsdr.gainIdx = ui->rtlsdrGainCombo->currentIndex() - 2;
    newSet.rtltcp.gainIdx = ui->rtltcpGainCombo->currentIndex() - 2;

    if (rawfilename.isEmpty())
    {
        newSet.rawfile.file.clear();
    }
    else
    {
        newSet.rawfile.file = rawfilename;
    }
    newSet.rawfile.loopEna = ui->loopCheckbox->isChecked();
    newSet.rawfile.format = static_cast<RawFileInputFormat>(ui->fileFormatCombo->currentIndex());
    newSet.rtltcp.tcpAddress = ui->rtltcpIpAddressEdit->text();
    newSet.rtltcp.tcpPort = ui->rtltcpIpPortSpinBox->value();
#ifdef HAVE_RARTTCP
    newSet.rarttcp.tcpAddress = ui->rarttcpIpAddressEdit->text();
    newSet.rarttcp.tcpPort = ui->rarttcpIpPortSpinBox->value();
#endif

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
#ifdef HAVE_RARTTCP
            if ((newSet.rarttcp.tcpAddress != m_settings.rarttcp.tcpAddress)
                || (newSet.rarttcp.tcpPort != m_settings.rarttcp.tcpPort))
            {
                inputDeviceChangeNeeded = true;
            }
#endif
            break;
        case InputDeviceId::RTLTCP:
            if ((newSet.rtltcp.tcpAddress != m_settings.rtltcp.tcpAddress)
                || (newSet.rtltcp.tcpPort != m_settings.rtltcp.tcpPort))
            {
                inputDeviceChangeNeeded = true;
            }
            break;
        case InputDeviceId::RAWFILE:
            if ((newSet.rawfile.file != m_settings.rawfile.file)
                || (newSet.rawfile.format != m_settings.rawfile.format))
            {
                inputDeviceChangeNeeded = true;
            }
            break;
        case InputDeviceId::AIRSPY:
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
    settingsChanged = (m_settings.rtlsdr.gainIdx != newSet.rtlsdr.gainIdx)
                      || (m_settings.rtltcp.gainIdx != newSet.rtltcp.gainIdx)
                      || (m_settings.rawfile.file != newSet.rawfile.file)
                      || (m_settings.rawfile.loopEna != newSet.rawfile.loopEna)
                      || (m_settings.rawfile.format != newSet.rawfile.format)
#ifdef HAVE_RARTTCP
                      || (m_settings.rarttcp.tcpAddress != newSet.rarttcp.tcpAddress)
                      || (m_settings.rarttcp.tcpPort != newSet.rarttcp.tcpPort)
#endif
                      || (m_settings.rtltcp.tcpAddress != newSet.rtltcp.tcpAddress)
                      || (m_settings.rtltcp.tcpPort != newSet.rtltcp.tcpPort);

    m_settings = newSet;
    if (settingsChanged)
    {
        emit newSettings();
    }
}

void SetupDialog::applyEnable()
{
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
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
    ui->deviceOptionsWidget->setCurrentIndex(ui->inputCombo->itemData(index).toInt());
    adjustSize();
}

void SetupDialog::onOpenFileButtonClicked()
{
    QString dir = QDir::homePath();
    if (!rawfilename.isEmpty())
    {
        dir = QFileInfo(rawfilename).path();
    }
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open IQ stream"), dir, tr("Binary files (*.bin *.s16 *.u8 *.raw)"));
    if (!fileName.isEmpty())
    {
        rawfilename = fileName;
        ui->fileNameLabel->setText(QFileInfo(fileName).fileName());
        ui->fileNameLabel->setToolTip(fileName);
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

        ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
    }
}

void SetupDialog::resetInputDevice()
{
    switch (m_settings.inputDevice)
    {
    case InputDeviceId::RTLSDR:
        ui->rtlsdrGainCombo->clear();
        ui->rtlsdrGainCombo->addItem("Device");
        ui->rtlsdrGainCombo->addItem("Software");
        break;
    case InputDeviceId::RTLTCP:
        ui->rtltcpGainCombo->clear();
        ui->rtltcpGainCombo->addItem("Device");
        ui->rtltcpGainCombo->addItem("Software");
        break;
    case InputDeviceId::RARTTCP:
    case InputDeviceId::UNDEFINED:
    case InputDeviceId::RAWFILE:
    case InputDeviceId::AIRSPY:
        return;
    }

    m_settings.inputDevice = InputDeviceId::UNDEFINED;

    int idx = ui->inputCombo->findData(QVariant(static_cast<int>(InputDeviceId::UNDEFINED)));
    ui->inputCombo->setCurrentIndex(idx);
}
