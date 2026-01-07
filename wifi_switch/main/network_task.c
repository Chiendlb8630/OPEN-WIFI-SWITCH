#include "network_cmd.h"
#include "network_task.h"
#include "switch_task.h"

uint8_t g_netMac[6];
char    g_topic[32];
static  int s_retry_num = 0;
static const char* html_form =
    "<!DOCTYPE html><html><head>"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<style>"
    "body{font-family:Arial;background:#f2f2f2;display:flex;justify-content:center;align-items:center;height:100vh;margin:0}"
    ".c{background:#fff;padding:20px;border-radius:8px;box-shadow:0 0 10px rgba(0,0,0,0.1);width:300px}"
    "input{width:100%;padding:10px;margin:5px 0 15px 0;border:1px solid #ccc;border-radius:4px;box-sizing:border-box}"
    "input[type=submit]{background:#4CAF50;color:white;border:none;cursor:pointer;font-weight:bold}"
    "</style></head>"
    "<body><div class=\"c\">"
    "<h2 style=\"text-align:center\">OPEN SWITCH WIFI CONFIG</h2>"
    "<form action=\"/connect\" method=\"post\">"
    "SSID:<br><input type=\"text\" name=\"ssid\" placeholder=\"WiFi Name\">"
    "Pass:<br><input type=\"password\" name=\"password\" placeholder=\"Password\"><br>"
    "<input type=\"submit\" value=\"Connect\">"
    "</form></div></body></html>";

static  EventGroupHandle_t s_wifi_event_group;
static httpd_handle_t    softAp_server = NULL;
esp_mqtt_client_handle_t mqtt_client;
static void mqtt_app_start(void);
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void netAp_disable(void * pv);

void replace_char(char *s, char from, char to) {
    while (*s) {
        if (*s == from) {
            *s = to;
        }
        s++;
    }
}

void save_wifi_credentials(const char *ssid, const char *password) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return;
    }
    err = nvs_set_str(my_handle, "ssid", ssid);
    if (err == ESP_OK) err = nvs_set_str(my_handle, "pass", password);
    if (err == ESP_OK) err = nvs_commit(my_handle);
    nvs_close(my_handle);
    ESP_LOGI("NVS", "WiFi Credentials Saved to Flash!");
}

bool load_wifi_credentials(char *ssid, char *password) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err != ESP_OK) return false;

    size_t ssid_len = 32;
    size_t pass_len = 64;

    err = nvs_get_str(my_handle, "ssid", ssid, &ssid_len);
    if (err == ESP_OK) {
        err = nvs_get_str(my_handle, "pass", password, &pass_len);
    }

    nvs_close(my_handle);
    return (err == ESP_OK);
}

void net_switch_sta(const char * ssid, const char *pass){
    ESP_LOGI(WIFI_TAG, "SWITCH TO STA");
    wifi_config_t wifi_config;
     memset(&wifi_config, 0, sizeof(wifi_config));
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';

    strncpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0'; // Đảm bảo null termination
    printf("%s\n", wifi_config.sta.ssid);
    printf("%s\n", wifi_config.sta.password);

    ESP_LOGI(WIFI_TAG, " SWITCH TO STA");
    ESP_LOGI(WIFI_TAG, "SSID :%s", ssid);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_MODE_STA ,g_netMac));
}

esp_err_t apRoot_get_handler(httpd_req_t *req) {
    httpd_resp_send(req, html_form, strlen(html_form));
    return ESP_OK;
}

esp_err_t ap_post_handler(httpd_req_t *req){
    char buf[100];
    int ret, remaining = req->content_len;
    if (remaining >= sizeof(buf)) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    char ssid[32] = {0};
    char password[64] = {0};
    char *ssid_ptr = strstr(buf, "ssid=");
    char *pass_ptr = strstr(buf, "&password=");

    if (ssid_ptr && pass_ptr) {
        ssid_ptr += 5;
        int ssid_len = pass_ptr - ssid_ptr;
        if(ssid_len > 31) ssid_len = 31;
        strncpy(ssid, ssid_ptr, ssid_len);

        pass_ptr += 10;
        strncpy(password, pass_ptr, 63);
        char* resp_str= "Connecting... Check device logs.";
        httpd_resp_send(req, resp_str, strlen(resp_str));
        xTaskCreate(netAp_disable, "netAp_disable", 2048, NULL, 5, NULL);
        replace_char(ssid, '+',' ');
        net_switch_sta(ssid, password);
    } else {
        httpd_resp_send_500(req);
    }
    return ESP_OK;
}

