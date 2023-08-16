#include <WiFi.h>
#include "WiFiTimeSync.h"

bool WiFiTimeSync::setup(bool wakeup)
{
    WiFiClass::mode(WIFI_STA);
    if (!wakeup)
    {
        restartTimeSync();
    }
    return true;
}

void WiFiTimeSync::process()
{
    if (!connecting)
        return;
    if (isConnected())
    {
        configTime(gmtOffset, daylightOffset, ntpServer.data(), nullptr, nullptr);
        connecting = false;
    }
}

bool WiFiTimeSync::isTimeSyncronized() const
{
    return time(nullptr) > 1692025000;
}

bool WiFiTimeSync::isConnected() const
{
    return WiFi.isConnected();
}

void WiFiTimeSync::restartTimeSync()
{
    WiFi.begin(ssid.data(), password.data());
    connecting = true;
}

void WiFiTimeSync::disconnect()
{
    if (isConnected())
    {
        WiFi.disconnect(false, true);
        WiFiClass::mode(WIFI_MODE_NULL);
        WiFiClass::mode(WIFI_STA);
    }
    connecting = false;
}
