# AbracaDABra
Abraca DAB radio is DAB and DAB+ Software Defined Radio (SDR) application. It works with cheap RTL-SDR (RTL2832U) USB sticks but also with AirSpy devices and with devices supported by <a href="https://github.com/pothosware/SoapySDR/wiki">SoapySDR</a>. 

Application is based on Qt6 cross-platform software development framework and can run on any platform supported by Qt6. 
Prebuilt binaries are released for Windows, MacOS (both Intel and Apple Silicon) and Linux x86-64 (AppImage). 
AbracaDABra can run also under Linux AARCH64 (ARM64) for example on Raspberry Pi 4. ArchLinux users can install AbracaDABra from <a href="https://aur.archlinux.org/packages/abracadabra">AUR</a>.

<img width="1634" alt="Snímek obrazovky 2022-06-28 v 22 32 25" src="https://user-images.githubusercontent.com/6438380/176279785-1e212201-3c1d-428f-9416-b1b0b464238b.png">

## Features
* Following input devices are supported:
  * RTL-SDR (default device)
  * Airspy (optional)
  * SoapySDR (optional)
  * RTL-TCP
  * Raw file input (in expert mode only, INT16 or UINT8 format)
* Band scan with automatic service list
* Service list management
* DAB (mp2) and DAB+ (AAC) audio decoding
* Announcements (all types including alarm test)
* Dynamic label (DL) and Dynamic label plus (DL+)
* MOT slideshow (SLS) and categorized slideshow (CatSLS) from PAD or from secondary data service
* Audio services reconfiguration
* Dynamic programme type (PTy)
* Ensemble structure view with all technical details
* Raw file dumping
* Only band III and DAB mode 1 is supported.
* Simple user-friendly interface, trying to follow DAB _Rules of implementation (<a href="https://www.etsi.org/deliver/etsi_ts/103100_103199/103176/02.04.01_60/ts_103176v020401p.pdf">TS 103 176</a>)_
* Multiplatform (Windows, MacOS and Linux)
* Dark theme supported on all platforms
* Localization to German, Polish and Czech

## Basic mode
<img width="663" alt="Snímek obrazovky 2022-06-28 v 22 19 47" src="https://user-images.githubusercontent.com/6438380/176277787-7737ca9b-adb1-4d91-bf5b-bd9331d27663.png">

Simple user interface that is focused on radio listening. Just select your favorite service from service list on the right side 
and enjoy the music with slideshow and DL(+). 
Service can be easily added to favorites by clicking "star" icon.  Most of the elements in UI have tool tip with more information.

## Expert mode
<img width="921" alt="Snímek obrazovky 2023-02-25 v 19 29 45" src="https://user-images.githubusercontent.com/6438380/221374046-2d5558fb-fe7e-4a32-b754-1174cb525bc0.png">

In addition to basic mode, expert mode shows ensemble tree with structure of services and additional details of currenly tuned service. 
Furthermore it is possible to change the DAB channel manually in this mode. 
This is particularly useful when antenna adjustment is done in order to receive ensemble that is not captured during automatic band scan.
Expert mode can be enabled in Settings dialog.

## DAB Announcements support

An announcement is a period of elevated interest within an audio programme. It is typically a spoken audio message, often with a lead-in and lead-out 
audio pattern (for example, a musical jingle). It may refer to various types of information such as traffic, news, sports and others. [<a href="https://www.etsi.org/deliver/etsi_ts/103100_103199/103176/02.04.01_60/ts_103176v020401p.pdf">TS 103 176</a>]

All DAB(+) announcement types are supported by AbracaDABra, including test alarm feature (ClusterID 0xFE according to <a href="https://www.etsi.org/deliver/etsi_ts/103100_103199/103176/02.04.01_60/ts_103176v020401p.pdf">TS 103 176</a>). 
The application is monitoring an announcement switching information and when the announcement is active, AbracaDABra switches audio output to a target 
subchannel (service). This behavior can be disabled by unchecking a particular announcement type in a Setup dialog. 
If an announcement occurs on the currently tuned service, it is only indicated by an icon after the service name. This icon can be clicked on, which 
suspends/resumes the ongoing announcement coming from another service. OE Announcements are not supported. 

The option "Bring window to foreground" tries to focus the application window when the alarm announcement starts. 
The alarm announcements carry emergency warning information that is of utmost importance to all radio listeners and they have the highest priority 
(according to <a href="https://www.etsi.org/deliver/etsi_ts/103100_103199/103176/02.04.01_60/ts_103176v020401p.pdf">TS 103 176</a>) in the sense that alarm announcement cannot be disabled and it can interrupt any ongoing regular announcement.

