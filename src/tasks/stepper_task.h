#pragma once

#include "queue.h"
#include "FreeRTOS.h"
#include "general/data.h"


struct StepperTaskParams {
    QueueHandle_t ControllerToStepperQ;
    EventGroupHandle_t SystemBits;

    //EventGroupHandle_t SystemBits;
    uint32_t in1;
    uint32_t in2;
    uint32_t in3;
    uint32_t in4;
};

void StepperTask(void *params);