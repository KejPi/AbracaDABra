# AbracaDABra
Abraca DAB radio is DAB and DAB+ Software Defined Radio (SDR) for RTL-SDR sticks. 
It is based on Qt6 and uses _dabsdr_ demodulation library that is free for use but closed source. 

![Snímek obrazovky 2022-05-13 v 22 51 00](https://user-images.githubusercontent.com/6438380/168488691-65716cac-63fa-49fe-9e89-d382956c28d3.png)

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
![Snímek obrazovky 2022-05-13 v 22 38 58](https://user-images.githubusercontent.com/6438380/168489179-7157c8e5-ecba-4cc2-9e7e-31432479acc2.png)

Service list is on the right side and currently playing audio service with all details on the left side. 
Service can be added to favorites by clicking "star" icon. 
Most of the elements in UI have tool tip with more information.

## Expert mode

![Snímek obrazovky 2022-05-13 v 22 49 36](https://user-images.githubusercontent.com/6438380/168489297-bf12730c-ffc9-415a-9e45-7e7cebe0de39.png)

In addition to basic mode, it shows ensemble tree with structure of services. 
Additionally it is possible to change the DAB channel manually in this mode. 
This is particularly useful when antenna adjustment is done in order to receive ensemble the is not received during automatic band scan.
Expert mode can be enabled in "three-dot" menu.

## Known limitations
* Slower switching between ensembles
* Reconfiguration not supported for data services
* Only SLS data service is supported 
* Apple Silicon build is not optimized
* No possibility to change configuration for RTL_TCP source
* Application is hanging when audio output device is removed (like bluetooth headphones disconnected) - PortAudio does not support hot swap