<img width="1279" alt="Snímek obrazovky 2023-01-12 v 22 07 17" src="https://user-images.githubusercontent.com/6438380/212181613-a7e163e2-d6e1-46cc-bf3d-6dfce276a8ae.png">


Announcements from other service display a thematic placeholder. <a href="https://www.flaticon.com/authors/basic-miscellany/lineal-color" title="linear color">The artwork of placeholders are created by Smashicons - Flaticon</a>

## Expert settings
Some settings can only be changed by editting of the INI file. File location is OS dependent:
* MacOS & Linux: `$HOME/.config/AbracaDABra/AbracaDABra.ini`
* Windows: `%USERPROFILE%\AppData\Roaming\AbracaDABra\AbracaDABra.ini`

Following settings can be changed by editing AbracaDABra.ini:

      [RTL-SDR]
      bandwidth=0     # 0 means automatic bandwidth, positive value means bandwidth value in Hz (e.g. bandwidth=1530000 is 1.53MHz BW)
      bias-T=false    # set to true to enable bias-T
      
      [AIRSPY]
      bias-T=false    # set to true to enable bias-T      

      [SOAPYSDR]
      bandwidth=0     # 0 means default bandwidth, positive value means bandwidth value in Hz (e.g. bandwidth=1530000 is 1.53MHz BW)

Application shall not run while changing INI file, otherwise the settings will be overwritten.

It is also possible to use other than default INI file using `--ini` or `-i` command line parameter.

## How to install

### MacOS

Download latest DMG file from [release page](https://github.com/KejPi/AbracaDABra/releases/latest) and install it like any other application. 

There are two versions of DMG, one for Intel Mac and the other from Apple Silicon Mac. Although Intel Mac application runs on both platforms, it is highly recommended to install Apple Silicon version if you have Apple Silicon Mac. Intel build requires at least MacOS Mojave (10.14) and Apple Silicon build needs at least MacOS BigSur (11).

### Windows

Download latest Windows zip file from [release page](https://github.com/KejPi/AbracaDABra/releases/latest) and unpack it to any folder on your drive. 

RTL-SDR and Airspy devices need WinUSB driver for correct functionality under Windows. To install it, use Zadig that can be downloaded from [zadig.akeo.ie](https://zadig.akeo.ie). Please follow installation steps described [here](https://www.rtl-sdr.com/getting-the-rtl-sdr-to-work-on-windows-10/). When the driver is installed, start `AbracaDABra.exe` and first band scan should start automatically when RTL-SDR device is recognized. 

Please note that other applications requiring Realtek driver will not work with WinUSB driver from Zadig. 

### Linux

The simplest way is to download latest AppImage file from [release page](https://github.com/KejPi/AbracaDABra/releases/latest), 
make it executable and run it. 

There are two versions of AppImage - one for Intel/AMD 64 bit CPU (x86_64) the other for ARM64 CPU (aarch64) so make soure you are donwloading the one matching your hardware.

ArchLinux users can install AbracaDABra from <a href="https://aur.archlinux.org/packages/abracadabra">AUR</a>.

## How to build
Following libraries are required:
* Qt6
* libusb
* rtldsdr
* faad2 (default) or fdk-aac (optional)
* mpg123
* portaudio
* airspy (optional)
* SoapySDR (optional)

For a fresh Ubuntu 22.04 installation you can use the following commands:

       sudo apt-get install git cmake build-essential mesa-common-dev
       sudo apt-get install libusb-dev librtlsdr-dev libfaad2 mpg123 libmpg123-dev libfaad-dev
       sudo apt-get install portaudio19-dev qt6-base-dev qt6-multimedia-dev libqt6svg6-dev rtl-sdr   
       sudo apt-get install qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools
       
Optional Airspy support:       

       sudo apt-get install libairspy-dev

Optional SoapySDR support:       

       sudo apt-get install libsoapysdr-dev

Then clone the project:

       git clone https://github.com/KejPi/AbracaDABra/
       cd AbracaDABra

1. Create build directory inside working copy and change to it

       mkdir build
       cd build

2. Run cmake

       cmake ..
       
    Optional Airspy support:          
       
       cmake .. -DAIRSPY=ON

    Optional SoapySDR support:          
       
       cmake .. -DSOAPYSDR=ON

3. Run make

       make             
       
## Known limitations
* Reconfiguration not supported for data services
* Only SLS data service is currently supported
       
