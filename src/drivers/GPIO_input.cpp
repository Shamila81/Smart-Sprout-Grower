#include "gpio_input.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

QueueHandle_t GPIO_Input::s_gpioIsrQ = nullptr;

void GPIO_Input::isr_callback(uint gpio, uint32_t events)
{
    if (!s_gpioIsrQ) return;

    BaseType_t hpw = pdFALSE;

    gpio_raw_event_t ev;
    ev.gpio   = gpio;
    ev.events = events;
    ev.ts_us  = (uint32_t)to_us_since_boot(get_absolute_time());

    xQueueSendFromISR(s_gpioIsrQ, &ev, &hpw);
    portYIELD_FROM_ISR(hpw);
}

void GPIO_Input::init_input_pullup(uint32_t gpio)
{
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_IN);
    gpio_pull_up(gpio);
}

GPIO_Input::GPIO_Input(const Pins& pins, QueueHandle_t isrQ)
        : m_pins(pins)
{
    s_gpioIsrQ = isrQ;

    init_input_pullup(m_pins.button_1);
    init_input_pullup(m_pins.button_2);
    init_input_pullup(m_pins.button_3);

    init_input_pullup(m_pins.enc_a);
    init_input_pullup(m_pins.enc_b);
    init_input_pullup(m_pins.enc_btn);
}

void GPIO_Input::EnableIRQs()
{
    gpio_set_irq_enabled_with_callback(
            m_pins.button_1,
            GPIO_IRQ_EDGE_FALL,
            true,
            &GPIO_Input::isr_callback
    );

    gpio_set_irq_enabled(m_pins.button_2, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(m_pins.button_3, GPIO_IRQ_EDGE_FALL, true);

    gpio_set_irq_enabled(m_pins.enc_a, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(m_pins.enc_btn, GPIO_IRQ_EDGE_FALL, true);
}