//NOTE: In the previous version the lid position of the container was saved to EEPROM since calibration was performed manually.
//In the current version, the lid is always calibrated after BOOT STATE using ultrasonic sensor, that was added late in development.
//Saving/reading lid position to/from EEPROM is no longer necessary, but this part was left because it does not
//affect functionality of the system, but removing would require extra refactoring.

#include "FreeRTOS.h"
#include "controller_task.h"
#include "queue.h"
#include "event_groups.h"
#include "task.h"
#include "general/data.h"
#include <stdio.h>
#include "drivers/bme68x_defs.h"
#include "timers.h"
#include "tasks/EEPROM_task.h"
#include <stdint.h>
#include <string.h>


#define DEMO_DAY_MS 60000
#define UI_WINDOW 40000
#define LID_OPEN 1
#define LID_CLOSED 2
#define LID_UNKNOWN 3
#define MQTT_PUBLISH_INTERVAL 2000
#define PUMP_RUN_MS 3000

#define SIMULATE_HIGH_T  30.0f
#define SIMULATE_LOW_RH    50.0f

static int day_saved = 0;

static TimerHandle_t phaseTimer = NULL;
static TimerHandle_t dayTimer = NULL;
static TimerHandle_t uiTimer = NULL;
static TimerHandle_t mqttTimer = NULL;

static TickType_t ui_deadline_tick = 0;

static uint8_t menu_sel = 0;
static bool menu_active = false;
static bool eeprom_ok = false;
static int last_menu_sec = -1;
static uint8_t last_menu_sel = 255;

static float last_temp = 0.0f;
static float last_rh   = 0.0f;
static char  current_status[16];

static char wifi_ssid[33] = {0};
static char wifi_pass[33] = {0};
static char wifi_status[15] = "WIFI- RESET";
static char wifi_current_char = 'A';
static bool wifi_editing = false;
static bool wifi_inited = false;
static bool wifi_connect_started = false;
static wifi_phase_t wifi_phase = WIFI_ENTER_SSID;

//==========================================================================================
static char ASCII_TABLE[96];
static void init_ascii_table_once()
{
    static bool inited = false;
    if (inited) return;

    for (int i = 0; i < 95; i++) {
        ASCII_TABLE[i] = (char)(32 + i);
    }
    ASCII_TABLE[95] = '\0';
    inited = true;
}

static inline int ascii_find_index(char c)
{
    for (int i = 0; i < 95; i++) if (ASCII_TABLE[i] == c) return i;
    return 0;
}

static inline char ascii_step(char current, int delta)
{
    int idx = ascii_find_index(current);
    idx = (idx + delta) % 95;
    if (idx < 0) idx += 95;
    return ASCII_TABLE[idx];
}


//TIMERS=============================================================================
static void MqttTimerCb(TimerHandle_t xTimer)
{
    QueueHandle_t q = (QueueHandle_t) pvTimerGetTimerID(xTimer);

    controller_event_t ev{};
    ev.type = EVT_MQTT_PUBLISH_TIMER;

    xQueueSend(q, &ev, 0);
}

static void PhaseTimerCb(TimerHandle_t xTimer) {
    QueueHandle_t q = (QueueHandle_t) pvTimerGetTimerID(xTimer);
    controller_event_t ev{};
    ev.type = EVT_PHASE_TIMEOUT;
    xQueueSend(q, &ev, 0);
}

static void DayTimerCb(TimerHandle_t xTimer) {
    QueueHandle_t q = (QueueHandle_t) pvTimerGetTimerID(xTimer);

    controller_event_t ev{};
    ev.type = EVT_DAY_ALARM;

    xQueueSend(q, &ev, 0);
}

static void UiTimerCb(TimerHandle_t xTimer)
{
    QueueHandle_t q = (QueueHandle_t) pvTimerGetTimerID(xTimer);

    controller_event_t ev{};
    ev.type = EVT_UI_WINDOW_TIMEOUT;

    xQueueSend(q, &ev, 0);
}


static void StartDayTimer(QueueHandle_t q, uint32_t day_ms) {
    if (dayTimer == NULL) {
        dayTimer = xTimerCreate(
                "day",
                pdMS_TO_TICKS(day_ms),
                pdTRUE,
                (void*) q,
                DayTimerCb
        );
        configASSERT(dayTimer != NULL);
    } else {
        xTimerChangePeriod(dayTimer, pdMS_TO_TICKS(day_ms), 0);
    }

    xTimerStart(dayTimer, 0);
}

static void StartUiWindowTimerMs(QueueHandle_t q, uint32_t ms)
{
    if (uiTimer == NULL) {
        uiTimer = xTimerCreate(
                "ui",
                pdMS_TO_TICKS(ms),
                pdFALSE,           // one-shot
                (void*) q,
                UiTimerCb
        );
        configASSERT(uiTimer != NULL);
    } else {
        xTimerChangePeriod(uiTimer, pdMS_TO_TICKS(ms), 0);
    }

    xTimerStart(uiTimer, 0);
}


static void StartMqttTimer(QueueHandle_t q, uint32_t ms)
{
    if (mqttTimer == NULL) {
        mqttTimer = xTimerCreate(
                "mqtt",
                pdMS_TO_TICKS(ms),
                pdTRUE,
                (void*) q,
                MqttTimerCb
        );
        configASSERT(mqttTimer != NULL);
    } else {
        xTimerChangePeriod(mqttTimer, pdMS_TO_TICKS(ms), 0);
    }

    xTimerStart(mqttTimer, 0);
}

