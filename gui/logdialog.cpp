#include <QDebug>
#include <QString>
#include <QScrollBar>
#include <QFontDatabase>
#include <QFileDialog>
#include <QFile>
#include <QStandardPaths>
#include <QLoggingCategory>
#include <QClipboard>
#include "logdialog.h"
#include "ui_logdialog.h"

Q_DECLARE_LOGGING_CATEGORY(application)

class LogModel : public QAbstractListModel
{
public:
    LogModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return m_msgList.size();
    }
    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid())
        {
            return QVariant();
        }

        if (m_msgList.size() > index.row())
        {
            switch (role)
            {
            case Qt::FontRole:
                return QVariant(fixedFont);
            case Qt::ForegroundRole:
                return QVariant(m_msgList.at(index.row()).color);
            case Qt::DisplayRole:
                return m_msgList.at(index.row()).msg;
            default:
                return QVariant();
            }
        }

        return QVariant();
    }
    bool setData(const QModelIndex & index, const QVariant & value, int role) override
    {
        if (!index.isValid())
        {
            return false;
        }
        if (m_msgList.size() > index.row())
        {
            m_msgList[index.row()].msg = value.toString();
            switch (static_cast<QtMsgType>(role))
            {
                case QtDebugMsg:
                    m_msgList[index.row()].color = QColor(Qt::blue);
                break;
                case QtInfoMsg:
                    m_msgList[index.row()].color = QColor(Qt::black);
                break;
                case QtWarningMsg:
                    m_msgList[index.row()].color = QColor(Qt::magenta);
                break;
                case QtCriticalMsg:
                    m_msgList[index.row()].color = QColor(Qt::red);
                break;
                case QtFatalMsg:
                    m_msgList[index.row()].color = QColor(Qt::red);
                break;
            }
            return true;
        }
        else
        {
            return false;
        }
    }
    bool insertRows(int position, int rows, const QModelIndex &index = QModelIndex()) override
    {
        beginInsertRows(QModelIndex(), position, position+rows-1);

        for (int row = 0; row < rows; ++row) {
            m_msgList.insert(position, LogItem());
        }

        endInsertRows();
        return true;
    }
    bool removeRows(int position, int rows, const QModelIndex &index = QModelIndex()) override
    {
        beginRemoveRows(QModelIndex(), position, position+rows-1);

        for (int row = 0; row < rows; ++row) {
            m_msgList.removeAt(position);
        }

        endRemoveRows();
        return true;
    }
private:
    const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    struct LogItem
    {
        QString msg;
        QColor color;
    };

    QList<struct LogItem> m_msgList;
};


LogDialog::LogDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LogDialog)
{
    ui->setupUi(this);

    m_dataModel = new LogModel(this);
    ui->logListView->setModel(m_dataModel);
    ui->logListView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    static const char kViewAtBottom[] = "viewAtBottom";
    auto *scrollBar = ui->logListView->verticalScrollBar();
    Q_ASSERT(scrollBar);
    auto rescroller = [scrollBar]() mutable {
        if (scrollBar->property(kViewAtBottom).isNull())
        {
            scrollBar->setProperty(kViewAtBottom, true);
        }
        auto const atBottom = scrollBar->property(kViewAtBottom).toBool();
        if (atBottom)
        {
            scrollBar->setValue(scrollBar->maximum());
        }
    };
    QObject::connect(scrollBar, &QAbstractSlider::rangeChanged, ui->logListView, rescroller, Qt::QueuedConnection);
    QObject::connect(scrollBar, &QAbstractSlider::valueChanged, ui->logListView, [scrollBar] {
        auto const atBottom = scrollBar->value() == scrollBar->maximum();
        scrollBar->setProperty(kViewAtBottom, atBottom);
    });

    QObject::connect(ui->clearButton, &QPushButton::clicked, this, [this] { m_dataModel->removeRows(0, m_dataModel->rowCount()); } );
    QObject::connect(ui->saveButton, &QPushButton::clicked, this, &LogDialog::saveLogToFile);
    QObject::connect(ui->copyButton, &QPushButton::clicked, this, &LogDialog::copyToClipboard);
}

LogDialog::~LogDialog()
{
    delete ui;
}

QAbstractItemModel *LogDialog::getModel() const
{
    return ui->logListView->model();
}

void LogDialog::saveLogToFile()
{
    QString f = QString("%1/AbracaDABra_%2.log").arg(QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                            QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"));

    QString fileName = QFileDialog::getSaveFileName(this,
                                            tr("Save application log"),
                                            QDir::toNativeSeparators(f),
                                            tr("Log files")+" (*.log)");

    if (!fileName.isEmpty())
    {
        QFile logFile(fileName);
        if (!logFile.open(QIODevice::WriteOnly))
        {
            qCCritical(application) << "Unable to open file: " << fileName;
            return;
        }

        qCInfo(application) << "Writing log to:" << fileName;

        QTextStream stream(&logFile);
        for (int n = 0; n < m_dataModel->rowCount(); ++n)
        {
            stream << m_dataModel->data(m_dataModel->index(n, 0)).toString() << Qt::endl;
        }
        stream.flush();
        logFile.close();
    }
    else
    { /* no file selected */ }
}

void LogDialog::copyToClipboard()
{
    QString logText("");
    QTextStream stream(&logText);
    for (int n = 0; n < m_dataModel->rowCount(); ++n)
    {
        stream << m_dataModel->data(m_dataModel->index(n, 0)).toString() << Qt::endl;
    }
    stream.flush();

    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(logText);
}
