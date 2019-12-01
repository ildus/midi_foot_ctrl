#include <string.h>
#include <stdlib.h>
#include "midi.h"

void midi_generate_note(midi_event_t *event, uint8_t note,
		uint8_t octave, uint8_t velocity)
{
	uint8_t note_code = note + octave  * 12;

	memset(event, 0, sizeof(*event));
	event->status     = 0x90;
	event->d1       = note_code;
	event->d2       = velocity;
}

void midi_setup_note(midi_event_t *event, uint16_t tm, bool on, uint8_t channel)
{
	event->header     = 0x80 | (tm >> 7);
	event->timestamp  = 0x80 | (tm & 0x003f);
	event->status = on ? 0x90 : 0x80 | (channel >> 4);
}

midi_event_t * parse_action(char *action_string, size_t *length, char **btn)
{
    char *str = action_string,
         *token;

    char *pos;

    *length = 1; /* only one for now */
    midi_event_t *event = calloc(1, sizeof(midi_event_t));
	event->header     = 0x80;
	event->timestamp  = 0x80;

    if ((pos = strstr(action_string, ":")) != NULL)
    {
        *pos = '\0';
        if (btn != NULL)
            *btn = str;
        str = pos + 1;
    }

    while ((token = strtok(str, "&")) != NULL)
    {
        char buf[30];
        char *key,
             *val;

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
            free(event);
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

            event->status = action_code;
        }
        else if (strcmp(key, "d1") == 0)
            event->d1 = (uint8_t) atoi(val) & 0x7F;    /* clear MSB */
        else if (strcmp(key, "d2") == 0)
            event->d2 = (uint8_t) atoi(val) & 0x7F;    /* clear MSB */

        str = NULL;
    }

    return event;
}
