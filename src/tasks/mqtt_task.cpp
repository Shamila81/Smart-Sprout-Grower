#include "mqtt_task.h"
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "general/data.h"
#include "IPStack.h"
#include "mqtt/Countdown.h"
#include "mqtt/MQTTClient.h"

static IPStack *ipstack = nullptr;
static MQTT::Client<IPStack, Countdown> *client = nullptr;

void MqttTask(void *params)
{
    auto *p = (MqttTaskParams *) params;
    QueueHandle_t ControllerEventQ = p->ControllerEventQ;
    EventGroupHandle_t SystemBits = p->SystemBits;
    QueueHandle_t ControllerToMqttQ = p->ControllerToMqttQ;

    (void)SystemBits;

    bool mqtt_connected = false;
    mqtt_cmd_t cmd{};

    while (1)
    {
        if (xQueueReceive(ControllerToMqttQ, &cmd, pdMS_TO_TICKS(5000)) == pdPASS) {

            switch (cmd.type) {
                case MQTT_CMD_CONNECT:
                {
                    //printf("[MQTT] CONNECT command received\n");
                    //printf("[MQTT] SSID: %s\n", cmd.ssid);
                    //printf("[MQTT] PASS: %s\n", cmd.pass);

                    if (ipstack == nullptr) {
                        ipstack = new IPStack(cmd.ssid, cmd.pass);
                    }

                    if (client == nullptr) {
                        client = new MQTT::Client<IPStack, Countdown>(*ipstack);
                    }

                    int attempts = 0;
                    int rc = -1;

                    while (attempts < 2 && !mqtt_connected) {

                        //printf("[MQTT] attempt %d\n", attempts + 1);
                        //printf("[MQTT] connecting TCP...\n");

                        rc = ipstack->connect("10.155.126.18", 1883);
                        //printf("[MQTT] TCP rc=%d\n", rc);

                        if (rc != 0) {
                            attempts++;
                            vTaskDelay(pdMS_TO_TICKS(500));
                            continue;
                        }

                        MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
                        data.MQTTVersion = 4;
                        data.clientID.cstring = (char *)"sprout_controller";

                        rc = client->connect(data);
                        //printf("[MQTT] MQTT connect rc=%d\n", rc);

                        if (rc == 0) {
                            //printf("[MQTT] MQTT connected\n");
                            mqtt_connected = true;

                            controller_event_t event;
                            event.type = EVT_WIFI_CONNECTED;
                            xQueueSend(ControllerEventQ, &event, portMAX_DELAY);
                            break;
                        }
                        attempts++;
                        vTaskDelay(pdMS_TO_TICKS(500));
                    }

                    if (!mqtt_connected) {
                       // printf("[MQTT] failed after 2 attempts\n");
                        controller_event_t event;
                        event.type = EVT_WIFI_CONNECT_FAILED;
                        xQueueSend(ControllerEventQ, &event, portMAX_DELAY);
                    }

                    break;
                }

                case MQTT_CMD_PUBLISH:
                {
                    if (!mqtt_connected || client == nullptr) {
                        //printf("[MQTT] publish skipped, not connected\n");
                        break;
                    }

                    MQTT::Message msg{};
                    msg.qos = MQTT::QOS0;
                    msg.retained = false;
                    msg.dup = false;
                    msg.payload = (void *)cmd.payload;
                    msg.payloadlen = strlen(cmd.payload);

                    int rc = client->publish(cmd.topic, msg);
                    //printf("[MQTT] publish rc=%d topic=%s payload=%s\n", rc, cmd.topic, cmd.payload);

                    break;
                }

                default:
                    //printf("[MQTT] unknown command\n");
                    break;
            }

        } else {
            if (mqtt_connected && client != nullptr) {
                int rc = client->yield(100);
                if (rc != 0) {
                    //printf("[MQTT] rc=%d\n", rc);
                    mqtt_connected = false;
                }
            } else {
                //printf("[MQTT] alive\n");
            }
        }
    }
}