#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

class GroupBitView
{
public:
    GroupBitView() = default;
    GroupBitView(void* group, int bit) : group(group), bit(bit) {}
    void* getGroup() const { return group; }
    bool wait(int timeoutMs) const
    {
        return (xEventGroupWaitBits(getGroup(), bit, pdFALSE, pdFALSE, timeoutMs / portTICK_PERIOD_MS) & bit) != 0;
    }
    bool waitClear(int timeoutMs) const
    {
        return (xEventGroupWaitBits(getGroup(), bit, pdTRUE, pdFALSE, timeoutMs / portTICK_PERIOD_MS) & bit) != 0;
    }
    bool wait() const
    {
        return wait(portMAX_DELAY);
    }
    bool waitClear() const
    {
        return waitClear(portMAX_DELAY);
    }
    void set() const
{
        xEventGroupSetBits(getGroup(), bit);
    }

    void clear() const
    {
        xEventGroupClearBits(getGroup(), bit);
    }
private:
    void* group = nullptr;
    EventBits_t bit = 0;
};
