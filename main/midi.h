#ifndef MIDI_H
#define MIDI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

enum Notes {
	C = 0,
	Ci,
	D,
	Di,
	E,
	F,
	Fi,
	G,
	Gi,
	A,
	Ai,
	B
};

typedef struct {
	uint16_t	conn_id;
} task_context;

typedef struct midi_event_t
{
	uint8_t  header;
	uint8_t  timestamp;
	uint8_t  status;
	uint8_t  d1;
	uint8_t  d2;
} midi_event_t;

void midi_generate_note(midi_event_t *event, uint8_t note,
		uint8_t octave, uint8_t velocity);
void midi_setup_note(midi_event_t *event, uint16_t tm, bool on, uint8_t channel);
midi_event_t *parse_action(char *action_string, size_t *length, char **btn);

void initialise_wifi();
void start_http_server();

enum {
    IDX_SVC,
    IDX_MIDI_CHAR,
    IDX_MIDI_CHAR_VAL,
    IDX_MIDI_CHAR_CFG,
    MIDI_IDX_NB,
};

typedef enum {
    TOP_LEFT,
    TOP_CENTER,
    TOP_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_CENTER,
    BOTTOM_RIGHT,
    N_BUTTONS,
} button_num_t;
#endif
