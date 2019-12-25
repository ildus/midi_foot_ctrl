#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/rtc_io.h"

#include "midi.h"

typedef struct
{
    gpio_num_t pin;
    button_num_t btn;
    xQueueHandle qu;
    volatile uint32_t ts;
} isr_context;

static void IRAM_ATTR gpio_isr_handler(void * arg)
{
    isr_context * ctx = arg;
    uint32_t last_ts = ctx->ts;
    ctx->ts = xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;

    if (last_ts != 0 && ctx->ts - last_ts <= 200)
        return;

    xQueueSendFromISR(ctx->qu, &ctx->btn, NULL);
}

static isr_context contexts[] = {
    {GPIO_NUM_15, TOP_LEFT, NULL, 0},
    {GPIO_NUM_17, TOP_CENTER, NULL, 0},
    {GPIO_NUM_18, TOP_RIGHT, NULL, 0},
    {GPIO_NUM_19, BOTTOM_LEFT, NULL, 0},
    {GPIO_NUM_21, BOTTOM_CENTER, NULL, 0},
    {GPIO_NUM_22, BOTTOM_RIGHT, NULL, 0},
};

static void gpio_handle_buttons(void * queue)
{
    while (true)
    {
        button_num_t btn;
        if (xQueueReceive(queue, &btn, portMAX_DELAY))
        {
            setup_unused_timer();
            trigger_button(btn);
        }
    }
}

static void idle_callback(TimerHandle_t arg)
{
    (void)arg;

    esp_wifi_stop();
    esp_bluedroid_disable();
    esp_bt_controller_disable();

    /* enable RTC, pull down G16, isolate G12 */
    gpio_isr_handler_remove(GPIO_NUM_15);
    gpio_uninstall_isr_service();
    rtc_gpio_isolate(GPIO_NUM_12);
    //gpio_set_direction(GPIO_NUM_15, GPIO_MODE_INPUT);
    gpio_pulldown_dis(GPIO_NUM_15);
    gpio_pullup_en(GPIO_NUM_15);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, 0);

    ESP_LOGI("power", "going to sleep");
    esp_deep_sleep_start();
}

void setup_unused_timer(void)
{
    static TimerHandle_t timer_handle = NULL;

    if (timer_handle)
    {
        ESP_LOGI("power", "timer postponed");
        xTimerReset(timer_handle, 0);
    }
    else
    {
        timer_handle = xTimerCreate("sleep if unused",
                pdMS_TO_TICKS(60000 * 15), pdFALSE, NULL, idle_callback);
        if (timer_handle)
            xTimerStart(timer_handle, 0);
    }
}

void init_gpio()
{
    gpio_config_t io_conf;

    /* init task */
    xQueueHandle qu = xQueueCreate(10, sizeof(button_num_t));
    xTaskCreate(gpio_handle_buttons, "handle_buttons", 2048, (void *)qu, 0, NULL);

    // set up pin mask
    memset(&io_conf, 0, sizeof(io_conf));
    for (size_t i = 0; i < N_BUTTONS; i++)
    {
        isr_context * ctx = &contexts[i];
        io_conf.pin_bit_mask |= 1ULL << ctx->pin;
    }

    //interrupt of low level
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    //io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;

    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    /* connect pins with corresponding action */
    for (size_t i = 0; i < N_BUTTONS; i++)
    {
        isr_context * ctx = &contexts[i];
        ctx->qu = qu;
        gpio_isr_handler_add(ctx->pin, gpio_isr_handler, (void *)ctx);
    }
    setup_unused_timer();
}
