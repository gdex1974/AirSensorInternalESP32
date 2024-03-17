#include "WiFiManager.h"

#include <esp_wifi_types.h>
#include <esp_wifi.h>
#include <cstring>
#include <freertos/event_groups.h>

#include "Debug.h"

namespace
{
constexpr int CONNECTED_BIT = BIT0;
constexpr int DISCONNECTED_BIT = BIT1;
constexpr int STOPPED_BIT = BIT2;
constexpr int CONNECTING_BIT = BIT3;
constexpr int STARTED_BIT = BIT4;
EventGroupHandle_t wifiEventGroup = nullptr;
}

WiFiManager::WiFiManager()
{
    wifiEventGroup = xEventGroupCreate();
}

WiFiManager::~WiFiManager()
{
    deinitWiFiSubsystem();
    vEventGroupDelete(wifiEventGroup);
}

bool WiFiManager::initWiFiSubsystem()
{
    if (state == State::NotInitialized)
    {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        defaultStaInterface = esp_netif_create_default_wifi_sta();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        state = State::Stopped;
    }
    return state == State::Stopped;
}

void WiFiManager::deinitWiFiSubsystem()
{
    if (state != State::NotInitialized)
    {
        esp_wifi_stop();
        esp_wifi_deinit();
        esp_netif_destroy_default_wifi(defaultStaInterface);
        defaultStaInterface = nullptr;
        esp_event_loop_delete_default();
        state = State::Stopped;
    }
}

void WiFiManager::eventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (arg)
    {
        auto manager = reinterpret_cast<WiFiManager*>(arg);
        manager->eventHandler(event_base, event_id, event_data);
    }
}

void WiFiManager::eventHandler(esp_event_base_t event_base, int32_t event_id, void* /*event_data*/)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
            case WIFI_EVENT_STA_START:
                if (state == State::Connecting)
                {
                    esp_wifi_connect();
                    xEventGroupSetBits(wifiEventGroup, CONNECTING_BIT);
                }
                else{
                    state = State::Started;
                    xEventGroupSetBits(wifiEventGroup, STARTED_BIT);
                }
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                if (state == State::Connecting && numberOfRetries++ < 5)
                {
                    DEBUG_LOG("Retrying to connect")
                    esp_wifi_connect();
                    return;
                }
                numberOfRetries = 0;
                state = State::Disconnected;
                xEventGroupSetBits(wifiEventGroup, DISCONNECTED_BIT);
                break;
            case WIFI_EVENT_STA_STOP:
                esp_wifi_disconnect();
                state = State::Stopped;
                xEventGroupSetBits(wifiEventGroup, STOPPED_BIT);
                break;
            default:
                break;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        numberOfRetries = 0;
        state = State::Connected;
        xEventGroupSetBits(wifiEventGroup, CONNECTED_BIT);
    }
}

bool WiFiManager::startSTA(std::string_view ssid, std::string_view password)
{
    if (state == State::Stopped)
    {
        wifi_config_t wifi_config {};
        std::memcpy(wifi_config.sta.ssid, ssid.begin(), std::min(ssid.size(), sizeof(wifi_config.sta.ssid) - 1));
        std::memcpy(wifi_config.sta.password, password.begin(), std::min(password.size(), sizeof(wifi_config.sta.password) - 1));
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
                                                   ESP_EVENT_ANY_ID,
                                                   &WiFiManager::eventHandler, this));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                   &WiFiManager::eventHandler, this));
        state = State::Connecting;
        DEBUG_LOG("Starting WiF STA")
        if (!startWiFi())
        {
            DEBUG_LOG("Failed to start WiFi")
            state = State::Stopped;
            return false;
        }
    }
    return true;
}

bool WiFiManager::startWiFi()
{
    return esp_wifi_start() == ESP_OK;
}

bool WiFiManager::stopSTA()
{
    if (state == State::Connected || state == State::Connecting)
    {
        if (stopWiFi())
        {
            if ((xEventGroupWaitBits(wifiEventGroup, STOPPED_BIT, pdTRUE, pdTRUE, portMAX_DELAY) & STOPPED_BIT) != 0)
            {
                DEBUG_LOG("Disconnected from AP")
                return true;
            }
            else
            {
                DEBUG_LOG("Failed to disconnect from AP")
                return false;
            }
        }
    }
    return state == State::Stopped;
}

bool WiFiManager::stopWiFi()
{
    return esp_wifi_stop() == ESP_OK;
}

bool WiFiManager::waitForConnection(int timeoutMs)
{
    return state == State::Connected || xEventGroupWaitBits(wifiEventGroup, CONNECTED_BIT, pdTRUE, pdTRUE, pdMS_TO_TICKS(timeoutMs)) & CONNECTED_BIT;
}

bool WiFiManager::waitForDisconnect(int timeoutMs)
{
    return state == State::Disconnected || xEventGroupWaitBits(wifiEventGroup, DISCONNECTED_BIT, pdTRUE, pdTRUE, pdMS_TO_TICKS(timeoutMs)) & DISCONNECTED_BIT;
}


