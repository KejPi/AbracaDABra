#include <QPushButton>
#include <QDesktopServices>
#include "config.h"
#include "updatedialog.h"
#include "ui_updatedialog.h"

UpdateDialog::UpdateDialog(const QString &version, const QString &releaseNotes, Qt::WindowFlags f, QWidget *parent)
    : QDialog(parent, f)
    , ui(new Ui::UpdateDialog)
{
    ui->setupUi(this);
    setModal(true);
    setWindowTitle(tr("Application update"));
    ui->title->setText(tr("AbracaDABra update available"));
    ui->currentLabel->setText(tr("Current version: %1").arg(PROJECT_VER));
#if 0
    ui->availableLabel->setText(tr("Available version: %1 (<a href=\"https://github.com/KejPi/AbracaDABra/releases/tag/%1\">link</a>)").arg(version));
    connect(
        ui->availableLabel, &QLabel::linkActivated,
        [=]( const QString & link ) { QDesktopServices::openUrl(QUrl::fromUserInput(link)); }
        );
#else
    ui->availableLabel->setText(tr("Available version: %1").arg(version));
#endif

    auto font = ui->title->font();
    font.setPointSize(font.pointSize() + 2);
    ui->title->setFont(font);
    ui->releaseNotes->setReadOnly(true);
    ui->releaseNotes->setMarkdown("**Changelog:**\n\n" + releaseNotes);
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Do not show again"));
    auto goTo = new QPushButton(tr("Go to release page"), this);
    connect(goTo, &QPushButton::clicked, this, [this, version]() {
        QDesktopServices::openUrl(QUrl::fromUserInput(QString("https://github.com/KejPi/AbracaDABra/releases/tag/%1").arg(version)));
    });
    ui->buttonBox->addButton(goTo, QDialogButtonBox::ActionRole);

    setFixedSize(size());
}

UpdateDialog::~UpdateDialog()
{
    delete ui;
}
