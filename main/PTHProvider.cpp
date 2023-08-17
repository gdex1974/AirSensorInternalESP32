#include "PTHProvider.h"

#include "BME280/I2CHelper.h"
#include "BME280/BME280.h"
#include "Wire.h"
#include "esp32-arduino/I2CBus.h"
#include "Delays.h"
#include "Debug.h"

namespace
{
constexpr std::string_view calibrationDataName = "PTHD";
}

bool PTHProvider::activate()
{
    return bme.startMeasurement();
}

bool PTHProvider::hibernate()
{
    if (!calibrationDataPresent)
    {
        bme.saveCalibrationData(storage, calibrationDataName);
    }
    return bme.stopMeasurement();
}

bool PTHProvider::setup(bool wakeUp)
{
    if (!wakeUp)
    {
        auto result = bme.init();
        DEBUG_LOG("BME280 is initialized with the result:" << result)
        return result == 0;
    }
    else
    {
        calibrationDataPresent = bme.loadCalibrationData(storage, calibrationDataName);
        if (!calibrationDataPresent)
        {
            DEBUG_LOG("BME280 calibration data load failed, trying to initialize")
            return setup(false);
        }
        DEBUG_LOG("BME280 calibration data loaded")
        return true;
    }
}

bool PTHProvider::doMeasure()
{
    while (bme.isMeasuring())
    {
        embedded::delay(1);
    }
    return bme.getMeasureData(temperature, pressure, humidity);
}
