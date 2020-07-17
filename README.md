<a href="https://www.microchip.com" rel="nofollow"><img src="images/microchip.png" alt="MCHP" width="300"/></a>

# ATtiny817 Digital Sound Recorder using DAC with evaluation kit

This example demonstrates a digital ound recorder using ADC for sampling and DAC for playback. Samples timed at a defined frequency, controlled by a timer and event system. SPI used to store raw data on an SD card, driver included. This example demonstrates direct register read and write operations for peripheral initialization, and therefore does not use peripheral drivers.

The example is explained in more details in the application note [AN2547](http://ww1.microchip.com/downloads/en/AppNotes/00002547A.pdf).

## Related Documentation

- [AN2547 -  Digital Sound Recorder using DAC](http://ww1.microchip.com/downloads/en/AppNotes/00002547A.pdf)
- [ATtiny817 Product Page](https://www.microchip.com/wwwproducts/en/ATtiny817)

## Software Used

- [MPLAB X IDE v5.40 or later](https://www.microchip.com/mplab/mplab-x-ide)
- [XC8 (v2.20)](https://www.microchip.com/mplab/compilers) alternatively [AVR/GNU C Compiler 5.4.0](https://www.microchip.com/mplab/avr-support/avr-and-arm-toolchains-c-compilers) can be used
- ATtiny_DFP 2.2.89 or later

## Hardware Used

-   [ATtiny817 Xplained Pro](https://www.microchip.com/DevelopmentTools/ProductDetails/attiny817-xpro) or [ATtiny817 Xplained Mini](https://www.microchip.com/developmenttools/ProductDetails/attiny817-xmini)
-   [I/O1 Xplained Pro Extension Kit](https://www.microchip.com/DevelopmentTools/ProductDetails/ATIO1-XPRO)
-   [OLED1 Xplained Pro Extension Kit](https://www.microchip.com/developmenttools/ProductDetails/atoled1-xpro)
-   Microphone and speaker

## Setup

- Connect I/O1 Xplained Pro to `EXT1`
- Connect OLED1 Xplained Pro to `EXT3`
- Connect a microphone to `ADC_IN7 (PA7)`
- Connect a speaker to `DAC_OUT (PA6)`

## Operation

1. Download the zip file or clone the example to get the source code.
2. Open `AVR42777DigitalSoundRecorder.X` in MPLAB.
3. Connect the ATtiny817 Xplained Pro with your computer. 
4. Make sure the kit is selected as the tool to be programmed under project settings.
5. Press the make and program button to program the device.
6. Use the buttons on the OLED display to start recording, erase or start playback.

## Conclusion

We have here shown how you can use a ATtiny817 as a digital sound recorder.