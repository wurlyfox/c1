#ifndef _PC_MIDI_H_
#define _PC_MIDI_H_

#include "common.h"

extern void SwMidiInit();
extern void SwMidiKill();
extern int16_t SwSepOpen(uint8_t *sep, int vab_id, int count);
extern void SwSepClose();
extern void SwSepSetVol(int san, int seq_num, uint32_t voll, uint32_t volr);
extern void SwSepPlay(int san, int seq_num, int mode, int loops);
extern void SwSepStop(int san, int seq_num);
extern void SwSepPause(int san, int seq_num);
extern void SwSepReplay(int san, int seq_num);
extern void SwSepSetCrescendo(int san, int seq_num, uint32_t vol, int ticks);
extern void SwSepSetDecrescendo(int san, int seq_num, uint32_t vol, int ticks);

extern void SwMidiProcess(int ch, float amp[2], float freq, int len, int16_t *data);

#endif /* _PC_MIDI_H_ */