#include "FreeRTOS.h"
#include "queue.h"
#include "pico/stdlib.h"
#include <stdint.h>
#include "drivers/GPIO_input.h"
#include "general/data.h"
#include "gpio_task.h"

#define BTN_DEBOUNCE_US 300000
#define ENC_DEBOUNCE_US 4000

#define BUTTON_3 21
#define BUTTON_1 26
#define BUTTON_2 22
#define ENC_A_PIN 19
#define ENC_B_PIN 18
#define ENC_BTN_PIN 20


static inline void send_button_event(QueueHandle_t q, int id)
{
    controller_event_t ev{};
    ev.data.button.id = id;

    if (id == 1) ev.type = EVT_BTN_1;
    else if (id == 2) ev.type = EVT_BTN_2;
    else if (id == 3) ev.type = EVT_BTN_3;
    else if (id == 4) ev.type = EVT_ENC_BTN;

    xQueueSend(q, &ev, 0);
}

void GpioTask(void *params)
{
    static GPIO_Input::Pins pins = {
            .button_1 = BUTTON_1,
            .button_2 = BUTTON_2,
            .button_3 = BUTTON_3,
            .enc_a    = ENC_A_PIN,
            .enc_b    = ENC_B_PIN,
            .enc_btn  = ENC_BTN_PIN
    };

    auto *p = (GpioTaskParams*)params;

    static GPIO_Input gpio(pins, p->gpioIsrQ);
    QueueHandle_t ControllerEventQ = p->ControllerEventQ;
    EventGroupHandle_t SystemBits = p->SystemBits;

    gpio.EnableIRQs();

    uint32_t last_btn_us = 0;
    uint32_t last_enc_us = 0;

    EventBits_t  busy_bit;


    while (1) {
        gpio_raw_event_t raw;

        if (xQueueReceive(p->gpioIsrQ, &raw, portMAX_DELAY) == pdPASS) {
            busy_bit = xEventGroupGetBitsFromISR(SystemBits);

            if (busy_bit & BIT_SYSTEM_BUSY) {
                continue;
            }

            // BUTTONS
            if (raw.gpio == pins.button_1 || raw.gpio == pins.button_2 || raw.gpio == pins.button_3 ||  raw.gpio == pins.enc_btn) {

                if ((raw.ts_us - last_btn_us) < BTN_DEBOUNCE_US) {
                    continue;
                }
                last_btn_us = raw.ts_us;

                if (raw.gpio == pins.button_1)send_button_event(p->ControllerEventQ, 1);
                else if (raw.gpio == pins.button_2) send_button_event(p->ControllerEventQ, 2);
                else if (raw.gpio == pins.button_3) send_button_event(p->ControllerEventQ, 3);
                else if (raw.gpio == pins.enc_btn) send_button_event(p->ControllerEventQ, 4);
            }

                // ENCODER A
            else if (raw.gpio == pins.enc_a) {

                if ((raw.ts_us - last_enc_us) < ENC_DEBOUNCE_US) {
                    continue;
                }
                last_enc_us = raw.ts_us;

                bool b = gpio_get(pins.enc_b);

                controller_event_t ev{};
                ev.type = b ? EVT_ENCODER_CCW : EVT_ENCODER_CW;
                xQueueSend(p->ControllerEventQ, &ev, 0);
            }
        }
    }
}