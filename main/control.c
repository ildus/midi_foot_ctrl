#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "midi.h"

typedef struct
{
    gpio_num_t pin;
    button_num_t btn;
    xQueueHandle qu;
    uint32_t    ts;
} isr_context;

static void IRAM_ATTR gpio_isr_handler(void * arg)
{
    isr_context * ctx = arg;
    uint32_t last_ts = ctx->ts;
    ctx->ts = xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;

    if (last_ts != 0 && ctx->ts - last_ts <= 120)
        return;

    xQueueSendFromISR(ctx->qu, &ctx->btn, NULL);
}

static isr_context contexts[] = {
    {GPIO_NUM_16, TOP_LEFT, NULL, 0},
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
            trigger_button(btn);
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

    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    /* connect pins with corresponding action */
    for (size_t i = 0; i < N_BUTTONS; i++)
    {
        isr_context * ctx = &contexts[i];
        ctx->qu = qu;
        gpio_isr_handler_add(ctx->pin, gpio_isr_handler, (void *)ctx);
    }
}
