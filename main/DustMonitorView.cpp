#include "DustMonitorView.h"

#include "PersistentStorage.h"
#include "BufferedOut.h"
#include "TimeFunctions.h"

#include "graphics/BaseGeometry.h"
#include "graphics/Canvas.h"
#include "graphics/EmbeddedFont.h"
#include "graphics/GFXFontsAdapter.h"
#include "graphics/SimpleFrameBuffer.h"
#include "eInk/Epd3in7Display.h"
#include "eInk/EpdInterface.h"

#include "Debug.h"

using Point = embedded::Point<int>;
using Size = embedded::Size<int>;
using Rect = embedded::Rect<int>;

using embedded::Epd3in7Display;

extern const uint8_t mainFontBitmapBegin[] asm("_binary_FreeSans15pt8bBitmaps_bin_start");
extern const uint8_t mainFontBitmapEnd[]   asm("_binary_FreeSans15pt8bBitmaps_bin_end");
extern const uint8_t mainFontGlyphsBegin[] asm("_binary_FreeSans15pt8bGlyphs_bin_start");
extern const uint8_t mainFontGlyphsEnd[]   asm("_binary_FreeSans15pt8bGlyphs_bin_end");

namespace
{
embedded::fonts::FontDescriptor FreeSans15pt8b {
    embedded::ConstBytesView(mainFontBitmapBegin, mainFontBitmapEnd),
        embedded::MemoryView<const embedded::fonts::GlyphDescriptor>(
        reinterpret_cast<const embedded::fonts::GlyphDescriptor*>(mainFontGlyphsBegin)
        , reinterpret_cast<const embedded::fonts::GlyphDescriptor*>(mainFontGlyphsEnd)),
    0x20, 0xFF, 0
};

embedded::fonts::EmbeddedFont sans15PtFont {FreeSans15pt8b};

std::optional<Epd3in7Display> epd;
SimpleFrameBuffer<Epd3in7Display::epdWidth, Epd3in7Display::epdHeight> image;
enum class Color : uint32_t { Black, White };
Canvas paint(image);

constexpr Point topLeft { 0, 0 };
constexpr Size displaySize { Epd3in7Display::epdWidth, Epd3in7Display::epdHeight};
constexpr Size viewAreaSize { displaySize.width - 2 * topLeft.x, displaySize.height - 2 * topLeft.y};
constexpr uint16_t timeHeight = 50;
constexpr uint16_t fullWidth = displaySize.width - 2 * topLeft.x;

constexpr Rect internalSensorArea = {topLeft, {fullWidth, (uint16_t )(viewAreaSize.height - timeHeight) / 2}};
constexpr Rect externalSensorArea = internalSensorArea + Size {0, internalSensorArea.size.height};
constexpr Rect timeArea = {topLeft + Size {0, viewAreaSize.height - timeHeight},
                           { viewAreaSize.width, timeHeight}};

enum class SensorFlags : uint32_t {
    BatteryFailure = 1 << 0,
};
}

bool DustMonitorView::setup(bool /*wakeUp*/)
{
    epdInterface.initPins();
    epd.emplace(epdInterface);
    if (auto data = storage.get<StoredData>("view"))
    {
        storedData = *data;
    }
    if (storedData.updateType != UpdateType::Partial)
    {
        epd->init();
        DEBUG_LOG("Screen init completed")
    }
    else
    {
        epd->wakeUp();
        DEBUG_LOG("Screen wakeup completed")
    }
    paint.setRotation(Canvas::Rotation::Rotation180);
    paint.clear((uint32_t)Color::White);
    paint.setColor((uint32_t)Color::Black);
    return true;
}

void DustMonitorView::displayText(std::string_view textString, const Rect &rectArea) const
{
    auto textSize = sans15PtFont.getTextBounds(textString);
    auto shift = (rectArea.size - textSize.size) / 2;
    paint.setColor((uint32_t)Color::White);
    paint.drawFilledRectangle(rectArea);
    auto pos = rectArea.topLeft + shift;
    pos.y += textSize.topLeft.y;
    paint.setColor((uint32_t)Color::Black);
    paint.drawStringAt(pos, textString, sans15PtFont);
}

bool DustMonitorView::SyncViewData()
{
    bool needFullRefresh = false;
    if (externalViewData.outerData)
    {
        if (!storedData.currentDisplayData.outerData)
        {
            storedData.currentDisplayData.outerData.emplace();
            DEBUG_LOG("Outer data initialized")
            needFullRefresh = true;
        }
    }
    else
    {
        if (storedData.currentDisplayData.outerData)
        {
            storedData.currentDisplayData.outerData = std::nullopt;
            needFullRefresh = true;
            DEBUG_LOG("Outer data cleared")
        }
    }
    return needFullRefresh;
}

void DustMonitorView::updateView()
{
    const bool needFullRefresh = SyncViewData();

    updateSensorArea(internalSensorArea, storedData.currentDisplayData.innerData, externalViewData.innerData);
    if (externalViewData.outerData)
    {
        updateSensorArea(externalSensorArea, *storedData.currentDisplayData.outerData, *externalViewData.outerData);
    }
    else if (needFullRefresh)
    {
        paint.setColor((uint32_t)Color::White);
        paint.drawFilledRectangle(externalSensorArea);
        paint.setColor((uint32_t)Color::Black);
    }

    drawTime();
    refreshScreen(needFullRefresh);

    storage.set("view", storedData);
}

