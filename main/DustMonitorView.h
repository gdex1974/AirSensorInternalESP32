#pragma once

#include <optional>
#include <string_view>
#include <cstdint>

struct SensorData
{
    float pressure = 0;
    float temperature = 0;
    float humidity = 0;
    int pm01 = -1;
    int pm2p5 = -1;
    int pm10 = -1;
    float voltage = 0;
    uint32_t flags = 0;
};

struct DustMonitorViewData
{
    SensorData innerData;
    std::optional<SensorData> outerData;
};

namespace embedded
{
template<typename T>
struct Rect;

class PersistentStorage;
class EpdInterface;
}

class DustMonitorView
{
public:
    DustMonitorView(embedded::PersistentStorage& storage, embedded::EpdInterface &epdInterface, const DustMonitorViewData& dustMoinitorViewData)
            : storage(storage), epdInterface(epdInterface), externalViewData(dustMoinitorViewData) {}

    bool setup(bool wakeUp);

    void updateView();

    void hibernate() const;

private:
    enum class UpdateType
    {
        Full,
        Partial,
        DeepSleep,
    };

    struct StoredData
    {
        DustMonitorViewData currentDisplayData;
        uint32_t refreshCounter = 0;
        UpdateType updateType = UpdateType::Full;
    };

    bool SyncViewData();
    void updateSensorArea(const embedded::Rect<int>& dataArea, SensorData& storedValue, const SensorData& newValue) const;
    void displayText(std::string_view textString, const embedded::Rect<int>& rectArea) const;
    void drawPMData(const embedded::Rect<int>& pm01Area, int value) const;

    embedded::PersistentStorage& storage;
    embedded::EpdInterface& epdInterface;
    const DustMonitorViewData& externalViewData;
    StoredData storedData;

    void drawTime() const;
    void refreshScreen(bool needFullRefresh);
};
