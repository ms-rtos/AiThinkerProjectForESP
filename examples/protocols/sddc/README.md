
# SDDC example

EdgerOS SDDC example for ESP8266.

## Hardware Required

This example can be run on any commonly available ESP8266 development board.

## Configure the project

```
make menuconfig
```

Set following parameter under Serial Flasher Options:

* Set `Default serial port`.

Set following parameters under Example Configuration Options:

* Set `WiFi SSID` of the Spirit (Access-Point).

* Set `WiFi Password` of the Spirit (Access-Point).

## Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
make -j4 flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

