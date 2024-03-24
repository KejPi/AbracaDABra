#include "mac.h"
#import <Cocoa/Cocoa.h>

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



// ===================================================================
// Dock icon menu - bot Qt and Cocoa classes are implemented as singleton
static DockMenuHandler* s_instancePtr = nullptr;

@interface DockMenuHandlerCocoa : NSObject <NSApplicationDelegate>

// Delegate method to return the dock menu.
- (NSMenu*)applicationDockMenu:(NSApplication*)sender;

// Method to set Mute item label
- (void)setMuteItemLabel:(NSString*)label;

@end

static DockMenuHandlerCocoa * s_instancePtrCocoa = nullptr;

@implementation DockMenuHandlerCocoa {
    NSMenu *dockMenu;
    NSMenuItem *muteMenuItem;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        dockMenu = [[NSMenu alloc] initWithTitle:@"DockMenu"];
        muteMenuItem = [[NSMenuItem alloc] initWithTitle:@"Mute" action:@selector(muteItemClicked:) keyEquivalent:@""];
        [dockMenu addItem:muteMenuItem];
    }
    return self;
}

- (void)muteItemClicked:(id)sender {
    // Handle menu item click
    // NSLog(@"Mute item clicked!");
    s_instancePtr->onMuteItemClicked();
}

- (NSMenu *)applicationDockMenu:(NSApplication *)sender {
    return dockMenu;
}

- (void)setMuteItemLabel:(NSString*)label {
    [muteMenuItem setTitle:(label)];
}
@end

DockMenuHandler::DockMenuHandler(QObject * parent) : QObject(parent)
{
    // Call Objective-C++ method to add dock menu item
    s_instancePtrCocoa = [[DockMenuHandlerCocoa alloc] init];
    CFRetain(s_instancePtrCocoa);

    [NSApp setDelegate:(s_instancePtrCocoa)];
    [s_instancePtrCocoa release];
}

DockMenuHandler::~DockMenuHandler()
{
    CFRelease(s_instancePtrCocoa);
}

DockMenuHandler *DockMenuHandler::getInstance()
{
  if (s_instancePtr == nullptr)
  {
      s_instancePtr = new DockMenuHandler();
  }
  return s_instancePtr;
}

void DockMenuHandler::onMuteItemClicked()
{
    emit muteItemClicked();
}

void DockMenuHandler::setMuteItemLabel(const QString &label)
{
    [s_instancePtrCocoa setMuteItemLabel:(label.toNSString())];
}
