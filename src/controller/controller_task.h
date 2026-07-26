#pragma once
#include "queue.h"
#include "general/data.h"
#include "FreeRTOS.h"
#include "event_groups.h"

struct ControllerTaskParams {
    EventGroupHandle_t SystemBits;
    QueueHandle_t ControllerEventQ;
    QueueHandle_t ControllerToOledQ;
    QueueHandle_t ControllerToStepperQ;
    QueueHandle_t ControllerToPumpQ;
    QueueHandle_t ControllerToEepromQ;
    QueueHandle_t ControllerToMqttQ;
};

void ControllerTask(void *params);