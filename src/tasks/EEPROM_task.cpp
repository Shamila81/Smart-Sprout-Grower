#include "FreeRTOS.h"
#include "queue.h"
#include "EEPROM_task.h"
#include "drivers/EEPROM.h"

#define DAY_ADDRESS       0x0000
#define LID_ADDRESS       0x0010
#define SSID_ADDRESS      0x0100
#define PASSWORD_ADDRESS  0x0140

void EepromTask(void *params){
    auto *p = (EepromTaskParams*) params;
    QueueHandle_t ControllerEventQ = p->ControllerEventQ;
    QueueHandle_t ControllerToEepromQ = p->ControllerToEepromQ;

    auto i2cbus = p->bus;

    EEPROM eeprom_storage(i2cbus, 0x50);

    controller_event_t ev{};

    ev.type = EVT_EEPROM_DATA;
    ev.data.eeprom.day = eeprom_storage.eeprom_read_numeric(DAY_ADDRESS, 0);
    ev.data.eeprom.lid_pos = eeprom_storage.eeprom_read_numeric(LID_ADDRESS, 3);

    std::string ssid_tmp;
    std::string pass_tmp;


    eeprom_storage.eeprom_read_string(SSID_ADDRESS, ssid_tmp, EEPROM_SSID_LEN);
    eeprom_storage.eeprom_read_string(PASSWORD_ADDRESS, pass_tmp, EEPROM_PASS_LEN);

    snprintf(ev.data.eeprom.ssid, sizeof(ev.data.eeprom.ssid), "%s", ssid_tmp.c_str());
    snprintf(ev.data.eeprom.password, sizeof(ev.data.eeprom.password), "%s", pass_tmp.c_str());

   // printf("[EEPROM]: DAY value fetched from EEPROM: %d\n",ev.data.eeprom.day);
   // printf("[EEPROM]: LID pos value fetched from EEPROM: %d\n",ev.data.eeprom.lid_pos);
   // printf("[EEPROM] SSID fetched from EEPROM: %s\n", ev.data.eeprom.ssid);
   // printf("[EEPROM] PASS fetched from EEPROM: %s\n", ev.data.eeprom.password);

    xQueueSend(ControllerEventQ, &ev, 0);

    while(1) {
        ee_msg_t data_from_ctrl= {};
        if (xQueueReceive(ControllerToEepromQ, &data_from_ctrl, portMAX_DELAY) == pdPASS) {
            switch (data_from_ctrl.cmd) {

                case EE_CMD_SAVE_DAY:
                    eeprom_storage.eeprom_write_numeric(DAY_ADDRESS, data_from_ctrl.day);
                    //printf("[EEPROM] DAY (updated): %d\n", data_from_ctrl.day);
                    break;

                case EE_CMD_SAVE_LID:
                    eeprom_storage.eeprom_write_numeric(LID_ADDRESS, data_from_ctrl.lid_pos);
                    //printf("[EEPROM] LID position (updated): %d\n", data_from_ctrl.lid_pos);
                    break;

                case EE_CMD_SAVE_WIFI:
                {
                    std::string ssid_tmp(data_from_ctrl.ssid);
                    std::string pass_tmp(data_from_ctrl.password);

                    eeprom_storage.eeprom_write_string(SSID_ADDRESS, ssid_tmp, EEPROM_SSID_LEN);

                    eeprom_storage.eeprom_write_string(PASSWORD_ADDRESS, pass_tmp, EEPROM_PASS_LEN);

                    //printf("[EEPROM] SSID saved: %s\n", data_from_ctrl.ssid);
                    //printf("[EEPROM] PASS saved: %s\n", data_from_ctrl.password);
                    break;
                }

                default:
                    break;
            }
        }
    }
}