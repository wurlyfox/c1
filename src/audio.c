#include "audio.h"
#include "midi.h"
#include "globals.h"
#include "math.h"
#include "gool.h"
#include "level.h"

#ifdef PSX
#include "psx/r3000a.h"
#define Volume SndVolume
#else
#include "pc/gfx/soft.h"
typedef struct {
  int left;
  int right;
} Volume;
#endif

typedef struct {
  uint32_t flags;
  uint32_t unknown;
  gool_object *obj;
  int8_t delay_counter;
  int8_t sustain_counter;
  int16_t amplitude;
  uint16_t case7val;
  int16_t pitch;
  vec r_trans;
  vec trans;
  int16_t tgt_amplitude;
  int16_t tgt_pitch;
  int32_t ramp_counter;
  int32_t ramp_step;
  int32_t glide_counter;
  int32_t glide_step;
} audio_voice_params;

typedef struct {
  int id;
  union {
    audio_voice_params;
    audio_voice_params params;
  };
} audio_voice;

/* .sbss */
int voice_id_ctr;                /* 8005663C; gp[0x90] */
Volume spatial_vol;              /* 80056640; gp[0x91] */
#ifdef PSX
char spu_alloc_top[SPU_MALLOC_SIZE]; /* 80056670; gp[0x9D] */
#endif
/* .bss */
#ifdef PSX
char seq_attr_tbl[2*SS_SEQ_TABSIZ]; /* 80056EE4 */
#endif
audio_voice voices[24];          /* 80056804 */
char keys_status[24];            /* 80056E64 */
audio_voice_params voice_params; /* 80056E7C */

#define master_voice voices[23]

extern ns_struct ns;
extern lid_t cur_lid;
extern entry *cur_zone;

extern int32_t ramp_rate;
extern uint32_t init_vol;
extern uint32_t fade_vol;
extern int32_t fade_vol_step;
extern int max_midi_voices;
extern uint32_t seq2_vol;

/* note: return types for AudioInit and AudioKill are void in orig impl;
 *       made int for subsys map compatibility */
//----- (8002FDE0) --------------------------------------------------------
int AudioInit() {
#ifdef PSX
  SpuCommonAttr attr;
#endif
  int reverb_mode, i;

  for (i=0;i<24;i++)
    voices[i].flags &= ~8;
#ifdef PSX
  SsInit();
  /* set SEQ/SEP data attribute table area,
     max times SsSeqOpen can be called before more data cannot be opened until SsSeqClose is called = 1
     there will be 2 SEQs in any SEP opened */
  SsSetTableSize(seq_attr_tbl, 1, 2);
  /* initialize sound buffer memory management
     specify the start address of the memory management table (dword_80056670)
     there will be 0 calls to SpuMalloc following this
     so the size of the area at 80056670 will be SPU_MALLOC_RECSIZ*(0+1) bytes = SPU_MALLOC_RECSIZ bytes */
  SpuInitMalloc(0, spu_alloc_top);
  /* set default master volume for voices */
  attr.mask = 3;
  attr.mvol.left = 0x3FFF;
  attr.mvol.right = 0x3FFF;
  SpuSetCommonAttr(&attr);
  /* ... */
  SpuSetTransferMode(0);
#else
  SwAudioInit();
#endif
  /* set inital values for master voice */
  master_voice.delay_counter = 1;
  master_voice.sustain_counter = 128;
  master_voice.amplitude = 0x3FFF;
  master_voice.pitch = 0x1000;
  master_voice.obj = 0;
  master_voice.case7val = 0;
  master_voice.r_trans.x = 0;
  master_voice.r_trans.y = 0;
  master_voice.r_trans.z = 0;
  master_voice.flags = (voice_params.flags & 0xFFFFF000) | 0x600;
  /* set reverb mode for levels with reverb */
  switch (cur_lid) {
  case LID_GENERATORROOM:
  case LID_TEMPLERUINS:
  case LID_JAWSOFDARKNESS:
  case LID_BONUSCORTEX:
    reverb_mode = 5;
    break;
  case LID_BONUSBRIO:
    reverb_mode = 3;
    break;
  default:
    reverb_mode = 0;
  }
  if (reverb_mode)
    AudioSetReverbAttr(reverb_mode, 0x2000, 0x2000, 0, 0);
  /* set init_vol (volume for voices) as sfx volume */
  AudioSetVoiceMVol((uint32_t)(0x3FFF00 * sfx_vol) >> 16);
  fade_vol = 0x3FFF;
  fade_vol_step = 0;
  seq2_vol = 0;
  /* finally set master volume */
#ifdef PSX
  SsSetMVol(127, 127);
#else
  SwSetMVol(127);
#endif
  return SUCCESS;
}

