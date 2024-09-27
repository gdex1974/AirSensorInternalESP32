#include "DustMonitorController.h"
#include "AppConfig.h"

#include "TimeFunctions.h"
#include "PersistentStorage.h"

#include "AnalogPin.h"
#include "esp32-esp-idf/GpioPinDefinition.h"

#include <driver/rtc_io.h>
#include <esp_sntp.h>
#include <freertos/event_groups.h>

#include "Debug.h"
#include "Delays.h"

namespace
{
const std::string_view viewDataTag = "DMC1";
const std::string_view controllerDataTag = "DMC2";
constexpr EventBits_t TIME_SYNC_BIT = BIT0;
constexpr EventBits_t TRANSPORT_COMPLETED_BIT = BIT1;
constexpr EventBits_t VIEW_COMPLETED_BIT = BIT2;
constexpr EventBits_t TIME_TASK_COMPLETED_BIT = BIT3;
constexpr EventBits_t MEASUREMENT_COMPLETED_BIT = BIT4;
constexpr auto secondsInHour = 60*60;

[[nodiscard]]
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

EventGroupHandle_t eventGroup = nullptr;

void time_sync_notification_cb(struct timeval *)
{
    DEBUG_LOG("Notification of a time synchronization event")
    xEventGroupSetBits(eventGroup, TIME_SYNC_BIT);
}

}

void DustMonitorController::timeSyncTask(void* pvParameters)
{
    reinterpret_cast<DustMonitorController*>(pvParameters)->timeSyncTask();
}

