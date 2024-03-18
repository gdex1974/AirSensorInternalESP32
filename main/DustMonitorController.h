#pragma once

#include "DustMonitorView.h"
#include "EspNowTransport.h"
#include "PTHProvider.h"
#include "SPS30DataProvider.h"
#include "WiFiManager.h"

#include <ctime>

namespace embedded
{
class PersistentStorage;
}

class DustMonitorController
{
public:
    enum class ProcessStatus
    {
        AwaitingForSync,
        NeedRefreshClock,
        Completed,
    };
    DustMonitorController(embedded::PersistentStorage &storage,
                          embedded::PacketUart &uart,
                          embedded::I2CHelper& i2CHelper,
                          embedded::EpdInterface& epdInterface);
    bool setup(bool wakeUp);
    ProcessStatus process() const;

    bool isMeasuring() const { return controllerData.sps30Status == SPS30Status::Measuring; }
    void hibernate();

private:
    static bool isTimeSyncronized() ;

    enum class SPS30Status
    {
        Startup,
        Sleep,
        Measuring,
    };

    struct ControllerData
    {
        SPS30Status sps30Status = SPS30Status::Startup;
        time_t lastPTHMeasureTime = 0;
        time_t lastPMMeasureTime = 0;
        time_t lastExternalDataTime = 0;
        time_t lastTimeSyncTime = 0;
    };

    WiFiManager wifiManager;
    PTHProvider meteoData;
    SPS30DataProvider dustData;
    EspNowTransport transport;
    embedded::PersistentStorage &storage;
    DustMonitorViewData dustMoinitorViewData;
    DustMonitorView view;
    ControllerData controllerData;

    bool fullCircle = false;
    bool timeSyncInitialized = false;
    static void updateDisplayTask(void* pvParameters);
    [[noreturn]] void updateDisplayTask();
    static void timeSyncTask(void* pvParameters);
    [[noreturn]] void timeSyncTask();
    static void measurementTask(void* pvParameters);
    [[noreturn]] void measurementTask();
    static void externalDataTask(void* pvParameters);
    [[noreturn]] void externalDataTask();
};
