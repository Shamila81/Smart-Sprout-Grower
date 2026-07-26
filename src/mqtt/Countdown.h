#ifndef COUNTDOWN_H
#define COUNTDOWN_H

#include "pico/time.h"

class Countdown
{
public:
    Countdown() { end_time = get_absolute_time(); }

    Countdown(int ms)
    {
        countdown_ms(ms);
    }

    bool expired()
    {
        return absolute_time_diff_us(get_absolute_time(), end_time) <= 0;
    }

    void countdown_ms(unsigned int ms)
    {
        end_time = make_timeout_time_ms(ms);
    }

    void countdown(unsigned int seconds)
    {
        countdown_ms(seconds * 1000);
    }

    int left_ms()
    {
        int64_t us = absolute_time_diff_us(get_absolute_time(), end_time);
        return us / 1000;
    }

private:
    absolute_time_t end_time;
};

#endif