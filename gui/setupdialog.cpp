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

    ui->statusLabel->setText("<span style=\"color:red\">No device connected</span>");

    QString ipRange = "(?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])";
    QRegularExpression ipRegex ("^" + ipRange
                               +                                + "(\\." + ipRange + ")"
                               +                                + "(\\." + ipRange + ")"
                               +                                + "(\\." + ipRange + ")$");
    QRegularExpressionValidator *ipValidator = new QRegularExpressionValidator(ipRegex, this);
    ui->rtltcpIpAddressEdit->setValidator(ipValidator);
    ui->rarttcpIpAddressEdit->setValidator(ipValidator);

    connect(ui->inputCombo, &QComboBox::currentIndexChanged, this, &SetupDialog::onInputChanged);
    connect(ui->openFileButton, &QPushButton::clicked, this, &SetupDialog::onOpenFileButtonClicked);

    connect(ui->connectButton, &QPushButton::clicked, this, &SetupDialog::onConnectDeviceClicked);

    connect(ui->rtlsdrGainSlider, &QSlider::valueChanged, this, &SetupDialog::onRtlSdrGainSliderChanged);
    connect(ui->rtlsdrGainModeHw, &QRadioButton::toggled, this, &SetupDialog::onRtlGainModeToggled);
    connect(ui->rtlsdrGainModeSw, &QRadioButton::toggled, this, &SetupDialog::onRtlGainModeToggled);
    connect(ui->rtlsdrGainModeManual, &QRadioButton::toggled, this, &SetupDialog::onRtlGainModeToggled);

    connect(ui->rtltcpGainSlider, &QSlider::valueChanged, this, &SetupDialog::onRtlTcpGainSliderChanged);
    connect(ui->rtltcpGainModeHw, &QRadioButton::toggled, this, &SetupDialog::onTcpGainModeToggled);
    connect(ui->rtltcpGainModeSw, &QRadioButton::toggled, this, &SetupDialog::onTcpGainModeToggled);
    connect(ui->rtltcpGainModeManual, &QRadioButton::toggled, this, &SetupDialog::onTcpGainModeToggled);
    connect(ui->rtltcpIpAddressEdit, &QLineEdit::editingFinished, this, &SetupDialog::onRtlTcpIpAddrEditFinished);
    connect(ui->rtltcpIpPortSpinBox, &QSpinBox::valueChanged, this, &SetupDialog::onRtlTcpPortValueChanged);

    connect(ui->loopCheckbox, &QCheckBox::stateChanged, [=](int val) { m_settings.rawfile.loopEna = (Qt::Unchecked != val); });
    connect(ui->fileFormatCombo, &QComboBox::currentIndexChanged, this, &SetupDialog::onRawFileFormatChanged);

#ifdef HAVE_AIRSPY
    connect(ui->airspyIFGainSlider, &QSlider::valueChanged, this, &SetupDialog::onAirspyIFGainSliderChanged);
    connect(ui->airspyLNAGainSlider, &QSlider::valueChanged, this, &SetupDialog::onAirspyLNAGainSliderChanged);
    connect(ui->airspyMixerGainSlider, &QSlider::valueChanged, this, &SetupDialog::onAirspyMixerGainSliderChanged);
    connect(ui->airspyLNAAGCCheckbox, &QCheckBox::stateChanged, this, &SetupDialog::onAirspyLNAAGCstateChanged);
    connect(ui->airspyMixerAGCCheckbox, &QCheckBox::stateChanged, this, &SetupDialog::onAirspyMixerAGCstateChanged);
    connect(ui->airspyGainModeHybrid, &QRadioButton::toggled, this, &SetupDialog::onAirspyModeToggled);
    connect(ui->airspyGainModeSw, &QRadioButton::toggled, this, &SetupDialog::onAirspyModeToggled);
    connect(ui->airspyGainModeManual, &QRadioButton::toggled, this, &SetupDialog::onAirspyModeToggled);
#endif
    adjustSize();
}

SetupDialog::Settings SetupDialog::settings() const
{
    return m_settings;
}

