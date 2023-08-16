# Air Quality Sensor - internal unit

## Description

This is a part of the air quality sensor project. The internal unit is equipped with e-Ink display and shows the temperature, humidity and pressure alongside with PM1/2.5/10 measurements from external unit and its own internal sensors. It's powered by 18500 accumulator and receive data from the external unit via Esp-Now transport protocol.
This unit is based on Firebeetle-ESP32 board, with the firmware based on ESP-IDF framework.

## Hardware

- [Firebeetle-ESP32](https://wiki.dfrobot.com/FireBeetle_ESP32_IOT_Microcontroller(V3.0)__Supports_Wi-Fi_&_Bluetooth__SKU__DFR0478) main microcontroller board
- [SPS30](https://sensirion.com/products/catalog/SPS30/) particulate matter sensor
- [BME280](https://www.bosch-sensortec.com/bst/products/all_products/bme280) temperature, humidity and pressure sensor
- [Pololu 2810](https://www.pololu.com/product/2810) low voltage module switch
- [HW-105 5V step-up module](https://www.aliexpress.com/item/33025722716.html) 5V step-up module
- [18650 battery case](https://www.aliexpress.com/item/1005001660193629.html) 18650 battery case
- [18650 accumulator]() 18650 accumulator
- [3,7" WaveShare e-ink display](https://www.waveshare.com/wiki/3.7inch_e-Paper_HAT) 3,7" WaveShare e-ink display
- [2x20 2,54mm DIP header](https://www.kiwi-electronics.com/en/40-pin-male-header-2x20-1174) 2x20 2,54mm DIP header
- [Prototyping board 40x60mm 2,54mm pitch] (https://www.kiwi-electronics.com/en/prototyping-board-4x6cm-2-54mm-pitch-3828) Prototype board 40x60mm 2,54mm pitch

## Software

- [esp-idf](https://github.com/espressif/esp-idf/releases/tag/v4.4.5) main framework
- [arduino-esp32](https://github.com/espressif/arduino-esp32/releases/tag/2.0.11) arduino framework for esp32

Currently, ESP-IDF 4.4.5 build system overrides C/C++ standard using `-std=gnu99` and `-std=gnu++11` flags.
It happens into estp-idf/tools/cmake/build.cmake file. To avoid this, the following lines should be commented out:

`list(APPEND c_compile_options   "-std=gnu99")`

`list(APPEND cpp_compile_options "-std=gnu++11")`

The patch file `fix_esp_idf_build.patch` is provided in the repository.

## Setup

All parts of the unit are fixed on frame and covered by external cover. The frame is designed to fit the Firebeetle board, switch, step-up converter, BME280 and SPS30 sensors. It's attached to battery case and the whole module is placed into the external cover. The fame and cover parts are 3D printed.
Connection schema:
- SPS30 sensor is connected to the UART2 port of the Firebeetle board.
  - `VCC` pin of the sensor is connected to the +5V output of the step-up converter
  - `RX` pin of the sensor is connected to the `IO26` pin of the board
  - `TX` pin of the sensor is connected to the `IO27` pin of the board
  - `SEL` pin of the sensor is not connected
  - `GND` pin of the sensor is connected to the `GND` pin of the board
- BME280 module is connected to the I2C port of the Firebeetle board
  - `SDA` pin of the sensor is connected to the `IO21` pin of the board
  - `SCL` pin of the sensor is connected to the `IO22` pin of the board
  - `GND` pin of the sensor is connected to the `GND` pin of the board
  - `VCC` pin of the sensor is connected to the `3V3` pin of the board
- Pololu 2810 switch is mounted under the main board:
  - `GND` connected to the GND pin of the board
  - `Enable` pin connected to the IO13 pin of the Firebeetle board
  - `VIN` pin connected to the battery+ of the Firebeetle board
  - `VOUT` pin connected to the + input of the step-up converter
- step-up converter's is fixed into separated place on the frame under the board
  - `+` input is connected to the VBAT pin of the Firebeetle board
  - `-` input is connected to the GND pin of the Firebeetle board
  - `+` output is connected to the VCC pin of the SPS30 sensor
- e-ink display is connected to the SPI port of the Firebeetle board. It's attached to the 2x20 DIP header soldered on the protoboard to emulate the Raspberry PI 40-pin header. To make it, the protoboard have to be cut to the 3x20 size and the header should be soldered to the board. The cords to the display are soldered to the socket (the 1st pin is top left):
  - `CS`: pin 24 of the header is connected to the `IO25` pin of the board
  - `DC`: pin 22 of the header is connected to the `IO4` pin of the board
  - `RST`: pin 11 of the header is connected to the `IO15` pin of the board
  - `BUSY`: pin 18 of the header is connected to the `IO16` pin of the board
  - `CLK`: pin 23 of the header is connected to the `IO18` pin of the board
  - `DIN`: pin 19 of the header is connected to the `IO23` pin of the board
  - `GND` pin 6 (possibly 9, 14, 20, 25, 30, 34, 39) of the socket is connected to the `GND` pin of the board
  - `3.3V` pin 1 of the socket is connected to the `3V3` pin of the board

## Firmware

The firmware is based on ESP-IDF framework (tested version is 4.4.5) and is written in C++ language. The main task is to show the data from internal sensors alongside with the ones received from the external unit via Esp-Now protocol. The firmware is divided into several modules:

- components - contains the code of the external unit
  - arduino - here the arduino framework is placed. The tested version is 2.0.11
  - general-support library - contains the code for general support functions, obtained from this repository.
  - SimpleDrivers - contains the code for the SPS30, BME280 and e-Ink display drivers, obtained from this repository.
- main - contains the main code of the external unit's firmware
  - AppConfig - contains the code for the application's configuration
  - ArduinoStubs - contains setup() and loop() functions, required by the arduino framework
  - DustMonitorController - contains the code for the controller class handling the main logic of the firmware
  - EspNowTransport - contains the code for the communication with the main unit based on Esp-Now protocol
  - PTHProvider - contains the code for the class providing the data from BME280 sensor
  - SPS30DataProvider - contains the code for the class providing the data from SPS30 sensor
  - WiFiTimeSync - contains the code for the class providing the time synchronization via NTP protocol
- CMakeLists.txt - main CMake file for the firmware
- sdkconfig - default configuration file for the ESP-IDF framework.
