MIDI footswitch controller over Bluetooth
------------------------------------------

The project contains source code for MIDI footswitch controller which is using
MIDI over Bluetooth to transfer commands. It's almost ready (MIDI part is done),
only GPIO part (reacting to buttons) is not implemented.

The goal of this controller is to control all kind of applications that
support MIDI over Bluetooth. It could Ableton Live, or in my case music
applications on IPad (synths, loopers and others)

ESP32-WROOM-32 module is used as a main platform. It provides decent
Wi-Fi and BLE support with a good documentation and examples. I use
[ESP32-DevKitC][1] module which is very convinient for development.

Basic components:

* ESP32-DevKitC module
* Battery holder (I'm going to use CR-123A batteries, but not tested them yet).
* Power switch
* Enclosure
* 6 momentary footswitches

At the end it should be something like [Looperverse Pedal][2]

[1] https://www.espressif.com/en/products/hardware/esp32-devkitc/overview
[2] https://www.youtube.com/watch?v=bb-JcCgHaWg
