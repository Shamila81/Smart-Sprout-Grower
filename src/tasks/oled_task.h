#pragma once
#include "pico/stdlib.h"
#include "queue.h"
#include "FreeRTOS.h"
#include "general/data.h"
#include "event_groups.h"
#include "i2c/PicoI2C.h"
#include "memory"

struct OledTaskParams {
    QueueHandle_t ControllerToOledQ;
    EventGroupHandle_t SystemBits;
    std::shared_ptr<PicoI2C> bus;
};

void OledTask(void *params);