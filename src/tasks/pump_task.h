#pragma once

#include "queue.h"
#include "FreeRTOS.h"
#include "general/data.h"
#include "event_groups.h"

struct PumpTaskParams {
    QueueHandle_t ControllerToPumpQ;
    EventGroupHandle_t SystemBits;
    uint32_t pump_pin;
};

void PumpTask(void *params);