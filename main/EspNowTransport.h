#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <memory>
#include "GroupBitView.h"

namespace embedded
{
    class PersistentStorage;
}
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
        uint32_t flags = 0;
    };

    EspNowTransport(embedded::PersistentStorage& storage, WiFiManager &wifiManager);
    bool setup(bool wakeup);
    bool init(GroupBitView event);
    std::optional<DataMessage> getLastMessage(uint32_t timeoutMilliseconds) const;
    bool sendResponce();
    void hibernate() const;

    void threadFunction();
private:
    embedded::PersistentStorage& storage;
    WiFiManager &wifiManager;
    std::unique_ptr<void, void(*)(void*)> wifiEventGroup;
    volatile bool isPeerInfoUpdated = false;
    int attemptsCounter = 0;
    static constexpr int maxAttempts = 10;
    GroupBitView externalEvent;
};
