#pragma once
#include <cstdint>

#define STEPS_180 256
#define OFFSET_STEPS 30
#define CW   1
#define CCW  0

// Constructor
class Stepper{
public:
    Stepper(uint32_t in1, uint32_t in2, uint32_t in3, uint32_t in4);
    void step(uint8_t direction, uint16_t steps);
    void open();
    void close();
    void step_small(uint8_t direction);
    void off();
private:
    uint32_t in1;
    uint32_t in2;
    uint32_t in3;
    uint32_t in4;

};