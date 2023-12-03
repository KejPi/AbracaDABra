# AbracaDABra
Abraca DAB radio is DAB and DAB+ Software Defined Radio (SDR) application. It works with cheap RTL-SDR (RTL2832U) USB sticks but also with Airspy R2 and Mini devices and with many devices supported by <a href="https://github.com/pothosware/SoapySDR/wiki">SoapySDR</a>. 

Application is based on Qt6 cross-platform software development framework and can run on any platform supported by Qt6. 
Prebuilt binaries are released for Windows, MacOS (both Intel and Apple Silicon) and Linux x86-64 and AARCH64 (AppImage). 
ArchLinux users can install AbracaDABra from <a href="https://aur.archlinux.org/packages/abracadabra">AUR</a>.
<p align="center" width="100%">
    <img width="889" alt="AbracaDABra application window" src="https://github.com/KejPi/AbracaDABra/assets/6438380/717ed65e-314b-4307-9e32-968c5582eeda"> 
</p>

## Features
* Following input devices are supported:
  * RTL-SDR (default device)
  * Airspy (optional) - only Airspy Mini and R2 are supported, HF+ devices do not work due to limited bandwidth. If you have problems with Airspy devices, please check the firmware version. Firmware update maybe required for correct functionality.
  * SoapySDR (optional)
  * RTL-TCP
  * Raw file input (in expert mode only, INT16 or UINT8 format)
* Band scan with automatic service list
* Service list management
* DAB (mp2) and DAB+ (AAC) audio decoding
* Announcements (all types including alarm test)
* Dynamic label (DL) and Dynamic label plus (DL+)
* MOT slideshow (SLS) and categorized slideshow (CatSLS) from PAD or from secondary data service
* RadioDNS
* SPI (Service and Programme information)
* Audio and data services reconfiguration
* Dynamic programme type (PTy)
* Ensemble structure view with all technical details
* Raw file dumping
* Audio recording
* Only band III and DAB mode 1 is supported.
* Simple user-friendly interface, trying to follow DAB _Rules of implementation (<a href="https://www.etsi.org/deliver/etsi_ts/103100_103199/103176/02.04.01_60/ts_103176v020401p.pdf">TS 103 176</a>)_
* Multiplatform (Windows, MacOS and Linux)
* Dark theme supported on all platforms
* Localization to German, Polish and Czech

## Basic mode
<p align="center" width="100%">
    <img width="663" alt="Application in basic mode" src="https://github.com/KejPi/AbracaDABra/assets/6438380/a3d0a656-9a7c-47bd-a2e2-bc8d283d080b">
</p>

Simple user interface that is focused on radio listening. Just select your favorite service from service list on the right side 
and enjoy the music with slideshow and DL(+). 
Service can be easily added to favorites by clicking "star" icon.  Most of the elements in UI have tool tip with more information.

## Expert mode
<p align="center" width="100%">
    <img width="889" alt="Application in expert mode" src="https://github.com/KejPi/AbracaDABra/assets/6438380/a1fae228-4148-477a-8c95-96fcc3080297">
    <img width="1152" alt="Ensemble details" src="https://github.com/KejPi/AbracaDABra/assets/6438380/d2d552c2-360c-4f44-b3c7-8baa214ba7f9">
</p>

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

## SPI application and RadioDNS

<a href="https://www.worlddab.org/dab/data-applications/service-and-programme-information">Service and programme information</a> (SPI) is supported partially. When SPI application is enabled in the settings and SPI is available for selected service and/or in the ensemble, application starts its decoding automatically.
SPI from X-PAD, from secondary service and from dedicated data service is supported, it can be even decoded from more sources in parallel. 
In general, SPI application is very slow and it takes several minutes to acquire all objects. AbracaDABra can use internet connection to download service logos and to retrieve service information using RadioDNS if it is supported by broadcaster of the selected service. 
Both internet connection and RadioDNS are optional features that are enabled by default.

<p align="center" width="100%">
    <img width="642" alt="Snímek obrazovky 2023-12-03 v 18 28 40" src="https://github.com/KejPi/AbracaDABra/assets/6438380/1b1ee56a-263f-4ade-90dd-36e1ddce52cd">
</p>

