#include <string.h>
#include "midi.h"

void midi_generate_note(midi_event_t *event, uint8_t note,
		uint8_t octave, uint8_t velocity)
{
	uint8_t note_code = note + (octave + 1) * 12;

	memset(event, 0, sizeof(*event));
	event->status     = 0x90;
	event->note       = note_code;
	event->velocity   = velocity;
}

void midi_setup_note(midi_event_t *event, uint16_t tm, bool on, uint8_t channel)
{
	event->header     = 0x80 | (tm >> 7);
	event->timestamp  = 0x80 | (tm & 0x003f);
	event->status = on ? 0x90 : 0x80 | (channel >> 4);
}
