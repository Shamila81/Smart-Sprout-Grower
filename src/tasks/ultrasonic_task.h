#pragma once

#include "queue.h"
#include "FreeRTOS.h"
#include "general/data.h"
#include "event_groups.h"

struct UltrasonicTaskParams {
    QueueHandle_t ControllerEventQ;
    EventGroupHandle_t SystemBits;
    uint32_t sig_pin;
};

void UltrasonicTask(void *params);