Service logos and internet download cache are stored in dedicated directory on the disk. Location of the cache is OS dependent:
* MacOS: `$HOME/Library/Caches/AbracaDABra/`
* Windows: `%USERPROFILE%\AppData\Local\AbracaDABra\cache\`
* Linux: `$HOME/.cache/AbracaDABra/`

## Audio recording

AbracaDABra features audio recording. Two options are available:
* Encoded DAB/DAB+ stream in MP2 or AAC format respectively
* Decoded audio in WAV format

Audio recording can be started and stopped from application menu. It can be also stopped from status bar. The recording files are stored automatically in predefined folder. 

<img width="1326" alt="Snímek obrazovky 2023-12-03 v 16 56 59" src="https://github.com/KejPi/AbracaDABra/assets/6438380/92bddcfe-614d-45ea-bb8d-10ede37b61cb">

_Note:_  Audio recording stops when enseble reconfigures or when any tuning operation is performed. 

## Expert settings
Some settings can only be changed by editting of the INI file. File location is OS dependent:
* MacOS: `$HOME/.config/AbracaDABra/AbracaDABra.ini`
* Windows: `%USERPROFILE%\AppData\Roaming\AbracaDABra\AbracaDABra.ini`
* Linux: `$HOME/.config/AbracaDABra/AbracaDABra.ini`

Following settings can be changed by editing AbracaDABra.ini:

      [General]
      audioFramework=0             # 0 means PortAudio (default if available), 1 means Qt audio framework
      keepServiceListOnScan=false  # delete (false, default value) or keep (true) current service list when running band scan 
                                   # note: favorites are not deleted
      
Application shall not run while changing INI file, otherwise the settings will be overwritten.

It is also possible to use other than default INI file using `--ini` or `-i` command line parameter.

## How to install

### MacOS
Download latest DMG file from [release page](https://github.com/KejPi/AbracaDABra/releases/latest) and install it like any other application. 

There are two versions of DMG, one for Intel Mac and the other from Apple Silicon Mac. Although Intel Mac application runs on both platforms, it is highly recommended to install Apple Silicon version if you have Apple Silicon Mac. Intel build requires at least MacOS Mojave (10.14) and Apple Silicon build needs at least MacOS BigSur (11).

<img width="752" alt="Snímek obrazovky 2023-12-03 v 19 03 37" src="https://github.com/KejPi/AbracaDABra/assets/6438380/395a0384-ae2d-48e9-aca1-56169d631a4d">

### Windows
Download latest Windows zip file from [release page](https://github.com/KejPi/AbracaDABra/releases/latest) and unpack it to any folder on your drive. 

RTL-SDR devices need WinUSB driver for correct functionality under Windows. To install it, use Zadig that can be downloaded from [zadig.akeo.ie](https://zadig.akeo.ie). Please follow installation steps described [here](https://www.rtl-sdr.com/getting-the-rtl-sdr-to-work-on-windows-10/). When the driver is installed, start `AbracaDABra.exe` and first band scan should start automatically when RTL-SDR device is recognized. 

Please note that other applications requiring Realtek driver will not work with WinUSB driver from Zadig. 

### Linux
The simplest way is to download latest AppImage file from [release page](https://github.com/KejPi/AbracaDABra/releases/latest), 
make it executable and run it. 

There are two versions of AppImage - one for Intel/AMD 64 bit CPU (x86_64) the other for ARM64 CPU (aarch64) so make sure you are downloading the one matching your hardware.

ArchLinux users can install AbracaDABra from <a href="https://aur.archlinux.org/packages/abracadabra">AUR</a>.

## How to build
Following libraries are required:
* Qt6
* libusb
* rtldsdr
* faad2 (default) or fdk-aac (optional)
* mpg123
* portaudio (optional but recommended)
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

4. Install application for all users (optional)

       sudo make install
       sudo ldconfig


_Note:_ `CMAKE_INSTALL_PREFIX` is `/usr/local` by default. It means that application installs to `/usr/local/bin` and library is installed to `/usr/local/lib`. Make sure that `/usr/local/lib` is in your `ldconfig` path, if it is not then use `LD_LIBRARY_PATH` evironment variable when running AbracaDABra:

       LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH /usr/local/bin/AbracaDABra &


## Known limitations
* Only service logos are currently supported from SPI application
       
