#include "FreeRTOS.h"
#include "task.h"
#include "drivers/ultrasonic_sensor.h"
#include "general/data.h"
#include "lid_ultrasonic_task.h"
#include "event_groups.h"

#define CLOSED_LID_DISTANCE 4
#define OPEN_LID_DISTANCE 6

static float avg3(Ultrasonic& u){
    float s=0;
    for(int i=0;i<3;i++){
        s += u.measure_cm();
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    return s/3.0f;
}

void LidUltrasonicTask(void *params) {
    auto *p = (LidUltrasonicTaskParams*)params;
    EventGroupHandle_t SystemBits = p->SystemBits;
    Ultrasonic ultra_lid(p->sig_pin);

    static constexpr int STABLE_N   = 3;

    bool saw_open_once = false;

    while(true){
        EventBits_t bits = xEventGroupGetBits(SystemBits);

        if (!(bits & BIT_CALIBRATION_ACTIVE)) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        float cm = avg3(ultra_lid);
        //printf("[ULTRA-LID] avg3: %.2f\n", cm);

        bool is_open  = (cm >= OPEN_LID_DISTANCE);
        bool is_closed = (cm <= CLOSED_LID_DISTANCE);

        if(is_open){
            saw_open_once = true;
        }

        if(saw_open_once && is_closed){
            EventBits_t bits = xEventGroupGetBits(SystemBits);
            if(!(bits & BIT_LID_CLOSED)){
                xEventGroupSetBits(SystemBits, BIT_LID_CLOSED);
            }
            saw_open_once = false;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}