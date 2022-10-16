#include "midi.h"
#include "globals.h"
#include "level.h"

#ifdef PSX
#include <LIBSND.H>
#define SepSetVol         SsSepSetVol
#define SepPlay           SsSepPlay
#define SepPause          SsSepPause
#define SepReplay         SsSepReplay
#define SepStop           SsSepStop
#define SepSetCrescendo   SsSepSetCrescendo
#define SepSetDecrescendo SsSepSetDecrescendo
#define SepOpen           SsSepOpen
#define SepClose          SsSepClose
#else
#include "pc/sound/midi.h"
#define SepSetVol         SwSepSetVol
#define SepPlay           SwSepPlay
#define SepPause          SwSepPause
#define SepReplay         SwSepReplay
#define SepStop           SwSepStop
#define SepSetCrescendo   SwSepSetCrescendo
#define SepSetDecrescendo SwSepSetDecrescendo
#define SepOpen           SwSepOpen
#define SepClose          SwSepClose
#endif

/* .sdata */
int32_t ramp_rate = 30;    /* 800564F4; gp[0x3E] */
int midi_fade_counter = 0; /* 800564F8; gp[0x3F] */
entry *next_midi = 0;      /* 800564FC; gp[0x40] */
/* .sbss */
uint32_t init_vol;         /* 80056644; gp[0x92] */
uint32_t seq_vol;          /* 80056648; gp[0x93] */
uint32_t unk_8005664C;     /* 8005664C; gp[0x94] */
eid_t cur_midi_eid;        /* 80056650; gp[0x95] */
int sep_access_num;        /* 80056654; gp[0x96] */
int vab_id;                /* 80056658; gp[0x97] */
int seq_count;             /* 8005665C; gp[0x98] */
uint32_t fade_vol;         /* 80056660; gp[0x99] */
int32_t fade_vol_step;     /* 80056664; gp[0x9A] */
int max_midi_voices;       /* 80056668; gp[0x9B] */
uint32_t seq2_vol;         /* 8005666C; gp[0x9C] */
int midi_state;            /* 800566C0; gp[0xB1] */
/* .bss */
midi_seq midi_seqs[2];     /* 80056EBC */
eid_t insts[8];            /* 8005728C */

extern lid_t cur_lid;

//----- (800311B0) --------------------------------------------------------
int MidiResetSeqVol(int16_t val) {
  int32_t vol;
  int i;

  seq_vol = val;
  vol = (int)val >> 7;
  for (i=(seq_count>1);i<seq_count;i++) {
    SepSetVol(sep_access_num, i, vol, vol);
  }
}

//----- (80031244) --------------------------------------------------------
int MidiInit() {
  int16_t vol;
  int i;

#ifdef PSX
  SsSetTickMode(1);
#endif
  seq_vol = (0x3FFF * mus_vol) >> 8;
  vol = seq_vol >> 7;
  for (i=(seq_count>1);i<seq_count;i++)
    SepSetVol(vab_id, i, vol, vol);
  cur_midi_eid = EID_NONE;
  midi_state = 0;  /* midi state initial val */
  sep_access_num = -1; /* sep access num initial val */
  vab_id = -1; /* vab id initial val */
  if (mus_vol == 0)
    max_midi_voices = 8;
  else if (sfx_vol == 0)
    max_midi_voices = 20;
  else {
    switch (cur_lid) {
    case LID_CORTEXPOWER:
      max_midi_voices = 11;
      break;
    case LID_GENERATORROOM:
    case LID_JUNGLEROLLERS:
    case LID_ROLLINGSTONES:
      max_midi_voices = 13;
      break;
    case LID_HEAVYMACHINERY:
    case LID_TOXICWASTE:
      max_midi_voices = 14;
      break;
    case LID_DRNEOCORTEX:
    case LID_STORMYASCENT:
    case LID_SLIPPERYCLIMB:
      max_midi_voices = 15;
      break;
    case LID_BOULDERS:
    case LID_BOULDERDASH:
      max_midi_voices = 17;
      break;
    case LID_PINSTRIPE:
    case LID_HOGWILD:
    case LID_ROADTONOWHERE:
    case LID_THEHIGHROAD:
    case LID_WHOLEHOG:
    case LID_BONUSTAWNA1:
    case LID_LIGHTSOUT:
    case LID_FUMBLINGINTHEDARK:
    case LID_BONUSTAWNA2:
      max_midi_voices = 18;
      break;
    case LID_PAPUPAPU:
      max_midi_voices = 21;
      break;
    default:
      max_midi_voices = 16;
      break;
    }
  }
#ifdef PSX
  return SpuVmSetMaxVoice(*(uint8_t*)(&max_midi_voices)); /* tentative name! */
#else
  // TODO
  return 0;
#endif
}

//----- (800313B4) --------------------------------------------------------
void MidiResetFadeStep() {
  fade_vol_step = -682;
}

