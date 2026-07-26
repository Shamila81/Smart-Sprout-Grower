#include "stepper.h"
#include "hardware/gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "general/data.h"

static const uint8_t seq[8][4] = {
        {1,0,0,0},
        {1,1,0,0},
        {0,1,0,0},
        {0,1,1,0},
        {0,0,1,0},
        {0,0,1,1},
        {0,0,0,1},
        {1,0,0,1}
};

//Constructor
Stepper::Stepper(uint32_t in1, uint32_t in2, uint32_t in3, uint32_t in4) :in1(in1), in2(in2), in3(in3), in4(in4) {
    gpio_init(in1);
    gpio_init(in2);
    gpio_init(in3);
    gpio_init(in4);

    gpio_set_dir(in1, GPIO_OUT);
    gpio_set_dir(in2, GPIO_OUT);
    gpio_set_dir(in3, GPIO_OUT);
    gpio_set_dir(in4, GPIO_OUT);

    gpio_put(in1, false);
    gpio_put(in2, false);
    gpio_put(in3, false);
    gpio_put(in4, false);
}

void Stepper::step(uint8_t direction, uint16_t steps) {
    for (uint16_t s = 0; s < steps; s++) {
        for (int i = 0; i < 8; i++) {
            int idx = (direction == CW) ? i : (7 - i);

            gpio_put(in1, seq[idx][0]);
            gpio_put(in2, seq[idx][1]);
            gpio_put(in3, seq[idx][2]);
            gpio_put(in4, seq[idx][3]);

            vTaskDelay(pdMS_TO_TICKS(2));
        }
    }

    gpio_put(in1, 0);
    gpio_put(in2, 0);
    gpio_put(in3, 0);
    gpio_put(in4, 0);
}

void Stepper::step_small(uint8_t direction) { // used for calib

    for (int i = 0; i < 8; i++) {
        int idx = (direction == CW) ? i : (7 - i);

        gpio_put(in1, seq[idx][0]);
        gpio_put(in2, seq[idx][1]);
        gpio_put(in3, seq[idx][2]);
        gpio_put(in4, seq[idx][3]);

        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void Stepper::off() {
    gpio_put(in1, 0);
    gpio_put(in2, 0);
    gpio_put(in3, 0);
    gpio_put(in4, 0);
}


void Stepper::open() {
    step(CW, STEPS_180);
}

void Stepper::close() {
    step(CCW, STEPS_180);
}