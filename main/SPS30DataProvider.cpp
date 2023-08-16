#include "SPS30DataProvider.h"
#include "Debug.h"

#include <esp_attr.h>

namespace
{
    RTC_DATA_ATTR uint32_t measurementsCounter = 0;
}

using embedded::Sps30Error;

bool SPS30DataProvider::setup(bool wakeUp)
{
    if (wakeUp)
    {
        return true;
    }
    auto spsInitResult = sps30.probe();
    if (spsInitResult != Sps30Error::Success)
    {
        DEBUG_LOG("Probe is failed with code: " << (int)spsInitResult)
        return false;
    }

    DEBUG_LOG("SPS30 sensor found")
    if (embedded::Sps30SerialNumber seriaNumber; (spsInitResult = sps30.getSerial(seriaNumber)) != Sps30Error::Success)
    {
        DEBUG_LOG("Serial number reading failed with code: " << (int) spsInitResult)
        return false;
    }
    else
    {
        DEBUG_LOG("Serial number: " << seriaNumber.serial)
    }
    sps30.resetSensor();
    auto versionResult = sps30.getVersion();
    if (std::holds_alternative<embedded::Sps30VersionInformation>(versionResult))
    {
        const auto& sps30Version = std::get<embedded::Sps30VersionInformation>(versionResult);
        DEBUG_LOG("Firmware revision: " << (int) sps30Version.firmware_major << "." << (int) sps30Version.firmware_minor)
        if (sps30Version.shdlc)
        {
            DEBUG_LOG("Hardware revision: " << (int)sps30Version.shdlc->hardware_revision)
            DEBUG_LOG("SHDLC revision: " << (int)sps30Version.shdlc->shdlc_major << "."
                                         << (int)sps30Version.shdlc->shdlc_minor)
        }
    }
    else
    {
        spsInitResult = std::get<Sps30Error>(versionResult);
        DEBUG_LOG("Firmware revision reading failed with code: " << (int) spsInitResult)
    }
    auto cleaningInterval = sps30.getFanAutoCleaningInterval();
    if (std::holds_alternative<uint32_t>(cleaningInterval))
    {
        DEBUG_LOG("Cleaning interval: " << (int) std::get<uint32_t>(cleaningInterval))
    }
    else
    {
        DEBUG_LOG("Fan cleaning interval reading error: " << (int) std::get<Sps30Error>(cleaningInterval))
    }

    return spsInitResult == Sps30Error::Success;
}

bool SPS30DataProvider::startMeasure()
{
    if (auto result = sps30.startMeasurement(true); result == Sps30Error::Success)
    {
        if (measurementsCounter++ % 168 == 0)
        {
            if (auto cleaningResult = sps30.startManualFanCleaning(); cleaningResult != Sps30Error::Success)
            {
                DEBUG_LOG("Cleaning start failed with the code " << (int)cleaningResult)
            }
        }
        return true;
    }
    return false;
}

bool SPS30DataProvider::wakeUp()
{
    return sps30.wakeUp() == Sps30Error::Success;
}

bool SPS30DataProvider::hibernate()
{
    sps30.stopMeasurement();
    return sps30.sleep() == Sps30Error::Success;
}

bool SPS30DataProvider::getMeasureData(int &pm1, int &pm25, int &pm10)
{
    const auto result = sps30.readMeasurement();
    if (std::holds_alternative<embedded::Sps30MeasurementData>(result))
    {
        const auto& measurementData = std::get<embedded::Sps30MeasurementData>(result);
        if (measurementData.measureInFloat)
        {
            pm1 = (int)measurementData.floatData.mc_1p0;
            pm25 = (int)measurementData.floatData.mc_2p5;
            pm10 = (int)measurementData.floatData.mc_10p0;
            return true;
        }
        else
        {
            pm1 = measurementData.unsignedData.mc_1p0;
            pm25 = measurementData.unsignedData.mc_2p5;
            pm10 = measurementData.unsignedData.mc_10p0;
        }
        return true;
    }
    return false;
}