//----- (800313C4) --------------------------------------------------------
void MidiTogglePlayback(int a1) {
  if (seq_count < 2) { return; } /* return if less than 2 seqs */
  if (midi_state != 3) { return; } /* return if not resuming playback */
  if (a1 >> 8 == 3) {
    if (mus_vol || sfx_vol)
      seq2_vol = 0x3FFF;
    else
      seq2_vol = 0;
    MidiControl(&midi_seqs[0], 2, 0); /* pause 1st seq; no arg for pause */
    MidiControl(&midi_seqs[1], 0, 1); /* play 2nd seq; set play_mode to 1=SSPLAY_PLAY */
  }
  else {
    MidiControl(&midi_seqs[1], 1, 0); /* stop 2nd seq; no arg for stop */
    MidiControl(&midi_seqs[0], 3, 0); /* resume 1st seq; */
    seq2_vol = 0;
  }
  SepSetVol(sep_access_num, midi_seqs[1].num, seq2_vol, seq2_vol);
}

#ifdef PSX
static inline int MidiGetVab(entry *midi) {
  midi_header *header;
  uint8_t *vh;

  header = (midi_header*)midi->items[0];
  NSOpen(&header->midi, 1, 1); /* preload the midi entry itself? from midi header */
  vh = (uint8_t*)midi->items[1]; /* vab data */
  /* open new vab header (item2) for vab body at arg3, and allocate a new vab id */
  vab_id = SsVabFakeHead(vh, -1, (dword_8005CFB0 << 16) | 0x2000);
  if (vab_id == -1) /* failed to open header or allocate? */
    exit(); /* abort */
  vab_id = SsVabFakeBody(vab_id); /* assign allocated vab id to vab body */
  if (vab_id == -1) /* failed to assign id? */
    exit(); /* abort */
  SsStart(); /* start the sound system */
  return vab_id;
}
#else
uint8_t *vabs[16];
int vab_count=0;

static inline int MidiGetVab(entry *midi) {
  entry *inst;
  midi_header *header;
  inst_header *i_header;
  eid_t inst_eid;
  uint8_t *vab,*vh,*vb,*vd;
  size_t vh_size, size;
  int i, ii;

  vh_size = midi->items[2] - midi->items[1];
  size = vh_size;
  for (i=0;i<7;i++) {
    header = (midi_header*)midi->items[0];
    inst_eid = header->insts[i];
    if (inst_eid == EID_NONE) { continue; }
    inst = NSOpen(&inst_eid, 1, 1);
    i_header = (inst_header*)inst->items[0];
    size += i_header->len;
  }
  vab = (uint8_t*)malloc(size);
  vd = vab;
  vh = (uint8_t*)midi->items[1];
  for (i=0;i<vh_size;i++)
    *(vd++) = *(vh++);
  for (i=0;i<7;i++) {
    inst_eid = header->insts[i];
    if (inst_eid == EID_NONE) { continue; }
    inst = NSOpen(&inst_eid, 1, 1);
    i_header = (inst_header*)inst->items[0];
    vb = (uint8_t*)inst->items[1];
    for (ii=0;ii<i_header->len;ii++)
      *(vd++) = *(vb++);
  }
  vabs[vab_count] = vab;
  return vab_count++;
}

void VabClose(int _vab_id) {
  free(vabs[_vab_id]);
}

#endif

//----- (800314C4) --------------------------------------------------------
int MidiOpenAndPlay(eid_t *eid) {
  midi_seq *seq;
  entry *midi;
  midi_header *header;
  eid_t inst_eid;
  uint8_t *sep;
  uint32_t vol;
  int i, ii, flag;

  if (seq2_vol) { return -1; } /* error if playback is paused and 80061914 or 80061918 */
  if (midi_state == 1) { /* currently stopped? */
    if (cur_midi_eid != EID_NONE) { /* current midi entry is still open? */
      SepClose(sep_access_num); /* close the sep */
      NSClose(&cur_midi_eid, 1); /* close the midi entry */
    }
    if (*eid == EID_NONE) { /* EID_NONE was passed? */
      cur_midi_eid = EID_NONE; /* set the midi eid/entry to none */
      return 0; /* return */
    }
    midi = NSOpen(eid, 1, 1); /* open the new midi entry */
    header = (midi_header*)midi->items[0];
    cur_midi_eid = midi->eid; /* set the current midi eid */
    for (i=0;i<7;i++) { /* pre-load inst entries */
      inst_eid = header->insts[i];
      if (inst_eid != EID_NONE)
        NSOpen(&header->insts[i], 0, 1);
    }
  }
  midi_state = 2; /* set state to paused */
  midi = NSOpen(eid, 1, 0); /* open the new midi entry */
  next_midi = 0; /* clear transition midi eid? */
  for (i=0;i<7;i++) {
    header = (midi_header*)midi->items[0];
    inst_eid = header->insts[i];
    if (inst_eid != EID_NONE && !NSClose(&header->insts[i], 0)) /* failed to close inst entry? */
      return 0; /* return */
  }
  midi_state = 3; /* set state to resumed */
  seq_count = header->track_count; /* load seq count from midi header */
  if (vab_id == -1) { /* no vab has been loaded? */
    vab_id = MidiGetVab(midi);
  }
  sep = midi->items[2]; /* sep data */
  sep_access_num = SepOpen(sep, vab_id, seq_count); /* open sep data */
  if (sep_access_num == -1) /* failed to open sep data? */
    exit(-1); /* abort */
  for (ii=0;ii<seq_count;ii++) { /* init seqs */
    seq = &midi_seqs[ii];
    if (flag) { /* no other midi/vab was previously open? */
      vol = (init_vol << 16) >> 23;
      MidiSeqInit(seq, vol | (vol << 16), ii); /* init seq with initial vol */
    }
    else {
      MidiSeqInit(seq, 0, ii); /* init seq with vol at 0 */
      SepSetCrescendo(sep_access_num, seq->num, seq_vol>>7, 30); /* fade in, in 30 ticks */
      /* the other midi will have just finished fading out at this point */
    }
  }
  MidiControl(&midi_seqs[0], 0, 0); /* begin playback of master seq */
  return 0;
}

