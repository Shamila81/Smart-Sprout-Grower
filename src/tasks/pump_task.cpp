#include "FreeRTOS.h"
#include "pump_task.h"
#include "drivers/pump.h"
#include "queue.h"
#include "general/data.h"
#include "event_groups.h"
#include <stdio.h>

static bool wait_with_stop(EventGroupHandle_t eg, uint32_t total_ms)
{
    const TickType_t total_ticks = pdMS_TO_TICKS(total_ms);
    const TickType_t step_ticks  = pdMS_TO_TICKS(10);

    TickType_t elapsed = 0;

    while (elapsed < total_ticks) {
        EventBits_t b = xEventGroupWaitBits(
                eg,
                BIT_WATER_LOW,
                pdFALSE,
                pdFALSE,
                step_ticks
        );

        if (b & BIT_STOP_PUMP) return true;
        elapsed += step_ticks;
    }
    return false;
}

void PumpTask(void *params) {
    auto *p = (PumpTaskParams *) params;
    QueueHandle_t ControllerToPumpQ = p->ControllerToPumpQ;
    EventGroupHandle_t SystemBits = p->SystemBits;

    Pump pump(p->pump_pin);
    Command cmd{};

    while(1){
        if (xQueueReceive(ControllerToPumpQ, &cmd, portMAX_DELAY) == pdPASS){
           // printf("[PUMP] cmd=%d dur=%lu\n", (int)cmd.type, (unsigned long)cmd.duration_ms);
            switch(cmd.type) {
                case PumpCMD::PUMP_RUN: {
                    if (xEventGroupGetBits(SystemBits) & BIT_STOP_PUMP) {
                       // printf("[PUMP] STOP active, ignoring RUN command\n");
                        cmd.type = PumpCMD::PUMP_STOP;
                        break;
                    }
                  //  printf("\n[PUMP] Rinsing for %lu ms\n", cmd.duration_ms);
                    pump.run();
                    bool stopped = wait_with_stop(SystemBits, cmd.duration_ms);
                    pump.stop() ;
                    xEventGroupSetBits(SystemBits, BIT_RINSE_DONE);
                    if (stopped){
                        //printf("[PUMP] STOPPED early!\n");
                        cmd.type = PumpCMD::PUMP_STOP;
                        break;
                    }
                } break;

                case PumpCMD::PUMP_STOP: {
                    //printf("\n[PUMP] Stop command received - setting STOP bit\n");
                    pump.stop();
                } break;
            }
        }
    }
}