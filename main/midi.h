#ifndef MIDI_H
#define MIDI_H

#include <stdint.h>
#include <stdbool.h>

#include "esp_err.h"

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
	uint8_t  note;
	uint8_t  velocity;
} midi_event_t;

void midi_generate_note(midi_event_t *event, uint8_t note,
		uint8_t octave, uint8_t velocity);
void midi_setup_note(midi_event_t *event, uint16_t tm, bool on, uint8_t channel);

void initialise_wifi();
void start_http_server();

enum {
    IDX_SVC,
    IDX_MIDI_CHAR,
    IDX_MIDI_CHAR_VAL,
    IDX_MIDI_CHAR_CFG,
    MIDI_IDX_NB,
};

#endif
