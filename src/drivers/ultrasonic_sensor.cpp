#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "ultrasonic_sensor.h"

#define SIG_PIN 15

//Constructor
Ultrasonic::Ultrasonic(uint32_t gpio_sig_pin) {
    sig_pin = gpio_sig_pin;
    assert(pwm_gpio_to_channel(gpio_sig_pin) == PWM_CHAN_B);
    slice_number = pwm_gpio_to_slice_num(gpio_sig_pin);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv_mode(&cfg, PWM_DIV_B_HIGH);
    pwm_config_set_clkdiv(&cfg, 125);
    pwm_init(slice_number, &cfg, false);
    gpio_set_function(gpio_sig_pin,GPIO_FUNC_PWM);
}

void Ultrasonic::trigger() {
    pwm_set_enabled(slice_number, false);

    gpio_set_function(sig_pin, GPIO_FUNC_SIO);
    gpio_init(sig_pin);
    gpio_set_dir(sig_pin, GPIO_OUT);
    gpio_put(sig_pin, 0);
    sleep_ms(2);

    gpio_put(sig_pin, 1);
    sleep_us(10);
    gpio_put(sig_pin, 0);

    gpio_set_dir(sig_pin, GPIO_IN);
    gpio_set_function(sig_pin, GPIO_FUNC_PWM);
}

uint32_t Ultrasonic::measure_time_us() {
    pwm_set_counter(slice_number, 0);
    pwm_set_enabled(slice_number, true);
    sleep_ms(50); //
    pwm_set_enabled(slice_number, false);
    uint32_t time_us = pwm_get_counter(slice_number);
    return time_us;
}

float Ultrasonic::measure_cm() {
    float v_sound = 0.034300; // in cm/us
    trigger();
    uint32_t  measured_time_us = measure_time_us();
    float distance_to_water = (v_sound * measured_time_us) / 2.0f;
   // printf("Distance: %.2f cm\n", distance_to_water);
    return distance_to_water;

}