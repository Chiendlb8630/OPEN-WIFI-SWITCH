#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "mqtt_client.h"
#include "protocol_examples_common.h"
#include "network_task.h"
#include "network_cmd.h"
#include "esp_http_server.h"

#define  WIFI_TAG "WIFI"
#define  MQTT_TAG "MQTT"

#define  SOFTAP_SSID      "OPEN_SWITCH"
#define  SOFTAP_PASSWORD  "kvt1ptit"
#define BROKER_ACCOUNT    "Chiendlb"
#define BROKER_PASS       "Chiendlb456"

#define EXAMPLE_ESP_WIFI_SSID   "IoT LAB"
#define EXAMPLE_ESP_WIFI_PASS   "kvt1ptit"

#define EXAMPLE_ESP_MAXIMUM_RETRY  7
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

extern uint8_t g_netMac[6];
extern char g_topic[32];
extern esp_mqtt_client_handle_t mqtt_client;

void wifi_init_sta(void);
void wifi_init_softap_provisioning(void);
void wifi_system_init(void);
void wifi_start_logic();