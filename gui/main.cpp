#include "mainwindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include "config.h"

int main(int argc, char *argv[])
{

    QCoreApplication::setApplicationName("AbracaDABra");
    QCoreApplication::setApplicationVersion(PROJECT_VER);

    QApplication a(argc, argv);


    QCommandLineParser parser;
    parser.setApplicationDescription("Abraca DAB radio: DAB/DAB+ Software Defined Radio (SDR)");
    parser.addHelpOption();
    parser.addVersionOption();

    // An option with a value
    QCommandLineOption iniFileOption(QStringList() << "i" << "ini",
            "Optional INI file. If not specified AbracaDABra.ini in system directory will be used.", "ini");
    parser.addOption(iniFileOption);

    // Process the actual command line arguments given by the user
    parser.process(a);

    QString iniFile = parser.value(iniFileOption);

#ifdef Q_OS_LINUX
    // Set icon
    a.setWindowIcon(QIcon(":/resources/appIcon-linux.png"));
#endif

    MainWindow w(iniFile);
    w.show();        
    return a.exec();
}
