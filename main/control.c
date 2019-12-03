#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "midi.h"

typedef struct {
	gpio_num_t		pin;
	button_num_t	btn;
	xQueueHandle	qu;
} isr_context;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
	isr_context *ctx = arg;
    xQueueSendFromISR(ctx->qu, &ctx->btn, NULL);
}

static isr_context contexts[] = {
	{GPIO_NUM_16, TOP_LEFT, NULL},
	{GPIO_NUM_17, TOP_CENTER, NULL},
	{GPIO_NUM_18, TOP_RIGHT, NULL},
	{GPIO_NUM_19, BOTTOM_LEFT, NULL},
	{GPIO_NUM_21, BOTTOM_CENTER, NULL},
	{GPIO_NUM_22, BOTTOM_RIGHT, NULL},
};

static void gpio_handle_buttons(void* queue) {
    while (true) {
		button_num_t	btn;
        if (xQueueReceive(queue, &btn, portMAX_DELAY)) {
            ESP_LOGI("gpio", "clicked button %d", btn);
        }
    }
}

void init_buttons() {
	uint64_t		pins_mask = 0;
	gpio_config_t	io_conf;

	/* init task */
    xQueueHandle qu = xQueueCreate(10, sizeof(button_num_t));
	xTaskCreate(gpio_handle_buttons, "handle_buttons", 2048, (void *) qu, 10, NULL);

	// set up pin mask
	memset(&io_conf, 0, sizeof(io_conf));
	for (size_t i = 0; i < N_BUTTONS; i++)
	{
		isr_context *ctx = &contexts[i];
		io_conf.pin_bit_mask |= 1ULL << ctx->pin;
	}

	//interrupt of low level
    io_conf.intr_type = GPIO_INTR_LOW_LEVEL;
    io_conf.pin_bit_mask = pins_mask;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;

    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

	/* connect pins with corresponding action */
	for (size_t i = 0; i < N_BUTTONS; i++)
	{
		isr_context *ctx = &contexts[i];
		ctx->qu = qu;
		gpio_isr_handler_add(ctx->pin, gpio_isr_handler, (void *) ctx);
	}
}