[[noreturn]] void DustMonitorController::timeSyncTask()
{
    while (true)
    {
        xEventGroupClearBits(eventGroup, TIME_TASK_COMPLETED_BIT);
        const bool relevantTime = isTimeSyncronized();
        const auto currentTime = time(nullptr);
        bool refreshRequired = relevantTime && (controllerData.lastTimeSyncTime + 12 * secondsInHour < currentTime)
                && getLocalTime(currentTime).tm_hour == 0 && controllerData.sps30Status != SPS30Status::Measuring;

        if (refreshRequired)
        {
            DEBUG_LOG("Time syncronization is required, waiting for transport completion")
            transport.init({ eventGroup, TRANSPORT_COMPLETED_BIT});
            xEventGroupWaitBits(eventGroup, TRANSPORT_COMPLETED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
            transport.hibernate();
            DEBUG_LOG("Transport completed")
        }
        if (!relevantTime || refreshRequired)
        {
            if (const auto state = wifiManager.getState(); state == WiFiManager::State::Stopped ||
                                                           state == WiFiManager::State::NotInitialized)
            {
                if (refreshRequired || (!isTimeSyncronized() && !timeSyncInitialized))
                {
                    DEBUG_LOG("Starting STA")
                    wifiManager.startSTA(AppConfig::WiFiSSID, AppConfig::WiFiPassword);
                }
            }
            else
            {
                DEBUG_LOG("WiFi manager state: " << static_cast<int>(state))
            }
            if (wifiManager.waitForConnection(20000))
            {
                DEBUG_LOG("Connected to AP")
                xEventGroupClearBits(eventGroup, TIME_SYNC_BIT);
                esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
                esp_sntp_setservername(0, AppConfig::ntpServer.data());
                esp_sntp_setservername(1, (char*)nullptr);
                esp_sntp_setservername(2, (char*)nullptr);
                esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
                esp_sntp_init();
                xEventGroupWaitBits(eventGroup, TIME_SYNC_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
                esp_sntp_stop();
                controllerData.lastTimeSyncTime = time(nullptr);
            }
            else
            {
                DEBUG_LOG("Failed to connect to WiFi")
            }
            wifiManager.stopSTA();
        }
        if (!transport.init({ eventGroup, TRANSPORT_COMPLETED_BIT}))
        {
            DEBUG_LOG("Failed to initialize ESP-NOW")
        }
        xEventGroupSetBits(eventGroup, TIME_TASK_COMPLETED_BIT);

        vTaskDelay(24*60*60*1000 / portTICK_PERIOD_MS); // sync every day if there is no deep sleep
    }
}

void DustMonitorController::updateDisplayTask(void* pvParameters)
{
    reinterpret_cast<DustMonitorController*>(pvParameters)->updateDisplayTask();
}

[[noreturn]] void DustMonitorController::updateDisplayTask()
{
    timeval timeVal;
    gettimeofday(&timeVal, nullptr);
    const auto timeSeconds = timeVal.tv_sec;
    if (const auto seconds = timeSeconds % 60; seconds != 0)
    {
        vTaskDelay(((60 - seconds)*1000 - timeVal.tv_usec / 1000) / portTICK_PERIOD_MS + 1);
    }
    auto lastUpdateTime = xTaskGetTickCount();
    while (true)
    {
        view.updateView();
        xEventGroupSetBits(eventGroup, VIEW_COMPLETED_BIT);
        xTaskDelayUntil(&lastUpdateTime, 60*1000 / portTICK_PERIOD_MS);
    }
}

void DustMonitorController::measurementTask(void* pvParameters)
{
    reinterpret_cast<DustMonitorController*>(pvParameters)->measurementTask();
}

[[noreturn]] void DustMonitorController::measurementTask()
{
    while (true)
    {
        auto lastUpdateTime = xTaskGetTickCount();
        const auto currentTime = time(nullptr);
        if (fullCircle && meteoData.activate())
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
        }

        xEventGroupSetBits(eventGroup, MEASUREMENT_COMPLETED_BIT);
        const auto delay = (controllerData.sps30Status == SPS30Status::Measuring ? 30 : 60) * 1000 / portTICK_PERIOD_MS;
        xTaskDelayUntil(&lastUpdateTime, delay);
    }
}


void DustMonitorController::externalDataTask(void* pvParameters)
{
    reinterpret_cast<DustMonitorController*>(pvParameters)->externalDataTask();
}

[[noreturn]] void DustMonitorController::externalDataTask()
{
    while (true)
    {
        if (auto message = transport.getLastMessage(3*60*1000))
        {
            auto currentTime = time(nullptr);
            controllerData.lastExternalDataTime = currentTime;
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
            sensorData.flags = message->flags;
        }
    }
}

bool DustMonitorController::setup(bool wakeUp)
{
    DEBUG_LOG((wakeUp ? "Waking up the controller" : "Initial setup of the controller"))
    initStepUpControl();
    eventGroup = xEventGroupCreate();
    wifiManager.initWiFiSubsystem();
    if (wakeUp)
    {
        if (auto data = storage.get<DustMonitorViewData>(viewDataTag))
        {
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
    if (wakeUp && isMeasuring())
    {
        xTaskCreate(&DustMonitorController::measurementTask, "measurement_task", 2048, this, 5, nullptr);
        return true;
    }
    fullCircle = true;
    const bool meteoSetupResult = meteoData.setup(wakeUp);
    const bool transportResult = transport.setup(wakeUp);
    const bool viewResult = view.setup(wakeUp);
    if (transportResult && viewResult && meteoSetupResult && pmSetupResult)
    {
        xTaskCreate(&DustMonitorController::timeSyncTask, "time_sync_task", 2048, this, 5, nullptr);
        if (!isTimeSyncronized())
        {
            xEventGroupWaitBits(eventGroup, TIME_SYNC_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
        }
        xTaskCreate(&DustMonitorController::measurementTask, "measurement_task", 2048, this, 5, nullptr);
        xTaskCreate(&DustMonitorController::externalDataTask, "external_data_task", 2048, this, 5, nullptr);
        xTaskCreate(&DustMonitorController::updateDisplayTask, "update_display_task", 2048, this, 5, nullptr);
        return true;
    }
    return false;
}

DustMonitorController::ProcessStatus DustMonitorController::process() const
{
    if (!fullCircle)
    {
        xEventGroupWaitBits(eventGroup, MEASUREMENT_COMPLETED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    }
    else
    {
        constexpr auto allBits =
                MEASUREMENT_COMPLETED_BIT | VIEW_COMPLETED_BIT | TIME_TASK_COMPLETED_BIT | TRANSPORT_COMPLETED_BIT;
        while ((xEventGroupWaitBits(eventGroup, allBits, pdTRUE, pdTRUE, portMAX_DELAY) & allBits) != allBits);
    }
    return ProcessStatus::Completed;
}

DustMonitorController::DustMonitorController(embedded::PersistentStorage &storage, embedded::PacketUart &uart,
                                             embedded::I2CHelper &i2CHelper, embedded::EpdInterface &epdInterface)
        : meteoData(i2CHelper, storage)
        , dustData(uart)
        , transport(storage, wifiManager)
        , storage(storage)
        , view(storage, epdInterface, dustMoinitorViewData) {}

void DustMonitorController::hibernate()
{
    if (fullCircle)
    {
        view.hibernate();
        transport.hibernate();
    }
    storage.set(viewDataTag, dustMoinitorViewData);
    storage.set(controllerDataTag, controllerData);
    DEBUG_LOG("Controller is ready to hibernate")
}

bool DustMonitorController::isTimeSyncronized()
{
    return (time(nullptr) > 1692025000);
}
