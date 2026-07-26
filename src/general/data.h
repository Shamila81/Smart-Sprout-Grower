#pragma once
#include "stdint.h"
#include "event_groups.h"

constexpr size_t WIFI_SSID_MAX = 32;
constexpr size_t WIFI_PASS_MAX = 32;
constexpr size_t WIFI_SSID_BUF = WIFI_SSID_MAX + 1;
constexpr size_t WIFI_PASS_BUF = WIFI_PASS_MAX + 1;
constexpr size_t EEPROM_SSID_LEN = WIFI_SSID_BUF;
constexpr size_t EEPROM_PASS_LEN = WIFI_PASS_BUF;

constexpr EventBits_t BIT_STOP_PUMP = (1u << 0);
constexpr EventBits_t BIT_WATER_LOW = (1u << 1);
constexpr EventBits_t BIT_LID_OPEN = (1u << 2);
constexpr EventBits_t BIT_LID_CLOSED = (1u << 3);
constexpr EventBits_t BIT_RINSE_DONE = (1u << 4);
constexpr EventBits_t BIT_USER_INPUT_ALLOWED = (1u << 5);
constexpr EventBits_t BIT_SYSTEM_BUSY = (1u << 6);
constexpr EventBits_t BIT_CALIBRATION_ACTIVE = (1u << 7);


typedef enum {
    EVT_BME_UPDATE,
    EVT_PHASE_TIMEOUT,
    EVT_DAY_ALARM,
    EVT_WATER_LOW_BIT_SET,
    EVT_ENCODER_CW,
    EVT_ENCODER_CCW,
    EVT_UI_WINDOW_TIMEOUT,
    EVT_BTN_1,
    EVT_BTN_2,
    EVT_BTN_3,
    EVT_ENC_BTN,
    EVT_EEPROM_DATA,
    EVT_MQTT_PUBLISH_TIMER,
    EVT_WIFI_CONNECTED,
    EVT_WIFI_CONNECT_FAILED,
} event_type_t;


typedef enum {
    EE_CMD_SAVE_DAY,
    EE_CMD_SAVE_LID,
    EE_CMD_SAVE_WIFI,

} ee_cmd_t;


typedef struct {
    ee_cmd_t cmd;
    uint16_t day;
    uint16_t lid_pos;
    char ssid[WIFI_SSID_BUF];
    char password[WIFI_PASS_BUF];
} ee_msg_t;


typedef enum {
    ST_BOOT,
    ST_CALIBRATION_REQUIRED,
    ST_AUTO_IDLE,
    ST_INTERRUPTS_ALLOWED,
    ST_SPROUTS_READY,
    ST_REFILL_TANK,
    ST_WIFI_CHOICE,
    ST_WIFI_SETUP,
    ST_WIFI_WAIT_CONNECT,
} system_state_t;


typedef struct {
    event_type_t type;
    union{
        struct { float temp_c; float rh;} bme;
        struct { uint8_t id; } button;
        struct {int day; uint16_t lid_pos;  char ssid[EEPROM_SSID_LEN]; char password[EEPROM_PASS_LEN];} eeprom;
    }data;
}controller_event_t;


enum class PumpCMD{
    PUMP_RUN,
    PUMP_STOP,
};


struct Command {
    PumpCMD type;
    uint32_t duration_ms;
};


typedef enum {
    OLED_SHOW_WELCOME,
    OLED_SHOW_AUTO,
    OLED_SHOW_MENU,
    OLED_SHOW_TEXT,
    OLED_SHOW_READY,
    OLED_SHOW_REFILL,
} oled_cmd_type_t;


typedef struct {
    oled_cmd_type_t type;
    union {
        struct { float temp_c; float rh; int day;  char status[16]; char wifi[12];} system_data;
        struct { char line1[21]; char line2[21]; char line3[21]; char line4[21];char line5[21];char line6[21];char line7[21];} text;
        struct { float temp_c; float rh; int day;  char status[16]; char wifi[12]; uint8_t sel; } menu;
    } data;
} oled_cmd_t;


typedef enum {
    STEPPER_CMD_CALIB_START,
    STEPPER_CMD_CALIB_STOP,
    STEPPER_OPEN,
    STEPPER_CLOSE

} stepper_cmd_type_t;


typedef struct {
    stepper_cmd_type_t type;
} stepper_cmd_t;


typedef enum {
    PH_RINSE,
    PH_VENT,
    PH_FINISH_ROUTINE,
    PH_INTERRUPTS_ALLOWED
} demo_phase_t;


typedef enum {
    WIFI_ENTER_SSID,
    WIFI_ENTER_PASS
} wifi_phase_t;


typedef enum {
    MQTT_CMD_CONNECT = 0,
    MQTT_CMD_PUBLISH
} mqtt_cmd_type_t;


typedef struct {
    mqtt_cmd_type_t type;
    char ssid[WIFI_SSID_BUF];
    char pass[WIFI_PASS_BUF];
    char topic[64];
    char payload[128];
} mqtt_cmd_t;