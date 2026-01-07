#pragma once

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "network_task.h"
#include "switch_task.h"

static const char *SWITCH_TAG = "APP_SWITCH";

#define LED1   GPIO_NUM_14
#define LED2   GPIO_NUM_12
#define LED3   GPIO_NUM_13
#define LEDstt GPIO_NUM_16

#define BTN1   GPIO_NUM_5
#define BTN2   GPIO_NUM_4
#define BTN3   GPIO_NUM_0

typedef struct {
    int id;
    gpio_num_t btn_pin;
    gpio_num_t led_pin;
} control_pair_t;

extern volatile int  output_state[3];
extern volatile int  last_btn_level[3];
extern const control_pair_t control_pairs[3];

#define OUTPUT_PIN_SEL ((1ULL << LED1) | (1ULL << LED2) | (1ULL << LED3))
#define INPUT_PIN_SEL  ((1ULL << BTN1) | (1ULL << BTN2) | (1ULL << BTN3))

void gpio_init(void);
void gpio_polling_task(void *arg);
void set_output_state(int idx, int new_level);

