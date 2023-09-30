#include "DustMonitorController.h"
#include "AppConfig.h"

#include "TimeFunctions.h"
#include "PersistentStorage.h"

#include "AnalogPin.h"
#include "esp32-esp-idf/GpioPinDefinition.h"

#include <driver/rtc_io.h>
#include <lwip/apps/sntp.h>

#include "Debug.h"
#include "Delays.h"

namespace
{
const std::string_view viewDataTag = "DMC1";
const std::string_view controllerDataTag = "DMC2";

int readVoltageRaw(uint8_t pin)
{
    embedded::GpioPinDefinition voltagePin { pin };
    const auto voltage = embedded::AnalogPin(voltagePin).singleRead();
    DEBUG_LOG("VoltageRaw is " << voltage)
    return voltage;
}

void initStepUpControl()
{
    const auto stepUpPin = (gpio_num_t)AppConfig::stepUpPin;
    rtc_gpio_init(stepUpPin); //initialize the RTC GPIO port
    rtc_gpio_set_direction(stepUpPin, RTC_GPIO_MODE_OUTPUT_ONLY); //set the port to output only mode
}

void switchStepUpConversion(bool enable)
{
    const auto stepUpPin = (gpio_num_t)AppConfig::stepUpPin;
    rtc_gpio_hold_dis(stepUpPin);
    rtc_gpio_set_level(stepUpPin, enable ? 1 : 0);
    if (enable)
    {
        embedded::delay(20);
    }
}

void holdStepUpConversion()
{
    const auto stepUpPin = (gpio_num_t)AppConfig::stepUpPin;
    rtc_gpio_hold_en(stepUpPin);
}

}

bool DustMonitorController::setup(bool wakeUp)
{
    DEBUG_LOG((wakeUp ? "Waking up the controller" : "Initial setup of the controller"))
    initStepUpControl();
    wifiManager.initWiFiSubsystem();
    if (wakeUp)
    {
        if (auto data = storage.get<DustMonitorViewData>(viewDataTag))
        {
            DEBUG_LOG("Restoring dust monitor view data")
            dustMoinitorViewData = *data;
            DEBUG_LOG("External sensor's data is " << (dustMoinitorViewData.outerData ? "available" : "absent"))
        }
        if (auto data = storage.get<ControllerData>(controllerDataTag))
        {
            controllerData = *data;
            DEBUG_LOG("Last external message recieved: " << controllerData.lastExternalDataTime)
        }
    }
    else
    {
        switchStepUpConversion(true);
    }
    const bool pmSetupResult = dustData.setup(wakeUp);
    if (pmSetupResult && !wakeUp)
    {
        dustData.sleep();
        switchStepUpConversion(false);
    }
    const bool meteoSetupResult = meteoData.setup(wakeUp);
    const bool transportResult = transport.setup(wakeUp);
    const bool viewResult = view.setup(wakeUp);
    return transportResult && viewResult && meteoSetupResult && pmSetupResult;
}

