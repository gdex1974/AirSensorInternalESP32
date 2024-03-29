# Air Quality Sensor - internal unit

## Description
The Air Quality Sensor project contains two parts:
- [External unit](https://github.com/gdex1974/AirSensorExternalESP32) - the main unit placed inside the building
- Internal unit - the unit placed outside the building

The internal unit is equipped with e-Ink display and shows the temperature, humidity and pressure alongside with PM1/2.5/10 measurements from external unit and its own internal sensors. It's powered by 18500 accumulator and receive data from the external unit via Esp-Now transport protocol.
This unit is based on Firebeetle-ESP32 board, with the firmware based on ESP-IDF framework. The board was chosen due to 32K oscillator for RTC and low power consumption.

## Hardware

- [Firebeetle-ESP32](https://wiki.dfrobot.com/FireBeetle_ESP32_IOT_Microcontroller(V3.0)__Supports_Wi-Fi_&_Bluetooth__SKU__DFR0478) main microcontroller board
- [SPS30](https://botland.store/air-quality-sensors/15062-dust-air-sensor-sps30-pm10-pm25-pm4--5904422366094.html) particulate matter sensor
- [BME280](https://botland.store/pressure-sensors/11803-bme280-humidity-temperature-and-pressure-5904422366179.html) temperature, humidity and pressure sensor
- [Pololu Mini MOSFET Switch](https://www.pololu.com/product/2810) low voltage module switch
- [Pololu U1V10F5](https://www.pololu.com/product/2564) 5V step-up module
- [18650 battery case](https://botland.store/battery-holders/5242-cell-holder-for-1x-18650-5904422333393.html) 18650 battery case
- [18650 accumulator](https://botland.store/li-ion-batteries/15216-18650-li-ion-samsung-inr18650-35e-3500mah-5904422343071.html) 18650 accumulator
- [3,7" WaveShare e-ink display](https://www.waveshare.com/wiki/3.7inch_e-Paper_HAT) 3,7" WaveShare e-ink display
- [2x20 2,54mm DIP header](https://www.kiwi-electronics.com/en/40-pin-male-header-2x20-1174) 2x20 2,54mm DIP header
- [Prototyping board 40x60mm 2,54mm pitch](https://www.kiwi-electronics.com/en/prototyping-board-4x6cm-2-54mm-pitch-3828) Prototype board 40x60mm 2,54mm pitch

## Software

- [esp-idf](https://github.com/espressif/esp-idf/releases/tag/v4.4.5) main framework
- [general-support-library](https://github.com/gdex1974/embedded-general-support-library) common embedded library
- [SimpleDrivers](https://github.com/gdex1974/embedded-simple-drivers) simple embedded drivers for SPS30 and BME280 sensors

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
  - `VIN` is connected to the VBAT pin of the Firebeetle board
  - `GND` is connected to the GND pin of the Firebeetle board
  - `VOUT`is connected to the VCC pin of the SPS30 sensor
- e-ink display is connected to the SPI port of the Firebeetle board. It's attached to the 2x20 DIP header soldered on the protoboard to emulate the Raspberry PI 40-pin header. To make it, the protoboard have to be cut to the 3x20 size and the header should be soldered to the board. The cords to the display are soldered to the socket (the 1st pin is top left):
  - `CS`: pin 24 of the header is connected to the `IO25` pin of the board
  - `DC`: pin 22 of the header is connected to the `IO4` pin of the board
  - `RST`: pin 11 of the header is connected to the `IO15` pin of the board
  - `BUSY`: pin 18 of the header is connected to the `IO16` pin of the board
  - `CLK`: pin 23 of the header is connected to the `IO18` pin of the board
  - `DIN`: pin 19 of the header is connected to the `IO23` pin of the board
  - `GND` pin 6 (possibly 9, 14, 20, 25, 30, 34, 39) of the socket is connected to the `GND` pin of the board
  - `3.3V` pin 1 of the socket is connected to the `3V3` pin of the board

![opened.jpeg](docs%2Fopened.jpeg)
![assembled.jpeg](docs%2Fassembled.jpeg)
![completed.jpeg](docs%2Fcompleted.jpeg)

## Firmware

The firmware is based on ESP-IDF framework (tested version is 4.4.5) and is written in C++ language. The main task is to show the data from internal sensors alongside with the ones received from the external unit via Esp-Now protocol. The firmware is divided into several modules:

- components - contains the code of the external unit
  - general-support library - contains the code for general support functions, obtained from this repository.
  - SimpleDrivers - contains the code for the SPS30, BME280 and e-Ink display drivers, obtained from this repository.
- main - contains the main code of the external unit's firmware
  - AppConfig - contains the code for the application's configuration
  - AppMain - contains the app_main() function and hosts the controller object
  - DustMonitorController - contains the code for the controller class handling the main logic of the firmware
  - DustMonitorView - contains the code for the class providing the data for the e-Ink display
  - EspNowTransport - contains the code for the communication with the main unit based on Esp-Now protocol
  - PTHProvider - contains the code for the class providing the data from BME280 sensor
  - SPS30DataProvider - contains the code for the class providing the data from SPS30 sensor
  - WiFiManager - contains the code for the class providing the Wi-Fi connection management
- CMakeLists.txt - main CMake file for the firmware
- sdkconfig - default configuration file for the ESP-IDF framework.
