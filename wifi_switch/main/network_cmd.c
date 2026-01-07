#include "cJSON.h"
#include <stdio.h>
#include "network_cmd.h"
#include "switch_task.h"
#include "network_task.h"

const char *channel_keys[] = {"ch1_status", "ch2_status", "ch3_status"};

char* cmd_macToTopic(void) {
    if (g_netMac == NULL || g_topic == NULL) {
        ESP_LOGE(MQTT_TAG, "Lỗi: Con trỏ MAC hoặc Topic NULL.");
        return NULL;
    }
    strcpy(g_topic, ROOT_TOPIC);
    sprintf(g_topic + ROOT_TOPIC_LEN,
            "%02X%02X%02X%02X%02X%02X",
            g_netMac[0], g_netMac[1], g_netMac[2],
            g_netMac[3], g_netMac[4], g_netMac[5]);
    return g_topic;
}

void cmd_parse_ctrlData(int data_len, const char *data) {
    if (!data || data_len <= 0) {
        ESP_LOGE(MQTT_TAG, "Data is not valid");
        return;
    }
    char *json_string = strndup(data, data_len);
    if (!json_string) {
        ESP_LOGE(MQTT_TAG, "Error malloc");
        return;
    }
    cJSON *root = cJSON_Parse(json_string);
    free(json_string);

    if (root == NULL) {
        ESP_LOGE(MQTT_TAG, "Json parse error");
        if (cJSON_GetErrorPtr() != NULL) {
            ESP_LOGE(MQTT_TAG, "Json error: %s", cJSON_GetErrorPtr());
        }
        return;
    }
    for (int i = 0; i < 3; i++) {
        cJSON *channel_item = cJSON_GetObjectItemCaseSensitive(root, channel_keys[i]);
        if (cJSON_IsNumber(channel_item)) {
            int state = channel_item->valueint;
            if (state == 0 || state == 1) {
                set_output_state(i, state);
            } else {
                ESP_LOGW(MQTT_TAG, "%s có giá trị không hợp lệ: %d", channel_keys[i], state);
            }
        }
    }
    cJSON_Delete(root);
}

void cmd_pub_staData(int i, int status) {
    const char *key_name = NULL;
    switch (i) {
        case 0: key_name = channel_keys[0]; break;
        case 1: key_name = channel_keys[1]; break;
        case 2: key_name = channel_keys[2]; break;
        default: return;
    }
    if (mqtt_client == NULL) return;
    char payload[64];
    int len = snprintf(payload, sizeof(payload), "{\"%s\":%d}", key_name, status);
    if (len > 0 && len < sizeof(payload)) {
        int msg_id = esp_mqtt_client_publish(mqtt_client, g_topic, payload, 0, 1, 0);
        ESP_LOGI(MQTT_TAG, "Pub OK: %s", payload);
    } else {
        ESP_LOGE(MQTT_TAG, "JSON format failed");
    }
}