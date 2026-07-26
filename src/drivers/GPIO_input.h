#pragma once
#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"

typedef struct {
    uint32_t gpio;
    uint32_t events;
    uint32_t ts_us;
} gpio_raw_event_t;

class GPIO_Input {
public:
    struct Pins {
        uint32_t button_1;
        uint32_t button_2;
        uint32_t button_3;
        uint32_t enc_a;
        uint32_t enc_b;
        uint32_t enc_btn;

    };

    GPIO_Input(const Pins& pins, QueueHandle_t isrQ);

    void EnableIRQs();

private:
    Pins m_pins{};
    static QueueHandle_t s_gpioIsrQ;

    static void isr_callback(uint gpio, uint32_t events);
    void init_input_pullup(uint32_t gpio);
};