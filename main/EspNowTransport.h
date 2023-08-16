#pragma once

#include <array>
#include <cstdint>
#include <optional>

class EspNowTransport
{
public:
    struct DataMessage
    {
        char spsSerial[32];
        uint16_t pm01;
        uint16_t pm25;
        uint16_t pm10;
        float humidity;
        float temperature;
        float pressure;
        float voltage;
        int64_t timestamp;
    };

    bool setup(bool wakeup);
    bool init() const;
    std::optional<DataMessage> getLastMessage() const;
    bool isCompleted() const;
    bool sendResponce();

private:
    volatile bool isPeerInfoUpdated = false;
};
