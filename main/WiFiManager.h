#pragma once

#include <cstdint>
#include <string_view>
struct esp_netif_obj;

class WiFiManager
{
public:
    enum class State {
        NotInitialized,
        Stopped,
        Started,
        Disconnected,
        Connecting,
        Connected,
    };
    WiFiManager();
    ~WiFiManager();

    bool initWiFiSubsystem();
    void deinitWiFiSubsystem();
    bool startSTA(std::string_view ssid, std::string_view password);
    bool stopSTA();
    bool startWiFi();
    bool stopWiFi();
    State getState() const volatile { return state; }
private:
    static void eventHandler(void* arg, const char* event_base,
                              int32_t event_id, void* event_data);
    void eventHandler(const char* event_base, int32_t event_id, void* event_data);
    int numberOfRetries = 0;
    volatile State state = State::NotInitialized;
    esp_netif_obj* defaultStaInterface = nullptr;
};
