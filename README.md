# AbracaDABra
Abraca DAB radio is DAB and DAB+ Software Defined Radio (SDR) application. It works with cheap RTL-SDR (RTL2832U) USB sticks but also with Airspy devices, SDRplay devices and with many devices supported by <a href="https://github.com/pothosware/SoapySDR/wiki">SoapySDR</a>. 

Application is based on Qt6 cross-platform software development framework and can run on any platform supported by Qt6 _(Qt version 6.5 or higher is required for full functionality)_. 
Prebuilt binaries are released for Windows, macOS (both Intel and Apple Silicon) and Linux x86-64 and AARCH64 (AppImage). AARCH64 AppImage is built to run on Raspberry Pi4/5 with 64 bit OS.
ArchLinux users can install AbracaDABra from <a href="https://aur.archlinux.org/packages/abracadabra">AUR</a>.
<p align="center" width="100%">
    <img width="889" alt="AbracaDABra application window" src="https://github.com/KejPi/AbracaDABra/assets/6438380/717ed65e-314b-4307-9e32-968c5582eeda"> 
</p>

## Features
* Following [input devices](#input-devices) are supported:
  * RTL-SDR (default device)
  * [Airspy](https://airspy.com) (optional)
  * [SoapySDR](https://github.com/pothosware/SoapySDR/wiki) (optional)
  * [SDRplay](https://www.sdrplay.com) (optional, requires [SoapySDR](https://github.com/pothosware/SoapySDR/wiki))
  * RTL-TCP
  * Raw file input
* Band scan with automatic service list
* Service list management
* DAB (mp2) and DAB+ (AAC) audio decoding
* Announcements (all types including alarm test)
* Dynamic label (DL) and Dynamic label plus (DL+)
* MOT slideshow (SLS) and categorized slideshow (CatSLS) from PAD or from secondary data service
* SPI (Service and Programme information)
* RadioDNS
* TII decoding and continuous scanning (DX)
* Audio and data services reconfiguration
* Dynamic programme type (PTy)
* Ensemble structure view with all technical details, upload to [FMLIST](https://www.fmlist.org)
* Raw file dumping (optionally with XML header)
* Audio recording
* DAB input signal spectrum visualization
* RF level estimation on supported devices
* Only band III and DAB mode 1 are supported
* Simple user-friendly interface, trying to follow DAB _Rules of implementation (<a href="https://www.etsi.org/deliver/etsi_ts/103100_103199/103176/02.04.01_60/ts_103176v020401p.pdf">TS 103 176</a>)_
* Multiplatform (Windows, macOS and Linux)
* Dark theme supported on all platforms
* Localization to German, Czech and Polish (not complete)


AbracaDABra desktop application is available for free and will remain so in the future. 
However, if you like it, you can [buy me a beer](https://www.buymeacoffee.com/kejpi) 🍺

## Basic mode
<p align="center" width="100%">
    <img width="663" alt="Application in basic mode" src="https://github.com/KejPi/AbracaDABra/assets/6438380/a3d0a656-9a7c-47bd-a2e2-bc8d283d080b">
</p>

Simple user interface that is focused on radio listening. Just select your favorite service from service list on the right side 
and enjoy the music with slideshow and DL(+). 
Service can be easily added to favorites by clicking "star" icon.  Most of the elements in UI have tool tip with more information.

## Expert mode
<p align="center" width="100%">
    <img width="889" alt="Application in expert mode" src="https://github.com/user-attachments/assets/2d304694-5993-4c5b-acb3-416c1b1106b1">
</p>
In addition to basic mode, expert mode shows ensemble tree with structure of services and additional details of currently tuned service. 
Furthermore it is possible to change the DAB channel manually in this mode. 
This is particularly useful when antenna adjustment is done in order to receive ensemble that is not captured during automatic band scan.
Expert mode can be enabled in Settings dialog.

## Input devices

AbracaDABra supports multiple input devices, some of them are optional. Device specific settings are described in this section. 

### RTL-SDR

RTL-SDR is default input device. It is highly recommended to use [RTL-SDR driver by old-dab](https://github.com/old-dab/rtlsdr) when compiling application from source code. Prebuilt binaries are using this driver. 

It is possible to select particular device when more devices are available. You can choose the device in settings (see screenshot below). When option called "Use any available RTL-SDR device if selected one fails" is active, application tries to connect the first functional RTL-SDR device if the selected device is not available, does not work or is already in use. Please note that all RTL-SDR devices have typically serial number 00000001 but you can modify it using `rtl_eeprom` tool to be able to distiguish your devices. Follow this [video guide](https://www.youtube.com/watch?v=xGEDglwOHng) to do it.  

RTL-SDR device supports 3 or 4 gain control modes:
* Software (default) - gain is controlled by the application
* Driver - available only for [RTL-SDR driver by old-dab](https://github.com/old-dab/rtlsdr). Gain is controlled by the driver.
* Hardware - internal RTL-SDR HW gain control. RF level estimation is not available in this mode
* Manual - manual control of the gain

_Note:_ SW AGC level threshold value can be adjusted to control maximum level threshold for automatic gain control. This control is more intended for debugging, default value is well tuned so it is typically not needed to be altered. Please do not report any issues when you change the value.

<p align="center" width="100%">
    <img width="651" alt="RTL-SDR" src="https://github.com/user-attachments/assets/e76aaad8-5efa-412d-8673-dbae6f65d98e" />
</p>

### RTL-TCP

RTL-TCP is a simple server that provides IQ signal stream. Application can connect to server running on localhost (127.0.0.1) as well as on remote server. Server implementation [by old-dab](https://github.com/old-dab/rtlsdr) supports also so called control port. Connection to this port is optional, but when it is connected application can estimate RF level in dBm. 

RTL-TCP device supports 3 gain control modes:
* Software (default) - gain is controlled by the application
* Hardware - internal HW gain control by RTL-SDR device server is connected to. RF level estimation is not available in this mode
* Manual - manual control of the gain

_Note:_ SW AGC level threshold value can be adjusted to control maximum level threshold for automatic gain control. This control is more intended for debugging, default value is well tuned so it is typically not needed to be altered. Please do not report any issues when you change the value.

<p align="center" width="100%">
    <img width="651" alt="RTL_TCP" src="https://github.com/user-attachments/assets/ddac7c26-e07a-461e-85c2-d835dd1c1967" />
</p>

### Airspy

Airspy is optional device when you build application from the source code. It is available in prebuilt binaries. Only Airspy Mini and R2 are supported, other devices do not have sufficient bandwith for DAB reception. If you have problems with Airspy devices, please check the firmware version. Firmware update maybe required for correct functionality.

It is possible to select particular device when more devices are available. You can choose the device in settings (see screenshot below). When option called "Use any available Airspy device if selected one fails" is active, application tries to connect the first functional Airspy device if the selected device is not available, does not work or is already in use.

Airspy device supports 4 gain control modes:
* Software (default) - gain is controlled by application. So called sensitivity gain is used for that.
* Hybrid - application controls IF gain but other two gains are controlled by driver.
* Sensitivity - manual control of sensitivity gain. Physical gain controls in the device are set by driver according to sensitivity gain index.
* Manual - full manual control of all available gains. It is possible to enable AGC for selected gain controls in this mode. 

_Note:_ RF level estimation is not available for Airspy devices.

<p align="center" width="100%">
    <img width="651" alt="Airspy" src="https://github.com/user-attachments/assets/32ba4fb1-793e-4c8c-9ce8-c710b1cb3fbb" />
</p>

### SDRplay

SDRplay is optional device. It is based on [SoapySDR](https://github.com/pothosware/SoapySDR/wiki) and requires [SoapySDR SDRplay3 plugin](https://github.com/pothosware/SoapySDRPlay3) to be in located in `SOAPY_SDR_PLUGIN_PATH`. Prebuilt binaries are distributed with necessary libraries and configured to find required plugin in the installation location. Nevertheless in order to make SDRplay device functional you also need to install [SDRplay API](https://www.sdrplay.com/api/) 3.15 with background service that is required to access SDRplay devices. 

It is possible to select particular device when more devices are available. You can choose the device in settings (see screenshot below). When option called "Use any available SDRplay device if selected one fails" is active, application tries to connect the first functional SDRplay device if the selected device is not available, does not work or is already in use.

SDRplay devices have 2 independent gain controls, application calls them RF and IF. RF gain controls LNA gain reduction and IF gain controls IF gain reduction. You can find more details about SDRplay devices in documentation available [online](https://www.sdrplay.com/apps-catalogue/).

SDRplay device supports 2 gain modes:
* Software (default) - RF and IF gain is controlled by application. Please note that control may fail in case of strong adjacent channel that could lead to LNA overload. 
* Manual - both RF ad IF gain is in manual mode however you can enable AGC on IF gain. In this case IF gain will be controlled by the application. 

<p align="center" width="100%">
    <img width="651" alt="SDRplay" src="https://github.com/user-attachments/assets/fce9cd85-4bec-4bae-b5e4-243397372b53" />
</p>

### SoapySDR

[SoapySDR](https://github.com/pothosware/SoapySDR/wiki) is an open-source generalized API and runtime library for interfacing with SDR devices. It supports many SDR devices through runtime-loadable modules. In order to use your device using this API, you need to get plugin library and set `SOAPY_SDR_PLUGIN_PATH` so that is is recognized by the application. Then you need to specify device parameters, antenna and RX channel and you can connect to the device. Screenshot below shows RSP1A device connected as SoapySDR device.

SoapySDR device supports 2 gain modes: 
* Device - gain control by device if available from SoapySDR API
* Manual - all gain controls reported by API are exposed for manual control

_Note:_ RF level estimation is not available for SoapySDR devices.

<p align="center" width="100%">
    <img width="651" alt="SoapySDR" src="https://github.com/user-attachments/assets/afde7352-4964-45d5-a678-8c8571bc5b97" />
</p>

### Raw file

Raw file is a virtual device that is used to play files with raw IQ signal recording. This device is only available in [Expert mode](#expert-mode). You can create compatible recording from Ensemble information dialog. These files are particularly useful for debugging. Application supports files with 8 bit unsigned or 16 bit signed integer samples and sampling rate 2048kHz. It is possible to play the file in endless loop and seek within the file using controls in settings dialog. 

<p align="center" width="100%">
    <img width="651" alt="RawFile" src="https://github.com/user-attachments/assets/ae7e2a17-36b9-4279-b9d1-2e22770b8913" />
</p>

## Ensemble Information

AbracaDABra shows technical information about ensemble structure in Ensemble Information dialog accessible from application menu in [Expert mode](#expert-mode). 

<p align="center" width="100%">
    <img width="1086" alt="Ensemble information" src="https://github.com/user-attachments/assets/66e39d7e-bd14-4934-848e-91c13cb56660" />
</p>

Following information is available in the dialog:
* Current channel details like frequency, SNR, AGC gain, etc.
* Current audio service and its subchannel parameters
* Current audio service bitrate details
* FIB and AU statistics - each of them can be reset from popup menu accessible by right click
* Subchannel allocation with interactive colorful bar. You can click on any subchannel to display details below. Subchannels with data services are shown with cross pattern.
* Details of all services in ensemble in text format. It is possible to select any part of the text and copy it to clipboard.

Furthermore, this dialog allows to export ensemble structure in CSV format and to upload it to FMLIST. _NOTE:_ Upload to FMLIST is enabled only when automatic upload is disabled in Settings. 

Last feature accessible from Ensemble Information is Raw data recording with optional timeout. This is used to record signal from tuner (IQ) that can be used as [Raw file input](#raw-file) later. _Note:_ The raw file grows very fast, for example RTL-SDR device stream with sample rate 2048kHz and 8 bit produces approximately 250 MByte file in 1 minute.


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
In general, SPI application is very slow and it takes several minutes to acquire all objects, decoding progress is indicated in main application window by default. 
You can disable progress indication from Settings.

<p align="center" width="100%">
    <img width="892" alt="Snímek obrazovky 2025-03-09 v 21 57 58" src="https://github.com/user-attachments/assets/48ddbecb-1ffc-4e98-9681-d503da1b6eee" />
</p>

AbracaDABra can use internet connection to download service logos and to retrieve service information using RadioDNS if it is supported by broadcaster of the selected service. 
Both internet connection and RadioDNS are optional features that are enabled by default.

<p align="center" width="100%">
    <img width="692" alt="uaSettings_2025-03-27" src="https://github.com/user-attachments/assets/725e4c59-1c64-4581-9dfb-19a7e0790d66" />
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
    <img width="692" alt="uaSettings_2025-03-27" src="https://github.com/user-attachments/assets/725e4c59-1c64-4581-9dfb-19a7e0790d66" />
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

Audio recording can be started and stopped from application menu. It can be also stopped from status bar. The recording files are stored automatically in predefined folder. Application stores also plain text file containg Dynamic label messages (DL) with timestamp. 

<p align="center" width="100%">
<img width="1326" alt="Snímek obrazovky 2023-12-03 v 16 56 59" src="https://github.com/KejPi/AbracaDABra/assets/6438380/92bddcfe-614d-45ea-bb8d-10ede37b61cb">
</p>

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
* Application does not block user to define overlapping recording schedules. If user adds schedule items that are overlapping, conflict is indicated by red triangle icon in audio recording schedule table ("Schedule #3" in the screenshot below). In case of conflicting schedules, the first scheduled item is completed as defined ("Schedule #2" in screenshot) and then the conflicting item starts. Recording is always performed in the order shown in the table.
* Ongoing scheduled audio recording can be stopped anytime from application menu or from status bar like any other audio recording.
* If service to be recorded is available from more ensembles, the last used ensemble is used for recording (like when user selects services from the service list).
* Audio recording schedule is stored in `AudioRecordingSchedule.json` file in INI file [location](#expert-settings)

<p align="center" width="100%">
<img width="738" alt="audioRecordingSchedule" src="https://github.com/KejPi/AbracaDABra/assets/6438380/7aa07e1f-ee41-44b2-bdb6-41e65d46261e">
</p>

## FMLIST upload

AbracaDABra uploads information about currently tuned ensemble to [FMLIST](https://www.fmlist.org) as small CSV file with list of services in current ensemble. This file is anonymous and contains no personal data, this is example of such file:

```text
Ensemble Name;Ensemble ID;Channel;Frequency;Service Name;Service ID;Service Language;Service Country;PTY;Short Label;ECC;Component Label;Component Language;Type;Sub channel ID;Codec;Bitrate;CU;Protection level;User application
COLOR DAB+ PHA;2117;6A;181936;Expres FM;203B;none;Czech Republic;none;ExpresFM;E22;Expres FM;none;Audio Stream DAB+;4;AAC+;96;72;EEP 3-A;SPI, MOT Slideshow
COLOR DAB+ PHA;2117;6A;181936;COLOR MusicRadio;2442;Czech;Czech Republic;none;COLOR;E22;COLOR MusicRadio;Czech;Audio Stream DAB+;1;AAC+;96;72;EEP 3-A;SPI, MOT Slideshow
COLOR DAB+ PHA;2117;6A;181936;Classic Praha;244A;none;Czech Republic;none;Classic;E22;Classic Praha;none;Audio Stream DAB+;10;AAC+;128;84;EEP 2-B;SPI, MOT Slideshow
COLOR DAB+ PHA;2117;6A;181936;HEY Radio;24F8;none;Czech Republic;none;HEYRadio;E22;HEY Radio;none;Audio Stream DAB+;2;AAC+;96;72;EEP 3-A;SPI, MOT Slideshow
COLOR DAB+ PHA;2117;6A;181936;DAB Test HQ;2F75;none;Czech Republic;none;TestHQ;E22;DAB Test HQ;none;Audio Stream DAB+;19;AAC+;128;96;EEP 3-A;SPI, MOT Slideshow
COLOR DAB+ PHA;2117;6A;181936;ZUN radio;2F76;none;Czech Republic;none;ZUN;E22;ZUN radio;none;Audio Stream DAB+;14;AAC+;72;72;EEP 2-A;SPI, MOT Slideshow
COLOR DAB+ PHA;2117;6A;181936;MUSICONLY;2F77;none;Czech Republic;none;MUSICONL;E22;MUSICONLY;none;Audio Stream DAB+;7;AAC+;72;54;EEP 3-A;SPI, MOT Slideshow
COLOR DAB+ PHA;2117;6A;181936;'Rádio SÁZAVA;2FBA;none;Czech Republic;none;SÁZAVA;E22;'Rádio SÁZAVA;none;Audio Stream DAB+;D;AAC+;72;54;EEP 3-A;SPI, MOT Slideshow
COLOR DAB+ PHA;2117;6A;181936;Americana Radio;2FBB;none;Czech Republic;none;American;E22;Americana Radio;none;Audio Stream DAB+;C;AAC+;64;42;EEP 2-B;SPI, MOT Slideshow
COLOR DAB+ PHA;2117;6A;181936;Black Hornet;2FD1;none;Czech Republic;none;BHornet;E22;Black Hornet;none;Audio Stream DAB+;11;AAC+;64;32;EEP 4-A;SPI, MOT Slideshow
```

This feature can be disabled in settings but please consider keeping this option enabled to help the community.

## TII decoding
TII decoder is considered to be advanced DX feature thus it is only available when application is in [Expert mode](#expert-mode). Before using it, the feature needs to be configured from application settings:

<p align="center" width="100%">
    <img width="692" alt="TII settings" src="https://github.com/user-attachments/assets/05e2183c-72f6-47b2-a7f6-76a848ff35ad" />
</p>

First update the DAB transmitter database kindly provided by [FMLIST](https://www.fmlist.org). _Note:_ you might need to configure network proxy in Others tab. 

Then set your location, 3 options are available:
* System - location provided by system. Application will ask for Location permission on macOS.
* Manual - manual configuration of the location using "latitude, longitude" format. Using [Google Maps](https://www.google.com/maps/) as suggested by "Tip" in the dialog is probably the easiest way to get your coordinates in expected format.
* NMEA Serial Port - using serial port GPS receiver compatible with NMEA standard. In this case you need to specify serial port device. _Note:_ Some users reported it is working but it was not tested by developer.

_Note:_ Map is centered in Prague when location is not valid.

You can also configure default folder to be used to store TII log in CVS format. By default it is `Documents` folder. Logging can be started from TII dialog. You can set timestamps to be in UTC instead of local time. This settings affects TII CVS log and [Scanning tool](#scanning-tool) log. Additionally GPS coordinates of receiver (RX) and transmitters (TX) can be appended to TII CSV log file. This option is particularly useful in mobile reception with NMEA serial port GPS device.

TII detector can run in _Sensitive_ or in _Reliable_ mode. _Sensitive_ mode is default, it gives good results but in some corner cases the detected weak codes might be invalid. _Reliable_ option on the other hand uses more conservative criteria to evaluate the TII, thus it generally detects less codes but with lower probability of fake detection. 

TII dialog user interface can be configured too. You can enable Spectrum plot option. This option is mostly for debug purposes. If enabled it displays spectrum-like plot in the TII Decoder dialog that shows sum of carrier pairs calculated from NULL symbol of DAB signal. 

_Note:_ It is not a real spectrum of NULL symbol but rather preprocessed TII information extracted from it.

Last option is to keep no longer detected transmitters on the map. When this option is enabled, all transmitters that were detected for current ensemble since the dialog was opened are shown on the map. Transmitters that were detected in last NULL symbol are shown with marker whose color depends on relative level of that transmitter. Transmitters that could not be detected in last NULL symbol have grey marker. If the option is enabled you can also configure timeout to remove undetectable transmitters from the map. For example, when timeout is set to 1 minute, transmitters that were not detected in NULL symbol during last minute will be removed from the map. Minimum timeout is 1 minute, maximum is 600 minutes, i.e. 10 hours.

Plot can be zoomed in both axes by mouse wheel or in one axis by clicking on the axis a zooming by mouse wheel. When zoomed plot can be dragged by mouse, zoom is reset to default by right click on plot area. _Note:_ Optional [QCustomPlot library](https://www.qcustomplot.com) is needed for this functionality.

<p align="center" width="100%">
    <img width="1172" alt="TII decoding" src="https://github.com/user-attachments/assets/18a48563-cdde-4883-aa3c-7c71a6a208f8" />
</p>

TII Decoder dialog shows an interactive map provided by [OpenStreetMap](https://www.openstreetmap.org/copyright), table of detected transmitter codes and ensemble information. Blue dot shows location configured in Settings.
Table shows TII code (Main & Sub), relative transmitter level, distance and azimuth if position of the transmitter is known. Table can be sorted by any column by clicking on its header, by default it is sorted by Level so that the strongest transmitter is on top.
To see details of particular transmitter, you can either select it by clicking on the row in table or you can click on position bubble in the map. Transmitter details are shown above the map in bottom right corner like in the screenshot above. 
It is also possible to record CVS log with received codes using "recording dot" button in bottom left corner of the dialog. Please note, that logs are stored to `Documents` folder unless the location is changed in Settings dialog (TII tab).

## Scanning tool
AbracaDABra offers a possibility to run an unattended DAB band scan and to store all received transmitters. This is an advanced DX feature thus it is only available when application is in [Expert mode](#expert-mode). [TII decoding](#tii-decoding) configuration is required for correct functionality of the tool. 

<p align="center" width="100%">
<img width="1323" alt="Snímek obrazovky 2025-03-11 v 20 27 27" src="https://github.com/user-attachments/assets/85f0e630-e138-4565-86c7-44f77a50c58f" />
</p>

Scanning tool can be configured to run in one of 3 different modes: 
* Fast - fast scanning but week transmitters might no be detected (about 4 seconds per ensemble)
* Normal (this is default mode) - the best compromise between scanning speed and TII decoding performance (about 8 seconds per ensemble)
* Precise - the best TII decoding performance but it is quite slow (about 16 seconds per ensemble). Ensemble configuration is additionally recorded in this mode. 

Number of scan cycles can be configured. One scan cycle means scanning all selected channels once. You can let the Scanning tool to run "forever" by setting number of cycles to Inf (value 0). 

By default all channels in band III are scanned (5A-13F, 38 channels in total) but you can scan only some channels using "Select channels" button.

Scanning results are displayed in the table as well as red circles on the map. Blue circle is location specified in TII settings. You can select any row in the table by clicking on it and corresponding transmitter is shown as bubble on map with detailed information in bottom right corner (see screenshot above). It works also the other way around by clicking the red circle on the map. Multiple rows selection is also supported. In this case corresponding bubbles are shown on the map but no details about transmitters are available. Table can be sorted by any column by clicking on it. It is possible to display ensemble structure by double clicking on the row or from context menu on right mouse click (Precise mode only).

Results can be stored to CVS file using Export as CSV button. Unlike TII dialog, Scanner tool does not store the results "on the fly" during scanning. You can also load previously stored CVS file to display the results in the dialog. Please note that this CSV file replaces the contents of the table and since location is not stored in the file, transmitter distances will be calculated using current location known by application. 

_Note:_ Application service list is preserved when Scanning tool is running. Use Band scan functionality if you want to update service list. Furthermore, iterations with application are limited when Scanning tool performs scanning.

## DAB signal overview
DAB signal overview is considered to be advanced feature thus it is only available when application is in [Expert mode](#expert-mode). It needs optional [QCustomPlot library](https://www.qcustomplot.com).
This feature can be accessed by clicking on SNR value in dB in status bar or from application menu. It displays spectrum of the input signal, time plot of SNR and other signal parameters known by application. Border between plots can be moved to very top or very bottom to hide spectrum or SNR plot respectively. It is possible to activate frequency offset correction of the spectrum in configuration menu (under tree dots at bottom right corner). This removes frequency offset of the signal and gives cleaner spectrum of DAB signal in general. Default refresh rate of the spectrum is about 500 ms but it can me modified in configuration menu. 

Spectrum plot can be zoomed in both axes by mouse wheel or in one axis by clicking on the axis a zooming by mouse wheel. Spectrum plot can be dragged by mouse, zoom is reset to default by right click on plot area or from configuration menu.

_Note:_ Spectrum calculation and visualization causes higher CPU load of the application. 

<p align="center" width="100%">
<img width="941" alt="DAB Signal Overview" src="https://github.com/user-attachments/assets/01b29563-ec8c-475d-86ad-5c8b6e5c358a" />
</p>

## Expert settings
Some settings can only be changed by editing of the INI file. File location is OS dependent:
* macOS: `$HOME/.config/AbracaDABra/AbracaDABra.ini`
* Windows: `%USERPROFILE%\AppData\Roaming\AbracaDABra\AbracaDABra.ini`
* Linux: `$HOME/.config/AbracaDABra/AbracaDABra.ini`

Following settings can be changed by editing AbracaDABra.ini:

      [General]
      keepServiceListOnScan=false  # delete (false, default value) or keep (true) current service list when running band scan 
                                   # note: favorites are not deleted
      
Application shall not run while changing INI file, otherwise the settings will be overwritten.

It is also possible to use other than default INI file using `--ini` or `-i` command line parameter. 

Service list is stored in dedicated `ServiceList.json` file since version 3.0.0. By default, application looks for it in the same folder as INI file but you can specify different file using `--service-list` or `-s` command line parameter. If the file does not exist, applications creates one and uses it for storing the service list. 

## How to install

### macOS
Download latest DMG file from [release page](https://github.com/KejPi/AbracaDABra/releases/latest) and install it like any other application. 

There are two versions of DMG, one for Intel Mac and the other from Apple Silicon Mac. Although Intel Mac application runs on both platforms, it is highly recommended to install Apple Silicon version if you have Apple Silicon Mac. Both Apple Silicon and Intel builds require at least MacOS BigSur (11).

<img width="752" alt="Snímek obrazovky 2023-12-03 v 19 03 37" src="https://github.com/KejPi/AbracaDABra/assets/6438380/395a0384-ae2d-48e9-aca1-56169d631a4d">

### Windows
Download latest Windows zip file from [release page](https://github.com/KejPi/AbracaDABra/releases/latest) and unpack it to any folder on your drive. 

RTL-SDR devices need WinUSB driver for correct functionality under Windows. To install it, use Zadig that can be downloaded from [zadig.akeo.ie](https://zadig.akeo.ie). Please follow installation steps described [here](https://www.rtl-sdr.com/getting-the-rtl-sdr-to-work-on-windows-10/). When the driver is installed, start `AbracaDABra.exe` and first band scan should start automatically when RTL-SDR device is recognized. 

Please note that other applications requiring Realtek driver will not work with WinUSB driver from Zadig. 

### Linux
The simplest way is to download latest AppImage file from [release page](https://github.com/KejPi/AbracaDABra/releases/latest), 
make it executable and run it. 

There are two versions of AppImage - one for Intel/AMD 64 bit CPU (x86_64) the other for ARM64 CPU (aarch64) so make sure you are downloading the one matching your hardware.

AppImage for x86_64 is compatible with Ubuntu 22.04 or newer. If you run it on other Linux distribution, GLIBC version 2.35 or higher is required. AppImage for AARCH64 platform is built on Debian Bookworm for the compatibility with Raspberry Pi 4/5.

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
* QCustomPlot (optional) - it is automatically cloned from [GitHub](https://github.com/legerch/QCustomPlot-library) during CMake execution. Use QCUSTOMPLOT=OFF option to disable QCustomPlot usage in the application.

For a fresh Ubuntu 22.04 installation you can use the following commands to install all required packages:

       sudo apt-get install git cmake build-essential mesa-common-dev 
       sudo apt-get install libcups2-dev libxkbcommon-x11-dev libxcb-cursor0 libxcb-cursor-dev
       sudo apt-get install libusb-dev librtlsdr-dev libfaad2 mpg123 libmpg123-dev libfaad-dev
       sudo apt-get install portaudio19-dev rtl-sdr    

Ubuntu 24.04 or lower does not support Qt>=6.5.0 that is required for full applications features. If you want to compile the application you shall [install](https://doc.qt.io/qt-6/qt-online-installation.html) Qt using [online installer](https://www.qt.io/download-qt-installer-oss). Following modules are sufficient to compile AbracaDABra:

![Screenshot from 2024-11-08 19-51-02](https://github.com/user-attachments/assets/56c32368-9472-4f8a-8d18-4cc65120b88b)

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

         export QT_PATH=$HOME/Qt/6.7.3/gcc_64

3. Run cmake

       cmake .. -DUSE_SYSTEM_QCUSTOMPLOT=OFF -DCMAKE_PREFIX_PATH=$QT_PATH/lib/cmake
       
    Optional Airspy support:          
       
       cmake .. -DUSE_SYSTEM_QCUSTOMPLOT=OFF -DCMAKE_PREFIX_PATH=$QT_PATH/lib/cmake -DAIRSPY=ON

    Optional SoapySDR support:          
       
       cmake .. -DUSE_SYSTEM_QCUSTOMPLOT=OFF -DCMAKE_PREFIX_PATH=$QT_PATH/lib/cmake -DSOAPYSDR=ON

   Optional disable QCustomPlot:

       cmake .. -DQCUSTOMPLOT=OFF -DCMAKE_PREFIX_PATH=$QT_PATH/lib/cmake

5. Run make

       make             

6. Install application for all users (optional)

       sudo make install
       sudo ldconfig

_Note:_ `CMAKE_INSTALL_PREFIX` is `/usr/local` by default. It means that application installs to `/usr/local/bin` and library is installed to `/usr/local/lib`. Make sure that `/usr/local/lib` is in your `ldconfig` path, if it is not then use `LD_LIBRARY_PATH` environment variable when running AbracaDABra:

       LD_LIBRARY_PATH=/usr/local/lib:$QT_PATH/lib:$LD_LIBRARY_PATH /usr/local/bin/AbracaDABra &    

Optional SDRplay support:

SoapySDR is required, additionally [SDRplay API](https://www.sdrplay.com/api/) 3.15 must be installed and [SoapySDR SDRplay3 plugin](https://github.com/pothosware/SoapySDRPlay3) needs to be built from source. Then specify `SOAPY_SDR_PLUGIN_PATH` when you run the application.



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
       


