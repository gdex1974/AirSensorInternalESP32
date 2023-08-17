#pragma once

#include "BME280/BME280.h"
#include "BME280/I2CHelper.h"

class PTHProvider {
public:
    PTHProvider(embedded::I2CHelper& i2CHelper, embedded::PersistentStorage &storage)
        :bme(i2CHelper)
        , storage(storage) {};
    bool setup(bool wakeUp);
    bool activate();
    bool hibernate();
    bool doMeasure();

    float getPressure() const
    {
        return pressure;
    }

    float getTemperature() const
    {
        return temperature;
    }

    float getHumidity() const
    {
        return humidity;
    }

private:
    float pressure{};
    float temperature{};
    float humidity{};
    bool calibrationDataPresent = false;
    embedded::BMPE280 bme;
    embedded::PersistentStorage &storage;
};
