#include "DustMonitorController.h"
#include "AppConfig.h"

#include "TimeFunctions.h"
#include "PersistentStorage.h"

#include "AnalogPin.h"
#include "esp32-arduino/GpioPinDefinition.h"

#include <driver/rtc_io.h>
#include <esp32-hal-gpio.h>

#include "Debug.h"

namespace
{
const std::string_view viewDataTag = "DMC1";
const std::string_view controllerDataTag = "DMC2";

int readVoltageRaw(uint8_t pin)
{
    embedded::GpioPinDefinition voltagePin { pin };
    const auto voltage = embedded::AnalogPin(voltagePin).singleRead();
    DEBUG_LOG("VoltageRaw is " << voltage);
    return voltage;
}
}

bool DustMonitorController::setup(bool wakeUp)
{
    const auto stepupPin = (gpio_num_t)AppConfig::stepUpPin;
    DEBUG_LOG((wakeUp ? "Waking up the controller" : "Initial setup of the controller"));
    rtc_gpio_init(stepupPin); //initialize the RTC GPIO port
    rtc_gpio_set_direction(stepupPin, RTC_GPIO_MODE_OUTPUT_ONLY); //set the port to output only mode
    rtc_gpio_hold_dis(stepupPin); //disable hold before setting the level
    if (wakeUp)
    {
        if (auto data = storage.get<DustMonitorViewData>(viewDataTag))
        {
            DEBUG_LOG("Restoring dust monitor view data");
            dustMoinitorViewData = *data;
            DEBUG_LOG("External sensor's data is " << (dustMoinitorViewData.outerData ? "available":"absent"))
        }
        if (auto data = storage.get<ControllerData>(controllerDataTag))
        {
            controllerData = *data;
            DEBUG_LOG("Last external message recieved: " << controllerData.lastExternalDataTime);
        }
    }
    else
    {
        rtc_gpio_set_level(stepupPin, HIGH); //turn on the step-up converter
    }

    const bool meteoSetupResult = meteoData.setup(wakeUp);
    const bool pmSetupResult = dustData.setup(wakeUp);
    const bool timeSyncResult = timeSync.setup(wakeUp);
    const bool transportResult = transport.setup(wakeUp);
    const bool viewResult = view.setup(wakeUp);
    return timeSyncResult && transportResult && viewResult && meteoSetupResult && pmSetupResult;
}

DustMonitorController::ProcessStatus DustMonitorController::process()
{
    timeSync.process();
    if (timeSync.isTimeSyncronized())
    {
        if (timeSync.isConnected())
        {
            timeSync.disconnect();
            bool successful = transport.init();
            DEBUG_LOG("ESP-Now initialization " << (successful ? "completed" : "failed"));
            (void)successful;
        }
    }
    else
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
        DEBUG_LOG("External sensor's data is outdated: " << controllerData.lastExternalDataTime << ", removing indication")
        dustMoinitorViewData.outerData.reset();
    }

    if (currentTime - controllerData.lastPTHMeasureTime > 58) // one second gap
    {
        if (meteoData.activate())
        {
            if (meteoData.doMeasure())
            {
                DEBUG_LOG("PTH measurement done");
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

    if (shallStartMeasurement)
    {
        if (controllerData.sps30Status != SPS30Status::Measuring)
        {
            const auto stepupPin = (gpio_num_t)AppConfig::stepUpPin;
            rtc_gpio_set_level(stepupPin, HIGH);
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
            rtc_gpio_hold_en(stepupPin);
        }
    }

    if (controllerData.sps30Status == SPS30Status::Measuring && currentTime - controllerData.lastPMMeasureTime > 30)
    {
        DEBUG_LOG("Attempting to obtain PMx data")
        auto &innerData = dustMoinitorViewData.innerData;
        if (dustData.getMeasureData(innerData.pm01, innerData.pm2p5, innerData.pm10))
        {
            DEBUG_LOG("PM1 = " << innerData.pm01);
            DEBUG_LOG("PM2.5 = " << innerData.pm2p5);
            DEBUG_LOG("PM10 = " << innerData.pm10);
        }
        dustData.hibernate();
        DEBUG_LOG("Sending SPS30 to sleep")
        controllerData.sps30Status = SPS30Status::Sleep;
        const auto stepupPin = (gpio_num_t)AppConfig::stepUpPin;
        rtc_gpio_hold_dis(stepupPin); //disable hold before setting the level
        rtc_gpio_set_level(stepupPin, LOW);
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
    return circleCompleted;
}

DustMonitorController::DustMonitorController(embedded::PersistentStorage &storage, embedded::PacketUart &uart,
                                             embedded::I2CHelper& i2CHelper, embedded::EpdInterface& epdInterface)
        : meteoData(i2CHelper, storage)
          ,dustData(uart)
          , timeSync(AppConfig::WiFiSSID, AppConfig::WiFiPassword, AppConfig::ntpServer,
                     AppConfig::timeZoneOffset, AppConfig::daylightSavingOffset)
          , storage(storage)
          , view(storage, epdInterface, dustMoinitorViewData) {}

void DustMonitorController::hibernate()
{
    meteoData.hibernate();
    view.hibernate();
    storage.set(viewDataTag, dustMoinitorViewData);
    storage.set(controllerDataTag, controllerData);
    DEBUG_LOG("Controller is ready to hibernate")
};