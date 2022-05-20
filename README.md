# AbracaDABra
Abraca DAB radio is DAB and DAB+ Software Defined Radio (SDR) for RTL-SDR sticks. 
It is based on Qt6 and uses _dabsdr_ demodulation library that is free for use but closed source. 

![Snímek obrazovky 2022-05-13 v 22 51 00](https://user-images.githubusercontent.com/6438380/169595803-3ead3b88-35ab-4f8e-8246-cacb97b54c32.png)

## Features
* Supports following input devices:
  * RTL-SDR (default device)
  * RTL-TCP (127.0.0.1:1234) 
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
![Snímek obrazovky 2022-05-20 v 21 06 26](https://user-images.githubusercontent.com/6438380/169595834-5a822b8f-6987-450f-aa12-d3c13f02936a.png)

Simple user interface that is focused on radio listening. Just select your favorite service from service list on the right side 
and enjoy the music with slide show and DL(+). 
Service can be easily added to favorites by clicking "star" icon.  Most of the elements in UI have tool tip with more information.

## Expert mode
![Snímek obrazovky 2022-05-13 v 22 49 36](https://user-images.githubusercontent.com/6438380/168489297-bf12730c-ffc9-415a-9e45-7e7cebe0de39.png)

In addition to basic mode, it shows ensemble tree with structure of services and additional details of currenly tuned service. 
Additionally it is possible to change the DAB channel manually in this mode. 
This is particularly useful when antenna adjustment is done in order to receive ensemble the is not received during automatic band scan.
Expert mode can be enabled in "three-dot" menu.

## How to build
Following libraries are required:
* Qt6
* libusb
* rtldsdr
* faad2 (default) or fdk-aac (optional)
* mpg123
* portaudio

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
* No possibility to change configuration for RTL_TCP source
* Application is hanging when audio output device is removed (like bluetooth headphones disconnected) - PortAudio does not support hot swap
