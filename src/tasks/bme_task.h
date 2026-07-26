#pragma once
#include <memory>
#include <stdint.h>
#include "i2c/PicoI2C.h"
#include "drivers/bme68x.h"
#include "queue.h"

struct BmeTaskParams {
    QueueHandle_t ControllerEventQ;
    std::shared_ptr<PicoI2C> bus;
    uint8_t addr;

};

BME68X_INTF_RET_TYPE bme_write_fn_ptr(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr);
BME68X_INTF_RET_TYPE bme_read_fn_ptr(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr);
void bme_delay_us(uint32_t period, void *intf_ptr);
void BmeTask(void *params);