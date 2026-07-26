#pragma once
#include <cstdint>

// Constructor
class Pump{
public:
    Pump(uint32_t pump_pin);
    void run();
    void stop();
private:
    uint32_t pump_pin;
};