static void StartPhaseTimerMs(QueueHandle_t q, uint32_t ms) {
    if (phaseTimer == NULL) {
        phaseTimer = xTimerCreate(
                "phase",
                pdMS_TO_TICKS(ms),
                pdFALSE,
                (void*) q,
                PhaseTimerCb
        );
        configASSERT(phaseTimer != NULL);
    } else {
        xTimerChangePeriod(phaseTimer, pdMS_TO_TICKS(ms), 0);
    }

    xTimerStart(phaseTimer, 0);
}
//MQTT============================================================================================

static void mqtt_send_status(QueueHandle_t mqttQ, float temp, float rh, int day)
{
    mqtt_cmd_t mcmd{};
    mcmd.type = MQTT_CMD_PUBLISH;

    snprintf(mcmd.topic, sizeof(mcmd.topic), "grow/status");
    snprintf(mcmd.payload, sizeof(mcmd.payload),
             "{\"temp\":%.1f,\"rh\":%.1f,\"day\":%d}",
             temp, rh, day);

    xQueueSend(mqttQ, &mcmd, 0);
}

//OLED========================================================================================
static void oled_show_menu_now(QueueHandle_t q, int day, const char *status, float t, float rh, uint8_t sel)
{
    oled_cmd_t ocmd{};
    ocmd.type = OLED_SHOW_MENU;
    ocmd.data.menu.day = day;
    ocmd.data.menu.temp_c = t;
    ocmd.data.menu.rh = rh;
    snprintf(ocmd.data.menu.status, sizeof(ocmd.data.menu.status), "%s", status);
    ocmd.data.menu.sel = sel;
    xQueueSend(q, &ocmd, 0);
}

static void oled_show_auto_now(QueueHandle_t q,
                               int day,
                               const char *status,
                               const char *wifi,
                               float t,
                               float rh)
{
    oled_cmd_t ocmd{};
    ocmd.type = OLED_SHOW_AUTO;

    ocmd.data.system_data.day = day;
    ocmd.data.system_data.temp_c = t;
    ocmd.data.system_data.rh = rh;

    snprintf(ocmd.data.system_data.status,
             sizeof(ocmd.data.system_data.status),
             "%s", status);

    snprintf(ocmd.data.system_data.wifi,
             sizeof(ocmd.data.system_data.wifi),
             "%s", wifi);

    xQueueSend(q, &ocmd, 0);
}

static inline void oled_send(QueueHandle_t q, const oled_cmd_t &cmd)
{
    xQueueSend(q, &cmd, 0);
}

static void oled_show_menu_with_countdown(QueueHandle_t q, int day, float t, float rh, uint8_t sel)
{
    TickType_t now = xTaskGetTickCount();
    int32_t ticks_left = (int32_t)(ui_deadline_tick - now);
    if (ticks_left < 0) ticks_left = 0;

    int32_t ms_left = ticks_left * (int32_t)portTICK_PERIOD_MS;
    int sec_left = (ms_left + 999) / 1000;

    if (sec_left == last_menu_sec && sel == last_menu_sel) {
        return;
    }

    last_menu_sec = sec_left;
    last_menu_sel = sel;

    char status[16];
    snprintf(status, sizeof(status), "%ds", sec_left);

    oled_show_menu_now(q, day, status, t, rh, sel);
}

static void oled_show(QueueHandle_t q, oled_cmd_type_t type)
{
    oled_cmd_t m{};
    m.type = type;
    oled_send(q, m);
}
static void oled_show_lines(QueueHandle_t q,
                            const char* l1,
                            const char* l2,
                            const char* l3,
                            const char* l4,
                            const char* l5,
                            const char* l6,
                            const char* l7)
{
    oled_cmd_t m{};
    m.type = OLED_SHOW_TEXT;

    if (l1) snprintf(m.data.text.line1, sizeof(m.data.text.line1), "%s", l1);
    if (l2) snprintf(m.data.text.line2, sizeof(m.data.text.line2), "%s", l2);
    if (l3) snprintf(m.data.text.line3, sizeof(m.data.text.line3), "%s", l3);
    if (l4) snprintf(m.data.text.line4, sizeof(m.data.text.line4), "%s", l4);
    if (l5) snprintf(m.data.text.line5, sizeof(m.data.text.line5), "%s", l5);
    if (l6) snprintf(m.data.text.line6, sizeof(m.data.text.line6), "%s", l6);
    if (l7) snprintf(m.data.text.line7, sizeof(m.data.text.line7), "%s", l7);

    oled_send(q, m);
}

//WIFI==============================================================================
static inline char* wifi_active_buf()
{
    return (wifi_phase == WIFI_ENTER_SSID) ? wifi_ssid : wifi_pass;
}

static inline size_t wifi_active_max()
{
    return (wifi_phase == WIFI_ENTER_SSID) ? WIFI_SSID_MAX : WIFI_PASS_MAX;
}

static void wifi_setup(QueueHandle_t oledQ)
{
    char line2[66];
    char line3[24];

    if (wifi_phase == WIFI_ENTER_SSID) {
        snprintf(line2, sizeof(line2), "%s_", wifi_ssid);
        snprintf(line3, sizeof(line3), "char: %c", wifi_current_char);

        oled_show_lines(oledQ,
                        "SSID: -> ENC",
                        line2,
                        line3,
                        "B1: delete",
                        "ENC B: next",
                        "B3: SAVE",
                        nullptr);
    } else {
        snprintf(line2, sizeof(line2), "%s_", wifi_pass);
        snprintf(line3, sizeof(line3), "char: %c", wifi_current_char);

        oled_show_lines(oledQ,
                        "PASS: -> ENC",
                        line2,
                        line3,
                        "B1: delete",
                        "EB: next",
                        "B3: SAVE",
                        nullptr);
    }
}