//----- (8002FFC0) --------------------------------------------------------
int AudioKill() {
#ifdef PSX
  SpuReverbAttr attr;

  attr.mask = 7;
  attr.mode = 0;
  attr.depth.left = 0;
  attr.depth.right = 0;
  SpuSetReverbModeParam(&attr);
  SpuSetReverb(0);
#else
  SwAudioKill();
#endif
  return SUCCESS;
}

//----- (8002FFFC) --------------------------------------------------------
void AudioSetVoiceMVol(uint32_t vol) {
  init_vol = vol;
}

//----- (80030008) --------------------------------------------------------
int AudioSetReverbAttr(int mode, int16_t depth_left, int16_t depth_right, int delay, int feedback) {
#ifdef PSX
  SpuReverbAttr attr;

  attr.mask = 7;
  attr.mode = mode | 0x100;
  attr.depth.left = depth_left;
  attr.depth.right = depth_right;
  if (mode == 7 || mode == 8) {
    attr.delay = delay;
    attr.feedback = feedback;
  }
  SpuSetReverbModeParam(&attr);
  SpuSetReverbDepth(&attr);
  SpuSetReverb(!!mode);
#endif
  return 0;
}

//----- (80030078) --------------------------------------------------------
static Volume AudioSpatialize(vec *v, int vol) {
  lid_t lid;
  vec va;
  uint32_t mag, ang_xz, amp;
  uint16_t left, right;

  va.x = v->x / 32; /* 3 or 15 bit frac fixed point */
  va.y = v->y / 32;
  va.z = (v->z-(1200<<8)) / 32;
  lid = ns.ldat->lid;
  /* orig impl had a left-over unused call to SqrMagnitude2 in the first if block */
#ifdef PSX
  if (lid == LID_STORMYASCENT || lid == LID_SLIPPERYCLIMB)
    mag = RSqrMagnitude3(va.x, va.z, va.y); /* in=n bit frac, out=2n-20 bit frac; i.e. 10 bit frac */
  else
    mag = RSqrMagnitude2(va.x, va.z); /* in=n bit frac, out=2n-13 bit frac */
#else
  if (lid == LID_STORMYASCENT || lid == LID_SLIPPERYCLIMB)
    mag = SwSqrMagnitude3(va.x, va.z, va.y);
  else
    mag = SwSqrMagnitude2(va.x, va.z);
#endif
  if (mag > (32<<10)-1) {
    left = (vol << 15) / mag; /* 5bffp or 17bffp... */
    right = left;
    if (!mono) {
      ang_xz = atan2(va.z, va.x);
      amp = cos(ang_xz) + 0x1000; /* cos in (0-2) range */
      /* 5bffp * 12bffp = 17bffp; 17bffp >> 11 = 7bffp / 2
         i.e. left = left*(1 + (2.0-amp))/2 */
      left = (left + left*(0x2000 - amp)) >> 11;
      right = (left + left*amp) >> 11;
    }
    left = limit(left, 0, vol);
    right = limit(right, 0, vol);
    spatial_vol.left = left;
    spatial_vol.right = right;
  }
  else {
    spatial_vol.left = vol;
    spatial_vol.right = vol;
  }
  return spatial_vol;
}

//----- (80030260) --------------------------------------------------------
void AudioVoiceFree(gool_object *obj) {
  audio_voice *voice;
  generic res;
  int i, count;

  if (!obj) { return; }
  count = max_midi_voices;
  for (i=0;i<24;i++) {
    voice = &voices[i];
    if ((voice->flags & 8) && voice->obj == obj) {
      ramp_rate = 9;
      AudioControl(voice->flags, 0x40000000, &res, voice->obj);
      voice->obj = 0;
      voice->sustain_counter = 0;
    }
  }
}

