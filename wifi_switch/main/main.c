#include "switch_task.h"
#include "network_task.h"

void app_main(void)
{
    ESP_LOGI("BOOT", "Waiting for power stabilization...");
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    gpio_init();
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_system_init();
    wifi_start_logic();
    xTaskCreate(gpio_polling_task, "gpio_polling_task", 4096, NULL, 5, NULL);
    ESP_LOGI(SWITCH_TAG, "System initialized. LED control active via Polling.");
    ESP_LOGI(WIFI_TAG, "ESP wifi init success");
}