static void wifi_show_empty_msg(QueueHandle_t oledQ, const char *what)
{
    char l1[24];
    char l2[24];

    snprintf(l1, sizeof(l1), "%s EMPTY", what);
    snprintf(l2, sizeof(l2), "ENTER %s", what);

    oled_show_lines(oledQ, l1, nullptr, l2, "ROTATE ENC", nullptr, nullptr, nullptr);
}
//===========================================================================================
static bool wifi_next_or_save(QueueHandle_t oledQ)
{
    if (wifi_phase == WIFI_ENTER_SSID) {

        if (wifi_ssid[0] == '\0') {
            wifi_show_empty_msg(oledQ, "SSID");
            return false;
        }

        wifi_phase = WIFI_ENTER_PASS;
        wifi_current_char = 'A';
        wifi_setup(oledQ);
        return false;
    }

    if (wifi_pass[0] == '\0') {
        wifi_show_empty_msg(oledQ, "PASS");
        return false;
    }

    return true;
}

static void wifi_start(QueueHandle_t oledQ)
{
    wifi_inited = true;
    wifi_phase = WIFI_ENTER_SSID;
    wifi_current_char = 'A';

    wifi_ssid[0] = '\0';
    wifi_pass[0] = '\0';

    wifi_editing = true;

    wifi_setup(oledQ);
}
//PUMP=============================================================================
static void send_cmd_pump(QueueHandle_t q, PumpCMD type, uint32_t duration_ms)
{
    Command c{type, duration_ms};
    xQueueSend(q, &c, portMAX_DELAY);
}

static void handle_low_water_bit_set(QueueHandle_t PumpQ,
                                     EventGroupHandle_t SystemBits,
                                     system_state_t &state,
                                     int day)
{
    send_cmd_pump(PumpQ, PumpCMD::PUMP_STOP, 0);

    if (uiTimer  != NULL) xTimerStop(uiTimer, 0);
    if (dayTimer != NULL) xTimerStop(dayTimer, 0);

    xEventGroupClearBits(SystemBits, BIT_USER_INPUT_ALLOWED);
    xEventGroupSetBits(SystemBits, BIT_STOP_PUMP);

    day_saved = day;
    state = ST_REFILL_TANK;

}

static void do_rinse(EventGroupHandle_t systemBits,
                     QueueHandle_t pumpQ)
{
    send_cmd_pump(pumpQ, PumpCMD::PUMP_RUN, PUMP_RUN_MS);

    xEventGroupWaitBits(systemBits,
                        BIT_RINSE_DONE,
                        pdTRUE,
                        pdTRUE,
                        portMAX_DELAY);
}

static void handle_phase_rinse(EventGroupHandle_t systemBits,bool &menu_active,char *status_buf,size_t status_buf_sz,QueueHandle_t oledQ,int day,float temp,float rh,QueueHandle_t pumpQ,demo_phase_t &phase,QueueHandle_t eventQ)
{
    xEventGroupClearBits(systemBits, BIT_USER_INPUT_ALLOWED);

    menu_active = false;

    snprintf(status_buf, status_buf_sz, "%s", "RINSING");
    oled_show_auto_now(oledQ, day, status_buf, wifi_status, temp, rh);

    do_rinse(systemBits, pumpQ);
    phase = PH_VENT;
    StartPhaseTimerMs(eventQ, 1);
}


//BME========================================================================================
static void handle_bme_update(const controller_event_t &event, QueueHandle_t oledQ, int day, const char *status, float &last_temp, float &last_rh, bool menu_active)
{
    last_temp = event.data.bme.temp_c;
    last_rh   = event.data.bme.rh;
    if (!menu_active) {
        oled_show_auto_now(oledQ, day, status, wifi_status, last_temp, last_rh);
    }
    //printf("T: %.1f C RH: %.1f %%\n", last_temp, last_rh);
}

static inline void update_bme_values(const controller_event_t &event)
{
    last_temp = event.data.bme.temp_c;
    last_rh   = event.data.bme.rh;
}

//ENCODER======================================================================================
static void handle_encoder_turn(int direction, EventGroupHandle_t systemBits, bool menu_active, uint8_t &menu_sel, QueueHandle_t oledQ, int day, const char *status, float temp,float rh)
{
    if (!menu_active) return;
    if (!(xEventGroupGetBits(systemBits) & BIT_USER_INPUT_ALLOWED)) return;
    menu_sel = (menu_sel + direction + 3) % 3;
    oled_show_menu_with_countdown(oledQ, day, temp, rh, menu_sel);
}

//STEPPER====================================================================================================
static void set_lid_pos(uint16_t pos, QueueHandle_t ctrlToEepromQ)
{
    if (!eeprom_ok) {
       // printf("[EEPROM DBG] EEPROM NOT AVAILABLE -> LID=%d NOT saved\n", pos);
        return;
    }

    ee_msg_t msg{};
    msg.cmd = EE_CMD_SAVE_LID;
    msg.lid_pos = pos;
    xQueueSend(ctrlToEepromQ, &msg, 0);
}

static void do_open_lid(EventGroupHandle_t systemBits,
                        QueueHandle_t oledQ,
                        QueueHandle_t ctrlToEepromQ,
                        QueueHandle_t stepperQ,
                        int day,
                        float temp,
                        float rh,
                        const char *status)
{
    stepper_cmd_t cmd{};
    cmd.type = STEPPER_OPEN;

    oled_show_auto_now(oledQ, day, status, wifi_status, temp, rh);
    set_lid_pos(LID_UNKNOWN, ctrlToEepromQ); //3
    xQueueSend(stepperQ, &cmd, 0);

    xEventGroupWaitBits(systemBits,
                        BIT_LID_OPEN,
                        pdFALSE,
                        pdTRUE,
                        portMAX_DELAY);
    set_lid_pos(LID_OPEN, ctrlToEepromQ);
}