void DustMonitorView::drawTime() const
{
    tm timeInfo = getLocalTime(time(nullptr));
    std::array<char, 20> string {};
    embedded::BufferedOut bufferedOut(string);
    bufferedOut << embedded::BufferedOut::fill{'0'} << embedded::BufferedOut::width {2}
                << timeInfo.tm_hour << ":"<< embedded::BufferedOut::width {2} << timeInfo.tm_min;
    displayText(bufferedOut.asStringView(), timeArea);
}

void DustMonitorView::refreshScreen(const bool needFullRefresh)
{
    DEBUG_LOG("Updating screen...")
    bool partialUpdate = false;
    switch (storedData.updateType)
    {
        case UpdateType::Full:
            storedData.updateType = UpdateType::Partial;
            break;
        case UpdateType::Partial:
            partialUpdate = true;
            if (++storedData.refreshCounter % 10 == 0)
            {
                storedData.updateType = UpdateType::DeepSleep;
            }
            break;
        case UpdateType::DeepSleep:
            storedData.updateType = UpdateType::Partial;
            break;
    }
    const auto startTime = microsecondsNow();
    epd->displayFrame(paint.getImage(), needFullRefresh || !partialUpdate ?
        Epd3in7Display::RefreshMode::FullBW : Epd3in7Display::RefreshMode::PartBW);
    DEBUG_LOG("Update time:" << (microsecondsNow() - startTime) << " us")
    (void)startTime;
}

void DustMonitorView::updateSensorArea(const Rect& dataArea, SensorData& storedValue, const SensorData& newValue) const
{
    const uint16_t rowHeight = dataArea.size.height / 5;
    const uint16_t halfWidth = dataArea.size.width / 2;
    const Size halfSize {halfWidth, rowHeight};
    const Size shiftRight {halfWidth,0};
    const Size shiftBottom {0, rowHeight};
    const Rect tempArea {dataArea.topLeft, halfSize};
    const Rect humidityArea = tempArea + shiftRight;
    const Rect pressureArea = tempArea + shiftBottom;
    const Rect voltageArea  = pressureArea + Size {halfWidth,0};
    const Rect pm01Area {dataArea.topLeft + Size {halfWidth, (uint16_t)(rowHeight * 2)},
                         {halfWidth, rowHeight}};
    const Rect pm25Area {dataArea.topLeft + Size {halfWidth, (uint16_t)(rowHeight * 3)},
                         halfSize};
    const Rect pm10Area { dataArea.topLeft + Size {halfWidth, (uint16_t)(rowHeight * 4)},
                          halfSize};
    std::array<char, 20> string {};
    embedded::BufferedOut bufferedOut(string);

    storedValue.humidity = newValue.humidity;
    bufferedOut << embedded::BufferedOut::precision{0} <<storedValue.humidity << "%";
    displayText(bufferedOut.asStringView(), humidityArea);

    storedValue.temperature = newValue.temperature;
    bufferedOut.clear();
    if (storedValue.temperature > 0)
    {
        bufferedOut << "+";
    }
    bufferedOut << embedded::BufferedOut::precision{1} << storedValue.temperature<< " °C";
    displayText(bufferedOut.asStringView(), tempArea);

    storedValue.pressure = newValue.pressure;
    bufferedOut.clear();
    bufferedOut << embedded::BufferedOut::precision{0} << storedValue.pressure / 100.f << " hPa";
    displayText(bufferedOut.asStringView(), pressureArea);


    storedValue.voltage = newValue.voltage;
    storedValue.flags = newValue.flags;
    DEBUG_LOG("Voltage = " << embedded::BufferedOut::precision{2} << storedValue.voltage)
    bufferedOut.clear();
    bufferedOut << embedded::BufferedOut::precision{2} << storedValue.voltage << "V";
    displayText(bufferedOut.asStringView(), voltageArea);
    if (newValue.flags & (uint64_t)SensorFlags::BatteryFailure)
    {
        paint.drawRectangle(voltageArea);
    }

    Rect pmHeaderArea = pressureArea + shiftBottom;

    displayText("PM1  ", pmHeaderArea);
    pmHeaderArea += shiftBottom;
    displayText("PM2.5", pmHeaderArea);
    pmHeaderArea += shiftBottom;
    displayText("PM10 ", pmHeaderArea);

    storedValue.pm01 = newValue.pm01;
    drawPMData(pm01Area, storedValue.pm01);

    storedValue.pm2p5 = newValue.pm2p5;
    drawPMData(pm25Area, storedValue.pm2p5);

    storedValue.pm10 = newValue.pm10;
    drawPMData(pm10Area, storedValue.pm10);
}

void
DustMonitorView::drawPMData(const Rect& pm01Area, int value) const
{
    if (value < 0)
    {
        displayText("---", pm01Area);
    }
    else
    {
        std::array<char, 20> string {};
        embedded::BufferedOut bufferedOut(string);
        bufferedOut << value;
        displayText(bufferedOut.asStringView(), pm01Area);
    }
}

void DustMonitorView::hibernate() const
{
    if (storedData.updateType == UpdateType::DeepSleep)
    {
        DEBUG_LOG("Sending display to sleep...")
        epd->waitUntilIdle();
        epd->sleep();
    }
}
