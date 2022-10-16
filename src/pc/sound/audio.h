#ifndef _PC_AUDIO_H_
#define _PC_AUDIO_H_

#include "common.h"

extern void SwAudioInit();
extern void SwAudioKill();
extern void SwSetMVol(uint32_t vol);
extern void SwLoadSample(int voice_idx, uint8_t *data, size_t size);
extern void SwUnloadSample(int voice_idx);
extern void SwNoteOn(int voice_idx);
extern void SwNoteOff(int voice_idx);
extern void SwVoiceSetVolume(int voice_idx, uint32_t voll, uint32_t volr);
extern void SwVoiceSetPitch(int voice_idx, uint32_t pitch);
extern void SwGetAllKeysStatus(uint8_t *status);

#endif /* _PC_AUDIO_H_ */
