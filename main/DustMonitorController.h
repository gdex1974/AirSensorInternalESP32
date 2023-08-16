#pragma once

#include "PTHProvider.h"
#include "SPS30DataProvider.h"
#include "DustMonitorView.h"
#include "EspNowTransport.h"
#include "WiFiTimeSync.h"

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
    ProcessStatus process();

    bool canHybernate() const;
    void hibernate();

private:

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
    };

    PTHProvider meteoData;
    SPS30DataProvider dustData;
    WiFiTimeSync timeSync;
    EspNowTransport transport;
    embedded::PersistentStorage &storage;
    DustMonitorViewData dustMoinitorViewData;
    DustMonitorView view;
    ControllerData controllerData;

    bool circleCompleted = false;
    bool needsToUpdateClock = true;
};
