MIDI footswitch controller over Bluetooth
------------------------------------------

The project contains source code for MIDI footswitch controller which is using
MIDI over Bluetooth to transfer commands.

The goal of this controller is to control all kind of applications that
support MIDI over Bluetooth. It could be Ableton Live, or in my case music
applications on IPad (synths, loopers and others)

ESP32 radio module is used as a main platform. It provides decent
Wi-Fi and BLE support with a good documentation and examples. I use
[ESP32-DevKitC](https://www.espressif.com/en/products/hardware/esp32-devkitc/overview)
module which is very convinient for development.

The device consists of 6 footswitches which are triggering MIDI commands. They
are fully configurable. To do that one can connect to `bigfoot` WiFi network
(default password is '12341234') and open the [configuration page](http://192.168.4.2).

The configuration page also can be used as a tool for testing MIDI commands.

Firmware
--------

1) Install [esp-idf](https://docs.espressif.com/projects/esp-idf/en/stable/get-started/index.html#get-esp-idf).
I used stable 3.3 version.
2) Build and flash the esp32:
```
    mkdir build && cd build
    cmake ..
    # set flash size
    make menuconfig
    # could require changing of the serial port
    make && make flash
```

Notes
------

* Use GPIO pins 15,17,18,19,21,22 for buttons. The pins are pulled up,
so buttons should connect pins to the ground to trigger midi commands. By default
buttons are configured as in [Looperverse Pedal](https://www.youtube.com/watch?v=bb-JcCgHaWg).
* In current version of the firmware WiFI and HTTP server are shutting down after
5 minutes of idle time (if you need to configure buttons just restart the device),
and the device is shutting down after 15 minutes of unused time. The button
connected to GPIO15 wakes up the device.

Basic components:
-----------------

* ESP32-DevKitC module
* Battery holder (I'm going to use CR-123A batteries, but not tested them yet).
* Power switch
* Enclosure
* 6 momentary footswitches
* One P-channel transistor.

Sample schematic
-----------------

![This is basic schematic of the project](https://umarta.com/scr/umqbbmcinnpxykzwyohl.png)
