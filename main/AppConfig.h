#pragma once

#include <string_view>

struct AppConfig
{
    static const std::string_view WiFiSSID;
    static const std::string_view WiFiPassword;
    static const std::string_view ntpServer;
    static const int timeZoneOffset;
    static const int daylightSavingOffset;
    // I2C Address of the BME280
    static const uint8_t bme280Address;
    // Serial2 is used for SPS30 communication
    static const int8_t serial2RxPin;
    static const int8_t serial2TxPin;
    // Pin used to control the LED
    static const uint8_t ledPin;
    // Pins used to control the EPD
    static const uint8_t epdBusyPin;
    static const uint8_t epdResetPin;
    static const uint8_t epdDcPin;
    static const uint8_t epdCsPin;
    // Pins used to control the step-up converter and measure battery voltage
    static const int8_t stepUpPin;
    static const int8_t voltagePin;
    // Voltage divider correction factor for the specific board
    static const float voltageDividerCorrection;
};