#include "FreeRTOS.h"
#include <stdio.h>
#include "ultrasonic_task.h"
#include "drivers/ultrasonic_sensor.h"
#include "queue.h"
#include "task.h"
#include "general/data.h"
#include "event_groups.h"

#define WATER_LOW_CM 25.0f

void UltrasonicTask(void *params) {
    auto *p = (UltrasonicTaskParams *) params;
    QueueHandle_t ControllerEventQ = p->ControllerEventQ;
    EventGroupHandle_t SystemBits = p->SystemBits;

    Ultrasonic ultra(p->sig_pin);

    while (1) {

            float cm = ultra.measure_cm();
           // printf("[ULTRA-water] %.2f cm\n", cm);

            if (cm > WATER_LOW_CM) {
                xEventGroupSetBits(SystemBits, BIT_WATER_LOW);
                controller_event_t ev{};
                ev.type = EVT_WATER_LOW_BIT_SET;
                xQueueSend(ControllerEventQ, &ev, 0);
               // printf("[ULTRA-water] Water low -> event sent\n");

            } else {
                xEventGroupClearBits(SystemBits, BIT_WATER_LOW);

            }
            vTaskDelay(pdMS_TO_TICKS(500));


    }
}