void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    httpd_uri_t root = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = apRoot_get_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t connect_uri = {
        .uri       = "/connect",
        .method    = HTTP_POST,
        .handler   = ap_post_handler,
        .user_ctx  = NULL
    };

    ESP_LOGI(WIFI_TAG, "Starting web server on port: '%d'", config.server_port);
    if (httpd_start(&softAp_server, &config) == ESP_OK) {
        httpd_register_uri_handler(softAp_server, &root);
        httpd_register_uri_handler(softAp_server, &connect_uri);
    }
}

// void wifi_init_softap_provisioning(void)
// {
//     s_wifi_event_group = xEventGroupCreate();
//     tcpip_adapter_init();
//     ESP_ERROR_CHECK(esp_event_loop_create_default());
    
//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
//     // Đăng ký event handler
//     ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
//     ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

//     // 2. KIỂM TRA FLASH XEM CÓ WIFI CHƯA
//     char ssid_buff[32] = {0};
//     char pass_buff[64] = {0};
//     bool has_config = load_wifi_credentials(ssid_buff, pass_buff);

//     if (has_config) {
//         // --- CASE A: ĐÃ CÓ CẤU HÌNH TRONG FLASH -> CHẠY STA ---
//         ESP_LOGI(WIFI_TAG, "Found config in Flash. Connecting to SSID: %s", ssid_buff);
        
//         // Tái sử dụng logic của hàm net_switch_sta nhưng viết trực tiếp để an toàn
//         wifi_config_t wifi_config;
//         memset(&wifi_config, 0, sizeof(wifi_config));
//         strncpy((char *)wifi_config.sta.ssid, ssid_buff, sizeof(wifi_config.sta.ssid));
//         strncpy((char *)wifi_config.sta.password, pass_buff, sizeof(wifi_config.sta.password));

//         ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//         ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
//         ESP_ERROR_CHECK(esp_wifi_start());
//         // esp_wifi_connect() sẽ được gọi trong event handler WIFI_EVENT_STA_START
//         ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_MODE_STA ,g_netMac));
//     } 
//     else {
//         // --- CASE B: CHƯA CÓ CẤU HÌNH -> CHẠY SOFTAP (PROVISIONING) ---
//         ESP_LOGI(WIFI_TAG, "No config found. Starting SoftAP for provisioning...");

//         wifi_config_t wifi_ap_config = {
//             .ap = {
//                 .ssid = "OPEN SWITCH",
//                 .ssid_len = strlen("OPEN SWITCH"),
//                 .password = "12345678",
//                 .max_connection = 2,
//                 .authmode = WIFI_AUTH_WPA_WPA2_PSK
//             },
//         };

//         if (strlen((char *)wifi_ap_config.ap.password) == 0) {
//             wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
//         }
//         ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
//         ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_ap_config));
//         ESP_ERROR_CHECK(esp_wifi_start());
        
//         ESP_LOGI(WIFI_TAG, "SoftAP started. SSID: %s", wifi_ap_config.ap.ssid);
//         start_webserver();
//     }
// }


