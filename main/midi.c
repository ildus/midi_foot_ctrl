#include <stdlib.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/portmacro.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "midi.h"
#define MAX_EVENTS 6

typedef struct
{
    QueueHandle_t queue;
    size_t total_len;
    size_t current;
    midi_event_t events[MAX_EVENTS];
} btn_command;

static btn_command commands[N_BUTTONS];

void midi_generate_note(midi_event_t * event, uint8_t note, uint8_t octave, uint8_t velocity)
{
    uint8_t note_code = note + octave * 12;

    memset(event, 0, sizeof(*event));
    event->status = 0x90;
    event->d1 = note_code;
    event->d2 = velocity;
}

void init_button_events(QueueHandle_t midi_queue)
{
    esp_err_t err;

    nvs_handle storage_handle;
    err = nvs_open(STORAGE, NVS_READWRITE, &storage_handle);
    ESP_ERROR_CHECK(err);

    for (size_t i = 0; i < N_BUTTONS; i++)
    {
        size_t vallen;
        char * key = button_names[i];
        char * val;

        err = nvs_get_str(storage_handle, key, NULL, &vallen);
        if (err == ESP_OK)
        {
            val = malloc(vallen);
            ESP_ERROR_CHECK(nvs_get_str(storage_handle, key, val, &vallen));
        }
        else
            val = strdup(default_values[i]);

        size_t events_len;
        midi_event_t * events = parse_action(val, &events_len, NULL);
        free(val);

        if (events == NULL)
        {
            ESP_LOGE("midi", "could not parse events for btn %u", i);
            continue;
        }

        commands[i].queue = midi_queue;
        commands[i].total_len = events_len;
        commands[i].current = 0;
        memset(commands[i].events, 0, sizeof(midi_event_t) * MAX_EVENTS);
        memcpy(commands[i].events, events, sizeof(midi_event_t) * events_len);
        free(events);

        ESP_LOGI("midi", "btn %u initialized, events count: %u", i, commands[i].total_len);
    }
    nvs_close(storage_handle);
}

void trigger_button(button_num_t btn)
{
    btn_command * cmd = &commands[btn];
    BaseType_t ret = xQueueSend(cmd->queue, &cmd->events[cmd->current], (TickType_t)0);

    if (ret == pdTRUE)
    {
        cmd->current += 1;
        if (cmd->current >= cmd->total_len)
            cmd->current = 0;

        ESP_LOGI("midi", "btn %d triggered", btn);
    }
}

void midi_setup_note(midi_event_t * event, uint16_t tm, bool on, uint8_t channel)
{
    event->header = 0x80 | (tm >> 7);
    event->timestamp = 0x80 | (tm & 0x003f);
    event->status = on ? 0x90 : 0x80 | (channel >> 4);
}

midi_event_t * parse_action(char * action_string, size_t * length, char ** btn)
{
    char *str = action_string, *token;

    char * pos;

    *length = 0;
    midi_event_t * events = calloc(MAX_EVENTS, sizeof(midi_event_t));

    if (btn)
        *btn = NULL;

    if ((pos = strstr(action_string, ":")) != NULL)
    {
        *pos = '\0';
        if (btn != NULL)
            *btn = str;

        str = pos + 1;
    }

    uint8_t found_vals = 0;
    midi_event_t curevent;

    while ((token = strtok(str, "&")) != NULL)
    {
        char buf[30];
        char *key, *val;

        strncpy(buf, token, sizeof(buf));
        buf[sizeof(buf) - 1] = '\0';

        if ((pos = strstr(buf, "=")) != NULL)
        {
            key = buf;
            *pos = '\0';
            val = pos + 1;
        }
        else
        {
            free(events);
            return NULL;
        }

        uint8_t action_code = 0x80;
        if (strcmp(key, "action") == 0)
        {
            if (strcmp(val, "note_off") == 0)
                action_code = 0x80;
            else if (strcmp(val, "note_on") == 0)
                action_code = 0x90;
            else if (strcmp(val, "cc") == 0)
                action_code = 0xB0;
            else if (strcmp(val, "pc") == 0)
                action_code = 0xC0;
            else if (strcmp(val, "song_pos_pointer") == 0)
                action_code = 0xF2;
            else if (strcmp(val, "song_select") == 0)
                action_code = 0xF3;
            else if (strcmp(val, "tune_request") == 0)
                action_code = 0xF6;

            curevent.status = action_code;
            found_vals |= 0b1000;
        }
        else if (strcmp(key, "channel") == 0)
        {
            uint8_t channel = (uint8_t)atoi(val) & 0x0F; /* clear 4 MSB bits */
            curevent.status |= channel;
            found_vals |= 0b0100;
        }
        else if (strcmp(key, "d1") == 0)
        {
            curevent.d1 = (uint8_t)atoi(val) & 0x7F; /* clear MSB */
            found_vals |= 0b0010;
        }
        else if (strcmp(key, "d2") == 0)
        {
            curevent.d2 = (uint8_t)atoi(val) & 0x7F; /* clear MSB */
            found_vals |= 0b0001;
        }

        if (found_vals == 0b1111)
        {
            found_vals = 0;
            curevent.header = 0x80;
            curevent.timestamp = 0x80;
            (*length)++;

            if (*length <= MAX_EVENTS)
            {
                events[*length - 1] = curevent;

                ESP_LOGI(
                    "midi",
                    "MIDI parsed: %.2x%.2x%.2x%.2x%.2x",
                    curevent.header,
                    curevent.timestamp,
                    curevent.status,
                    curevent.d1,
                    curevent.d2);
            }
            else
                ESP_LOGI(
                    "midi",
                    "MIDI skipped, max is 6: %.2x%.2x%.2x%.2x%.2x",
                    curevent.header,
                    curevent.timestamp,
                    curevent.status,
                    curevent.d1,
                    curevent.d2);

            memset(&curevent, 0, sizeof(curevent));
        }

        str = NULL;
    }

    return events;
}
