#pragma once
#include "queue.h"
#include "general/data.h"
#include "FreeRTOS.h"
#include "i2c/PicoI2C.h"
#include "drivers/EEPROM.h"

struct EepromTaskParams {
    QueueHandle_t ControllerEventQ;
    QueueHandle_t ControllerToEepromQ;
    std::shared_ptr<PicoI2C> bus;
};

void EepromTask(void *params);