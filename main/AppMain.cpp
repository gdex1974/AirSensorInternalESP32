#include "DustMonitorController.h"
#include "PersistentStorage.h"
#include "TimeFunctions.h"
#include "AppConfig.h"

#include "SpiDevice.h"
#include "display/EpdInterface.h"

#include "esp32-esp-idf/I2CBus.h"
#include "esp32-esp-idf/PacketUartImpl.h"
#include "esp32-esp-idf/GpioPinDefinition.h"
#include "esp32-esp-idf/SpiBus.h"

#include <esp_attr.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <soc/rtc.h>
#include <nvs_flash.h>

#include "Debug.h"

namespace
{
constexpr uint32_t rtcCalibrationFactor = (1 << 19);

constexpr uint32_t wakeupDelay = 870000;

RTC_DATA_ATTR std::array<uint8_t, 2048> persistentArray;
std::optional<embedded::PersistentStorage> persistentStorage;

struct MainData
{
    uint32_t wakeupCounter = 0;
    uint32_t rtcCalibrationResult = 0;
    timeval rtcTimeBeforeDeepSleep { .tv_sec = 0, .tv_usec=0 };
    uint64_t rtcSlowTicksBeforeDeepSleep = 0;
};

class ControllersHolder
{
public:
    ControllersHolder(embedded::PersistentStorage &storage, int spiBusNum,
                      int serialPortNum, int8_t address)
            : i2CBus(0)
              , i2CDevice(i2CBus, address)
              , uartDevice(serialPortNum)
              , sps30Uart(uartDevice)
              , spiBus(spiBusNum)
              , spiDevice(spiBus)
              , epdHAL(spiDevice, rstPin, dcPin, csPin, busyPin)
              , controller(storage, sps30Uart, i2CDevice, epdHAL)
    {
        i2CBus.init(AppConfig::SDA, AppConfig::SCL, 400000);
        spiBus.init(sckPin, misoPin, mosiPin);
    }
    DustMonitorController& getController() { return controller; }
private:
    embedded::I2CBus i2CBus;
    embedded::I2CHelper i2CDevice;
    embedded::PacketUart::UartDevice uartDevice;
    embedded::PacketUart sps30Uart;
    embedded::SpiBus spiBus;
    embedded::SpiDevice spiDevice;
    embedded::GpioPinDefinition rstPin{AppConfig::epdResetPin};
    embedded::GpioPinDefinition dcPin{AppConfig::epdDcPin};
    embedded::GpioPinDefinition csPin{AppConfig::epdCsPin};
    embedded::GpioPinDefinition busyPin{AppConfig::epdBusyPin};
    embedded::GpioPinDefinition sckPin{AppConfig::epdSckPin};
    embedded::GpioPinDefinition misoPin{AppConfig::epdMisoPin};
    embedded::GpioPinDefinition mosiPin{AppConfig::epdMosiPin};
    embedded::EpdInterface epdHAL;
    DustMonitorController controller;
};

std::optional<ControllersHolder> controllersHolder;

auto getMicrosecondsTillNextMinute()
{
    const int64_t microSeconds = microsecondsNow();
    const int64_t wholeMinutePast = (microSeconds / microsecondsInMinute) * microsecondsInMinute;
    return wholeMinutePast + microsecondsInMinute - microSeconds;
}

uint32_t calculateHibernationDelay()
{
    const auto timeTillNextMinute = getMicrosecondsTillNextMinute();
    DEBUG_LOG("Time till next minute:" << timeTillNextMinute)
    uint32_t delayTime;
    if (timeTillNextMinute <= wakeupDelay)
    {
        DEBUG_LOG("Too short delay " << timeTillNextMinute << " (" << wakeupDelay << "), skipping next wakeup")
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
    const uint32_t calCount = 1000;
    uint32_t caliVal;

    for (int i = 0; i < 5; ++i)
    {
        caliVal = rtc_clk_cal(RTC_CAL_32K_XTAL, calCount);
        DEBUG_LOG("Calibration " << i << ": " << embedded::BufferedOut::precision { 3 } << rtcCalibrationFactor * 1000.0f / (float)caliVal << " kHz")
    }
    return caliVal;
}

void adjustClock(MainData &mainData)
{
    if (mainData.rtcCalibrationResult != 0)
    {
        auto newCircles = rtc_time_get();
        uint64_t diffTicks = newCircles - mainData.rtcSlowTicksBeforeDeepSleep;
        auto diffMicroseconds = diffTicks * mainData.rtcCalibrationResult / rtcCalibrationFactor;
        auto timeMicroseconds =
                mainData.rtcTimeBeforeDeepSleep.tv_sec * 1000000ull +
                mainData.rtcTimeBeforeDeepSleep.tv_usec;
        timeMicroseconds += diffMicroseconds;
        const uint32_t seconds = timeMicroseconds / 1000000;
        const uint32_t microseconds = timeMicroseconds - seconds * 1000000;
        timeval timeVal { .tv_sec = static_cast<time_t>(seconds), .tv_usec= static_cast<suseconds_t>(microseconds) };
        settimeofday(&timeVal, nullptr);
        DEBUG_LOG("Calculated sleep time : " << diffMicroseconds)
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

    setenv("TZ", AppConfig::timeZone.begin(), 1);
    tzset();

    const bool initialSetup = mainData->wakeupCounter++ == 0;
#ifdef DEBUG_SERIAL_OUT
    embedded::PacketUart::UartDevice::init(0, AppConfig::serial1RxPin, AppConfig::serial1TxPin, 115200);
#endif
    embedded::PacketUart::UartDevice::init(2, AppConfig::serial2RxPin, AppConfig::serial2TxPin, 115200);
    embedded::GpioPinDefinition ledPinDefinition { AppConfig::ledPin };
    embedded::GpioDigitalPin ledPin(ledPinDefinition);
    ledPin.init();
    ledPin.reset();
    controllersHolder.emplace(*persistentStorage, 2, 2, AppConfig::bme280Address);
    if (initialSetup)
    {
        DEBUG_LOG("Initial setup")
        mainData->rtcCalibrationResult = calibrateRtcOscillator();
    }
    else
    {
        adjustClock(*mainData);
    }
    if (!controllersHolder->getController().setup(!initialSetup))
    {
        DEBUG_LOG("Setup failed")
    }
    persistentStorage->set("main", *mainData);
}

[[noreturn]] void process()
{
    auto& controller = controllersHolder->getController();
    auto mainData = persistentStorage->get<MainData>("main");
    controller.process();
    controller.hibernate();
    auto delayTime = calculateHibernationDelay();
    if (controller.isMeasuring())
    {
        delayTime = std::min(delayTime, decltype(delayTime)(30 * microsecondsInSecond));
    }
    DEBUG_LOG("Next wakeup in " << delayTime / 1000 << " ms")
    gettimeofday(&mainData->rtcTimeBeforeDeepSleep, nullptr);
    mainData->rtcSlowTicksBeforeDeepSleep = rtc_time_get();
    persistentStorage->set("main", *mainData);
    esp_deep_sleep(delayTime);
}

extern "C" void app_main()
{
    if (const auto ret = nvs_flash_init();
        ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase() );
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    setup();
    process();
}
