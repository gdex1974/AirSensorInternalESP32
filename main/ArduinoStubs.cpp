#include "DustMonitorController.h"
#include "PersistentStorage.h"
#include "TimeFunctions.h"
#include "AppConfig.h"

#include "esp32-arduino/I2CBus.h"
#include "esp32-arduino/PacketUartImpl.h"
#include <WiFi.h>
#include <Wire.h>
#include <SPI.h>
#include <esp_sleep.h>
#include <soc/rtc.h>
#include <esp32-hal-gpio.h>

#include "SpiDevice.h"
#include "esp32-arduino/GpioPinDefinition.h"
#include "eInk/EpdInterface.h"
#include "esp32-arduino/SpiBus.h"

#include "Debug.h"

namespace
{
constexpr uint32_t rtcCalubrationFactor = (1 << 19);

constexpr uint32_t wakeupDelay = 1000000;

RTC_DATA_ATTR std::array<uint8_t, 2048> persistentArray;
std::optional<embedded::PersistentStorage> persistentStorage;

struct MainData
{
    uint32_t wakeupCounter = 0;
    uint32_t rtcCalibrationResult = 0;
    timeval rtcTimeBeforeDeepSleep { .tv_sec = 0, .tv_usec=0 };
    uint64_t rtcSlowTicksBeforeDeepSleep = 0;
};

// for STM32 platform or pure esp-idf UartDevice is a nontrivial class,
// but for Arduino-based platforms Hardware Serial provides enough functionality.
// We can't just make name aliasing to allow forward declaration, so
// we have to use trivial inheritance.
static_assert(sizeof(embedded::PacketUart::UartDevice) == sizeof(HardwareSerial));
static_assert(std::is_base_of_v<HardwareSerial, embedded::PacketUart::UartDevice>);
class ControllersHolder
{
public:
    ControllersHolder(embedded::PersistentStorage &storage, TwoWire& wire, SPIClass &spiClass,
                      HardwareSerial& serial, int8_t address)
            : i2CBus(wire)
              , i2CDevice(i2CBus, address)
              , uart2(static_cast<embedded::PacketUart::UartDevice&>(serial))
              , spiBus(spiClass)
              , spiDevice(spiBus)
              , epdHAL(spiDevice, rstPin, dcPin, csPin, busyPin)
              , controller(storage, uart2, i2CDevice, epdHAL)
    {
    }
    DustMonitorController& getController() { return controller; }
private:
    embedded::I2CBus i2CBus;
    embedded::I2CHelper i2CDevice;
    embedded::PacketUart uart2;
    embedded::SpiBus spiBus;
    embedded::SpiDevice spiDevice;
    embedded::GpioPinDefinition rstPin{AppConfig::epdResetPin};
    embedded::GpioPinDefinition dcPin{AppConfig::epdDcPin};
    embedded::GpioPinDefinition csPin{AppConfig::epdCsPin};
    embedded::GpioPinDefinition busyPin{AppConfig::epdBusyPin};
    embedded::EpdInterface epdHAL;
    DustMonitorController controller;
};

std::optional<ControllersHolder> controllersHolder;

uint32_t calculateHibernationDelay()
{
    const int64_t microSeconds = microsecondsNow();
    const int64_t wholeMinutePast = (microSeconds / microsecondsInMinute) * microsecondsInMinute;
    const auto timeTillNextMinute = wholeMinutePast + microsecondsInMinute - microSeconds;
    DEBUG_LOG("Time till next minute:" << timeTillNextMinute)
    const auto minimumDelay = microsecondsInSecond;
    uint32_t delayTime = 0;
    if (timeTillNextMinute <= minimumDelay)
    {
        DEBUG_LOG("Too short delay " << timeTillNextMinute << " (" << minimumDelay << "), skipping next wakeup")
        delayTime = microsecondsInMinute + timeTillNextMinute - wakeupDelay;
    }
    else
    {
        delayTime = timeTillNextMinute - wakeupDelay;
    }
    return delayTime;
}

uint32_t calibrateRtcOscillator()
{
    const uint32_t cal_count = 1000;
    uint32_t cali_val;

    for (int i = 0; i < 5; ++i)
    {
        cali_val = rtc_clk_cal(RTC_CAL_32K_XTAL, cal_count);
        DEBUG_LOG("Calibration " << i << ": " << embedded::BufferedOut::precision { 3 } << rtcCalubrationFactor * 1000.0f / (float)cali_val
                                << " kHz");
    }
    return cali_val;
}

void adjustClock(MainData &mainData)
{
    if (mainData.rtcCalibrationResult != 0)
    {
        auto newCircles = rtc_time_get();
        uint64_t diffTicks = newCircles - mainData.rtcSlowTicksBeforeDeepSleep;
        auto diffMicroseconds = diffTicks * mainData.rtcCalibrationResult / rtcCalubrationFactor;
        auto timeMicroseconds =
                mainData.rtcTimeBeforeDeepSleep.tv_sec * 1000000ull +
                mainData.rtcTimeBeforeDeepSleep.tv_usec;
        timeMicroseconds += diffMicroseconds;
        const uint32_t seconds = timeMicroseconds / 1000000;
        const uint32_t microseconds = timeMicroseconds - seconds * 1000000;
        timeval tv { .tv_sec = static_cast<time_t>(seconds), .tv_usec= static_cast<suseconds_t>(microseconds) };
        settimeofday(&tv, nullptr);
        DEBUG_LOG("Calculated sleep time : " << diffMicroseconds);
    }
    else
    {
        esp_sync_counters_rtc_and_frc();
    }
}

} // namespace

