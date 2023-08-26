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
    EspNowTransport::DataMessage measurementDataMessage;

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

    void onDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len)
    {
        correctionData.correctionMessage.receiveMicroseconds = microsecondsNow();
        if (len != sizeof(EspNowTransport::DataMessage))
        {
            DEBUG_LOG("Received " << len << " bytes, expected " << (int)sizeof(EspNowTransport::DataMessage))
            return;
        }
        DEBUG_LOG("Received " << len << " bytes")
        memcpy(remoteMac.begin(), mac, remoteMac.size());
        memcpy(&measurementDataMessage, incomingData, sizeof(measurementDataMessage));
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

std::optional<EspNowTransport::DataMessage> EspNowTransport::getLastMessage() const
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