DustMonitorController::ProcessStatus DustMonitorController::process()
{
    if (const auto state = wifiManager.getState(); state == WiFiManager::State::Connected)
    {
        if (isTimeSyncronized())
        {
            if (wifiManager.getState() == WiFiManager::State::Connected)
            {
                timeSyncInitialized = false;
                sntp_stop();
                DEBUG_LOG("Time is synchronized, disconnecting")
                wifiManager.stopSTA();
                bool successful = transport.setup(true);
                DEBUG_LOG("ESP-Now initialization " << (successful ? "completed" : "failed"))
                (void)successful;
            }
        }
        else if (!timeSyncInitialized)
        {
            DEBUG_LOG("WiFi connected, waiting for time syncronization")

            if (sntp_enabled())
            {
                sntp_stop();
            }
            sntp_setoperatingmode(SNTP_OPMODE_POLL);
            sntp_setservername(0, (char*)AppConfig::ntpServer.data());
            sntp_setservername(1, (char*)nullptr);
            sntp_setservername(2, (char*)nullptr);
            sntp_init();
            timeSyncInitialized = true;
        }
    }
    else if (state == WiFiManager::State::Stopped || state == WiFiManager::State::NotInitialized)
    {
        if (!isTimeSyncronized() && !timeSyncInitialized)
        {
            DEBUG_LOG("Starting STA")
            wifiManager.startSTA(AppConfig::WiFiSSID, AppConfig::WiFiPassword);
        }
    }

    if (!isTimeSyncronized())
    {
        return ProcessStatus::AwaitingForSync;
    }
    auto currentTime = time(nullptr);
    if (auto message = transport.getLastMessage())
    {
        controllerData.lastExternalDataTime = currentTime;
        transport.sendResponce();
        DEBUG_LOG("External voltage: " << embedded::SerialOut::precision { 2 } << message->voltage
                                       << ", timestamp: " << message->timestamp)
        if (!dustMoinitorViewData.outerData)
        {
            dustMoinitorViewData.outerData.emplace();
        }
        auto &sensorData = *dustMoinitorViewData.outerData;
        sensorData.humidity = message->humidity;
        sensorData.temperature = message->temperature;
        sensorData.pressure = message->pressure;
        sensorData.pm01 = message->pm01;
        sensorData.pm2p5 = message->pm25;
        sensorData.pm10 = message->pm10;
        sensorData.voltage = message->voltage;
    }
    if (dustMoinitorViewData.outerData && (currentTime > controllerData.lastExternalDataTime + 180))
    {
        DEBUG_LOG("External sensor's data is outdated: " << controllerData.lastExternalDataTime
                                                         << ", removing indication")
        dustMoinitorViewData.outerData.reset();
    }

    if (currentTime - controllerData.lastPTHMeasureTime > 58) // one second gap
    {
        if (meteoData.activate())
        {
            if (meteoData.doMeasure())
            {
                DEBUG_LOG("PTH measurement done")
                controllerData.lastPTHMeasureTime = currentTime;
                dustMoinitorViewData.innerData.humidity = meteoData.getHumidity();
                dustMoinitorViewData.innerData.temperature = meteoData.getTemperature();
                dustMoinitorViewData.innerData.pressure = meteoData.getPressure();
                meteoData.hibernate();
            }
        }
    }

    bool shallStartMeasurement = controllerData.sps30Status == SPS30Status::Startup;
    if (!shallStartMeasurement && controllerData.sps30Status != SPS30Status::Measuring)
    {
        shallStartMeasurement = getLocalTime(time(nullptr)).tm_min == 59;
    }

    if (shallStartMeasurement && currentTime - controllerData.lastPMMeasureTime > 10*60)
    {
        if (controllerData.sps30Status != SPS30Status::Measuring)
        {
            switchStepUpConversion(true);
            const auto voltagePin = (gpio_num_t)AppConfig::voltagePin;
            const float rawToVolts = 3.3f / 0.5f / 4095.f * AppConfig::voltageDividerCorrection;
            dustMoinitorViewData.innerData.voltage = float(readVoltageRaw(voltagePin)) * rawToVolts;
            if (controllerData.sps30Status == SPS30Status::Sleep)
            {
                DEBUG_LOG("Waking up SPS30")
                dustData.wakeUp();
            }
            DEBUG_LOG("Starting PM measurement")
            dustData.startMeasure();
            controllerData.sps30Status = SPS30Status::Measuring;
            controllerData.lastPMMeasureTime = currentTime;
            holdStepUpConversion();
        }
    }

    bool wakeUpForMeasurement = false;
    if (controllerData.sps30Status == SPS30Status::Measuring && currentTime - controllerData.lastPMMeasureTime >= 30)
    {
        DEBUG_LOG("Attempting to obtain PMx data")
        auto &innerData = dustMoinitorViewData.innerData;
        if (dustData.getMeasureData(innerData.pm01, innerData.pm2p5, innerData.pm10))
        {
            DEBUG_LOG("PM1 = " << innerData.pm01)
            DEBUG_LOG("PM2.5 = " << innerData.pm2p5)
            DEBUG_LOG("PM10 = " << innerData.pm10)
        }
        dustData.hibernate();
        DEBUG_LOG("Sending SPS30 to sleep")
        controllerData.sps30Status = SPS30Status::Sleep;
        switchStepUpConversion(false);
        wakeUpForMeasurement = true;
        needsToUpdateClock = time(nullptr) % 60 == 0;
    }

    currentTime = time(nullptr);
    if (currentTime % 60 == 0)
    {
        if (needsToUpdateClock)
        {
            DEBUG_LOG("Updating view")
            view.updateView();
            needsToUpdateClock = false;
        }
    }
    else if (currentTime % 60 == 59)
    {
        needsToUpdateClock = true;
    }
    if (!needsToUpdateClock && (wakeUpForMeasurement && !needsToUpdateClock))
    {
        return ProcessStatus::Completed;
    }
    if (!circleCompleted)
    {
        circleCompleted = transport.isCompleted();
    }
    if (!circleCompleted)
    {
        return ProcessStatus::AwaitingForSync;
    }
    if (needsToUpdateClock)
    {
        return ProcessStatus::NeedRefreshClock;
    }

    return ProcessStatus::Completed;
}

bool DustMonitorController::canHybernate() const
{
    return circleCompleted || !needsToUpdateClock;
}

DustMonitorController::DustMonitorController(embedded::PersistentStorage &storage, embedded::PacketUart &uart,
                                             embedded::I2CHelper &i2CHelper, embedded::EpdInterface &epdInterface)
        : meteoData(i2CHelper, storage)
        , dustData(uart)
        , transport(wifiManager)
        , storage(storage)
        , view(storage, epdInterface, dustMoinitorViewData) {}

void DustMonitorController::hibernate()
{
    meteoData.hibernate();
    view.hibernate();
    storage.set(viewDataTag, dustMoinitorViewData);
    storage.set(controllerDataTag, controllerData);
    DEBUG_LOG("Controller is ready to hibernate")
}

bool DustMonitorController::isTimeSyncronized() const
{
    return (time(nullptr) > 1692025000);
}
