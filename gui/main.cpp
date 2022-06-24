#include "mainwindow.h"
#include <QApplication>
#include "config.h"

int main(int argc, char *argv[])
{

    QCoreApplication::setApplicationName("AbracaDABra");
    QCoreApplication::setApplicationVersion(PROJECT_VER);

    QApplication a(argc, argv);

#ifdef Q_OS_LINUX
    // Set icon
    a.setWindowIcon(QIcon(":/resources/appIcon-linux.png"));
#endif

    MainWindow w;
    w.show();        
    return a.exec();
}
