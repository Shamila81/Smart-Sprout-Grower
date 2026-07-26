#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "i2c/PicoI2C.h"
#include "controller/controller_task.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "tasks/pump_task.h"
#include "tasks/stepper_task.h"
#include "tasks/oled_task.h"
#include "tasks/ultrasonic_task.h"
#include "tasks/bme_task.h"
#include "tasks/gpio_task.h"
#include "drivers/GPIO_input.h"
#include "tasks/EEPROM_task.h"
#include "tasks/lid_ultrasonic_task.h"
#include "tasks/mqtt_task.h"
#include "controller/controller_task.h"
#include "general/data.h"

#define SIG_PIN 15
#define PUMP_ENA 2
#define BME_ADDR 0x76
#define STEPPER_IN1 6
#define STEPPER_IN2 7
#define STEPPER_IN3 8
#define STEPPER_IN4 9
#define LID_SIG_PIN 27

#include "hardware/timer.h"
extern "C" {
uint32_t read_runtime_ctr(void) {
    return timer_hw->timerawl;
}
}


int main()
{

    stdio_init_all();
    sleep_ms(2000);
    //printf("System boot...\n");

    //=========================================================================================
    EventGroupHandle_t SystemBits = xEventGroupCreate();
    configASSERT(SystemBits);
    xEventGroupClearBits(SystemBits,  BIT_WATER_LOW | BIT_STOP_PUMP | BIT_LID_OPEN | BIT_LID_CLOSED | BIT_RINSE_DONE| BIT_SYSTEM_BUSY | BIT_USER_INPUT_ALLOWED| BIT_CALIBRATION_ACTIVE);

    //========================================================================================
    QueueHandle_t gpioIsrQ = xQueueCreate(8, sizeof(gpio_raw_event_t));
    configASSERT(gpioIsrQ);

    //=========================================================================================
    QueueHandle_t ControllerEventQ = xQueueCreate(20, sizeof(controller_event_t));
    configASSERT(ControllerEventQ);

    //=========================================================================================
    QueueHandle_t ControllerToOledQ = xQueueCreate(10, sizeof(oled_cmd_t ));
    configASSERT(ControllerToOledQ);

    //=========================================================================================
    QueueHandle_t ControllerToStepperQ = xQueueCreate(8, sizeof(stepper_cmd_t ));
    configASSERT(ControllerToStepperQ);

    //=========================================================================================
    QueueHandle_t ControllerToPumpQ = xQueueCreate(8, sizeof(Command));
    configASSERT(ControllerToPumpQ);

    //=====================================================================================================
    QueueHandle_t ControllerToEepromQ = xQueueCreate(8, sizeof(ee_msg_t));
    configASSERT(ControllerToEepromQ);

    //=====================================================================================================
    QueueHandle_t ControllerToMqttQ = xQueueCreate(8, sizeof(mqtt_cmd_t));
    configASSERT(ControllerToMqttQ);


    auto i2c_bus = std::make_shared<PicoI2C>(0, 100000);


    //=======================================================================================
    static ControllerTaskParams ControllerParams = {
            .SystemBits = SystemBits,
            .ControllerEventQ = ControllerEventQ,
            .ControllerToOledQ = ControllerToOledQ,
            .ControllerToStepperQ = ControllerToStepperQ,
            .ControllerToPumpQ = ControllerToPumpQ,
            .ControllerToEepromQ = ControllerToEepromQ,
            .ControllerToMqttQ = ControllerToMqttQ

    };
    xTaskCreate(ControllerTask,"Controller",1024,&ControllerParams,2, nullptr);
    //======================================================================================================

    static EepromTaskParams EepromParams = {
            .ControllerEventQ = ControllerEventQ,
            .ControllerToEepromQ = ControllerToEepromQ,
            .bus = i2c_bus
    };

    xTaskCreate(EepromTask,"Eeprom",512, &EepromParams,1, nullptr);

    //=========================================================================================================
    static PumpTaskParams PumpParams = {
            .ControllerToPumpQ = ControllerToPumpQ,
            .SystemBits = SystemBits,
            .pump_pin = PUMP_ENA
    };

    xTaskCreate(PumpTask,"Pump",512, &PumpParams,1, nullptr);

    //==========================================================================================================

    static BmeTaskParams BmeParams = {
            .ControllerEventQ = ControllerEventQ,
            .bus  = i2c_bus,
            .addr=BME_ADDR

    };
    xTaskCreate(BmeTask,"Bme",1024, &BmeParams,1, nullptr);
    //=====================================================================================================================
    static OledTaskParams OledParams = {
            .ControllerToOledQ = ControllerToOledQ,
            .SystemBits = SystemBits,
            .bus = i2c_bus
    };
    xTaskCreate(OledTask,"Oled",1024, &OledParams,1, nullptr);

    //=======================================================================================================
    static StepperTaskParams StepperParams = {
            .ControllerToStepperQ = ControllerToStepperQ,
            .SystemBits = SystemBits,
            .in1 = STEPPER_IN1,
            .in2 = STEPPER_IN2,
            .in3 = STEPPER_IN3,
            .in4 = STEPPER_IN4
    };
    xTaskCreate(StepperTask,"Stepper",512, &StepperParams,1, nullptr);
    //=====================================================================================================================
    static GpioTaskParams GpioParams = {
            .gpioIsrQ = gpioIsrQ,
            .ControllerEventQ = ControllerEventQ,
            .SystemBits = SystemBits,

    };
    xTaskCreate(GpioTask, "GPIO", 512, &GpioParams, 1, NULL);

    //=======================================================================================================
    static UltrasonicTaskParams UltrasonicParams = {
            .ControllerEventQ = ControllerEventQ,
            .SystemBits = SystemBits,
            .sig_pin = SIG_PIN
    };

    xTaskCreate(UltrasonicTask,"Ultra",512, &UltrasonicParams,1, nullptr);
    //=======================================================================================================
    static LidUltrasonicTaskParams LidUltraParams = {
            .SystemBits = SystemBits,
            .sig_pin = LID_SIG_PIN
    };
    xTaskCreate(LidUltrasonicTask, "LidUltra", 256, &LidUltraParams, 1, nullptr);
    //========================================================================================================
    static MqttTaskParams MqttParams = {
            .ControllerEventQ = ControllerEventQ,
            .SystemBits = SystemBits,
            .ControllerToMqttQ = ControllerToMqttQ

    };
    xTaskCreate(MqttTask,"Mqtt",2048,&MqttParams,1,NULL);
    //===============================================================================================================

    vTaskStartScheduler();

    while (true) {
        tight_loop_contents();
    }
}