void wifi_start_logic(void)
{
    esp_wifi_stop();

    char ssid_buff[32] = {0};
    char pass_buff[64] = {0};
    bool has_config = load_wifi_credentials(ssid_buff, pass_buff);

    if (has_config) {
        ESP_LOGI(WIFI_TAG, "Load from NVS. Connecting to %s...", ssid_buff);

        wifi_config_t wifi_config;
        memset(&wifi_config, 0, sizeof(wifi_config));
        strncpy((char *)wifi_config.sta.ssid, ssid_buff, sizeof(wifi_config.sta.ssid));
        strncpy((char *)wifi_config.sta.password, pass_buff, sizeof(wifi_config.sta.password));

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_MODE_STA ,g_netMac));
    }
    else {
        ESP_LOGI(WIFI_TAG, "No config. Starting SoftAP...");
        wifi_config_t wifi_ap_config = {
            .ap = {
                .ssid = "OPEN SWITCH",
                .ssid_len = strlen("OPEN SWITCH"),
                .password = "12345678",
                .max_connection = 2,
                .authmode = WIFI_AUTH_WPA_WPA2_PSK
            },
        };
        if (strlen((char *)wifi_ap_config.ap.password) == 0) {
            wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
        }

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_ap_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        start_webserver();
    }
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED){
        if (s_retry_num < 5) { // Ví dụ max retry = 5
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(WIFI_TAG, "Retry connect... (%d)", s_retry_num);
        } else {
            ESP_LOGW(WIFI_TAG, "Connect failed! Switching to SoftAP mode...");
            s_retry_num = 0;
            nvs_handle_t my_handle;
            if (nvs_open("storage", NVS_READWRITE, &my_handle) == ESP_OK) {
                nvs_erase_key(my_handle, "ssid");
                nvs_erase_key(my_handle, "pass");
                nvs_commit(my_handle);
                nvs_close(my_handle);
            }
            wifi_start_logic();
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_TAG, "got ip:%s", ip4addr_ntoa(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        // --- ĐOẠN MỚI THÊM: LƯU VÀO FLASH ---
        wifi_config_t conf;
        // Lấy config hiện tại đang kết nối thành công
        esp_wifi_get_config(ESP_IF_WIFI_STA, &conf);
        // Kiểm tra xem trong Flash đã lưu đúng cái này chưa, nếu chưa thì lưu
        char saved_ssid[32] = {0};
        char saved_pass[64] = {0};
        bool exist = load_wifi_credentials(saved_ssid, saved_pass);
        // Nếu chưa tồn tại hoặc khác với cái mới -> Lưu lại
        if (!exist || strcmp((char*)conf.sta.ssid, saved_ssid) != 0 || strcmp((char*)conf.sta.password, saved_pass) != 0) {
            save_wifi_credentials((char*)conf.sta.ssid, (char*)conf.sta.password);
        }
        // ------------------------------------

        mqtt_app_start();
    }
}

// void wifi_init_sta(void)
// {
//     s_wifi_event_group = xEventGroupCreate();
//     tcpip_adapter_init();
//     ESP_ERROR_CHECK(esp_event_loop_create_default());
//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));
//     ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
//     ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

//     wifi_config_t wifi_config = {
//         .sta = {
//             .ssid     = EXAMPLE_ESP_WIFI_SSID,
//             .password = EXAMPLE_ESP_WIFI_PASS
//         },
//     };
//     if (strlen((char *)wifi_config.sta.password)) {
//         wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
//     }
//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//     ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_MODE_STA ,g_netMac));
//     ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
//     ESP_ERROR_CHECK(esp_wifi_start());
//     ESP_LOGI(WIFI_TAG, "wifi_init_sta finished.");
//     EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
//             WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
//             pdFALSE,
//             pdFALSE,
//             portMAX_DELAY);

//     if (bits & WIFI_CONNECTED_BIT) {
//         ESP_LOGI(WIFI_TAG, "connected to ap SSID:%s password:%s",
//                  EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
//     } else if (bits & WIFI_FAIL_BIT) {
//         ESP_LOGI(WIFI_TAG, "Failed to connect to SSID:%s, password:%s",
//                  EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
//     } else {
//         ESP_LOGE(WIFI_TAG, "UNEXPECTED EVENT");
//     }

//     ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler));
//     ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,  &event_handler));
//     vEventGroupDelete(s_wifi_event_group);
// }

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_CONNECTED");
            cmd_pub_staData(0, output_state[0]);
            cmd_pub_staData(1, output_state[1]);
            cmd_pub_staData(2, output_state[2]);
            msg_id = esp_mqtt_client_subscribe(client, cmd_macToTopic() , 1);
            ESP_LOGI(MQTT_TAG, g_topic);
            ESP_LOGI(MQTT_TAG, "Sub succcess to Mac topic %s , msg_id=%d",g_topic, msg_id);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            // ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DATA");
            // printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            cmd_parse_ctrlData(event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_ESP_TLS) {
                ESP_LOGI(MQTT_TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
                ESP_LOGI(MQTT_TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGI(MQTT_TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
            } else {
                ESP_LOGW(MQTT_TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
            }
            break;
        default:
            ESP_LOGI(MQTT_TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(MQTT_TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

static void mqtt_app_start(void)
{
    if (mqtt_client != NULL) {
        ESP_LOGI(MQTT_TAG, "MQTT Client already initialized. Skipping...");
        return;
    }


    const esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_BROKER_URI,
        .cert_pem = NULL,
        .username = BROKER_ACCOUNT,
        .password = BROKER_PASS,
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, mqtt_client);
    esp_mqtt_client_start(mqtt_client);
}

void netAp_disable(void * pv){
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    if (softAp_server) {
        httpd_stop(softAp_server);
        softAp_server = NULL;
        ESP_LOGI(WIFI_TAG,"Webserver stopped successfully");
    }
    vTaskDelete(NULL);
}

void wifi_system_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Init Network Stack (CHỈ 1 LẦN)
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 3. Init Wifi Driver (CHỈ 1 LẦN)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 4. Đăng ký Handler (CHỈ 1 LẦN)
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
    
    s_wifi_event_group = xEventGroupCreate();
}