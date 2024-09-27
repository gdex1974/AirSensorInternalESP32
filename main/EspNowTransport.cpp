#include "EspNowTransport.h"
#include "WiFiManager.h"

#include <array>
#include <cstring>
#include <esp_now.h>
#include <memory>

#include "Debug.h"
#include "TimeFunctions.h"
#include "PersistentStorage.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <variant>

namespace
{

struct TransportData
{
    std::array<uint8_t, 6> remoteMac;
};

struct CorrectionMessage
{
    int64_t currentMicroseconds;
    int64_t receiveMicroseconds;
};

union
{
    CorrectionMessage correctionMessage;
    std::array<uint8_t, sizeof(correctionMessage)> bytes;
} correctionData;

enum class EventType {SendCallback, ReceiveCallback, Exit};

struct EventData
{
    EventType type = EventType::Exit;
    std::array<uint8_t, 6> macAddr = {};
    std::variant<int64_t, esp_now_send_status_t> data;
};

constexpr std::string_view transportDataTag = "ESPN";
EspNowTransport::DataMessage measurementDataMessage;
auto espnowQueue = std::unique_ptr<std::remove_pointer_t<QueueHandle_t>, decltype(&vQueueDelete)>(nullptr, &vQueueDelete);

using macValue = std::array<uint8_t, 6>;
std::optional<macValue> remoteMac;

bool addPeer(const std::array<uint8_t,6>& macAddr)
{
    esp_now_peer_info_t peerInfo;
    std::memset(&peerInfo, 0, sizeof(peerInfo));
    std::memcpy(peerInfo.peer_addr, macAddr.begin(), macAddr.size());
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;
    return esp_now_add_peer(&peerInfo) == ESP_OK;
}

#if __GNUC__ >= 9
void onDataRecv(const esp_now_recv_info_t * esp_now_info, const uint8_t *incomingData, int len)
{
    const auto mac = esp_now_info->src_addr;
#else
void onDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len)
{
#endif

    if (len != sizeof(EspNowTransport::DataMessage))
    {
        DEBUG_LOG("Received " << len << " bytes, expected " << (int)sizeof(EspNowTransport::DataMessage) << " bytes")
        return;
    }
    correctionData.correctionMessage.receiveMicroseconds = microsecondsNow();

    if (!remoteMac)
    {
        remoteMac.emplace();
        std::copy(mac, mac + remoteMac->size(), remoteMac->begin());
    }
    if (std::memcmp(remoteMac->begin(), mac, remoteMac->size()) == 0)
    {
        memcpy(&measurementDataMessage, incomingData, len);
        EventData evt;
        evt.type = EventType::ReceiveCallback;
        memcpy(evt.macAddr.begin(), mac, evt.macAddr.size());
        evt.data = correctionData.correctionMessage.receiveMicroseconds;
        xQueueSend(espnowQueue.get(), &evt, portMAX_DELAY);
    }
    else
    {
        DEBUG_LOG("Received data from unknown peer")
    }
}

void onDataSend(const uint8_t* macAddr, esp_now_send_status_t status) {
    EventData evt;
    evt.type = EventType::SendCallback;
    memcpy(evt.macAddr.begin(), macAddr, evt.macAddr.size());
    evt.data = status;
    xQueueSend(espnowQueue.get(), &evt, portMAX_DELAY);
}

void espnowTask(void *pvParameter)
{
    auto* transport = reinterpret_cast<EspNowTransport*>(pvParameter);
    transport->threadFunction();
}

const EventBits_t DATA_RECEIVED_BIT = BIT1;
}

