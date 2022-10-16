#ifndef _MIDI_H_
#define _MIDI_H_

#include "common.h"
#include "ns.h"
#include "formats/midi.h"
#include "formats/inst.h"

typedef struct {
  int16_t num;
  int16_t unk_2;
  int state;
  int misc1;
  int misc2;
  int16_t voll;
  int16_t volr;
} midi_seq;

extern int MidiResetSeqVol(int16_t val);
extern int MidiInit();
extern void MidiResetFadeStep();
extern void MidiTogglePlayback(int a1);
extern int MidiOpenAndPlay(eid_t *eid);
extern void MidiSetStateStopped();
extern void MidiUpdate(void *ref);
extern int MidiKill();
extern void MidiSeqInit(midi_seq *seq, uint32_t vol, int16_t seq_num);
extern void MidiClose(int16_t _sep_access_num, int close_vab);
extern void MidiControl(midi_seq *seq, int op, uint32_t arg);

#endif /* _MIDI_H_ */
