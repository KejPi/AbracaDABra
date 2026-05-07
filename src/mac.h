#ifndef MAC_H
#define MAC_H

#include <QObject>
#include <QString>

bool macDarkThemeAvailable();
bool macIsInDarkTheme();
void macSetToDarkTheme();
void macSetToLightTheme();
void macSetToAutoTheme();

void macSetupMediaRemoteCommands(QObject *app);
void macTeardownMediaRemoteCommands();
void macUpdateNowPlayingInfo(const QString &stationName);
void macUpdateNowPlayingSubtitle(const QString &dl);
void macUpdateNowPlayingPlaybackState(bool isPlaying);

#endif  // MAC_H
