#pragma once

#include "queue.h"
#include "FreeRTOS.h"
#include "general/data.h"
#include "event_groups.h"

struct GpioTaskParams {
    QueueHandle_t gpioIsrQ;
    QueueHandle_t ControllerEventQ;
    EventGroupHandle_t SystemBits;
};

void GpioTask(void *params);