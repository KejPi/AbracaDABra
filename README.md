# AbracaDABra
Abraca DAB radio is DAB and DAB+ Software Defined Radio (SDR) for RTL-SDR sticks. 
It is based on Qt6 and uses _dabsdr_ demodulation library that is free for use but not open source. 

<img width="1634" alt="Snímek obrazovky 2022-06-28 v 22 32 25" src="https://user-images.githubusercontent.com/6438380/176279785-1e212201-3c1d-428f-9416-b1b0b464238b.png">

## Features
* Supports following input devices:
  * RTL-SDR (default device)
  * RTL-TCP
  * Raw file input (in expert mode only, int16_t or uint8_t format)
* Band scan with automatic service list
* Service list management
* DAB (mp2) and DAB+ (AAC) audio decoding
* Dynamic label (DL) and Dynamic label plus (DL+)
* MOT slideshow (SLS) and categorized slideshow (CatSLS) from PAD or from secondary data service.
* Audio services reconfiguration (experimental support)
* Ensemble structure view with all technical details.
* Raw file dumping from RTL-SDR or RTL-TCP sources
* Only band III and DAB mode 1 is supported.
* Simple user-friendly interface, trying to follow DAB _Rules of implementation (TS 103 176)_
* Multiplatform

## Basic mode
<img width="663" alt="Snímek obrazovky 2022-06-28 v 22 19 47" src="https://user-images.githubusercontent.com/6438380/176277787-7737ca9b-adb1-4d91-bf5b-bd9331d27663.png">

Simple user interface that is focused on radio listening. Just select your favorite service from service list on the right side 
and enjoy the music with slide show and DL(+). 
Service can be easily added to favorites by clicking "star" icon.  Most of the elements in UI have tool tip with more information.

## Expert mode
<img width="873" alt="Snímek obrazovky 2022-06-28 v 22 37 21" src="https://user-images.githubusercontent.com/6438380/176281878-405e3d99-64ef-4a95-a30e-7b74127b178e.png">

In addition to basic mode, it shows ensemble tree with structure of services and additional details of currenly tuned service. 
Additionally it is possible to change the DAB channel manually in this mode. 
This is particularly useful when antenna adjustment is done in order to receive ensemble the is not received during automatic band scan.
Expert mode can be enabled in "three-dot" menu.

## Expert settings
Some settings can only be changed by editting of the AbracaDABra.ini file. File location is OS dependent:
* MacOS & Linux: `$HOME/.config/AbracaDABra/AbracaDABra.ini`
* Windows: `%USERPROFILE%\AppData\Roaming\AbracaDABra\AbracaDABra.ini`

Following settings can be changed for RTL-SDR device by editing AbracaDABra.ini:

      [RTL-SDR]
      bandwidth=0     # 0 means automatic bandwidth, positive value means bandwidth value in Hz (e.g. bandwidth=1530000 is 1.53MHz BW)
      bias-T=false    # set to true to enable bias-T

Application shall not run while changing these settings, otherwise it will be overwritten.

## How to build
Following libraries are required:
* Qt6
* libusb
* rtldsdr
* faad2 (default) or fdk-aac (optional)
* mpg123
* portaudio

For a fresh Ubuntu 22.04 installation you can use the following commands:

       sudo apt-get install git cmake build-essential mesa-common-dev
       sudo apt-get install libusb-dev librtlsdr-dev libfaad2 mpg123 libmpg123-dev libfaad-dev
       sudo apt-get install portaudio19-dev qt6-base-dev qt6-multimedia-dev libqt6svg6-dev rtl-sdr

Then clone the project:

       git clone https://github.com/KejPi/AbracaDABra/
       cd AbracaDABra

1. Create build directory inside working copy and change to it

       mkdir build
       cd build

2. Run cmake

       cmake ..

3. Run make

       make
       
## Known limitations
* Slower switching between ensembles
* Reconfiguration not supported for data services
* Only SLS data service is supported 
* Apple Silicon build is not optimized
* Application is hanging when audio output device is removed (like bluetooth headphones disconnected) - PortAudio does not support hot swap
