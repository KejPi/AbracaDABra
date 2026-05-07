#include "mac.h"
#import <Cocoa/Cocoa.h>
#import <MediaPlayer/MediaPlayer.h>

bool macDarkThemeAvailable()
{
    if (__builtin_available(macOS 10.14, *))
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool macIsInDarkTheme()
{
    if (__builtin_available(macOS 10.14, *))
    {
        auto appearance = [NSApp.effectiveAppearance bestMatchFromAppearancesWithNames:
                                                         @[ NSAppearanceNameAqua, NSAppearanceNameDarkAqua ]];
        return [appearance isEqualToString:NSAppearanceNameDarkAqua];
    }
    return false;
}

void macSetToDarkTheme()
{
    // https://stackoverflow.com/questions/55925862/how-can-i-set-my-os-x-application-theme-in-code
    if (__builtin_available(macOS 10.14, *))
    {
        [NSApp setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]];
    }
}

void macSetToLightTheme()
{
    // https://stackoverflow.com/questions/55925862/how-can-i-set-my-os-x-application-theme-in-code
    if (__builtin_available(macOS 10.14, *))
    {
        [NSApp setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameAqua]];
    }
}

void macSetToAutoTheme()
{
    if (__builtin_available(macOS 10.14, *))
    {
        [NSApp setAppearance:nil];
    }
}

// Tokens held to allow removal of remote command handlers
static id s_nextTrackToken = nil;
static id s_prevTrackToken = nil;
static id s_togglePlayPauseToken = nil;
static id s_playToken = nil;
static id s_pauseToken = nil;

void macSetupMediaRemoteCommands(QObject *app)
{
    MPRemoteCommandCenter *center = [MPRemoteCommandCenter sharedCommandCenter];

    // Enable only the commands we handle; disable the ones we don't
    center.playCommand.enabled = YES;   // on-screen widget sends this when muted
    center.pauseCommand.enabled = YES;  // on-screen widget sends this when unmuted
    center.stopCommand.enabled = NO;
    center.changePlaybackRateCommand.enabled = NO;
    center.seekForwardCommand.enabled = NO;
    center.seekBackwardCommand.enabled = NO;
    center.skipForwardCommand.enabled = NO;
    center.skipBackwardCommand.enabled = NO;
    center.changePlaybackPositionCommand.enabled = NO;
    center.nextTrackCommand.enabled = YES;
    center.previousTrackCommand.enabled = YES;
    center.togglePlayPauseCommand.enabled = YES;

    s_nextTrackToken = [center.nextTrackCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *) {
        QMetaObject::invokeMethod(app, "onNextFavoriteService", Qt::QueuedConnection);
        return MPRemoteCommandHandlerStatusSuccess;
    }];

    s_prevTrackToken = [center.previousTrackCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *) {
        QMetaObject::invokeMethod(app, "onPreviousFavoriteService", Qt::QueuedConnection);
        return MPRemoteCommandHandlerStatusSuccess;
    }];

    s_togglePlayPauseToken = [center.togglePlayPauseCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *) {
        QMetaObject::invokeMethod(app, "toggleMute", Qt::QueuedConnection);
        return MPRemoteCommandHandlerStatusSuccess;
    }];

    s_playToken = [center.playCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *) {
        QMetaObject::invokeMethod(app, "toggleMute", Qt::QueuedConnection);
        return MPRemoteCommandHandlerStatusSuccess;
    }];

    s_pauseToken = [center.pauseCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *) {
        QMetaObject::invokeMethod(app, "toggleMute", Qt::QueuedConnection);
        return MPRemoteCommandHandlerStatusSuccess;
    }];

    // Seed Now Playing info so the OS considers this app a media session
    NSMutableDictionary *info = [NSMutableDictionary dictionary];
    info[MPNowPlayingInfoPropertyIsLiveStream] = @YES;
    info[MPNowPlayingInfoPropertyMediaType] = @(MPNowPlayingInfoMediaTypeAudio);
    [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = info;
}

void macTeardownMediaRemoteCommands()
{
    MPRemoteCommandCenter *center = [MPRemoteCommandCenter sharedCommandCenter];
    if (s_nextTrackToken)
    {
        [center.nextTrackCommand removeTarget:s_nextTrackToken];
        s_nextTrackToken = nil;
    }
    if (s_prevTrackToken)
    {
        [center.previousTrackCommand removeTarget:s_prevTrackToken];
        s_prevTrackToken = nil;
    }
    if (s_togglePlayPauseToken)
    {
        [center.togglePlayPauseCommand removeTarget:s_togglePlayPauseToken];
        s_togglePlayPauseToken = nil;
    }
    if (s_playToken)
    {
        [center.playCommand removeTarget:s_playToken];
        s_playToken = nil;
    }
    if (s_pauseToken)
    {
        [center.pauseCommand removeTarget:s_pauseToken];
        s_pauseToken = nil;
    }
    [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = nil;
}

void macUpdateNowPlayingInfo(const QString &stationName)
{
    NSMutableDictionary *info = [NSMutableDictionary dictionary];
    // Preserve existing DL subtitle so a station-name refresh doesn't clear it
    NSDictionary *existing = [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo;
    if (existing[MPMediaItemPropertyArtist])
    {
        info[MPMediaItemPropertyArtist] = existing[MPMediaItemPropertyArtist];
    }
    info[MPMediaItemPropertyTitle] = stationName.toNSString();
    info[MPNowPlayingInfoPropertyIsLiveStream] = @YES;
    info[MPNowPlayingInfoPropertyMediaType] = @(MPNowPlayingInfoMediaTypeAudio);
    [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = info;
}

void macUpdateNowPlayingSubtitle(const QString &dl)
{
    NSMutableDictionary *info = [[MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo mutableCopy];
    if (!info)
    {
        return;
    }
    if (dl.isEmpty())
    {
        [info removeObjectForKey:MPMediaItemPropertyArtist];
    }
    else
    {
        info[MPMediaItemPropertyArtist] = dl.toNSString();
    }
    [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = info;
}

void macUpdateNowPlayingPlaybackState(bool isPlaying)
{
    if (__builtin_available(macOS 11.0, *))
    {
        [MPNowPlayingInfoCenter defaultCenter].playbackState =
            isPlaying ? MPNowPlayingPlaybackStatePlaying : MPNowPlayingPlaybackStatePaused;
    }
}