void EspNowTransport::threadFunction()
{
    EventData evt;
    while (xQueueReceive(espnowQueue.get(), &evt, portMAX_DELAY) == pdTRUE) {
        switch (evt.type) {
            case EventType::SendCallback:
                if (std::holds_alternative<esp_now_send_status_t>(evt.data))
                {
                    if (const auto status = std::get<esp_now_send_status_t>(evt.data); status != ESP_NOW_SEND_SUCCESS)
                    {
                        DEBUG_LOG("Last Packet delivery to " << embedded::BytesView(evt.macAddr) << " fail")
                        if (attemptsCounter < maxAttempts)
                        {
                            DEBUG_LOG("Retrying to send packet to " << embedded::BytesView(evt.macAddr) << " attempt " << (attemptsCounter + 1))
                            if (!sendResponce())
                            {
                                DEBUG_LOG("Error sending packet to " << embedded::BytesView(evt.macAddr))
                                externalEvent.set();
                            }
                        }
                        else
                        {
                            DEBUG_LOG("Max delivery attempts to " << embedded::BytesView(evt.macAddr) << " reached")
                            externalEvent.set();
                        }
                    }
                    else
                    {
                        DEBUG_LOG("Last packet successfully sent to " << embedded::BytesView(evt.macAddr) << " from " << attemptsCounter << " attempt")
                        externalEvent.set();
                        attemptsCounter = 0;
                    }
                }
                break;
            case EventType::ReceiveCallback:
                if (std::holds_alternative<int64_t>(evt.data))
                {
                    if (!isPeerInfoUpdated)
                    {
                        addPeer(*remoteMac);
                        isPeerInfoUpdated = true;
                    }
                    xEventGroupSetBits(wifiEventGroup.get(), DATA_RECEIVED_BIT);
                    sendResponce();
                }
                break;
            case EventType::Exit:
                return;
        }
    }
}

bool EspNowTransport::setup(bool /*wakeup*/)
{
    if (auto data = storage.get<TransportData>(transportDataTag))
    {
        remoteMac.emplace();
        std::copy(data->remoteMac.begin(), data->remoteMac.end(), remoteMac->begin());
        DEBUG_LOG("Peer mac address loaded: " << data->remoteMac)
    }
    espnowQueue.reset(xQueueCreate(6, sizeof(EventData)));
    wifiEventGroup.reset(xEventGroupCreate());
    xTaskCreate(espnowTask, "espnowTask", 2048, this, 4, nullptr);
    return true;
}

bool EspNowTransport::init(GroupBitView event)
{
    externalEvent = event;
    if (WiFiManager::startWiFi() && esp_now_init() != ESP_OK)
    {
        DEBUG_LOG("Error initializing ESP-NOW")
        return false;
    }

    if (remoteMac)
    {
        if (!addPeer(*remoteMac))
        {
            DEBUG_LOG("Error adding peer")
            return false;
        }
        isPeerInfoUpdated = true;
    }
    return esp_now_register_recv_cb(onDataRecv) == ESP_OK &&
        esp_now_register_send_cb(onDataSend) == ESP_OK;
}

std::optional<EspNowTransport::DataMessage> EspNowTransport::getLastMessage(uint32_t timeoutMilliseconds) const
{
    if (xEventGroupWaitBits(wifiEventGroup.get(), DATA_RECEIVED_BIT, pdTRUE, pdTRUE, timeoutMilliseconds / portTICK_PERIOD_MS) & DATA_RECEIVED_BIT)
    {
        return measurementDataMessage;
    }
    return std::nullopt;
}

bool EspNowTransport::sendResponce()
{
    correctionData.correctionMessage.currentMicroseconds = microsecondsNow();
    ++attemptsCounter;
    return esp_now_send(remoteMac->begin(), correctionData.bytes.begin(), correctionData.bytes.size()) == ESP_OK;
}

void EspNowTransport::hibernate() const
{
    esp_now_deinit();
    WiFiManager::stopWiFi();
    if (remoteMac)
    {
        storage.set(transportDataTag, TransportData{ *remoteMac });
    }
}

EspNowTransport::EspNowTransport(embedded::PersistentStorage& storage, WiFiManager &wifiManager)
    : storage(storage)
    , wifiManager(wifiManager)
    , wifiEventGroup { nullptr, vEventGroupDelete}
{
}