static void do_close_lid(EventGroupHandle_t systemBits,
                         QueueHandle_t oledQ,
                         QueueHandle_t ctrlToEepromQ,
                         QueueHandle_t stepperQ,
                         int day,
                         float temp,
                         float rh,
                         const char *status)
{
    stepper_cmd_t cmd{};
    cmd.type = STEPPER_CLOSE;

    oled_show_auto_now(oledQ, day, status, wifi_status,temp, rh);
    set_lid_pos(LID_UNKNOWN, ctrlToEepromQ);
    xQueueSend(stepperQ, &cmd, 0);

    xEventGroupWaitBits(systemBits,
                        BIT_LID_CLOSED,
                        pdFALSE,
                        pdTRUE,
                        portMAX_DELAY);
    set_lid_pos(LID_CLOSED, ctrlToEepromQ);
}


static void handle_phase_vent(EventGroupHandle_t systemBits,char *status_buf,size_t status_buf_sz,QueueHandle_t oledQ, QueueHandle_t ctrlToEepromQ,int day,float temp,float rh,QueueHandle_t stepperQ, demo_phase_t &phase, QueueHandle_t eventQ)
{
    if (xEventGroupGetBits(systemBits) & BIT_WATER_LOW) {

        return;
    }

    snprintf(status_buf, status_buf_sz, "%s", "VENTING");
    oled_show_auto_now(oledQ, day, status_buf, wifi_status, temp, rh);
    do_open_lid(systemBits, oledQ, ctrlToEepromQ, stepperQ,
                day, temp, rh, status_buf);

    phase = PH_FINISH_ROUTINE;
    StartPhaseTimerMs(eventQ, 3000);
}

static void handle_phase_finish_routine(EventGroupHandle_t systemBits,char *status_buf,size_t status_buf_sz,QueueHandle_t oledQ, QueueHandle_t ctrlToEepromQ, int day,float temp,float rh,QueueHandle_t stepperQ,demo_phase_t &phase,QueueHandle_t eventQ)
{
    //printf("[CTRL] Venting routine: closing lid\n");
    snprintf(status_buf, status_buf_sz, "%s", "VENTING");
    oled_show_auto_now(oledQ, day, status_buf, wifi_status,temp, rh);
    do_close_lid(systemBits, oledQ, ctrlToEepromQ, stepperQ,
                 day, temp, rh, status_buf);

    phase = PH_INTERRUPTS_ALLOWED;
    StartPhaseTimerMs(eventQ, 1);
}

static void ensure_lid_closed(EventGroupHandle_t systemBits, QueueHandle_t stepperQ, QueueHandle_t oledQ, QueueHandle_t ctrlToEepromQ)
{
    EventBits_t bits = xEventGroupGetBits(systemBits);
    if (bits & BIT_LID_CLOSED) {
        set_lid_pos(LID_CLOSED, ctrlToEepromQ);
        return;
    }

    stepper_cmd_t cmd{};
    cmd.type = STEPPER_CLOSE;
    xQueueSend(stepperQ, &cmd, 0);

    oled_cmd_t ocmd{};
    ocmd.type = OLED_SHOW_TEXT;
    snprintf(ocmd.data.text.line1, sizeof(ocmd.data.text.line1), "RESUMING...");
    snprintf(ocmd.data.text.line3, sizeof(ocmd.data.text.line3), "Please wait");
    xQueueSend(oledQ, &ocmd, 0);

    xEventGroupWaitBits(systemBits,
                        BIT_LID_CLOSED,
                        pdFALSE,
                        pdTRUE,
                        portMAX_DELAY);
    set_lid_pos(LID_CLOSED, ctrlToEepromQ);

}

static bool stepper_send(QueueHandle_t q, stepper_cmd_type_t type, TickType_t waitTicks)
{
    stepper_cmd_t cmd{};
    cmd.type = type;
    return (xQueueSend(q, &cmd, waitTicks) == pdPASS);
}
//=============================================================================================
static void wait_lid_closed(EventGroupHandle_t bits)
{
    xEventGroupWaitBits(bits,
                        BIT_LID_CLOSED,
                        pdFALSE,
                        pdTRUE,
                        portMAX_DELAY);
}

//EEPROM==============================================================================================
static void eeprom_reset(QueueHandle_t ctrlToEepromQ)
{
    if (!eeprom_ok) return;


    ee_msg_t msg{};
    msg.cmd = EE_CMD_SAVE_DAY;
    msg.day = 0;
    xQueueSend(ctrlToEepromQ, &msg, 0);

    msg = {};
    msg.cmd = EE_CMD_SAVE_LID;
    msg.lid_pos = LID_UNKNOWN;
    xQueueSend(ctrlToEepromQ, &msg, 0);
}

static void save_wifi_to_eeprom(const char *ssid,
                                const char *password,
                                QueueHandle_t ctrlToEepromQ)
{
    if (!eeprom_ok) {
        printf("[EEPROM DBG] EEPROM NOT AVAILABLE -> WIFI NOT saved\n");
        return;
    }

    ee_msg_t msg{};
    msg.cmd = EE_CMD_SAVE_WIFI;

    snprintf(msg.ssid, sizeof(msg.ssid), "%s", ssid);
    snprintf(msg.password, sizeof(msg.password), "%s", password);

    xQueueSend(ctrlToEepromQ, &msg, 0);

    printf("[CTRL] WIFI saved to EEPROM\n");
}

//SYSTEM===============================================================================================
static void set_day(int &day, int new_day, QueueHandle_t ctrlToEepromQ)
{
    if (day == new_day) {
        return;
    }
    day = new_day;

    if (!eeprom_ok) {
        return;
    }

    ee_msg_t msg{};
    msg.cmd = EE_CMD_SAVE_DAY;
    msg.day = day;
    xQueueSend(ctrlToEepromQ, &msg, 0);
}