//----- (800317DC) --------------------------------------------------------
void MidiSetStateStopped() {
  if (midi_state == 0) /* playing? */
    midi_state = 1; /* set state to stopped */
}

//----- (800317F8) --------------------------------------------------------
void MidiUpdate(void *en_ref) {
  entry_ref *ref;

  ref = (entry_ref*)en_ref;
  if (midi_state == 0) { return; }
  /* fading and fade counter has reached 0? */
  if (midi_state == 4 && (midi_fade_counter == 0 || (--midi_fade_counter) == 0)) /* decrement fade counter! */
    midi_state = 1; /* stop playback (will hit MidiOpenAndPlay below) */
  if (midi_state == 3) { /* resumed/already playing? */
    if (ref->is_eid && ref->eid == cur_midi_eid
      || ref->en->eid == cur_midi_eid) /* no change in midi entry? */
      return;
    /* playback not paused, vab is already open, and haven't already scheduled a fade? */
    if (!seq2_vol && vab_id != -1 && !next_midi) {
      SepSetDecrescendo(sep_access_num, midi_seqs[0].num, seq_vol>>7, 31); /* fade out in 31 ticks */
      midi_fade_counter = 30; /* [re]set fade counter */
      next_midi = ref->en; /* set target midi entry */
      midi_state = 4; /* set state to fading */
    }
  }
  else if (midi_state > 0 && midi_state < 3) /* stopped or paused? */
    MidiOpenAndPlay(&ref->eid); /* open and play the new midi  */
}

//----- (80031938) --------------------------------------------------------
int MidiKill() {
  MidiClose(sep_access_num, 1);

#ifdef PSX
  return SsEnd();
#else
  int i;
  for (i=0;i<vab_count;i++)
    VabClose(i);
#endif
}

//----- (80031964) --------------------------------------------------------
void MidiSeqInit(midi_seq *seq, uint32_t vol, int16_t seq_num) {
  if (seq_count < 2 || seq_num != 1) { /* playing, stopped, or non-slave seq? */
    seq->voll = (int16_t)(vol >> 16);
    seq->volr = (int16_t)vol;
  }
  else { /* slave seq and either paused, resumed, or fading */
    seq->voll = seq2_vol;
    seq->volr = seq2_vol;
  }
  seq->num = seq_num;
  seq->state = -1; /* set state to inited */
  SepSetVol(sep_access_num, seq_num, seq->voll, seq->volr); /* set seq init vol */
}

//----- (800319DC) --------------------------------------------------------
void MidiClose(int16_t _sep_access_num, int close_vab) {
  if (_sep_access_num != -1)
    SepClose(_sep_access_num);
  if (close_vab && vab_id != -1)
    VabClose(vab_id);
}

//----- (80031A40) --------------------------------------------------------
void MidiControl(midi_seq *seq, int op, uint32_t arg) {
  int i;
  if (!seq) { return; }
  switch (op) {
  case 0: /* play */
    seq->state = 1; /* playing */
    SepPlay(sep_access_num, seq->num, 1, arg);
    break;
  case 1: /* stop */
    seq->state = 2; /* stopped */
    SepStop(sep_access_num, seq->num);
    break;
  case 2: /* pause */
    seq->state = 0; /* paused */
    SepPause(sep_access_num, seq->num);
    break;
  case 3: /* resume */
    seq->state = 1; /* playing */
    SepReplay(sep_access_num, seq->num);
    break;
  case 4: /* fade */
    seq->voll = (int16_t)(arg >> 16);
    seq->volr = (int16_t)(arg);
    for (i=(seq_count>1);i<seq_count;i++)
      SepSetVol(sep_access_num, seq->num, seq->voll, seq->volr);
    break;
  }
}
