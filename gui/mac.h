#ifndef MAC_H
#define MAC_H

#include <QObject>

bool macDarkThemeAvailable();
bool macIsInDarkTheme();
void macSetToDarkTheme();
void macSetToLightTheme();
void macSetToAutoTheme();

// This is Qt wrapper class to handle dock icon menu
class DockMenuHandler : public QObject {
    Q_OBJECT
public:
    DockMenuHandler(const DockMenuHandler & other) = delete;   // delete copy constructor
    ~DockMenuHandler();
    static DockMenuHandler *getInstance();
    void onMuteItemClicked();
    void setMuteItemLabel(const QString & label);
signals:
    void muteItemClicked();
private:
    DockMenuHandler(QObject *parent = nullptr);

};

#endif // MAC_H
