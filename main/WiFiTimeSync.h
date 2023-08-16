#pragma once

#include <cstdint>
#include <string_view>

class WiFiTimeSync
{
public:
    WiFiTimeSync(std::string_view ssid, std::string_view password, std::string_view ntpServer, int gmtOffset, int daylightOffset)
            : ssid(ssid)
              , password(password)
              , ntpServer(ntpServer)
              , gmtOffset(gmtOffset)
              , daylightOffset(daylightOffset)
              , connecting(false)
              , connected(false) {}

    bool setup(bool wakeup);
    void process();
    bool isTimeSyncronized() const;
    bool isConnected() const;
    void restartTimeSync();
    void disconnect();

private:
    std::string_view ssid;
    std::string_view password;
    std::string_view ntpServer;
    int gmtOffset;
    int daylightOffset;
    bool connecting;
    bool connected;
};