/**
 * allocate an audio voice
 *
 * the function tries for the first free/unused (flag bit 4 clear) voice.
 * if no free voice is found it then tries for the voice with the shortest
 * remaining lifetime (the one that will sound off soonest). if multiple
 * voices have this particular remaining amount then it tries for the
 * quietest one.
 *
 * the following cases prevent the function from otherwise stealing from
 * voices that have not yet sounded:
 * - if the shortest remaining lifetime is longer than the default initial
 *   value then the function will return error (-1)
 * - if the shortest remaining lifetime is equal to the default initial
 *   value and the quietest voice amongst those that have that remaining
 *   lifetime is louder than 12.5% of full volume, then the function has a
 *   50/50 chance of not allocating a voice and instead returning an error
 */
//----- (80030328) --------------------------------------------------------
int AudioVoiceAlloc() {
  audio_voice *voice;
  uint8_t min_ttl; /* minimum number of iterations until a voice is freed */
  uint16_t min_vol;
  uint16_t left, right;
  int i, idx_min;

  min_ttl = 255;
  min_vol = 0x3FFF;
  for (i=max_midi_voices;i<24;i++) { /* find a free voice (has flag bit 4 clear) and return its idx */
    voice = &voices[i];
    if (voice->sustain_counter < min_ttl)
      min_ttl = voice->sustain_counter; /* also keep track of shortest remaining lifetime */
    if (!(voice->flags & 8)) /* free voice? */
      return i; /* return the index */
  }
  /* no such voice was found if this point is reached */
  if (min_ttl > voice_params.sustain_counter) { return -1; } /* return error if min rem. lifetime too high */
  for (i=max_midi_voices;i<24;i++) { /* if more than one voice has the shortest remaining lifetime, find the quietest one */
    voice = &voices[i];
    if (voice->sustain_counter == min_ttl) {
#ifdef PSX
      attr.voice = 1 << i;
      SpuGetVoiceAttr(&attr);
      left=abs(attr.volume.left);
      right=abs(attr.volume.right);
#else
      Volume volume;
      if (voice->obj->status_b & 0x200) { /* no spatialization? */
        volume.left  = voice->amplitude;
        volume.right = voice->amplitude;
        voice->flags &= ~0x200;
      }
      else
        volume = AudioSpatialize(&voice->r_trans, voice->amplitude);
      left=abs(volume.left);
      right=abs(volume.right);
#endif
      if (min(left, right) < min_vol) {
        min_vol = min(left, right);
        idx_min = i;
      }
    }
  }
  /* master voice has shortest rem. lifetime and that voice is loud? */
  if (min_ttl == voice_params.sustain_counter && min_vol >= 0x800) {
    if (randb(100) >= 50) { return -1; } /* possibly return error */
  }
  return idx_min; /* return idx of shortest rem. lifetime voice (or the quietest of them if multiple) */
}

/**
 * create an audio voice for the given object and wavebank (entry reference or address),
 * with the given volume.
 *
 * the voice amplitude is set as vol times voice_vol (the voice master volume)
 * the voice obj is set as obj, and if obj is nonzero, the voice trans is set as obj trans
 * all other parameters for the voice are initialized from 'voice_params', which can
 * be preset with AudioControl operations on voice id 0. (note that the 'voice_params'
 * are reset back to the default values after this function runs, so AudioControl
 * must again be used to set the values for the next voice created, and so on, if
 * any values other than the defaults are desired.)
 *
 * a lower level VoiceAttr object is created for the voice, the volume of which is
 * set to the 'spatialized' voice amplitude, which is based on distance of the object
 * from camera and scalar projection in the XZ plane. (if spatialization is disabled
 * volume values for both channels are set directly to the voice amplitude). the attr
 * is also setup for reverb if reverb flag is set. key on is immediately set for the
 * voice if bit 5 is set in the initial flags.
 *
 * the function returns the newly allocated voice id for the new voice.
 */
