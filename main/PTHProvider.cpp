#include "PTHProvider.h"

#include "BME280/I2CHelper.h"
#include "BME280/BME280.h"
#include "Wire.h"
#include "esp32-arduino/I2CBus.h"
#include "Delays.h"
#include "Debug.h"

bool PTHProvider::activate() {
    return bme.startMeasurement();
}

bool PTHProvider::hibernate() {
    return bme.stopMeasurement();
}

bool PTHProvider::setup(bool /* wakeUp */)
{
    auto result = bme.init();
    DEBUG_LOG("BME280 is initialized with the result:" << result)
    return result == 0;
}

bool PTHProvider::doMeasure()
{
    while (bme.isMeasuring())
        embedded::delay(1);
    return bme.getMeasureData(temperature, pressure, humidity);
}
