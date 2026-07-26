#include "FreeRTOS.h"
#include <stdio.h>
#include "stepper_task.h"
#include "drivers/stepper.h"
#include "queue.h"
#include "task.h"
#include "general/data.h"
#include "event_groups.h"

void StepperTask(void *params) {
    auto *p = (StepperTaskParams *) params;
    QueueHandle_t ControllerToStepperQ = p->ControllerToStepperQ;
    EventGroupHandle_t SystemBits = p->SystemBits;

    Stepper stepper(p->in1, p->in2,p->in3, p->in4);
    bool calibrating = false;

    while (1) {
        stepper_cmd_t cmd;

        if (!calibrating) {

            if (xQueueReceive(ControllerToStepperQ, &cmd, portMAX_DELAY) == pdPASS) {
                switch(cmd.type) {
                    case STEPPER_CMD_CALIB_START:
                       // printf("[Stepper] Calibrating...\n");
                        calibrating = true;
                        break;

                    case STEPPER_OPEN:
                        stepper.open();
                        xEventGroupClearBits(SystemBits, BIT_LID_CLOSED);
                        xEventGroupSetBits(SystemBits, BIT_LID_OPEN);
                        break;

                    case STEPPER_CLOSE:
                        stepper.close();
                        xEventGroupSetBits(SystemBits, BIT_LID_CLOSED);
                        xEventGroupClearBits(SystemBits, BIT_LID_OPEN);
                        break;
                }
            }
        } else {
            if (xQueueReceive(ControllerToStepperQ, &cmd, 5) == pdPASS) {
                if (cmd.type == STEPPER_CMD_CALIB_STOP) {
                    stepper.step(CCW, OFFSET_STEPS);
                    stepper.off();
                    //printf("[Stepper] Calib finished\n");
                    calibrating = false;
                }
            } else {
                stepper.step_small(CCW);
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
    }
}