//----- (800304C8) --------------------------------------------------------
int AudioVoiceCreate(gool_object *obj, eid_t *eid, int vol) {
#ifdef PSX
  SpuVoiceAttr attr;
  SndVolume volume;
  uint32_t tag, addr;
#else
  Volume volume;
  entry *adio;
  size_t size;
#endif
  audio_voice *voice;
  int idx;

  if (!sfx_vol) { return 0; } /* return if sfx are muted */
  idx = AudioVoiceAlloc(); /* try to allocate a voice */
  if (idx <= 0) { return -1; } /* return error on failure to allocate */
  voice = &voices[idx]; /* get the allocated voice */
#ifdef PSX
  if (eid & 1) /* eid pointer is actually an address? */
    addr = (uint32_t)eid & 0xFFFFFFFE;
  else {
    tag = NSLookup(eid);
    if (ISERRORCODE(tag)) { return -1; } /* return error on failed lookup */
    addr = tag >> 2;
  }
#else
  adio = NSLookup(eid);
  size = adio->items[1]-adio->items[0];
  SwLoadSample(idx, adio->items[0], size);
#endif
  voice->params = voice_params;
  voice->amplitude = (vol*init_vol) >> 14;
  if (voice->flags & 0x40)
    voice->ramp_step = (voice->tgt_amplitude-voice->amplitude)/voice->ramp_counter;
  voice->obj = obj;
  if (obj) {
    voice->trans = obj->trans;
    GoolTransform2(&obj->trans, &voice->r_trans, 1);
  }
  /* reset voice_params to the defaults in case they were changed by AudioControl */
  voice_params.delay_counter = 1;
  voice_params.sustain_counter = 128;
  voice_params.amplitude = 0x3FFF;
  voice_params.pitch = 0x1000;
  voice_params.obj = 0;
  voice_params.case7val = 0;
  voice_params.r_trans.x = 0;
  voice_params.r_trans.y = 0;
  voice_params.r_trans.z = 0;
  voice_params.flags = (voice_params.flags & 0xFFFFF000) | 0x600;
  /* bugfix: orig impl did not test voice->obj before accessing it here */
  if (voice->obj && voice->obj->status_b & 0x200) { /* no spatialization? */
    volume.left  = voice->amplitude;
    volume.right = voice->amplitude;
    voice->flags &= ~0x200;
  }
  else
    volume = AudioSpatialize(&voice->r_trans, voice->amplitude);
#ifdef PSX
  /* set default voice attribs and voice address */
  attr.mask = 0xFF93;
  attr.voice = 1 << idx;
  attr.volume = volume;
  attr.a_mode = 1;
  attr.s_mode = 1;
  attr.r_mode = 1;
  attr.addr = addr;
  attr.ar = 0;
  attr.sr = 0;
  attr.rr = 0;
  attr.sl = 15;
  attr.pitch = voice->pitch;
  SpuSetVoiceAttr(&attr);
  SpuSetReverbVoice((voice->flags>>10)&1, attr.voice);
  if (!(voice->flags & 0x10)) /* non-delayed voice? */
    SpuSetKey(SPU_ON, attr.voice); /* turn key on immediately */
#else
  if (!(voice->flags & 0x10)) /* non-delayed voice? */
    SwNoteOn(idx); /* turn key on immediately */
  SwVoiceSetVolume(idx, volume.left, volume.right);
  SwVoiceSetPitch(idx, voice->pitch);
#endif
  voice->id = ++voice_id_ctr; /* allocate next id for the voice */
  voice->flags |= 8; /* set 'used' flag */
  return voice_id_ctr; /* return the voice id */
}

