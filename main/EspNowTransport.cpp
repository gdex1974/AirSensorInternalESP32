#include "EspNowTransport.h"
#include "WiFiManager.h"

#include <array>
#include <cstring>
#include <esp_now.h>
#include "Debug.h"
#include "TimeFunctions.h"

namespace
{
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
    EspNowTransport::DataMessageV2 measurementDataMessage;

    using macValue = std::array<uint8_t, 6>;
    macValue remoteMac {0};
    volatile bool updated = false;
    volatile bool completed = false;

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

#if __GCC_VERSION__ >= 90000
    void onDataRecv(const esp_now_recv_info_t * esp_now_info, const uint8_t *incomingData, int len)
#else
    void onDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len)
#endif
    {
        correctionData.correctionMessage.receiveMicroseconds = microsecondsNow();
        switch (len)
        {
            case sizeof(EspNowTransport::DataMessage):
                DEBUG_LOG("Received data message")
                break;
            case sizeof(EspNowTransport::DataMessageV2):
                DEBUG_LOG("Received data message v2")
                break;
            default:
                DEBUG_LOG("Received " << len << " bytes, expected " << (int)sizeof(EspNowTransport::DataMessage) << " or " << (int)sizeof(EspNowTransport::DataMessageV2))
                return;

        }
#if __GCC_VERSION__ >= 90000
        memcpy(remoteMac.begin(), esp_now_info->src_addr, remoteMac.size());
#else
        memcpy(remoteMac.begin(), mac, remoteMac.size());
#endif
        memcpy(&measurementDataMessage, incomingData, len);
        updated = true;
    }

    void onDataSend(const uint8_t */*mac_addr*/, esp_now_send_status_t status) {
        DEBUG_LOG("Responce sending was " << (status == ESP_OK ? "successful" : "failed"))
        completed = true;
    }

}

bool EspNowTransport::setup(bool wakeup)
{
    if (!wakeup)
    {
        return true;
    }

    return init();

}

bool EspNowTransport::init() const
{
    if (wifiManager.startWiFi() && esp_now_init() != ESP_OK)
    {
        DEBUG_LOG("Error initializing ESP-NOW")
        return false;
    }

    return esp_now_register_recv_cb(onDataRecv) == ESP_OK &&
        esp_now_register_send_cb(onDataSend) == ESP_OK;
}

std::optional<EspNowTransport::DataMessageV2> EspNowTransport::getLastMessage() const
{
    if (updated)
    {
        updated = false;
        return measurementDataMessage;
    }
    return std::nullopt;
}

bool EspNowTransport::isCompleted() const
{
    bool result = completed;
    if (result)
        completed = false;
    return result;
}

bool EspNowTransport::sendResponce()
{
    if (!isPeerInfoUpdated)
    {
        addPeer(remoteMac);
        isPeerInfoUpdated = true;
    }
    correctionData.correctionMessage.currentMicroseconds = microsecondsNow();
    return esp_now_send(remoteMac.begin(), correctionData.bytes.begin(), correctionData.bytes.size()) == ESP_OK;
}
