# AbracaDABra
Abraca DAB radio is DAB and DAB+ Software Defined Radio (SDR) application. It works with cheap RTL-SDR (RTL2832U) USB sticks but also with Airspy R2 and Airspy Mini devices and with many devices supported by <a href="https://github.com/pothosware/SoapySDR/wiki">SoapySDR</a>. 

Application is based on Qt6 cross-platform software development framework and can run on any platform supported by Qt6. 
Prebuilt binaries are released for Windows, macOS (both Intel and Apple Silicon) and Linux x86-64 and AARCH64 (AppImage). 
ArchLinux users can install AbracaDABra from <a href="https://aur.archlinux.org/packages/abracadabra">AUR</a>.
<p align="center" width="100%">
    <img width="889" alt="AbracaDABra application window" src="https://github.com/KejPi/AbracaDABra/assets/6438380/717ed65e-314b-4307-9e32-968c5582eeda"> 
</p>

## Features
* Following input devices are supported:
  * RTL-SDR (default device)
  * [Airspy](https://airspy.com) (optional) - only Airspy Mini and R2 are supported, HF+ devices do not work due to limited bandwidth. If you have problems with Airspy devices, please check the firmware version. Firmware update maybe required for correct functionality.
  * [SoapySDR](https://github.com/pothosware/SoapySDR/wiki) (optional)
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
* Raw file dumping (optionally with XML header)
* Audio recording
* Only band III and DAB mode 1 are supported.
* Simple user-friendly interface, trying to follow DAB _Rules of implementation (<a href="https://www.etsi.org/deliver/etsi_ts/103100_103199/103176/02.04.01_60/ts_103176v020401p.pdf">TS 103 176</a>)_
* Multiplatform (Windows, macOS and Linux)
* Dark theme supported on all platforms
* Localization to German, Czech and Polish (not complete)

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

In addition to basic mode, expert mode shows ensemble tree with structure of services and additional details of currently tuned service. 
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
<a href="https://www.worlddab.org/dab/data-applications/service-and-programme-information">Service and programme information</a> (SPI) application is supported. 
When SPI application is enabled in the settings and SPI is available for selected service and/or in the ensemble, application starts its decoding automatically.
SPI from X-PAD, from secondary service and from dedicated data service is supported, it can be even decoded from more sources in parallel. 
In general, SPI application is very slow and it takes several minutes to acquire all objects. AbracaDABra can use internet connection to download service logos and to retrieve service information using RadioDNS if it is supported by broadcaster of the selected service. 
Both internet connection and RadioDNS are optional features that are enabled by default.

<p align="center" width="100%">
    <img width="659" alt="uaSettings_20240209" src="https://github.com/KejPi/AbracaDABra/assets/6438380/8680dc2c-3cce-47f1-a87d-525121ab6270">
</p>

Service logos, XML files and internet download cache are stored in dedicated directory on the disk. Location of the cache is OS dependent:
* macOS: `$HOME/Library/Caches/AbracaDABra/`
* Windows: `%USERPROFILE%\AppData\Local\AbracaDABra\cache\`
* Linux: `$HOME/.cache/AbracaDABra/`

AbracaDABra visualizes Electronic Program Guide (EPG) if it is provided by broadcaster in SPI application and/or over RadioDNS. In such case "Program guide..." menu item becomes active and user can browse through services program in interactive user interface.

<p align="center" width="100%">
<img width="1112" alt="EPG_20240209" src="https://github.com/KejPi/AbracaDABra/assets/6438380/fc2712e8-2b9d-483e-b089-169a77efa98f">
<p align="center" width="100%">

EPG dialog supports dragging or scrolling by mouse, specific program detail can be displayed by clicking on item. Audio service can be selected by clicking on the service name on the left side. 

## User application data storage
AbracaDABra can be configured to store all incoming data from slideshow (SLS) and/or SPI application. The configuration consists of storage folder that is common for both applications 
and subfolder template configurable for each application individually. Storage of data can be enabled for each application individually.

<p align="center" width="100%">
    <img width="659" alt="uaSettings_20240209" src="https://github.com/KejPi/AbracaDABra/assets/6438380/8680dc2c-3cce-47f1-a87d-525121ab6270">
</p>

Default storage folder is OS dependent:
* macOS: `$HOME/Downloads/AbracaDABra/`
* Windows: `%USERPROFILE%\Downloads\AbracaDABra\`
* Linux: `$HOME/Downloads/AbracaDABra/`

Overwrite checkbox enables overwriting of the files with the same name. If it is not checked (default), new file with existing name will be ignored and not stored.

Subfolder template for each application can be created individually. Following tokens are supported:

| SLS   | SPI   | Token | Description | Example |
| :---: | :---: | :---- | :-----------| :-------|
| * | * | `{serviceId}` | Current audio service ID (ECC+SID) as hexadecimal number | `e01234` |
| * | * | `{ensId}`     | Current ensemble ID (ECC+UEID) as hexadecimal number | `e0eeee` |
|   | * | `{scId}`      | Data service component ID as 12bit decimal number | `47` |
| * | * | `{transportId}` | MOT Object transport ID as 16bit decimal number, it shall uniquely identify "file" within single data MOT channel. [[EN 301 234 7.2.7.4](http://www.etsi.org/deliver/etsi_en/301200_301299/301234/02.01.01_60/en_301234v020101p.pdf)]  | `123456` |
|   | * | `{directoryId}` | Directory ID is transport ID of MOT directory. | `654321` |
| * | * | `{contentName}` | MOT object content name parameter. It is used to uniquely identify an object. [[EN 301 234 6.2.2.1.1](http://www.etsi.org/deliver/etsi_en/301200_301299/301234/02.01.01_60/en_301234v020101p.pdf)]  | `stationLogo` |
| * |   | `{contentNameWithExt}` | MOT object content name parameter with extension that is added based on MIME type if content name is without extension. | `stationLogo.jpeg` |

_Important notes:_
* Path template uses slash `/` as directories separator, backslash `\` is forbidden character.
* Path template can contain each token multiple times. All occurrences are then replaces by respective value. On the other hand, invalid token is left in the file path as it is.
* MOT object content name does not have to be a valid filename for any operating system, thus application replaces potentially "dangerous" characters by underscore `_`. For example this is perfectly valid content name: `http://www.example.com:80/logo1`. 
  Application transforms it to following string when subfolder path is created: `http___www.example.com_80_logo1`
* SLS is transferred in so called header mode [[EN 301 234 7.1](http://www.etsi.org/deliver/etsi_en/301200_301299/301234/02.01.01_60/en_301234v020101p.pdf)]. In this mode only one MOT object (slide) is available.
* SPI application uses directory mode [[EN 301 234 7.2](http://www.etsi.org/deliver/etsi_en/301200_301299/301234/02.01.01_60/en_301234v020101p.pdf)]. In this mode one directory is available but this directory contains several MOT objects in so called carrousel. These objects ("files") have their own transport ID and content name. Directory typically contains several station logos and binary encoded XML files [[TS 102 371](https://www.etsi.org/deliver/etsi_ts/102300_102399/102371/03.03.01_60/ts_102371v030301p.pdf)]. When SPI data is stored, application stores all data from carrousel and additionally also decoded XML file for each binary encoded file.
* MOT directory ID is one of the objects transmitted in SPI application so it has its own transport ID that is called `{directoryID}` in the template.
* SPI application can process data from multiple sources at the same time. These sources are packet data service components within the ensemble each having unique service component ID and XPAD data of selected audio service. Application assigns "virtual" service component ID 65535 to XPAD data. User should take parallel processing into considerations when defining path template - for example service component ID `{scId}` shall be unique but directory ID `{directoryId}` is generally not unique within ensemble.
* SPI data retrieved using RadioDNS feature are not stored.

Default template for SLS application is `SLS/{serviceId}/{contentNameWithExt}`. Using examples from the table, it would store this file under macOS and Linux: `$HOME/Downloads/AbracaDABra/SLS/e01234/stationLogo.jpeg`

Default template for SPI application is `SPI/{ensId}/{scId}_{directoryId}/{contentName}`. Using examples from the table, it would store this file under macOS and Linux: `$HOME/Downloads/AbracaDABra/SPI/e0eeee/47_654321/stationLogo`

## Audio recording
AbracaDABra features audio recording. Two options are available:
* Encoded DAB/DAB+ stream in MP2 or AAC format respectively
* Decoded audio in WAV format

Audio recording can be started and stopped from application menu. It can be also stopped from status bar. The recording files are stored automatically in predefined folder. 

<img width="1326" alt="Snímek obrazovky 2023-12-03 v 16 56 59" src="https://github.com/KejPi/AbracaDABra/assets/6438380/92bddcfe-614d-45ea-bb8d-10ede37b61cb">

_Note:_  Audio recording stops when ensemble reconfigures or when any tuning operation is performed. 

### Audio recording schedule
Audio recording can be also planned in advance. Plan is defined by:
* Name
* Start time
* Duration (End time is calculated from duration)
* Service to be recorded

New audio recording schedule can be added from Audio recording schedule dialog (see screenshot below) accessible from application menu or from EPG (Schedule audio recording button). 

_Notes:_
* Scheduled audio recording uses the same settings as immediate audio recording.
* Audio recording start time is taken from system time not from DAB time.
* When scheduled recording is about to start, the dialog with warning to user pops up (30 seconds before scheduled time) and then service to be recorded is selected about 10 seconds before scheduled time.
* Application must run in order to make scheduled recoding happen. In other words, application does not automatically start when audio recording is scheduled.
* Any ongoing recording is blocking scheduled item to start.
* Application does not block user to define overlapping recording schedules. If user adds shedule items that are overlaping, conflict is indicated by red triangle icon in audio recording schedule table ("Schedule #3" in the screenshot below). In case of conflicting schedules, the first scheduled item is completed as defined ("Schedule #2" in screenshot) and then the conflicting item starts. Recording is always performed in the order shown in the table.
* Ongoing scheduled audio recording can be stopped anytime from application menu or from status bar like any other audio recording.
* If service to be recorded is available from more ensembles, the last used ensemble is used for recording (like when user selects services from the service list).

<img width="738" alt="audioRecordingSchedule" src="https://github.com/KejPi/AbracaDABra/assets/6438380/7aa07e1f-ee41-44b2-bdb6-41e65d46261e">

## TII decoding

TII decoder is considered to be advanced feature thus it is only available when application is in [Expert mode](#expert-mode). Before using it, the feature needs to be configured from application settings:

<img width="642" alt="TII_settings" src="https://github.com/user-attachments/assets/3bf9cfd5-9489-478f-ba35-4ad0adad5061">

First update the DAB transmitter database kindly provided by [FMLIST](https://www.fmlist.org). _Note:_ you might need to configure network proxy in Others tab. 

Then set your location, 3 options are available:
* System - location provided by system. This option works well under macOS and requires granting Location permission for the application.
* Manual - manual configuration of the location using latitude, longitude format. Using [Google Maps](https://www.google.com/maps/) as suggested by "Tip" in the dialog is probably the easiest way to get your coordinated in expected format.
* NMEA Serial Port - using serial port GPS receiver compatible with NMEA standard. In this case you need to specify serial port device. _Note:_ Some users reported it is working but it was not tested by developer.

_Note:_ Map is centered in Prague when location is not valid.

Last TII related option is a possibility to enable spectrum plot. This option is mostly for debug purposes. If enabled it displays spectrum-like plot in the TII Decoder dialog that shows sum of carrier pairs calculated from NULL symbol of DAB signal. 

<img width="915" alt="TII_1" src="https://github.com/user-attachments/assets/e842a718-258a-4f30-8b23-daae828d7966">

TII Decoder dialog shows an interactive map provided by [OpenStreetMap](https://www.openstreetmap.org/copyright), table of detected transmitter codes and ensemble information. Blue dot shows location configured in Settings.
Table shows TII code (Main & Sub), relative transmitter level, distance and azimuth if position of the transmitter is known. Table can be sorted by any column by clicking on its header, by default it is sorted by Level so that the strongest transmitter is on top.
To see details of particular transmitter, you can either select it by clicking on the row in table or you can click on position bubble in the map. Transmitter details are shown above the map in bottom right corner like in the screenshot above. 

## Expert settings
Some settings can only be changed by editing of the INI file. File location is OS dependent:
* macOS: `$HOME/.config/AbracaDABra/AbracaDABra.ini`
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

### macOS
Download latest DMG file from [release page](https://github.com/KejPi/AbracaDABra/releases/latest) and install it like any other application. 

There are two versions of DMG, one for Intel Mac and the other from Apple Silicon Mac. Although Intel Mac application runs on both platforms, it is highly recommended to install Apple Silicon version if you have Apple Silicon Mac. Intel build requires at least MacOS Mojave (10.14) and Apple Silicon build needs at least MacOS BigSur (11).

<img width="752" alt="Snímek obrazovky 2023-12-03 v 19 03 37" src="https://github.com/KejPi/AbracaDABra/assets/6438380/395a0384-ae2d-48e9-aca1-56169d631a4d">

_Note:_ It seems that Apple did some changes in Sonoma and as a result Intel binaries do not run under macOS Catalina or older. Last binary that is known to run is version 2.2.4. Nevertheless the application code is still compatible with Qt6.4 thus it is possible to build AbracaDABra from source code on device running old macOS version.

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
* Qt >= 6.5.0
* libusb
* rtldsdr
* faad2 (default) or fdk-aac (optional)
* mpg123
* portaudio (optional but recommended)
* airspy (optional)
* SoapySDR (optional)

Ubuntu 24.04 or lower does not support Qt>=6.5.0 that is required for full applications features. If you want to compile the application you shall [install](https://doc.qt.io/qt-6/qt-online-installation.html) Qt using online installer. Following modules are sufficcient to compile AbracaDABra:

<img width="264" alt="Snímek obrazovky 2024-09-13 v 21 40 15" src="https://github.com/user-attachments/assets/b2e938db-843b-431f-903f-0abb0e178cb0">

_Note: Currently you can still compile the application with obsolete Qt version delivered with Ubuntu 24.04 but it is an unsupported configuration with limited functionality._
       
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
   
3. Export QT path

         export QT_PATH=$HOME/Qt/6.7.2/gcc_64

3. Run cmake

       cmake .. -DUSE_SYSTEM_QCUSTOMPLOT=OFF -DCMAKE_PREFIX_PATH=$QT_PATH/lib/cmake
       
    Optional Airspy support:          
       
       cmake .. -DUSE_SYSTEM_QCUSTOMPLOT=OFF -DCMAKE_PREFIX_PATH=$QT_PATH/lib/cmake -DAIRSPY=ON

    Optional SoapySDR support:          
       
       cmake .. -DUSE_SYSTEM_QCUSTOMPLOT=OFF -DCMAKE_PREFIX_PATH=$QT_PATH/lib/cmake -DSOAPYSDR=ON

4. Run make

       make             

5. Install application for all users (optional)

       sudo make install
       sudo ldconfig


_Note:_ `CMAKE_INSTALL_PREFIX` is `/usr/local` by default. It means that application installs to `/usr/local/bin` and library is installed to `/usr/local/lib`. Make sure that `/usr/local/lib` is in your `ldconfig` path, if it is not then use `LD_LIBRARY_PATH` environment variable when running AbracaDABra:

       LD_LIBRARY_PATH=/usr/local/lib:$QT_PATH/lib:$LD_LIBRARY_PATH /usr/local/bin/AbracaDABra &    

## USBFS buffer size

USBFS buffer size limitation is typical problem under Linux. In such case RTL-SDR disconnects just after tuning. You may also see a message like this in terminal:

```
Failed to submit transfer 10
Please increase your allowed usbfs buffer size with the following command:
echo 0 > /sys/module/usbcore/parameters/usbfs_memory_mb
```

You can set unlimited size using this command:
```
sudo bash -c 'echo 0 > /sys/module/usbcore/parameters/usbfs_memory_mb'
```
There are instructions on the internet how to make this settings persistent, for example [here](https://github.com/OpenKinect/libfreenect2/issues/807)
       
