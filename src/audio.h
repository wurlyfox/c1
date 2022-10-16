#ifndef _AUDIO_H_
#define _AUDIO_H_

#include "common.h"
#include "geom.h"
#include "gool.h"
#ifdef PSX
#include <LIBSPU.H>
#include <LIBSND.H>
#else
#include "pc/sound/audio.h"
#endif

typedef union {
  gool_object *o;
  vec v;
  int32_t s32;
  int16_t s16;
  int8_t s8;
  uint32_t u32;
  uint16_t u16;
  uint8_t u8;
} generic;

extern int AudioInit();
extern int AudioKill();
extern void AudioSetVoiceMVol(uint32_t vol);
extern int AudioSetReverbAttr(int mode,
  int16_t depth_left, int16_t depth_right, int delay, int feedback);
extern void AudioVoiceFree(gool_object *obj);
extern int AudioVoiceAlloc();
extern int AudioVoiceCreate(gool_object *obj, eid_t *eid, int vol);
extern void AudioControl(int id, int op, generic *arg, gool_object *obj);
extern void AudioUpdate();

#endif /* _AUDIO_H_ */
