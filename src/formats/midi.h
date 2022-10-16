#ifndef _F_MIDI_H_
#define _F_MIDI_H_

typedef struct {
  int track_count;
  eid_t midi;
  eid_t insts[7];
} midi_header;

#endif /* _F_MIDI_H_ */
