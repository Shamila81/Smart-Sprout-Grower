#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "i2c/PicoI2C.h"
#include "drivers/bme68x.h"
#include "queue.h"
#include "FreeRTOS.h"
#include <memory>
#include "task.h"
#include "bme_task.h"
#include "general/data.h"


BME68X_INTF_RET_TYPE bme_write_fn_ptr(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr){
    struct BmeTaskParams *bme= (struct BmeTaskParams*) intf_ptr;
    std::shared_ptr<PicoI2C> bus = bme->bus;

    uint8_t buffer[33];
    if (length > 32) return 1;
    buffer[0] = reg_addr;
    for (uint32_t i = 0; i < length; i++)
    {
        buffer[1 + i] = reg_data[i];
    }
    uint32_t  count  = bus->write(bme->addr, buffer, 1 + length);
    if (count == (1 + length))
    {
        return 0;
    }
    else
    {
        return 1;
    }
}


BME68X_INTF_RET_TYPE bme_read_fn_ptr(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr){
    struct BmeTaskParams *bme= (struct BmeTaskParams*) intf_ptr;
    std::shared_ptr<PicoI2C> bus = bme->bus;

    uint32_t count  = bme->bus->transaction(bme->addr, &reg_addr, 1, reg_data,length );
    if (count == (1 + length))
    {
        return 0;
    }
    else
    {
        return 1;
    }
}


void bme_delay_us(uint32_t period, void *intf_ptr){
    struct BmeTaskParams *bme= (struct BmeTaskParams*) intf_ptr;
    sleep_us(period);

}


void BmeTask(void *params)
{
    auto *p = (BmeTaskParams*)params;
    auto i2cbus = p->bus;
    QueueHandle_t ControllerEventQ = p->ControllerEventQ;

    static BmeTaskParams bme;
    bme.bus  = i2cbus;
    bme.addr = p->addr;

    // Bosch device struct
    static struct bme68x_dev dev;
    dev.intf     = BME68X_I2C_INTF;
    dev.intf_ptr = &bme;
    dev.amb_temp = 25;
    dev.read     = bme_read_fn_ptr;
    dev.write    = bme_write_fn_ptr;
    dev.delay_us = bme_delay_us;

    int8_t rslt = bme68x_init(&dev);
    //printf("[BME] bme68x_init = %d\n", rslt);

    struct bme68x_conf conf;
    bme68x_get_conf(&conf, &dev);

    conf.os_temp = BME68X_OS_2X;
    conf.os_hum  = BME68X_OS_1X;

    bme68x_set_conf(&conf, &dev);

    //int8_t bme68x_set_op_mode(const uint8_t op_mode, struct bme68x_dev *dev)

    //int8_t result = bme68x_set_op_mode(BME68X_FORCED_MODE , &dev);

    while(1) {
        int8_t r1  = bme68x_set_op_mode(BME68X_FORCED_MODE , &dev);
        vTaskDelay(pdMS_TO_TICKS(200));

        struct bme68x_data sensor_data;
        uint8_t n_data = 0;
        uint8_t  r2 = bme68x_get_data(BME68X_FORCED_MODE , &sensor_data, &n_data, &dev);
        vTaskDelay(pdMS_TO_TICKS(200));

       // printf("[BME] set=%d get=%d n=%d\n", r1, r2, n_data);
        if(n_data >0){
            //printf("T=%.1f C\n", sensor_data.temperature);
            //printf("RH=%.1f %%\n", sensor_data.humidity);

            controller_event_t event;
            event.type = EVT_BME_UPDATE;
            event.data.bme.temp_c = sensor_data.temperature;
            event.data.bme.rh = sensor_data.humidity;

            xQueueSend(ControllerEventQ, &event, portMAX_DELAY);

        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}