static void handle_phase_interrupts_allowed(system_state_t &state,EventGroupHandle_t systemBits,bool &menu_active,uint8_t &menu_sel,char *status_buf,size_t status_buf_sz,QueueHandle_t oledQ,int day,float temp,float rh,QueueHandle_t eventQ)
{
    state = ST_INTERRUPTS_ALLOWED;

    xEventGroupSetBits(systemBits, BIT_USER_INPUT_ALLOWED);

    menu_active = false;
    menu_sel = 0;

    snprintf(status_buf, status_buf_sz, "%s", "IDLE");
    oled_show_auto_now(oledQ, day, status_buf,wifi_status, temp, rh);

    StartUiWindowTimerMs(eventQ, UI_WINDOW);
    ui_deadline_tick = xTaskGetTickCount() + pdMS_TO_TICKS(UI_WINDOW);
}

static void handle_day_alarm(EventGroupHandle_t systemBits,bool &menu_active,int &day,system_state_t &state,demo_phase_t &phase,char *status_buf,size_t status_buf_sz,QueueHandle_t eventQ, QueueHandle_t ctrlToEepromQ)
{
    xEventGroupClearBits(systemBits, BIT_USER_INPUT_ALLOWED);
    menu_active = false;

    set_day(day, day + 1, ctrlToEepromQ);

    if (day > 3) {
        eeprom_reset(ctrlToEepromQ);
        state = ST_SPROUTS_READY;
        return;
    }

    phase = PH_RINSE;
    snprintf(status_buf, status_buf_sz, "%s", "Rinsing");
    StartPhaseTimerMs(eventQ, 1);
}

static void run_manual_action(uint8_t sel,EventGroupHandle_t systemBits,QueueHandle_t oledQ, QueueHandle_t ControllerToEepromQ, QueueHandle_t pumpQ,QueueHandle_t stepperQ,int day,float temp,float rh,char *status_buf,size_t status_buf_sz)
{
    xEventGroupSetBits(systemBits, BIT_SYSTEM_BUSY);
    // IDLE
    if (sel == 0) {
        menu_active = false;

        snprintf(status_buf, status_buf_sz, "%s", "IDLE");
        oled_show_auto_now(oledQ, day, status_buf, wifi_status, temp, rh);
        xEventGroupClearBits(systemBits, BIT_SYSTEM_BUSY);
        return;
    }

    // manual rinse
    if (sel == 1) {
        xEventGroupClearBits(systemBits, BIT_USER_INPUT_ALLOWED);


        snprintf(status_buf, status_buf_sz, "%s", "MAN RINSE");
        oled_show_auto_now(oledQ, day, status_buf, wifi_status, temp, rh);

        do_rinse(systemBits, pumpQ);

        snprintf(status_buf, status_buf_sz, "%s", "IDLE");
        oled_show_auto_now(oledQ, day, status_buf, wifi_status, temp, rh);

        xEventGroupSetBits(systemBits, BIT_USER_INPUT_ALLOWED);
        xEventGroupClearBits(systemBits, BIT_SYSTEM_BUSY);

        return;
    }

    // manual vent
    if (sel == 2) {
        xEventGroupClearBits(systemBits, BIT_USER_INPUT_ALLOWED);

        snprintf(status_buf, status_buf_sz, "%s", "MAN VENT");
        oled_show_auto_now(oledQ, day, status_buf, wifi_status, temp, rh);

        do_open_lid(systemBits, oledQ, ControllerToEepromQ, stepperQ,
                    day, temp, rh, status_buf);
        if (xEventGroupGetBits(systemBits) & BIT_WATER_LOW) {
            return;
        }

        vTaskDelay(pdMS_TO_TICKS(3000));

        do_close_lid(systemBits, oledQ, ControllerToEepromQ, stepperQ,
                     day, temp, rh, status_buf);
        snprintf(status_buf, status_buf_sz, "%s", "IDLE");
        oled_show_auto_now(oledQ, day, status_buf, wifi_status, temp, rh);

        xEventGroupSetBits(systemBits, BIT_USER_INPUT_ALLOWED);
        xEventGroupClearBits(systemBits, BIT_SYSTEM_BUSY);
        return;
    }
}

static void run_lid_calibration(EventGroupHandle_t bits,
                                QueueHandle_t oledQ,
                                QueueHandle_t stepperQ)
{
    xEventGroupClearBits(bits, BIT_LID_CLOSED);
    oled_show_lines(oledQ, "CALIBRATING...", nullptr, "PLEASE WAIT", nullptr, nullptr, nullptr, nullptr);

    stepper_send(stepperQ, STEPPER_CMD_CALIB_START, pdMS_TO_TICKS(100));
    wait_lid_closed(bits);
    stepper_send(stepperQ, STEPPER_CMD_CALIB_STOP, pdMS_TO_TICKS(100));

    oled_show_lines(oledQ, nullptr, "SYSTEM READY...", nullptr, nullptr, nullptr, nullptr, nullptr);
}

static void handle_sprouts_ready(QueueHandle_t oledQ)
{
    //printf("[CTRL] Sprouts ready\n");

    oled_show(oledQ, OLED_SHOW_READY);
    vTaskDelay(portMAX_DELAY);
}

static void go_to_auto_idle(system_state_t &state,
                            demo_phase_t &phase,
                            QueueHandle_t eventQ)
{
    phase = PH_RINSE;
    state = ST_AUTO_IDLE;
    StartPhaseTimerMs(eventQ, 1);
    StartDayTimer(eventQ, DEMO_DAY_MS);
}