/**
 * controls a single audio voice or set of audio voices for an object
 *
 * to control a single voice, set id to the voice id
 * to control all voice(s) for a particular object,
 * set id to -1 and set obj to the object
 * to control the initial params for the next voice created, set id to 0
 *
 * the lower 3 bytes of op should be one of the following control ops,
 * and arg should be set to a value of the corresponding type:
 *
 * | op | description                | arg description  | arg type     |
 * |----|----------------------------|------------------|--------------|
 * | 0  | set amplitude              | amplitude        | signed int   |
 * | 1  | set pitch                  | pitch            | signed int   |
 * | 2  | set location [emittance pt]| location         | vector       |
 * | 3  | set location (pre-rotated) | rotated location | vector       |
 * | 4  | set delay amount           | delay amount     | signed byte  |
 * | 5  | set object                 | object           | object       |
 * | 6  | set glide/ramp rate        | glide/ramp rate  | signed int   |
 * | 7  | trigger key on             | case7val?        | unsigned int |
 * | 8  | enable spatialization      | n/a              | n/a          |
 * | 9  | disable spatialization     | n/a              | n/a          |
 * | 10 |                            | flag << 8        | unsigned int |
 * | 11 | enable/disable reverb      | flag << 8        | unsigned int |
 * | 12 | set sustain amount         | sustain amount   | signed byte  |
 *
 * the upper byte of op can include the following control flags:
 *
 * bit 6 - enable ramp or glide, when op 0 or 1 are used
 *         ramp will occur from current amplitude to target amplitude
 *         glide will occur from current pitch to target pitch
 * bit 7 - force key off at end of glide/ramp
 * bit 8 - force key off
 *
 * id    - id of the voice to control
 *         -1 if controlling all voices for an object
 *          0 if controlling initial params for next voice created
 * op    - op to perform (lower 3 bytes) and control flags (upper byte) (see above)
 * arg   - control argument
 * obj   - object with voice(s) to control, if id == -1
 */
//----- (80030840) --------------------------------------------------------
void AudioControl(int id, int op, generic *arg, gool_object *obj) {
#ifdef PSX
  SpuVoiceAttr attr; // [sp+28h] [-90h] BYREF
#endif
  Volume volume;
  audio_voice *voice; // $s3
  audio_voice_params *params; // $s0
  vec v; // [sp+90h] [-28h] BYREF
  int i;

  for (i=max_midi_voices;i<24;i++) {
    if (id && ((id == -1 && voices[i].obj != obj) /* id is -1 and voice i does not have the specified obj? */
      || voices[i].id != id)) /* ...or id is nonzero and voice i does not have this id? */
      continue; /* skip the voice */
    voice = (audio_voice*)((uint8_t*)&voice_params-sizeof(uint32_t)); /* cur voice */
    if (id)
      voice = &voices[i];
    if (op & 0x80000000) { voice->flags |= 1; } /* set flag for 'force off'                    if bit 32 is set */
    if (op & 0x40000000) { voice->flags |= 2; } /* set flag for 'force off when flag clear'    if bit 31 is set */
    if (op & 0x20000000) { voice->flags |= 4; } /* set flag for 'amplitude ramp/glide enabled' if bit 30 is set */
#ifdef PSX
    attr.mask = 0;
#endif
    switch (op & 0xFFFFFFF) {
    case 0: /* set amplitude */
      if (voice->flags & 4) { /* amplitude ramp enabled? */
        voice->tgt_amplitude = arg->s32;
        voice->ramp_counter = ramp_rate;
        if (id) /* single voice control mode? */
          voice->ramp_step = (voice->tgt_amplitude - voice->amplitude) / ramp_rate;
        voice->flags |= 0x40; /* set 'ramping' status */
      }
      else {
        voice->amplitude = arg->s32;
        if (id) { /* single voice control mode? */
          volume = AudioSpatialize(&voice->r_trans, voice->amplitude);
#ifdef PSX
          attr.mask |= 3;
          attr.volume = volume; /* set in the voice attr as well */
#else
          SwVoiceSetVolume(i, volume.left, volume.right);
#endif
        }
      }
      break;
    case 1: /* set pitch */
      if (voice->flags & 4) { /* glide/portamento enabled? */
        voice->tgt_pitch = arg->s32;
        voice->glide_counter = ramp_rate; /* calculate counter */
        voice->glide_step = (voice->tgt_pitch - voice->pitch) / ramp_rate; /* calculate step */
        voice->flags |= 0x80; /* set 'gliding' status */
      }
      else {
        voice->pitch = arg->s32; /* set pitch directly */
        if (id) { /* single voice control mode? */
#ifdef PSX
          attr.mask = 0x10;
          attr.pitch = voice->pitch; /* set in the voice attr as well */
#else
          SwVoiceSetPitch(i, voice->pitch);
#endif
        }
      }
      break;
    case 2: /* set voice location */
      v = arg->v;
      GoolTransform2(&v, &arg->v, 1);
      /* fall through!!! */
    case 3: /* set voice location (pre-rotated) */
      if (voice->flags & 4) { break; } /* skip if amplitude ramp or portamento enabled */
      voice->r_trans = arg->v;
      if (id) {
        voice->r_trans = arg->v;
        volume = AudioSpatialize(&voice->r_trans, voice->amplitude);
#ifdef PSX
        attr.mask = 3;
        attr.volume = volume;
#else
        SwVoiceSetVolume(i, volume.left, volume.right);
#endif
      }
      break;
    case 4: /* set delay amount */
      voice->delay_counter = arg->s8;
      break;
    case 5: /* set voice object */
      voice->obj = arg->o;
      break;
    case 6: /* set glide/ramp rate */
      ramp_rate = arg->s32 ? arg->s32 : 1;
      break;
    case 7: /* delay voice */
      voice->case7val = arg->u32;
      voice->flags |= 0x10;
      break;
    case 8: /* set as an object voice */
      voice->flags |= 0x200;
      break;
    case 9: /* unset as an object voice */
      voice->flags &= ~0x200;
      break;
    case 10:
      voice->flags = (voice->flags & 0xFFFFF7FF) | ((arg->u32<<3)&0x800);
      break;
    case 11: /* enable reverb */
      voice->flags = (voice->flags & 0xFFFFFBFF) | ((arg->u32<<2)&0x400);
      break;
    case 12: /* set sustain amount */
      voice->sustain_counter = arg->s8;
      break;
    default:
      break;
    }
#ifdef PSX
    if (attr.mask) { /* changes to voice? */
      attr.voice = 1 << cur_voice_idx;
      SpuSetVoiceAttr(&attr); /* apply changes */
    }
#endif
    if (id != -1) { return; } /* quit if not controlling [other] voices for a single object */
  }
}

