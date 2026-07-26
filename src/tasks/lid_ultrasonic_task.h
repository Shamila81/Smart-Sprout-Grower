#pragma once
#include "FreeRTOS.h"
#include "queue.h"
#include "event_groups.h"

struct LidUltrasonicTaskParams {
    EventGroupHandle_t SystemBits;
    uint32_t sig_pin;
};

void LidUltrasonicTask(void *params);