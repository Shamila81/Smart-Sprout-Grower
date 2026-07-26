#include "FreeRTOS.h"
#include <stdio.h>
#include "oled_task.h"
#include "display/ssd1306os.h"
#include "queue.h"
#include "general/data.h"
#include "event_groups.h"
#include "general/bitmaps.h"

static void display_welcome(ssd1306os &oled)
{
    oled.fill(0);

    oled.text("SMART GROWER", 10, 0);
    oled.text("Starting...", 20, 17);

    mono_vlsb leaf(leaf_bitmap, leaf_w, leaf_h);
    oled.blit(leaf, 40, 27);

    oled.show();
}

static void display_data(ssd1306os &oled, const oled_cmd_t &m, EventGroupHandle_t SystemBits)
{
    oled.fill(0);

    char b1[24];
    char b2[24];
    char b3[24];
    char b4[24];

    oled.text(m.data.system_data.status, 0, 0);

    snprintf(b1, sizeof(b1), "T:%.1fC",  m.data.system_data.temp_c);
    snprintf(b2, sizeof(b2), "RH:%.1f%%", m.data.system_data.rh);
    snprintf(b3, sizeof(b3), "DAY:%d",    m.data.system_data.day);
    snprintf(b4, sizeof(b4), "%s",        m.data.system_data.wifi);

    oled.text(b3, 80, 0);
    oled.text(b1, 0, 16);
    oled.text(b2, 0, 26);
    oled.text(b4, 0, 36);


    if (xEventGroupGetBits(SystemBits) & BIT_USER_INPUT_ALLOWED) {
        oled.text("B3-> MENU", 0, 50);
    }

    oled.show();
}

static void display_text(ssd1306os &oled, const oled_cmd_t &m)
{
    oled.fill(0);
    oled.text(m.data.text.line1, 0, 0);
    oled.text(m.data.text.line2, 0, 20);
    oled.text(m.data.text.line3, 0, 30);
    oled.text(m.data.text.line4, 0, 40);
    oled.text(m.data.text.line5, 0, 48);
    oled.text(m.data.text.line6, 0, 56);
    oled.text(m.data.text.line7, 0, 64);
    oled.show();
}

static void display_ready(ssd1306os &oled){
    oled.fill(0);

    oled.text("SMART GROWER", 10, 0);
    oled.text("SPROUTS READY", 10, 17);

    mono_vlsb leaf(sprout_bitmap, leaf_w, leaf_h);
    oled.blit(leaf, 40, 27);

    oled.show();
}

static void display_warning(ssd1306os &oled){
    oled.fill(0);

    oled.text("!REFILL TANK!", 10, 0);
    oled.text("after refill", 0, 44);
    oled.text("PRESS B3", 0, 54);

    oled.show();
}

static void display_menu(ssd1306os &oled, const oled_cmd_t &cmd){
    oled.fill(0);

    oled.text("MENU", 40, 0);
    oled.text("CHOOSE: ENC + EB", 0, 56);

    // Menu items
    const char *items[3] = {"IDLE", "RINSE", "VENT"};
    int y0 = 20;
    for (int i = 0; i < 3; i++) {
        if (cmd.data.menu.sel == i) oled.text("->", 0, y0 + i*10);
        oled.text(items[i], 16, y0 + i*10);
    }
    oled.text(cmd.data.menu.status, 0, 0);
    oled.show();
}


void OledTask(void *params) {
    auto *p = (OledTaskParams*)params;
    auto ControllerToOledQ = p->ControllerToOledQ;
    EventGroupHandle_t SystemBits = p->SystemBits;

    auto i2cbus = p->bus;

    ssd1306os oled(i2cbus);


    while(1){
        oled_cmd_t ocmd{};
        if(xQueueReceive(ControllerToOledQ, &ocmd, portMAX_DELAY) == pdPASS){
            switch(ocmd.type){
                case OLED_SHOW_WELCOME:
                    display_welcome(oled);
                    break;

                case OLED_SHOW_AUTO:
                    display_data(oled, ocmd, SystemBits);

                    break;
                case OLED_SHOW_MENU: {
                    display_menu(oled, ocmd);
                } break;
                case OLED_SHOW_TEXT:
                    display_text(oled, ocmd);
                    break;
                case OLED_SHOW_READY:
                    display_ready(oled);
                    break;
                case OLED_SHOW_REFILL:
                    display_warning(oled);
                    break;
                default:
                    break;
                
            }
        }
    }
}