void SetupDialog::setGainValues(const QList<int> * pList)
{

    switch (m_settings.inputDevice)
    {
    case InputDeviceId::RTLSDR:
        rtlsdrGainList.clear();
        rtlsdrGainList = *pList;
        ui->rtlsdrGainSlider->setMinimum(0);
        ui->rtlsdrGainSlider->setMaximum(rtlsdrGainList.size()-1);
        ui->rtlsdrGainSlider->setValue((m_settings.rtlsdr.gainIdx >= 0) ? m_settings.rtlsdr.gainIdx : 0);
        break;
    case InputDeviceId::RTLTCP:
        rtltcpGainList.clear();
        rtltcpGainList = *pList;
        ui->rtltcpGainSlider->setMinimum(0);
        ui->rtltcpGainSlider->setMaximum(rtltcpGainList.size()-1);
        ui->rtltcpGainSlider->setValue((m_settings.rtltcp.gainIdx >= 0) ? m_settings.rtltcp.gainIdx : 0);
        break;
    case InputDeviceId::UNDEFINED:
    case InputDeviceId::RARTTCP:
    case InputDeviceId::RAWFILE:
    case InputDeviceId::AIRSPY:
        return;
    }
}

void SetupDialog::setSettings(const Settings &settings)
{
    //qDebug() << Q_FUNC_INFO;
    m_settings = settings;
    setStatusLabel();
}

void SetupDialog::showEvent(QShowEvent *event)
{
    int index = ui->inputCombo->findData(QVariant(static_cast<int>(m_settings.inputDevice)));
    if (index < 0)
    {  // not found -> show rtlsdr as default
        index = ui->inputCombo->findData(QVariant(static_cast<int>(InputDeviceId::RTLSDR)));
    }
    ui->inputCombo->setCurrentIndex(index);

    if (!rtlsdrGainList.isEmpty())
    {
        ui->rtlsdrGainSlider->setValue(m_settings.rtlsdr.gainIdx);
        onRtlSdrGainSliderChanged(m_settings.rtlsdr.gainIdx);
    }
    else
    {
        ui->rtlsdrGainSlider->setValue(0);
        ui->rtlsdrGainValueLabel->setText("N/A");
    }
    if (!rtltcpGainList.isEmpty())
    {
        ui->rtltcpGainSlider->setValue(m_settings.rtltcp.gainIdx);
        onRtlTcpGainSliderChanged(m_settings.rtltcp.gainIdx);
    }
    else
    {
        ui->rtltcpGainSlider->setValue(0);
        ui->rtltcpGainValueLabel->setText("N/A");
    }

    switch (m_settings.rtlsdr.gainMode) {
    case GainMode::Software:
        ui->rtlsdrGainModeSw->setChecked(true);
        break;
    case GainMode::Hardware:
        ui->rtlsdrGainModeHw->setChecked(true);
        break;
    case GainMode::Manual:
        ui->rtlsdrGainModeManual->setChecked(true);
        break;
    default:
        break;
    }

    switch (m_settings.rtltcp.gainMode) {
    case GainMode::Software:
        ui->rtltcpGainModeSw->setChecked(true);
        break;
    case GainMode::Hardware:
        ui->rtltcpGainModeHw->setChecked(true);
        break;
    case GainMode::Manual:
        ui->rtltcpGainModeManual->setChecked(true);
        break;
    default:
        break;
    }

#ifdef HAVE_AIRSPY
    switch (m_settings.airspy.gainMode) {
    case GainMode::Software:
        ui->airspyGainModeSw->setChecked(true);
        break;
    case GainMode::Hardware:
        ui->airspyGainModeHybrid->setChecked(true);
        break;
    case GainMode::Manual:
        ui->airspyGainModeManual->setChecked(true);
        break;
    default:
        break;
    }

    ui->airspyIFGainSlider->setValue(m_settings.airspy.ifGainIdx);
    ui->airspyLNAGainSlider->setValue(m_settings.airspy.lnaGainIdx);
    ui->airspyMixerGainSlider->setValue(m_settings.airspy.mixerGainIdx);
    ui->airspyMixerAGCCheckbox->setChecked(m_settings.airspy.mixerAgcEna);
    ui->airspyLNAAGCCheckbox->setChecked(m_settings.airspy.lnaAgcEna);
#endif

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

    setStatusLabel();
}


