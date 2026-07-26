#pragma once
#include <stdint.h>
#include "pico/stdlib.h"


// Constructor
class Ultrasonic{
public:
    explicit Ultrasonic(uint32_t gpio_sig_pin);
    float measure_cm();
private:
    uint32_t sig_pin;
    uint slice_number;

    void trigger();
    uint32_t measure_time_us();
};