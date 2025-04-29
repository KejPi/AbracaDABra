# AbracaDABra
Abraca DAB radio is a DAB and DAB+ Software Defined Radio (SDR) application. It works with cheap RTL-SDR (RTL2832U) USB sticks, but also with Airspy devices, SDRplay devices and with devices supported by <a href="https://github.com/pothosware/SoapySDR/wiki">SoapySDR</a>. 

The application is based on the Qt6 cross-platform software development framework and can run on any platform supported by Qt6 _(Qt version 6.5 or higher is required for full functionality)_. 
Prebuilt binaries are released for Windows, macOS (both Intel and Apple Silicon) and Linux x86-64 and AARCH64 (AppImage). The AARCH64 AppImage is built to run on Raspberry Pi 4/5 with a 64 bit OS.
Arch Linux users can install AbracaDABra from the <a href="https://aur.archlinux.org/packages/abracadabra">AUR</a>.
<p align="center" width="100%">
    <img width="889" alt="AbracaDABra application window" src="https://github.com/KejPi/AbracaDABra/assets/6438380/717ed65e-314b-4307-9e32-968c5582eeda"> 
</p>

## Features
* The following [input devices](#input-devices) are supported:
  * RTL-SDR (default device)
  * [Airspy](https://airspy.com) (optional)
  * [SoapySDR](https://github.com/pothosware/SoapySDR/wiki) (optional)
  * [SDRplay](https://www.sdrplay.com) (optional, requires [SoapySDR](https://github.com/pothosware/SoapySDR/wiki))
  * RTL-TCP
  * Raw file input
* Band scan with automatic service list creation
* Service list management
* DAB (MP2) and DAB+ (AAC) audio decoding
* Announcements (all types supported, including alarm tests)
* Dynamic label (DL) and Dynamic label plus (DL+)
* MOT slideshow (SLS) and categorized slideshow (CatSLS) from PAD or from a secondary data service
* SPI (Service and Programme information)
* RadioDNS
* TII decoding and continuous scanning (DX)
* Reconfiguration of audio and data services
* Dynamic programme type (PTy)
* Ensemble structure view with all technical details, upload to [FMLIST](https://www.fmlist.org)
* Raw file dumping (optionally with XML header)
* Audio recording
* DAB input signal spectrum visualization
* RF level estimation on supported devices
* Only band III and DAB mode 1 are supported
* Simple user-friendly interface, that aims to follow the DAB _Rules of implementation (<a href="https://www.etsi.org/deliver/etsi_ts/103100_103199/103176/02.04.01_60/ts_103176v020401p.pdf">TS 103 176</a>)_
* Multiplatform (Windows, macOS and Linux)
* Dark theme is supported on all platforms
* Localization to German, Czech and Polish (not complete)


The AbracaDABra desktop application is available for free and will remain so in the future. 
However, if you like it, you can [buy me a beer](https://www.buymeacoffee.com/kejpi) 🍺

## Basic mode
<p align="center" width="100%">
    <img width="663" alt="Application in basic mode" src="https://github.com/KejPi/AbracaDABra/assets/6438380/a3d0a656-9a7c-47bd-a2e2-bc8d283d080b">
</p>

Basic mode provides a simple user interface that is focused on radio listening. Just select your favorite service from the service list on the right side 
and enjoy the music with slideshow and DL(+). 
Services can be easily added to the favorites list by clicking "star" icon.  Most of the elements in UI have a tool tip with more information.

## Expert mode
<p align="center" width="100%">
    <img width="924" alt="Application in expert mode" src="https://github.com/user-attachments/assets/e5bb891a-e4b8-4479-a2be-7df09f1d55b6" />
</p>
In addition to the information available in basic mode, expert mode shows the structure of services in an ensemble tree as well as additional details about the currently tuned service. 
Furthermore it is also possible to change the DAB channel manually in this mode. 
This is particularly useful when performing antenna adjustment in order to receive an ensemble that was not previously captured during an automatic band scan.
Expert mode can be enabled in the Settings dialog.

## Input devices

AbracaDABra supports multiple input devices, some of them are optional. Device specific settings are described in this section. 

### RTL-SDR

RTL-SDR is the default input device. It is highly recommended to use the [RTL-SDR driver by old-dab](https://github.com/old-dab/rtlsdr) when compiling the application from source code. Prebuilt binaries use this driver. 

It is possible to select a particular device when more than one device is connected. You can choose the device to use in settings (see screenshot below). When the option "Use any available RTL-SDR device if selected one fails" is enabled, the application tries to connect to the first functional RTL-SDR device if the selected device is not available, does not work or is already in use. Please note that RTL-SDR devices typically have the serial number 00000001, but you can modify it using the `rtl_eeprom` tool to be able to distiguish your devices. Follow this [video guide](https://www.youtube.com/watch?v=xGEDglwOHng) to do it.  

RTL-SDR devices support 3 to 4 gain control modes, depending on the driver the application is compiled with:
* Software (default) - gain is controlled by the application
* Driver - available only for the [RTL-SDR driver by old-dab](https://github.com/old-dab/rtlsdr). Gain is controlled by the driver.
* Hardware - uses the internal RTL-SDR HW gain control. RF level estimation is not available in this mode
* Manual - manual control of the gain

_Note:_ The SW AGC level threshold value can be adjusted to control the maximum level threshold for automatic gain control. This control is more intended for debugging, the default value is well tuned so it does typically not needed to be altered. Please do not report any issues if you have changed the value.

<p align="center" width="100%">
    <img width="651" alt="RTL-SDR" src="https://github.com/user-attachments/assets/e76aaad8-5efa-412d-8673-dbae6f65d98e" />
</p>

### RTL-TCP

RTL-TCP is a simple server that provides an IQ signal stream. The application can connect to a server running on localhost (127.0.0.1) as well as on a remote server. The server implementation [by old-dab](https://github.com/old-dab/rtlsdr) also supports the so called control port. Connection to this port is optional, but when it is connected, the application can estimate the RF level in dBm. 

RTL-TCP devices support 3 gain control modes:
* Software (default) - gain is controlled by the application
* Hardware - internal HW gain control by the RTL-SDR device the server is connected to. RF level estimation is not available in this mode.
* Manual - manual control of the gain

_Note:_ The SW AGC level threshold value can be adjusted to control the maximum level threshold for automatic gain control. This control is more intended for debugging, the default value is well tuned so it does typically not needed to be altered. Please do not report any issues if you have changed the value.

<p align="center" width="100%">
    <img width="651" alt="RTL_TCP" src="https://github.com/user-attachments/assets/ddac7c26-e07a-461e-85c2-d835dd1c1967" />
</p>

### Airspy

Airspy is an optional device when you build the application from the source code. It is available in prebuilt binaries. Only the Airspy Mini and R2 are supported, other devices do not have sufficient bandwith for DAB reception. If you have problems with Airspy devices, please check the firmware version. A firmware update may be required for correct functionality.

It is possible to select a particular device when more than one device is connected. You can choose the device to use in settings (see screenshot below). When the option "Use any available Airspy device if selected one fails" is enabled, the application tries to connect to the first functional Airspy device if the selected device is not available, does not work or is already in use.

Airspy devices support 4 gain control modes:
* Software (default) - gain is controlled by the application. So called sensitivity gain is used.
* Hybrid - the application controls IF gain but the other two gains are controlled by driver.
* Sensitivity - manual control of sensitivity gain. Physical gain controls in the device are set by the driver according to the sensitivity gain index.
* Manual - full manual control of all the available gains. It is possible to enable AGC for specific gain controls in this mode. 

_Note:_ RF level estimation is not available for Airspy devices.

<p align="center" width="100%">
    <img width="651" alt="Airspy" src="https://github.com/user-attachments/assets/32ba4fb1-793e-4c8c-9ce8-c710b1cb3fbb" />
</p>

### SDRplay

SDRplay is an optional device. It is based on [SoapySDR](https://github.com/pothosware/SoapySDR/wiki) and requires the [SoapySDR SDRplay3 plugin](https://github.com/pothosware/SoapySDRPlay3) to be in located in `SOAPY_SDR_PLUGIN_PATH`. Prebuilt binaries are distributed with the necessary libraries and configured to find the required plugin in the installation location. Nevertheless, in order to make SDRplay devices functional you also need to install [SDRplay API](https://www.sdrplay.com/api/) 3.15, with the background service that is required to access SDRplay devices. 

It is possible to select a particular device when more than one device is connected. You can choose the device to use in settings (see screenshot below). When the option "Use any available SDRplay device if selected one fails" is enabled, the application tries to connect to the first functional SDRplay device if the selected device is not available, does not work or is already in use.

SDRplay devices have 2 independent gain controls, the application calls them RF and IF. RF gain controls LNA gain reduction and IF gain controls IF gain reduction. You can find more details about SDRplay devices in the documentation available [online](https://www.sdrplay.com/apps-catalogue/).

SDRplay devices support 2 gain modes:
* Software (default) - both RF and IF gain are controlled by the application. Please note that this may fail in case when there are strong adjacent channels that could overload the LNA. 
* Manual - both RF ad IF gain is manual, however you can enable AGC for IF gain. In this case IF gain will be controlled by the application. 

<p align="center" width="100%">
    <img width="651" alt="SDRplay" src="https://github.com/user-attachments/assets/fce9cd85-4bec-4bae-b5e4-243397372b53" />
</p>

### SoapySDR

[SoapySDR](https://github.com/pothosware/SoapySDR/wiki) is an open-source generalized API and runtime library for interfacing with SDR devices. It supports many SDR devices through runtime-loadable modules. In order to use your device using this API, you need to get  a plugin library and set `SOAPY_SDR_PLUGIN_PATH` so that is is recognized by the application. Then you need to specify the device parameters, antenna and RX channel and then you can connect to the device. The screenshot below shows an RSP1A device connected as a SoapySDR device.

SoapySDR devices support 2 gain modes: 
* Device - gain control is performed by the device if possible from the SoapySDR API
* Manual - all gain controls reported by API are exposed for manual control

_Note:_ RF level estimation is not available for SoapySDR devices.

<p align="center" width="100%">
    <img width="651" alt="SoapySDR" src="https://github.com/user-attachments/assets/afde7352-4964-45d5-a678-8c8571bc5b97" />
</p>

### Raw file

Raw file is a virtual device that is used to play files with a raw IQ signal recording. This device is only available in [Expert mode](#expert-mode). You can create compatible recordings from the Ensemble information dialog. These files are particularly useful for debugging. The application supports files with 8 bit unsigned or 16 bit signed integer samples and a sampling rate of 2048kHz. It is possible to play the file in an endless loop and seek within the file using the controls in the settings dialog. 

<p align="center" width="100%">
    <img width="651" alt="RawFile" src="https://github.com/user-attachments/assets/ae7e2a17-36b9-4279-b9d1-2e22770b8913" />
</p>

## Ensemble Information

AbracaDABra shows technical information about the ensemble structure in the Ensemble Information dialog accessible from the application menu in [Expert mode](#expert-mode). 

<p align="center" width="100%">
    <img width="1112" alt="Ensemble information" src="https://github.com/user-attachments/assets/3ee53997-a38d-4b04-b9b1-e262cc933f70" />
</p>

The following information is available in the dialog:
* Details for the current channel like its frequency, SNR, AGC gain, etc.
* The current audio service and its subchannel parameters
* The current audio service bitrate details
* FIB and AU statistics - each of them can be reset from the popup menu accessible with a right click
* Subchannel allocation with a interactive colorful bar. You can click on any subchannel to display its details below. Subchannels with data services are shown with cross pattern.
* Details of all services in the ensemble in text format. It is possible to select any part of the text and copy it to the clipboard.

The ensemble structure can also be exported in CSV format and uploaded to FMLIST from this dialog. _NOTE:_ The upload to FMLIST button is only enabled when automatic uploading is disabled in settings. 

The last feature accessible from Ensemble Information is raw data recording with an optional timeout. This is used to record the signal from tuner (IQ), which that can be used as a [Raw file input](#raw-file) later. _Note:_ The raw file grows very fast, for example an RTL-SDR device stream with a sample rate of 2048kHz and 8 bit samples produces an approximately 250 MByte file in 1 minute.


## DAB Announcements support
An announcement is a period of elevated interest within an audio programme. It is typically a spoken audio message, often with a lead-in and lead-out 
audio pattern (for example, a musical jingle). It may refer to various types of information such as traffic, news, sports and others. [<a href="https://www.etsi.org/deliver/etsi_ts/103100_103199/103176/02.04.01_60/ts_103176v020401p.pdf">see TS 103 176</a>]

All DAB(+) announcement types are supported by AbracaDABra, including the test alarm feature (ClusterID 0xFE according to <a href="https://www.etsi.org/deliver/etsi_ts/103100_103199/103176/02.04.01_60/ts_103176v020401p.pdf">TS 103 176</a>). 
The application monitors announcement switching information and when an announcement is active, AbracaDABra switches audio output to the specified 
subchannel (service). This behavior can be disabled by unchecking a particular announcement type in the Setup dialog. 
If an announcement occurs on the currently tuned service, it is only indicated by an icon after the service name. This icon can be clicked on, which 
suspends/resumes the ongoing announcement coming from another service. OE Announcements are not supported. 

The option "Bring window to foreground" tries to focus the application window when the alarm announcement starts. 
Alarm announcements carry emergency warning information that is of utmost importance to all radio listeners and they have the highest priority 
(according to <a href="https://www.etsi.org/deliver/etsi_ts/103100_103199/103176/02.04.01_60/ts_103176v020401p.pdf">TS 103 176</a>) in the sense that alarm announcement cannot be disabled and it can interrupt any ongoing regular announcement.

<img width="1279" alt="Snímek obrazovky 2023-01-12 v 22 07 17" src="https://user-images.githubusercontent.com/6438380/212181613-a7e163e2-d6e1-46cc-bf3d-6dfce276a8ae.png">

Announcements from other services display a thematic placeholder. <a href="https://www.flaticon.com/authors/basic-miscellany/lineal-color" title="linear color">The artwork of placeholders are created by Smashicons - Flaticon</a>

## SPI application and RadioDNS
The <a href="https://www.worlddab.org/dab/data-applications/service-and-programme-information">Service and programme information</a> (SPI) application is supported. 
When the SPI application is enabled in the settings and SPI is available for the selected service and/or in the ensemble, the application starts decoding it automatically.
SPI from X-PAD, from a secondary service or from a dedicated data service is supported, it can be even decoded from multiple sources in parallel. 
In general, the SPI application is very slow and it takes several minutes to acquire all objects, decoding progress is indicated in the main application window by default. 
You can disable progress indication from Settings.

<p align="center" width="100%">
    <img width="892" alt="Snímek obrazovky 2025-03-09 v 21 57 58" src="https://github.com/user-attachments/assets/48ddbecb-1ffc-4e98-9681-d503da1b6eee" />
</p>

AbracaDABra can use an internet connection to download service logos and to retrieve service information using RadioDNS if it is supported by the broadcaster of the selected service. 
Both the internet connection and RadioDNS features are optional, but are enabled by default.

<p align="center" width="100%">
    <img width="692" alt="uaSettings_2025-03-27" src="https://github.com/user-attachments/assets/725e4c59-1c64-4581-9dfb-19a7e0790d66" />
</p>

Service logos, XML files and the internet download cache are stored in a dedicated directory on disk. The location of the cache is OS dependent:
* macOS: `$HOME/Library/Caches/AbracaDABra/`
* Windows: `%USERPROFILE%\AppData\Local\AbracaDABra\cache\`
* Linux: `$HOME/.cache/AbracaDABra/`

AbracaDABra can visualize the Electronic Program Guide (EPG) if it is provided by the broadcaster in the SPI application and/or over RadioDNS. In such case the "Program guide..." menu item becomes active and the user can browse through the service's schedule in an interactive user interface.

<p align="center" width="100%">
<img width="1112" alt="EPG_20240209" src="https://github.com/KejPi/AbracaDABra/assets/6438380/fc2712e8-2b9d-483e-b089-169a77efa98f">
<p align="center" width="100%">

The EPG dialog supports dragging or scrolling by mouse, and specific program details can be displayed by clicking on an item. An audio service can be selected by clicking on the service name on the left side. 

## User application data storage
AbracaDABra can be configured to store all incoming data from the slideshow (SLS) and/or the SPI application. The configuration consists of storage folder that is common for both applications 
and a subfolder template configurable for each application individually. Storage of data can be enabled for each application individually.

<p align="center" width="100%">
    <img width="692" alt="uaSettings_2025-03-27" src="https://github.com/user-attachments/assets/725e4c59-1c64-4581-9dfb-19a7e0790d66" />
</p>

The default storage folder is OS dependent:
* macOS: `$HOME/Downloads/AbracaDABra/`
* Windows: `%USERPROFILE%\Downloads\AbracaDABra\`
* Linux: `$HOME/Downloads/AbracaDABra/`

The overwrite checkbox enables overwriting of the files with the same name. If it is not checked (default), new files with existing name will be ignored and not stored.

The subfolder template for each application can be created individually. Following tokens are supported:

| SLS   | SPI   | Token | Description | Example |
| :---: | :---: | :---- | :-----------| :-------|
| * | * | `{serviceId}` | Current audio service ID (ECC+SID) as hexadecimal number | `e01234` |
| * | * | `{ensId}`     | Current ensemble ID (ECC+UEID) as hexadecimal number | `e0eeee` |
|   | * | `{scId}`      | Data service component ID as 12bit decimal number | `47` |
| * | * | `{transportId}` | MOT Object transport ID as 16bit decimal number, it shall uniquely identify a "file" within a single data MOT channel. [[EN 301 234 7.2.7.4](http://www.etsi.org/deliver/etsi_en/301200_301299/301234/02.01.01_60/en_301234v020101p.pdf)]  | `123456` |
|   | * | `{directoryId}` | Directory ID is transport ID of MOT directory. | `654321` |
| * | * | `{contentName}` | MOT object content name parameter. It is used to uniquely identify an object. [[EN 301 234 6.2.2.1.1](http://www.etsi.org/deliver/etsi_en/301200_301299/301234/02.01.01_60/en_301234v020101p.pdf)]  | `stationLogo` |
| * |   | `{contentNameWithExt}` | MOT object content name parameter with extension that is added based on MIME type if content name is without an extension. | `stationLogo.jpeg` |

_Important notes:_
* The path template uses slash `/` as directory separator, backslash `\` is a forbidden character.
* A path template can contain each token multiple times. All occurrences are then replaced by their respective value. On the other hand, invalid tokens are left in the file path as-is.
* A MOT object content name does not have to be a valid filename for any operating system, thus the application replaces potentially "dangerous" characters with an underscore `_`. For example this is a perfectly valid content name: `http://www.example.com:80/logo1`. 
  The application transforms it to the following string when the subfolder path is created: `http___www.example.com_80_logo1`
* SLS is transferred in so called header mode [[EN 301 234 7.1](http://www.etsi.org/deliver/etsi_en/301200_301299/301234/02.01.01_60/en_301234v020101p.pdf)]. In this mode only one MOT object (slide) is available.
* The SPI application uses directory mode [[EN 301 234 7.2](http://www.etsi.org/deliver/etsi_en/301200_301299/301234/02.01.01_60/en_301234v020101p.pdf)]. In this mode one directory is available but this directory contains several MOT objects in a so called carrousel. These objects ("files") have their own transport ID and content name. A directory typically contains several station logos and binary encoded XML files [[TS 102 371](https://www.etsi.org/deliver/etsi_ts/102300_102399/102371/03.03.01_60/ts_102371v030301p.pdf)]. When SPI data is stored, the application stores all data from the carrousel and additionally a decoded XML file for each binary encoded file.
* The MOT directory ID is one of the objects transmitted in the SPI application so it has its own transport ID that is called `{directoryID}` in the template.
* The SPI application can process data from multiple sources at the same time. These sources are packet data service components within the ensemble (each having a unique service component ID) and the XPAD data of the selected audio service. The application assigns "virtual" service component ID 65535 to XPAD data. The user should take parallel processing into consideration when defining the path template - for example service component ID `{scId}` will be unique but the directory ID `{directoryId}` is generally not unique within the ensemble.
* SPI data retrieved using the RadioDNS feature are not stored.

The default template for SLS application is `SLS/{serviceId}/{contentNameWithExt}`. Using the examples from the table, it would store this file under macOS and Linux as: `$HOME/Downloads/AbracaDABra/SLS/e01234/stationLogo.jpeg`

The default template for SPI application is `SPI/{ensId}/{scId}_{directoryId}/{contentName}`. Using the examples from the table, it would store this file under macOS and Linux as: `$HOME/Downloads/AbracaDABra/SPI/e0eeee/47_654321/stationLogo`

## Audio recording
AbracaDABra features audio recording. Two options are available:
* Encoded DAB/DAB+ stream in MP2 or AAC format respectively
* Decoded audio in WAV format

Audio recording can be started and stopped from the application menu. It can be also stopped from the status bar. The recording files are stored automatically in a predefined folder. The application stores also a plain text file containg Dynamic label messages (DL) with timestamps. 

<p align="center" width="100%">
<img width="1326" alt="Snímek obrazovky 2023-12-03 v 16 56 59" src="https://github.com/KejPi/AbracaDABra/assets/6438380/92bddcfe-614d-45ea-bb8d-10ede37b61cb">
</p>

_Note:_  Audio recording stops when an ensemble reconfigures or when any tuning operation is performed. 

### Audio recording schedule
Audio recording can be also scheduled in advance. Scheduled recordings are defined by:
* Name
* Start time
* Duration (End time is calculated from duration)
* Service to be recorded

A scheduled recording can be created from the audio recording schedule dialog (see screenshot below) accessible from the application menu or from the EPG (Schedule audio recording button). 

_Notes:_
* Scheduled audio recording uses the same settings as immediate audio recording.
* The audio recording start time is taken from system time not from DAB time.
* When a scheduled recording is about to start, a dialog with a warning to the user pops up (30 seconds before scheduled time) and then the service to be recorded is selected about 10 seconds before the scheduled time.
* The application must be running in order for a scheduled recording tp happen. In other words, the application does not automatically start when an audio recording is scheduled.
* An ongoing recording wil prevent a scheduled recording from starting.
* The application does not stop the user defining overlapping scheduled recordings. If the user adds schedule items that are overlapping, the conflict is indicated by a red triangle icon in the audio recording schedule table ("Schedule #3" in the screenshot below). When there are conflicting schedules, the first scheduled item is completed as defined ("Schedule #2" in screenshot) after which the second conflicting item will start recording as soon as the first finishes. Recording is always performed in the order shown in the table.
* An ongoing scheduled audio recording can be stopped anytime from the application menu or from the status bar like any other audio recording.
* If the service to be recorded is available from more than one ensemble, the last used ensemble is used for recording (like when the user selects services from the service list).
* The audio recording schedule is stored in the `AudioRecordingSchedule.json` file in the configuration INI file. More information [here](#expert-settings).

<p align="center" width="100%">
<img width="738" alt="audioRecordingSchedule" src="https://github.com/KejPi/AbracaDABra/assets/6438380/7aa07e1f-ee41-44b2-bdb6-41e65d46261e">
</p>

## FMLIST upload

AbracaDABra uploads information about the currently tuned ensemble automatically to [FMLIST](https://www.fmlist.org) as a small CSV file with a list of services in the current ensemble. This file is anonymous and contains no personal data, this is an example of such a file:

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
TII decoder is considered to be an advanced DX feature thus it is only available when the application is in [Expert mode](#expert-mode). Before using it, the feature needs to be configured in the application settings:

<p align="center" width="100%">
    <img width="692" alt="TII settings" src="https://github.com/user-attachments/assets/84d122d9-c021-455e-98ae-e451049ed2d1" />
</p>

First, update the DAB transmitter database kindly provided by [FMLIST](https://www.fmlist.org). _Note:_ you might need to configure a network proxy in Others tab. 

Then set your location, 3 options are available:
* System - the location provided by system. The application will ask for location permission on macOS.
* Manual - manual configuration of the location using the "latitude, longitude" format. Using [Google Maps](https://www.google.com/maps/) as suggested by "Tip" in the dialog is probably the easiest way to get your coordinates in the expected format.
* NMEA Serial Port - using a serial port GPS receiver compatible with the NMEA standard. In this case you need to specify a serial port device. _Note:_ This feature is untested by the developer, but some users have reported that it works.

_Note:_ The map is centered in Prague when the location provided is invalid.

You can also configure the default folder to be used to store the TII log in CVS format. By default it is the `Documents` folder. Logging can be started from the TII dialog. You can set timestamps to be in UTC instead of local time. This settings affects the TII CVS log and the [Scanning tool](#scanning-tool) log. Additionally the GPS coordinates of the receiver (RX) and transmitters (TX) can be appended to the TII CSV log file. This option is particularly useful while mobile and using a NMEA serial port GPS device.

The TII detector can run in _Sensitive_ or in _Reliable_ mode. _Sensitive_ mode is the default, as it gives good results but in some edge cases the detected weak codes might be invalid. _Reliable_ option on the other hand uses more conservative criteria to evaluate the TII, thus it generally detects less codes but with a lower probability of incorrect detections. 

The TII dialog user interface can be configured too. You can enable the Spectrum plot option. This option is mostly for debugging. If enabled it displays a spectrum-like plot in the TII Decoder dialog that shows the sum of carrier pairs calculated from the NULL symbol of the DAB signal. 

_Note:_ It is not a real spectrum of the NULL symbol but rather preprocessed TII information extracted from it.

The last option is to keep no longer detected transmitters on the map. When this option is enabled, all transmitters that were detected for the current ensemble since the dialog was opened are shown on the map. Transmitters that were detected in the last NULL symbol are shown with a marker whose color depends on relative signal level of that transmitter. Transmitters that could not be detected in the last NULL symbol have a grey marker. If the option is enabled you can also configure a timeout to remove undetectable transmitters from the map. For example, when the timeout is set to 1 minute, transmitters that were not detected in the NULL symbol during the last minute will be removed from the map. The minimum timeout is 1 minute, the maximum is 600 minutes, i.e. 10 hours.

The plot can be zoomed in both axes by mouse wheel or in one axis by clicking on the axis and zooming with the mouse wheel. The zoomed plot can be dragged with the mouse, and the zoom level can be reset to the default with a right click on plot area. _Note:_ The optional [QCustomPlot library](https://www.qcustomplot.com) is needed for this functionality.

<p align="center" width="100%">
    <img width="1172" alt="TII decoding" src="https://github.com/user-attachments/assets/18a48563-cdde-4883-aa3c-7c71a6a208f8" />
</p>

The TII Decoder dialog shows an interactive map provided by [OpenStreetMap](https://www.openstreetmap.org/copyright), a table of detected transmitter codes and ensemble information. The blue dot shows the location configured in Settings.
The table shows the TII code (Main & Sub), the relative transmitter level, and distance and azimuth if the position of the transmitter is known. The table can be sorted by any column by clicking on its header, by default it is sorted by Level so that the strongest transmitter is on top.
To see details of a particular transmitter, you can either select it by clicking on the row in the table or you can click on the position bubble in the map. Transmitter details are shown above the map in bottom right corner like in the screenshot above. 
It is also possible to record a CSV log with received codes using the "recording dot" button in the bottom left corner of the dialog. Please note that logs are stored in the `Documents` folder unless the location is changed in Settings dialog (TII tab).

## Scanning tool
AbracaDABra offers the possibility to run an unattended DAB band scan and to store all received transmitters. This is an advanced DX feature thus it is only available when application is in [Expert mode](#expert-mode). [TII decoding](#tii-decoding) configuration is required for the tool to function correctly. 

<p align="center" width="100%">
<img width="1323" alt="Snímek obrazovky 2025-03-11 v 20 27 27" src="https://github.com/user-attachments/assets/85f0e630-e138-4565-86c7-44f77a50c58f" />
</p>

The scanning tool can be configured to run in one of 3 different modes: 
* Fast - fast scanning but weak transmitters might not be detected (about 4 seconds per ensemble)
* Normal (this is the default mode) - the best compromise between scanning speed and TII decoding performance (about 8 seconds per ensemble)
* Precise - the best TII decoding performance but it is quite slow (about 16 seconds per ensemble). The ensemble configuration is also recorded in this mode. 

The number of scan cycles can be configured. One scan cycle means scanning all selected channels once. You can let the Scanning tool run "forever" by setting number of cycles to be infinite (value 0). 

By default all the channels in band III are scanned (5A-13F, 38 channels in total) but you can only scan some channels using the "Select channels" button.

The scanning results are displayed in the table as well as as red circles on the map. The blue circle is the location specified in the TII settings. You can select any row in the table by clicking on it and the corresponding transmitter is shown as a bubble on the map with detailed information shown in the bottom right corner (see screenshot above). It also works the other way around by clicking the red circle on the map. Selection of multiple rowsis also supported. In this case the corresponding dots are shown on the map but no details about the transmitters are available. The table can be sorted by any column by clicking on it. It is possible to display the ensemble structure by double clicking on the row or from the context menu shown with a right mouse click (Precise mode only).

Results of scanning can be stored to a CSV file using the "Export as CSV" button. Unlike the TII dialog, the scanner tool does not store the results "on the fly" during scanning. You can also load previously stored CSV files to display the results in the dialog. Please note that this CSV file replaces the contents of the table and since reciever location is not stored in the file, transmitter distances will be calculated using the current location known by application. 

_Note:_ The application service list is preserved when the Scanning tool is running. Use the band scan functionality if you want to update the service list. Furthermore, iterations with the application are limited when scanning tool performs scanning.

## DAB signal overview
The DAB signal overview is considered to be advanced feature thus it is only available when application is in [Expert mode](#expert-mode). It needs the optional [QCustomPlot library](https://www.qcustomplot.com).
This feature can be accessed by clicking on the SNR value in dB in the status bar or from the application menu. It displays the spectrum of the input signal, a time plot of SNR and other signal parameters known by the application. The border between plots can be moved to the very top or very bottom to hide the spectrum or SNR plot respectively. It is possible to activate frequency offset correction of the spectrum in the configuration menu (select the three dots at the bottom right corner). This eliminates the frequency offset of the signal and gives a cleaner spectrum of DAB signal in general. The default refresh rate of the spectrum is 500 ms but it can be modified in the configuration menu. 

The spectrum plot can be zoomed in both axes with the mouse wheel or in one axis by clicking on the axis and zooming. The spectrum plot can be dragged with the mouse, and the zoom level can be reset to the default with a right click on the plot area or in the configuration menu

_Note:_ Spectrum calculation and visualization leads to higher CPU use by the application. 

<p align="center" width="100%">
<img width="941" alt="DAB Signal Overview" src="https://github.com/user-attachments/assets/01b29563-ec8c-475d-86ad-5c8b6e5c358a" />
</p>

## Expert settings
Some settings can only be changed by editing the INI file. The location of the file is OS dependent:
* macOS: `$HOME/.config/AbracaDABra/AbracaDABra.ini`
* Windows: `%USERPROFILE%\AppData\Roaming\AbracaDABra\AbracaDABra.ini`
* Linux: `$HOME/.config/AbracaDABra/AbracaDABra.ini`

The following settings can be changed by editing AbracaDABra.ini:

      [General]
      keepServiceListOnScan=false  # delete (false, default value) or keep (true) current service list when running band scan 
                                   # note: favorites are not deleted
      
The application should not be running while you are editing the INI file, otherwise the settings will be overwritten.

It is also possible to use a different INI file instead of the default one using the `--ini` or `-i` command line parameter. 

The service list is stored in a dedicated `ServiceList.json` file since version 3.0.0. By default, the application looks for it in the same folder as the INI file but you can specify different file using the `--service-list` or `-s` command line parameter. If the file does not exist, the application creates one and uses it for storing the service list. 

## How to install

### macOS
Download the latest DMG file from [the release page](https://github.com/KejPi/AbracaDABra/releases/latest) and install it like any other application. 

There are two versions of the DMG, one for Intel Macs and the other for Apple Silicon Macs. Although the Intel Mac version can run on both platforms, it is highly recommended to install Apple Silicon version if you have an Apple Silicon Mac. Both the Apple Silicon and Intel builds require at least MacOS Big Sur (11).

<img width="752" alt="Snímek obrazovky 2023-12-03 v 19 03 37" src="https://github.com/KejPi/AbracaDABra/assets/6438380/395a0384-ae2d-48e9-aca1-56169d631a4d">

### Windows
Download the latest Windows zip file from [the release page](https://github.com/KejPi/AbracaDABra/releases/latest) and unzip it to any folder on your drive. 

RTL-SDR devices need the WinUSB driver to work corectly under Windows. To install it, use Zadig, which be downloaded from [zadig.akeo.ie](https://zadig.akeo.ie). Please follow the installation steps [here](https://www.rtl-sdr.com/getting-the-rtl-sdr-to-work-on-windows-10/). When the driver is installed, start `AbracaDABra.exe` and the first band scan should start automatically when a RTL-SDR device is recognized. 

Please note that other applications requiring a default Realtek driver will not work with a WinUSB driver from Zadig. 

### Linux
The simplest way to install is to download the latest AppImage file from [the release page](https://github.com/KejPi/AbracaDABra/releases/latest), 
make it executable and run it. 

There are two versions of the AppImage - one for Intel/AMD 64 bit CPUs (x86_64) and the other is for ARM64 CPUs (aarch64) so make sure you download the one that matches your hardware.

The AppImage for x86_64 CPUs is compatible with Ubuntu 22.04 or newer. If you run it on other Linux distributions, GLIBC version 2.35 or higher is required. The AppImage for AARCH64 platform is built on Debian Bookworm for compatibility with Raspberry Pi 4/5s.

Arch Linux users can install AbracaDABra from the <a href="https://aur.archlinux.org/packages/abracadabra">AUR</a>.

## How to build
The following libraries are required:
* Qt >= 6.5.0
* libusb
* rtldsdr
* faad2 (default) or fdk-aac (optional)
* mpg123
* portaudio (optional but recommended)
* airspy (optional)
* SoapySDR (optional)
* QCustomPlot (optional) - it is automatically cloned from [GitHub](https://github.com/legerch/QCustomPlot-library) during CMake execution. Use QCUSTOMPLOT=OFF option to disable QCustomPlot usage in the application.

On a fresh Ubuntu 22.04 installation you can use the following commands to install all the required packages:

       sudo apt-get install git cmake build-essential mesa-common-dev 
       sudo apt-get install libcups2-dev libxkbcommon-x11-dev libxcb-cursor0 libxcb-cursor-dev
       sudo apt-get install libusb-dev librtlsdr-dev libfaad2 mpg123 libmpg123-dev libfaad-dev
       sudo apt-get install portaudio19-dev rtl-sdr    

Ubuntu 24.04 or lower does not support the version of Qt (at least 6.5.0) that is required for all the application's features. If you want to compile the application you should [install](https://doc.qt.io/qt-6/qt-online-installation.html) Qt using the [online installer](https://www.qt.io/download-qt-installer-oss). The following modules are sufficient to compile AbracaDABra:

![Screenshot from 2024-11-08 19-51-02](https://github.com/user-attachments/assets/56c32368-9472-4f8a-8d18-4cc65120b88b)

_Note:_ Currently you can still compile the application with the obsolete Qt version delivered with Ubuntu 24.04 but it is an unsupported configuration with limited functionality.
       
For optional Airspy support:       

       sudo apt-get install libairspy-dev

For optional SoapySDR support:       

       sudo apt-get install libsoapysdr-dev

Then clone the project:

       git clone https://github.com/KejPi/AbracaDABra/
       cd AbracaDABra

1. Create the build directory inside the working copy and enter it

       mkdir build
       cd build
   
3. Export the QT path

         export QT_PATH=$HOME/Qt/6.7.3/gcc_64

3. Run cmake

       cmake .. -DUSE_SYSTEM_QCUSTOMPLOT=OFF -DCMAKE_PREFIX_PATH=$QT_PATH/lib/cmake
       
    With optional Airspy support:          
       
       cmake .. -DUSE_SYSTEM_QCUSTOMPLOT=OFF -DCMAKE_PREFIX_PATH=$QT_PATH/lib/cmake -DAIRSPY=ON

    With optional SoapySDR support:          
       
       cmake .. -DUSE_SYSTEM_QCUSTOMPLOT=OFF -DCMAKE_PREFIX_PATH=$QT_PATH/lib/cmake -DSOAPYSDR=ON

   Optionally disabling QCustomPlot:

       cmake .. -DQCUSTOMPLOT=OFF -DCMAKE_PREFIX_PATH=$QT_PATH/lib/cmake

   Optionally disabling FMLIST support:

       cmake .. -DFMLIST=OFF -DCMAKE_PREFIX_PATH=$QT_PATH/lib/cmake
   
5. Run make

       make             

6. Install application for all users (optional)

       sudo make install
       sudo ldconfig

_Note:_ `CMAKE_INSTALL_PREFIX` is `/usr/local` by default. It means that application installs to `/usr/local/bin` and libraries are installed in `/usr/local/lib`. Make sure that `/usr/local/lib` is in your `ldconfig` path, if it is not then use the `LD_LIBRARY_PATH` environment variable when running AbracaDABra:

       LD_LIBRARY_PATH=/usr/local/lib:$QT_PATH/lib:$LD_LIBRARY_PATH /usr/local/bin/AbracaDABra &    

Optional SDRplay support:

SoapySDR is required, additionally [SDRplay API](https://www.sdrplay.com/api/) 3.15 must be installed and the [SoapySDR SDRplay3 plugin](https://github.com/pothosware/SoapySDRPlay3) needs to be built from source. Then specify `SOAPY_SDR_PLUGIN_PATH` when you run the application.



## USBFS buffer size

The USBFS buffer size limitation is a typical problem under Linux. A common symptom is that the RTL-SDR disconnects just after tuning. You may also see a message like this in the terminal:

```
Failed to submit transfer 10
Please increase your allowed usbfs buffer size with the following command:
echo 0 > /sys/module/usbcore/parameters/usbfs_memory_mb
```

You can set the buffer size to be unlimited using this command:
```
sudo bash -c 'echo 0 > /sys/module/usbcore/parameters/usbfs_memory_mb'
```
There are instructions on the internet that explain how to make this setting persistent, for example [here](https://github.com/OpenKinect/libfreenect2/issues/807)
       


