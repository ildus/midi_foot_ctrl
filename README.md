MIDI footswitch controller over Bluetooth
------------------------------------------

The project contains source code for MIDI footswitch controller which is using
MIDI over Bluetooth to transfer commands. It's almost ready (MIDI part is done),
GPIO part (reacting to buttons) is partially implemented.

The goal of this controller is to control all kind of applications that
support MIDI over Bluetooth. It could Ableton Live, or in my case music
applications on IPad (synths, loopers and others)

ESP32-WROOM-32 module is used as a main platform. It provides decent
Wi-Fi and BLE support with a good documentation and examples. I use
[ESP32-DevKitC](https://www.espressif.com/en/products/hardware/esp32-devkitc/overview) module which is very convinient for development.

Basic components:

* ESP32-DevKitC module
* Battery holder (I'm going to use CR-123A batteries, but not tested them yet).
* Power switch
* Enclosure
* 6 momentary footswitches

Using
--------
1) Install [esp-idf](https://docs.espressif.com/projects/esp-idf/en/stable/get-started/index.html#get-esp-idf). I used stable  3.3 version.
2) Build and flash the esp32:
```
    mkdir build && cd build
    cmake ..
    # set flash size
    make menuconfig
    # could require to change serial port
    make && make flash
```

At the end it should be something like [Looperverse Pedal](https://www.youtube.com/watch?v=bb-JcCgHaWg).