void setup()
{
    persistentStorage.emplace(persistentArray,esp_reset_reason() != ESP_RST_DEEPSLEEP);

    auto mainData = persistentStorage->get<MainData>("main");
    if (!mainData)
    {
        mainData.emplace();
    }
    if (auto tzData = persistentStorage->get<std::array<char, 33>>("TZ"))
    {
        setenv("TZ", tzData->begin(), 0);
        tzset();
    }
    const bool initialSetup = mainData->wakeupCounter++ == 0;
    Wire.begin(SDA, SCL, 400000);
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, AppConfig::serial2RxPin, AppConfig::serial2TxPin);
    SPI.begin();
    embedded::GpioPinDefinition ledPinDefinition { AppConfig::ledPin };
    embedded::GpioDigitalPin ledPin(ledPinDefinition);
    ledPin.init();
    ledPin.reset();
    controllersHolder.emplace(*persistentStorage, Wire, SPI, Serial2, AppConfig::bme280Address);
    if (initialSetup)
    {
        DEBUG_LOG(WiFi.macAddress().c_str());
        DEBUG_LOG("Initial setup")
        mainData->rtcCalibrationResult = calibrateRtcOscillator();
    }
    else
    {
        adjustClock(*mainData);
    }
    if (controllersHolder->getController().setup(!initialSetup))
    {
        DEBUG_LOG("Setup completed");
    }
    else
    {
        DEBUG_LOG("Setup failed");
    }
    persistentStorage->set("main", *mainData);
}

void loop()
{
    auto& controller = controllersHolder->getController();
    auto mainData = persistentStorage->get<MainData>("main");
    switch (controller.process())
    {
        case DustMonitorController::ProcessStatus::AwaitingForSync:
            delay(1);
            break;
        case DustMonitorController::ProcessStatus::NeedRefreshClock:
            delay(100);
            break;
        case DustMonitorController::ProcessStatus::Completed:
            if (controller.canHybernate())
            {
                if (auto tzString = std::getenv("TZ"))
                {
                    std::array<char, 33> tzArray {};
                    std::strncpy(tzArray.begin(), tzString, tzArray.size() - 1);
                    persistentStorage->set("TZ", tzArray);
                }
                controller.hibernate();
                uint32_t delayTime = calculateHibernationDelay();
                DEBUG_LOG("Next wakeup in " << delayTime / 1000 << " ms")
                gettimeofday(&mainData->rtcTimeBeforeDeepSleep, nullptr);
                mainData->rtcSlowTicksBeforeDeepSleep = rtc_time_get();
                persistentStorage->set("main", *mainData);
                esp_deep_sleep(delayTime);
            }
            else
            {
                delay(calculateHibernationDelay());
            }
            break;
    }
}
