#include"switch_task.h"
#include"network_task.h"

const control_pair_t control_pairs[3] = {
    {0, BTN1, LED1},
    {1, BTN2, LED2},
    {2, BTN3, LED3}
};

volatile int output_state[3] =   {0, 0, 0};
volatile int last_btn_level[3] = {0, 0, 0};

void set_output_state(int idx, int new_level) {
    output_state[idx] = new_level;
    gpio_set_level(control_pairs[idx].led_pin, new_level);
    // ESP_LOGI(SWITCH_TAG, "BTN%d pressed. LED%d set to: %s", idx + 1, (int)control_pairs[idx].led_pin, new_level ? "ON (HIGH)" : "OFF (LOW)");
}

void gpio_polling_task(void *arg)
{
    const TickType_t poll_delay = pdMS_TO_TICKS(10);
    static int btn3_hold_counter = 0;
    const int HOLD_TIME_5S = 500; // 500 * 10ms = 5000ms = 5s

    while(1){
        for (int i = 0; i < 3; i++) {
            gpio_num_t current_btn = control_pairs[i].btn_pin;
            int current_level = gpio_get_level(current_btn);

            if (current_level == 0 && last_btn_level[i] == 1) {
                int new_level = !output_state[i];
                set_output_state(i, new_level);
                cmd_pub_staData(i,  new_level);
                last_btn_level[i] = 0;
            }
            else if (current_level == 1 && last_btn_level[i] == 0) {
                last_btn_level[i] = 1;
                if (i == 2) {
                    btn3_hold_counter = 0;
                }
            }
            if (i == 2) {
                if (current_level == 0) { // Nếu nút đang được giữ (mức 0)
                    btn3_hold_counter++;
                    if (btn3_hold_counter >= HOLD_TIME_5S) {
                        ESP_LOGI("SWITCH_TAG", "BTN3 Held for 5s. Starting AP Mode...");
                        btn3_hold_counter = 0; 
                        for(int k=0; k<3; k++){
                            gpio_set_level(control_pairs[2].led_pin, 0);
                            vTaskDelay(pdMS_TO_TICKS(100));
                            gpio_set_level(control_pairs[2].led_pin, 1);
                            vTaskDelay(pdMS_TO_TICKS(100));
                        }

                        nvs_handle_t my_handle;
                        if (nvs_open("storage", NVS_READWRITE, &my_handle) == ESP_OK) {
                        nvs_erase_key(my_handle, "ssid");
                        nvs_erase_key(my_handle, "pass");
                        nvs_commit(my_handle);
                        nvs_close(my_handle);
                        }
                        wifi_start_logic();
                        esp_restart();
                    }
                } else {
                    btn3_hold_counter = 0;
                }
            }
        }
        vTaskDelay(poll_delay);
    }
}

void gpio_init(void){
    gpio_config_t io_conf_out = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = OUTPUT_PIN_SEL | (1ULL << LEDstt),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };

    gpio_set_level(LEDstt, 1);
    gpio_config_t io_conf_in = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = INPUT_PIN_SEL,
        .pull_up_en = 0,
        .pull_down_en = 1,
    };
    gpio_config(&io_conf_out);
    gpio_config(&io_conf_in);
}