static void handle_refill_tank(QueueHandle_t eventQ,QueueHandle_t ctrlToEepromQ,EventGroupHandle_t systemBits, QueueHandle_t stepperQ,QueueHandle_t oledQ,system_state_t &state,demo_phase_t &phase,int &day)
{
    xEventGroupClearBits(systemBits, BIT_SYSTEM_BUSY);
    oled_show(oledQ, OLED_SHOW_REFILL);

    controller_event_t event;

    while (1) {
        if (xQueueReceive(eventQ, &event, portMAX_DELAY) == pdPASS) {

            if (event.type == EVT_DAY_ALARM) {
                continue;
            }

            if (event.type == EVT_BTN_3) {
                if (xEventGroupGetBits(systemBits) & BIT_WATER_LOW) {

                    continue;
                }
                ensure_lid_closed(systemBits,stepperQ, oledQ, ctrlToEepromQ);

                set_day(day, day_saved, ctrlToEepromQ);

                xEventGroupClearBits(systemBits, BIT_STOP_PUMP);

                go_to_auto_idle(state, phase, eventQ);
                return;
            }
        }
    }
}

//============================================================================================

void ControllerTask(void *params){

    auto *p = (ControllerTaskParams *) params;
    EventGroupHandle_t SystemBits = p->SystemBits;
    QueueHandle_t ControllerEventQ = p->ControllerEventQ;
    QueueHandle_t ControllerToOledQ = p->ControllerToOledQ;
    QueueHandle_t ControllerToStepperQ = p->ControllerToStepperQ;
    QueueHandle_t ControllerToPumpQ = p->ControllerToPumpQ;
    QueueHandle_t ControllerToEepromQ = p->ControllerToEepromQ;
    QueueHandle_t ControllerToMqttQ = p->ControllerToMqttQ;

    static system_state_t state = ST_BOOT;
    demo_phase_t phase = PH_RINSE;

    int day = 0;
    int lid_pos = LID_UNKNOWN;
    wifi_ssid[0] = '\0';
    wifi_pass[0] = '\0';

    controller_event_t event;

    snprintf(current_status, sizeof(current_status), "BOOT");

    while(1) {
        switch (state) {
            case ST_BOOT:
            {
                //printf("[CTRL] Booting...\n");
                xEventGroupSetBits(SystemBits, BIT_SYSTEM_BUSY);
                TickType_t start = xTaskGetTickCount();
                const TickType_t timeout_total = pdMS_TO_TICKS(1000);

                while ((xTaskGetTickCount() - start) < timeout_total) {
                    if (xQueueReceive(ControllerEventQ, &event, pdMS_TO_TICKS(200)) == pdPASS) {

                        if (event.type == EVT_EEPROM_DATA) {

                            int day_tmp = event.data.eeprom.day;
                            int lid_tmp = event.data.eeprom.lid_pos;

                            if (day_tmp >= 0 && day_tmp <= 4 &&
                                (lid_tmp == LID_OPEN || lid_tmp == LID_CLOSED || lid_tmp == LID_UNKNOWN)) {

                                day = day_tmp;
                                lid_pos = lid_tmp;

                                snprintf(wifi_ssid, sizeof(wifi_ssid), "%s", event.data.eeprom.ssid);
                                snprintf(wifi_pass, sizeof(wifi_pass), "%s", event.data.eeprom.password);

                                eeprom_ok = true;

                               // printf("[CTRL] data fetched from eeprom\n");
                               // printf("[CTRL] DAY %d\n", day);
                               // printf("[CTRL] LID %d\n", lid_pos);
                               // printf("[CTRL] SSID %s\n", wifi_ssid);
                               // printf("[CTRL] PASS %s\n", wifi_pass);
                            }
                            else {
                                //printf("[CTRL] EEPROM data invalid\n");
                            }
                            break;
                        }
                    }
                }
                oled_show(ControllerToOledQ, OLED_SHOW_WELCOME);
                vTaskDelay(pdMS_TO_TICKS(2000));

                if (lid_pos == LID_UNKNOWN || lid_pos == LID_OPEN || lid_pos == LID_CLOSED) {  // calibrate anyway
                    state = ST_CALIBRATION_REQUIRED;
                    StartPhaseTimerMs(ControllerEventQ, 5000);
                    break;
                }
            }

            case ST_CALIBRATION_REQUIRED:
                xEventGroupSetBits(SystemBits, BIT_CALIBRATION_ACTIVE);
                run_lid_calibration(SystemBits, ControllerToOledQ, ControllerToStepperQ);
                xEventGroupClearBits(SystemBits, BIT_CALIBRATION_ACTIVE);
                //printf("[CTRL] Calibration done\n");
                state = ST_WIFI_CHOICE;
                StartPhaseTimerMs(ControllerEventQ, 5000);
                break;

            case ST_WIFI_CHOICE:
            {
                xEventGroupClearBits(SystemBits, BIT_SYSTEM_BUSY);
                oled_show_lines(ControllerToOledQ, "WiFi Setup", nullptr, "B1: Skip", "B2: Setup", nullptr, nullptr, nullptr);

                if (xQueueReceive(ControllerEventQ, &event, portMAX_DELAY) == pdPASS) {
                    switch(event.type){
                        case EVT_BTN_1:
                            if (wifi_ssid[0] != '\0') {
                                state = ST_WIFI_WAIT_CONNECT;
                            } else {
                                snprintf(wifi_status, sizeof(wifi_status), "WIFI- RESET");
                                go_to_auto_idle(state, phase, ControllerEventQ);
                            }
                            break;
                        case EVT_BTN_2:
                            state = ST_WIFI_SETUP;
                            break;

                        case EVT_BME_UPDATE:
                            update_bme_values(event);
                            break;
                        case EVT_PHASE_TIMEOUT:
                            if (wifi_ssid[0] != '\0') {
                                state = ST_WIFI_WAIT_CONNECT;
                            } else {
                                snprintf(wifi_status, sizeof(wifi_status), "WIFI- RESET");
                                go_to_auto_idle(state, phase, ControllerEventQ);
                            }
                    }
                }
            }
                break;

            case ST_WIFI_SETUP: //=============================================================
            {
                init_ascii_table_once();

                if (!wifi_inited) {
                    wifi_start(ControllerToOledQ);
                }

                if (xQueueReceive(ControllerEventQ, &event, portMAX_DELAY) == pdPASS) {

                    switch (event.type) {
                        case EVT_BME_UPDATE:
                            update_bme_values(event);
                            break;

                        case EVT_ENCODER_CW:
                            if (!wifi_editing) break;
                            wifi_current_char = ascii_step(wifi_current_char, +1);
                            {
                                wifi_setup(ControllerToOledQ);
                            }
                            break;

                        case EVT_ENCODER_CCW:
                            if (!wifi_editing) break;
                            wifi_current_char = ascii_step(wifi_current_char, -1);
                            {
                                wifi_setup(ControllerToOledQ);
                            }
                            break;
                        case EVT_BTN_1:   // = backspace
                        {
                            if (!wifi_editing) break;

                            char* buf = wifi_active_buf();
                            size_t max = wifi_active_max();

                            size_t len = strnlen(buf, max);
                            if (len > 0) {
                                buf[len - 1] = '\0';
                            }

                            wifi_setup(ControllerToOledQ);
                            break;
                        }

                        case EVT_ENC_BTN:
                        {
                            if (!wifi_editing) break;

                            char* buf = wifi_active_buf();
                            size_t max = wifi_active_max();
                            size_t len = strnlen(buf, max);
                            if (len < max) {
                                buf[len] = wifi_current_char;
                                buf[len + 1] = '\0';
                            }
                           // printf("[WIFI] SSID='%s' PASS='%s' cur='%c'\n", wifi_ssid, wifi_pass, wifi_current_char);
                            wifi_setup(ControllerToOledQ);
                            break;
                        }
                        case EVT_BTN_3:
                        {
                            if (!wifi_editing) break;
                            bool done = wifi_next_or_save(ControllerToOledQ);
                            if (!done) break;
                            //printf("[WIFI] SSID='%s' PASS='%s'\n", wifi_ssid, wifi_pass);
                            save_wifi_to_eeprom(wifi_ssid, wifi_pass, ControllerToEepromQ);
                            wifi_inited = false;
                            state = ST_WIFI_WAIT_CONNECT;
                            break;
                        }
                        default:
                            break;
                    }
                }
                break;
            }

            case ST_WIFI_WAIT_CONNECT: //==================================================================
            {
                xEventGroupSetBits(SystemBits, BIT_SYSTEM_BUSY);
                if (!wifi_connect_started) {
                    wifi_connect_started = true;

                    mqtt_cmd_t mcmd{};
                    mcmd.type = MQTT_CMD_CONNECT;
                    snprintf(mcmd.ssid, sizeof(mcmd.ssid), "%s", wifi_ssid);
                    snprintf(mcmd.pass, sizeof(mcmd.pass), "%s", wifi_pass);
                    xQueueSend(ControllerToMqttQ, &mcmd, 0);
                }

                oled_show_lines(ControllerToOledQ, "WiFi CONNECTING", nullptr, nullptr, "PLEASE WAIT...", nullptr, nullptr, nullptr);

                if (xQueueReceive(ControllerEventQ, &event, portMAX_DELAY) == pdPASS) {
                    switch (event.type) {

                        case EVT_WIFI_CONNECTED:
                            wifi_connect_started = false;
                            snprintf(wifi_status, sizeof(wifi_status), "WIFI OK");
                            go_to_auto_idle(state, phase, ControllerEventQ);
                            StartMqttTimer(ControllerEventQ, MQTT_PUBLISH_INTERVAL);
                            break;

                        case EVT_WIFI_CONNECT_FAILED:
                            wifi_connect_started = false;
                            snprintf(wifi_status, sizeof(wifi_status), "WIFI- RESET");
                            go_to_auto_idle(state, phase, ControllerEventQ);
                            break;

                        case EVT_BME_UPDATE:
                            update_bme_values(event);
                            break;

                        default:
                            break;
                    }
                }
                break;
            }

            case ST_AUTO_IDLE:  //=================================================================================
                if (day == 0) set_day(day, 1, ControllerToEepromQ);
                xEventGroupSetBits(SystemBits, BIT_SYSTEM_BUSY);
                if (xQueueReceive(ControllerEventQ, &event, portMAX_DELAY) == pdPASS) {
                    switch (event.type) {
                        case EVT_BME_UPDATE:
                            handle_bme_update(event, ControllerToOledQ, day, current_status, last_temp, last_rh,
                                              menu_active);
                            break;

                        case EVT_MQTT_PUBLISH_TIMER:
                            mqtt_send_status(ControllerToMqttQ, last_temp, last_rh, day);
                            break;

                        case EVT_WATER_LOW_BIT_SET:
                            handle_low_water_bit_set(ControllerToPumpQ, SystemBits, state, day);
                            break;

                        case EVT_PHASE_TIMEOUT:

                            if (phase == PH_RINSE) {
                                handle_phase_rinse(SystemBits, menu_active, current_status, sizeof(current_status),
                                                   ControllerToOledQ, day, last_temp, last_rh, ControllerToPumpQ, phase,
                                                   ControllerEventQ);

                            } else if (phase == PH_VENT) {
                                handle_phase_vent(SystemBits, current_status, sizeof(current_status), ControllerToOledQ, ControllerToEepromQ,
                                                  day, last_temp, last_rh, ControllerToStepperQ, phase,
                                                  ControllerEventQ);

                            } else if (phase == PH_FINISH_ROUTINE) {
                                handle_phase_finish_routine(SystemBits, current_status, sizeof(current_status),
                                                            ControllerToOledQ, ControllerToEepromQ,day, last_temp, last_rh,
                                                            ControllerToStepperQ, phase, ControllerEventQ);
                            } else if (phase == PH_INTERRUPTS_ALLOWED) {
                                handle_phase_interrupts_allowed(state,SystemBits,menu_active,menu_sel,current_status, sizeof(current_status),ControllerToOledQ,day,last_temp,last_rh,ControllerEventQ);
                                phase = PH_RINSE;
                            }

                            break;
                        case EVT_DAY_ALARM:
                            handle_day_alarm(SystemBits, menu_active, day, state, phase, current_status,
                                             sizeof(current_status), ControllerEventQ, ControllerToEepromQ);
                            break;

                    }
                }
                break;

            case ST_INTERRUPTS_ALLOWED://=====================================================================
                xEventGroupClearBits(SystemBits, BIT_SYSTEM_BUSY);
                if (xQueueReceive(ControllerEventQ, &event, pdMS_TO_TICKS(10)) == pdPASS) {
                    switch (event.type) {

                        case EVT_BME_UPDATE:
                            handle_bme_update(event, ControllerToOledQ, day, current_status, last_temp, last_rh, menu_active);
                            // in real system RINSING and VENTING actions will be taken automatically based on sensor's readings
                            break;

                        case EVT_MQTT_PUBLISH_TIMER:
                            mqtt_send_status(ControllerToMqttQ, last_temp, last_rh, day);
                            break;

                        case EVT_ENCODER_CW:
                            handle_encoder_turn(+1, SystemBits, menu_active, menu_sel,
                                                ControllerToOledQ, day, current_status, last_temp, last_rh);
                            break;

                        case EVT_ENCODER_CCW:
                            handle_encoder_turn(-1, SystemBits, menu_active, menu_sel,
                                                ControllerToOledQ, day, current_status, last_temp, last_rh);
                            break;
                        case EVT_BTN_1:
                           // printf("[CTRL] Simulating high T -> venting\n");
                            if (!menu_active) {
                                last_temp = SIMULATE_HIGH_T;
                                run_manual_action(2, SystemBits, ControllerToOledQ, ControllerToEepromQ,
                                                  ControllerToPumpQ, ControllerToStepperQ,
                                                  day, last_temp, last_rh, current_status, sizeof(current_status));
                            }
                            break;

                        case EVT_BTN_2:
                            //printf("[CTRL] Simulating low RH -> rinsing \n");
                            if (!menu_active) {
                                last_rh = SIMULATE_LOW_RH;
                                run_manual_action(1, SystemBits, ControllerToOledQ, ControllerToEepromQ,
                                                  ControllerToPumpQ, ControllerToStepperQ,
                                                  day, last_temp, last_rh, current_status, sizeof(current_status));
                            }
                            break;

                        case EVT_BTN_3:
                            if (!menu_active) {
                                menu_active = true;
                                menu_sel = 0;
                                last_menu_sec = -1;
                                last_menu_sel = 255;
                                oled_show_menu_with_countdown(ControllerToOledQ, day, last_temp, last_rh, menu_sel);
                            } else {
                                menu_active = false;
                                last_menu_sec = -1;
                                last_menu_sel = 255;
                                oled_show_auto_now(ControllerToOledQ, day, current_status, wifi_status, last_temp, last_rh);
                            }
                            break;
                        case EVT_ENC_BTN:
                            if (menu_active) {
                                menu_active = false;
                                oled_show_auto_now(ControllerToOledQ, day, current_status, wifi_status, last_temp, last_rh);

                                run_manual_action(menu_sel, SystemBits, ControllerToOledQ, ControllerToEepromQ,
                                                  ControllerToPumpQ, ControllerToStepperQ,
                                                  day, last_temp, last_rh, current_status, sizeof(current_status));
                            }
                            break;

                        case EVT_UI_WINDOW_TIMEOUT:
                            xEventGroupClearBits(SystemBits, BIT_USER_INPUT_ALLOWED);
                            state = ST_AUTO_IDLE;
                            menu_active = false;
                            menu_sel = 0;
                            last_menu_sec = -1;
                            last_menu_sel = 255;
                            snprintf(current_status, sizeof(current_status), "%s", "IDLE");
                            oled_show_auto_now(ControllerToOledQ, day, current_status,wifi_status, last_temp, last_rh);
                            break;

                        case EVT_DAY_ALARM:
                            menu_active = false;
                            last_menu_sec = -1;
                            last_menu_sel = 255;
                            handle_day_alarm(SystemBits, menu_active, day, state, phase,
                                             current_status, sizeof(current_status), ControllerEventQ, ControllerToEepromQ);
                            break;
                        case EVT_WATER_LOW_BIT_SET:
                            handle_low_water_bit_set(ControllerToPumpQ, SystemBits, state, day);
                            break;

                        default:
                            break;
                    }
                }

                if (menu_active) {
                    oled_show_menu_with_countdown(ControllerToOledQ, day, last_temp, last_rh, menu_sel);
                }
                break;

            case ST_SPROUTS_READY: //======================================================================
                handle_sprouts_ready(ControllerToOledQ);
                break;

            case ST_REFILL_TANK: //========================================================================
                handle_refill_tank(ControllerEventQ,ControllerToEepromQ, SystemBits, ControllerToStepperQ, ControllerToOledQ, state, phase, day);
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}