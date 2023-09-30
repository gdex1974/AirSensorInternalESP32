#pragma once

#include <array>
#include <cstdint>
#include <optional>

class WiFiManager;

class EspNowTransport
{
public:
    struct DataMessage
    {
        char spsSerial[32];
        int16_t pm01;
        int16_t pm25;
        int16_t pm10;
        float humidity;
        float temperature;
        float pressure;
        float voltage;
        int64_t timestamp;
    };

    EspNowTransport(WiFiManager &wifiManager)
            : wifiManager(wifiManager) {}
    bool setup(bool wakeup);
    bool init() const;
    std::optional<DataMessage> getLastMessage() const;
    bool isCompleted() const;
    bool sendResponce();

private:
    WiFiManager &wifiManager;
    volatile bool isPeerInfoUpdated = false;
};