void SetupDialog::onConnectDeviceClicked()
{
    ui->connectButton->setHidden(true);
    m_settings.inputDevice = static_cast<InputDeviceId>(ui->inputCombo->itemData(ui->inputCombo->currentIndex()).toInt());
    setStatusLabel();
    switch (m_settings.inputDevice)
    {
    case InputDeviceId::RTLSDR:
        activateRtlSdrControls(true);
        break;
    case InputDeviceId::RTLTCP:
        m_settings.rtltcp.tcpAddress = ui->rtltcpIpAddressEdit->text();
        m_settings.rtltcp.tcpPort = ui->rtltcpIpPortSpinBox->value();
        activateRtlTcpControls(true);
        break;
    case InputDeviceId::RARTTCP:
        break;
    case InputDeviceId::UNDEFINED:
        break;
    case InputDeviceId::RAWFILE:
        m_settings.rawfile.file = rawfilename;
        m_settings.rawfile.loopEna = ui->loopCheckbox->isChecked();
        m_settings.rawfile.format = static_cast<RawFileInputFormat>(ui->fileFormatCombo->currentIndex());
        break;
    case InputDeviceId::AIRSPY:
#ifdef HAVE_AIRSPY
        activateAirspyControls(true);
#endif
        break;
    }
    emit inputDeviceChanged(m_settings.inputDevice);
}

void SetupDialog::onRtlSdrGainSliderChanged(int val)
{
    ui->rtlsdrGainValueLabel->setText(QString("%1 dB").arg(rtlsdrGainList.at(val)/10.0));
    m_settings.rtlsdr.gainIdx = val;
    emit newSettings();
}

void SetupDialog::onRtlTcpGainSliderChanged(int val)
{
    ui->rtltcpGainValueLabel->setText(QString("%1 dB").arg(rtltcpGainList.at(val)/10.0));
    m_settings.rtltcp.gainIdx = val;
    emit newSettings();
}

void SetupDialog::onRtlTcpIpAddrEditFinished()
{
    if (ui->rtltcpIpAddressEdit->text() != m_settings.rtltcp.tcpAddress)
    {
        ui->connectButton->setVisible(true);
    }
}

void SetupDialog::onRtlTcpPortValueChanged(int val)
{
    if (val != m_settings.rtltcp.tcpPort)
    {
        ui->connectButton->setVisible(true);
    }

}


void SetupDialog::onRtlGainModeToggled(bool checked)
{
    if (checked)
    {
        if (ui->rtlsdrGainModeHw->isChecked())
        {
            m_settings.rtlsdr.gainMode = GainMode::Hardware;
        }
        else if (ui->rtlsdrGainModeSw->isChecked())
        {
            m_settings.rtlsdr.gainMode = GainMode::Software;
        }
        else if (ui->rtlsdrGainModeManual->isChecked())
        {
            m_settings.rtlsdr.gainMode = GainMode::Manual;
        }
        activateRtlSdrControls(true);
        emit newSettings();
    }
}

void SetupDialog::onTcpGainModeToggled(bool checked)
{
    if (checked)
    {
        if (ui->rtltcpGainModeHw->isChecked())
        {
            m_settings.rtltcp.gainMode = GainMode::Hardware;
        }
        else if (ui->rtltcpGainModeSw->isChecked())
        {
            m_settings.rtltcp.gainMode = GainMode::Software;
        }
        else if (ui->rtltcpGainModeManual->isChecked())
        {
            m_settings.rtltcp.gainMode = GainMode::Manual;
        }
        activateRtlTcpControls(true);
        emit newSettings();
    }
}

void SetupDialog::activateRtlSdrControls(bool en)
{
    ui->rtlsdrGainModeGroup->setEnabled(en);
    ui->rtlsdrGainWidget->setEnabled(en && (GainMode::Manual == m_settings.rtlsdr.gainMode));
}

void SetupDialog::activateRtlTcpControls(bool en)
{
    ui->rtltcpGainModeGroup->setEnabled(en);
    ui->rtltcpGainWidget->setEnabled(en && (GainMode::Manual == m_settings.rtltcp.gainMode));
}

void SetupDialog::onRawFileFormatChanged(int idx)
{
    if (static_cast<RawFileInputFormat>(idx) != m_settings.rawfile.format)
    {
        ui->connectButton->setVisible(true);
    }
}

#ifdef HAVE_AIRSPY
void SetupDialog::activateAirspyControls(bool en)
{
    ui->airspyGainModeGroup->setEnabled(en);
    ui->airspyGainWidget->setEnabled(en && (GainMode::Manual == m_settings.airspy.gainMode));
}

