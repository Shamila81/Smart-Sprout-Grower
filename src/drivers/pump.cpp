#include "pump.h"
#include "hardware/gpio.h"
#include <stdio.h>

//Constructor
Pump::Pump(uint32_t pump_pin) :pump_pin(pump_pin) {
    gpio_init(pump_pin);
    gpio_set_dir(pump_pin, GPIO_OUT);
    gpio_put(pump_pin, false);
}

void Pump::run() {
    gpio_put(pump_pin, true);
    //printf("Pump ON\n");
}

void Pump::stop() {
    gpio_put(pump_pin, false);
    //printf("Pump OFF\n");
}