/**
 * update all voices
 *
 * this includes decrementing all counters, incl. sustain counters,
 * delay counters, ramp counters, and glide counters, keeping
 * voices on while they sustain (sustain counter > 0), incrementing/
 * decrementing amplitude and/or pitch while they ramp/glide, delaying
 * the key on event while they delay, and clearing corresponding flags
 * when target counter values are reached. in particular, a voice key
 * is turned off and the voice is freed when the sustain/'remaining
 * lifetime' reaches zero, or when it is forced off
 *
 * all updated values are updated as attributes in the corresponding
 * lower level VoiceAttr object for a particular voice.
 */
//----- (80030CC0) --------------------------------------------------------
void AudioUpdate() {
#ifdef PSX
  SpuVoiceAttr attr; // [sp+18h] [-50h] BYREF
#endif
  Volume volume;
  audio_voice *voice;
  zone_header *header;
  uint16_t vol;
  int i, flag;

  flag = 0;
  /* reset volumes if title */
  if (ns.ldat->lid == LID_TITLE) {
    init_vol = (0x3FFF * sfx_vol) >> 8;
    vol = (0x3FFF00 * mus_vol) >> 16;
    MidiResetSeqVol(vol);
  }
  /* update midi */
  if (cur_zone) {
    header = (zone_header*)cur_zone->items[0];
    MidiUpdate(&header->midi); /* 0x304 */
  }
  if (fade_vol_step) { /* volume fading? */
    if (fade_vol_step < 0 && fade_vol < abs(fade_vol_step)) { /* fading out and fade_vol < abs(fade_vol_step)? */
      fade_vol = 0; /* lower limit at 0 */
      fade_vol_step = 0; /* stop fading */
    }
    if (fade_vol_step > 0 && 0x3FFF - fade_vol < fade_vol_step) { /* fading in and fade_vol > fade_vol - fade_vol_step ? */
      fade_vol = 0x3FFF; /* upper limit fade_vol at 0x3FFF */
      fade_vol_step = 0; /* stop fading */
    }
    fade_vol += fade_vol_step; /* increase/decrease volume */
#ifdef PSX
    SsSetMVol(fade_vol, fade_vol); /* set master volume */
#else
    SwSetMVol(fade_vol);
#endif
  }
#ifdef PSX
#define KEYS_STATUS_ON SPU_ON_ENV_OFF
  SpuGetAllKeysStatus(keys_status);
#else
#define KEYS_STATUS_ON 1
  SwGetAllKeysStatus(keys_status);
#endif
  flag=0;
  for (i=max_midi_voices;i<24;i++) {
    voice = &voices[i];
    if (!(voice->flags & 8)) { continue; } /* skip inactive/free voices */
    if (voice->flags & 0x10) { /* delayed voice? */
      if (--voice->delay_counter == 0) { /* decrement delay; has countdown finished? */
        voice->flags &= ~0x10; /* clear key triggered status */
#ifdef PSX
        SpuSetKey(SPU_ON, 1<<i); /* set key on */
#else
        SwNoteOn(i);
#endif
      }
    }
    if (keys_status[i] == KEYS_STATUS_ON) {
      if (--voice->sustain_counter != 0) {/* decrement sustain; still sustaining? */
#ifdef PSX
        SpuSetKey(SPU_ON, 1<<i); /* keep key on */
#endif
      }
      else {
#ifdef PSX
        SpuSetKey(SPU_OFF, 1<<i); /* else turn key off */
#else
        SwNoteOff(i);
#endif
        voice->flags &= ~8; /* and free up the voice */
        /*
        if (voice->flags & 1 || (!flag && voice->flags & 2)) {
          SpuSetKey(SPU_OFF, 1<<i);
          voice->flags &= ~8;
        }
        */
        continue;
      }
    }
#ifndef PSX
    else {
      voice->flags &= ~8;
    }
#endif
#ifdef PSX
    attr.mask = 0;
#endif
    if (voice->flags & 0x40) { /* currently ramping (amplitude)? */
      voice->amplitude += voice->ramp_step; /* increase amplitude */
      if (--voice->ramp_counter > 0) /* not done ramping? */
        flag = 1; /* set flag */
      else
        voice->flags &= ~0x40; /* else clear currently ramping flag */
      /* bugfix: orig impl did not test voice->obj before accessing it here */
      if (voice->obj && (voice->obj->status_b & 0x200)) { /* no spatialization? */
        volume.left = voice->amplitude;  /* set volume directly to the amplitude */
        volume.right = voice->amplitude;
      }
      else
        volume = AudioSpatialize(&voice->r_trans, voice->amplitude); /* else spatialize */
#ifdef PSX
      attr.mask |= 3; /* set mask for applying changes to volume */
      attr.volume = volume;
#else
      SwVoiceSetVolume(i, volume.left, volume.right);
#endif
    }
    if (voice->flags & 0x80) { /* currently gliding? */
      voice->pitch += voice->glide_step; /* increase pitch */
      if (--voice->glide_counter > 0) /* not done gliding? */
        flag = 1; /* set flag */
      else
        voice->flags &= ~0x80; /* else clear currenly gliding flag */
#ifdef PSX
      attr.mask |= 0x10; /* set mask for applying changes to pitch */
      attr.pitch = voice->pitch; /* set new pitch */
#else
      SwVoiceSetPitch(i, voice->pitch);
#endif
    }
    if ((voice->flags & 0x200) && voice->obj) { /* voice emitted from object? */
      voice->trans = voice->obj->trans; /* set to object trans */
      GoolTransform2(&voice->trans, &voice->r_trans, 1); /* trans, rotate, and scale */
      volume = AudioSpatialize(&voice->r_trans, voice->amplitude); /* spatialize w.r.t. object */
#ifdef PSX
      attr.mask |= 3; /* set mask for applying changes to volume */
      attr.volume = volume;
#else
      SwVoiceSetVolume(i, volume.left, volume.right);
#endif
    }
#ifdef PSX
    if (attr.mask) { /* changes to voice? */
      attr.voice = 1 << i;
      SpuSetVoiceAttr(&attr); /* apply changes */
    }
#endif
    if ((voice->flags & 1) || (!flag && (voice->flags & 2))) { /* forced off? */
#ifdef PSX
      SpuSetKey(SPU_OFF, 1<<i); /* turn key off */
#else
      SwNoteOff(i);
#endif
      voice->flags &= ~8; /* free up voice */
    }
  }
}