void SetupDialog::onAirspyModeToggled(bool checked)
{
    if (checked)
    {
        if (ui->airspyGainModeHybrid->isChecked())
        {
            m_settings.airspy.gainMode = GainMode::Hardware;
        }
        else if (ui->airspyGainModeSw->isChecked())
        {
            m_settings.airspy.gainMode = GainMode::Software;
        }
        else if (ui->airspyGainModeManual->isChecked())
        {
            m_settings.airspy.gainMode = GainMode::Manual;
        }
        activateAirspyControls(true);
        emit newSettings();
    }
}

void SetupDialog::onAirspyIFGainSliderChanged(int val)
{
    ui->airspyIFGainLabel->setText(QString::number(val));
    m_settings.airspy.ifGainIdx = val;
    emit newSettings();
}

void SetupDialog::onAirspyLNAGainSliderChanged(int val)
{
    ui->airspyLNAGainLabel->setText(QString::number(val));
    m_settings.airspy.lnaGainIdx = val;
    emit newSettings();
}

void SetupDialog::onAirspyMixerGainSliderChanged(int val)
{
    ui->airspyMixerGainLabel->setText(QString::number(val));
    m_settings.airspy.mixerGainIdx = val;
    emit newSettings();
}

void SetupDialog::onAirspyLNAAGCstateChanged(int state)
{
    bool ena = (Qt::Unchecked == state);
    ui->airspyLNAGain->setEnabled(ena);
    ui->airspyLNAGainLabel->setEnabled(ena);
    ui->airspyLNAGainSlider->setEnabled(ena);

    m_settings.airspy.lnaAgcEna = !ena;
    emit newSettings();
}

void SetupDialog::onAirspyMixerAGCstateChanged(int state)
{
    bool ena = (Qt::Unchecked == state);
    ui->airspyMixerGain->setEnabled(ena);
    ui->airspyMixerGainLabel->setEnabled(ena);
    ui->airspyMixerGainSlider->setEnabled(ena);

    m_settings.airspy.mixerAgcEna = !ena;
    emit newSettings();
}
#endif

void SetupDialog::setStatusLabel()
{
    switch (m_settings.inputDevice)
    {
    case InputDeviceId::RTLSDR:
        ui->statusLabel->setText("RTL SDR device connected");
        break;
    case InputDeviceId::RTLTCP:
        ui->statusLabel->setText("RTL TCP device connected");
        break;
    case InputDeviceId::RARTTCP:
        ui->statusLabel->setText("RART TCP device connected");
        break;
    case InputDeviceId::UNDEFINED:
        ui->statusLabel->setText("<span style=\"color:red\">No device connected</span>");
        break;
    case InputDeviceId::RAWFILE:
        ui->statusLabel->setText("Raw file connected");
        break;
    case InputDeviceId::AIRSPY:
        ui->statusLabel->setText("Airspy device connected");
        break;
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
    int inputDeviceInt = ui->inputCombo->itemData(index).toInt();
    ui->deviceOptionsWidget->setCurrentIndex(inputDeviceInt-1);
    adjustSize();

    ui->connectButton->setHidden(m_settings.inputDevice == static_cast<InputDeviceId>(inputDeviceInt));
    if (m_settings.inputDevice != static_cast<InputDeviceId>(inputDeviceInt))
    {   // selected input device does not match current input device

        switch (static_cast<InputDeviceId>(inputDeviceInt))
        {
        case InputDeviceId::RTLSDR:
            activateRtlSdrControls(false);
            break;            
        case InputDeviceId::RTLTCP:
            activateRtlTcpControls(false);
            break;
        case InputDeviceId::RARTTCP:
            break;
        case InputDeviceId::UNDEFINED:
            break;
        case InputDeviceId::RAWFILE:
            break;
        case InputDeviceId::AIRSPY:
#ifdef HAVE_AIRSPY
            activateAirspyControls(false);
#endif
            return;
        }
    }
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

        ui->connectButton->setVisible(true);
    }
}

void SetupDialog::resetInputDevice()
{
    switch (m_settings.inputDevice)
    {
    case InputDeviceId::RTLSDR:
        activateRtlSdrControls(false);
        break;
    case InputDeviceId::RTLTCP:
        activateRtlTcpControls(false);
        break;
    case InputDeviceId::RAWFILE:
        break;
    case InputDeviceId::RARTTCP:
    case InputDeviceId::UNDEFINED:
    case InputDeviceId::AIRSPY:
        return;
    }

    m_settings.inputDevice = InputDeviceId::UNDEFINED;
    setStatusLabel();
    ui->connectButton->setVisible(true);
}
