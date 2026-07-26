#pragma once
#include "FreeRTOS.h"
#include "queue.h"
#include "event_groups.h"

struct MqttTaskParams {
    QueueHandle_t ControllerEventQ;
    EventGroupHandle_t SystemBits;
    QueueHandle_t ControllerToMqttQ;
};

void MqttTask(void *params);