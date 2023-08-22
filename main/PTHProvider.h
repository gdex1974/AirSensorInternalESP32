#pragma once

#include "BME280/BME280.h"
#include "BME280/I2CHelper.h"

class PTHProvider {
public:
    PTHProvider(embedded::I2CHelper& i2CHelper, embedded::PersistentStorage &storage)
        : bme(i2CHelper)
        , storage(storage) {};
    bool setup(bool wakeUp);
    bool activate();
    bool hibernate();
    bool doMeasure();

    float getPressure() const
    {
        return static_cast<float>(measurementData.pressure) / 256.f;
    }

    float getTemperature() const
    {
        return static_cast<float>(measurementData.temperature) / 100.f;;
    }

    float getHumidity() const
    {
        return static_cast<float>(measurementData.humidity) / 1024.f;
    }

private:
    embedded::BMPE280::MeasurementData measurementData{};
    bool calibrationDataPresent = false;
    embedded::BMPE280 bme;
    embedded::PersistentStorage &storage;
};
