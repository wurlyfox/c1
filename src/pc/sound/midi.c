/*
 * fluidsynth pc midi backend
 */
#include <fluidsynth.h>

#include "midi.h"
#include "util.h"
#include "formats/smf.h"
#include "formats/psx.h"
#include "pc/time.h"

extern int seq_count;
extern uint8_t *vabs[2];

typedef struct {
  fluid_synth_t *synth;
  fluid_player_t *player;
  int sfid;
  float vol[2];
  int fading;
  uint32_t fade_start;
  uint32_t fade_len;
  float fade_vol[2];
  float fade_rate;
} midi_context_t;

static const midi_context_t def_context = {
  .sfid = -1,
  .vol = { 1.0, 1.0 }
};

fluid_settings_t *settings;
fluid_sfloader_t *loader = 0;
midi_context_t midi_contexts[2] = { 0 };

/* in-memory sf2 loader */
typedef struct {
  char dummy[4];
  uint8_t *data;
  size_t size;
} mbuf_t;

typedef struct {
  uint8_t *start;
  uint8_t *end;
  uint8_t *p;
} mstream_t;

static void *mem_open(const char *filename) {
  mstream_t *stream;
  mbuf_t *buf;

  buf = (mbuf_t*)filename;
  stream = (mstream_t*)malloc(sizeof(mstream_t));
  stream->start = buf->data;
  stream->end = buf->data + buf->size;
  stream->p = buf->data;
  return (void*)stream;
}
static int mem_read(void *buf, int count, void *handle) {
  mstream_t *stream;

  stream = (mstream_t*)handle;
  memcpy(buf, stream->p, count);
  stream->p += count;
  return FLUID_OK;
}
static int mem_seek(void *handle, long offset, int origin) {
  mstream_t *stream;

  stream = (mstream_t*)handle;
  if (origin==SEEK_SET)
    stream->p = stream->start + offset;
  else if (origin==SEEK_CUR)
    stream->p += offset;
  else if (origin==SEEK_END)
    stream->p = stream->end + offset;
  return FLUID_OK;
}
static int mem_close(void *handle) {
  mstream_t *stream;

  stream = (mstream_t*)handle;
  free(stream);
  return FLUID_OK;
}
static long mem_tell(void *handle) {
  mstream_t *stream;
  long offset;

  stream = (mstream_t*)handle;
  offset = stream->p - stream->start;
  return offset;
}

static void SwPlayerProcess(midi_context_t *context, int ch, float amp[2], float freq, int len, int16_t *data) {
  fluid_synth_t *synth;
  float *vol;
  int16_t *s, *d;
  int16_t buf[2048];
  int i;

  vol = context->vol;
  synth = context->synth;
  fluid_synth_write_s16(synth, len, buf, 0, 2, buf, 1, 2);
  d = data;
  s = buf;
  for (i=0;i<len;i++) {
    *(d++) = (int16_t)(*(s++)*vol[0]);
    *(d++) = (int16_t)(*(s++)*vol[1]);
  }
}

void SwMidiProcess(int ch, float amp[2], float freq, int len, int16_t *data) {
  midi_context_t *context;
  int32_t *p32, buf32[2048];
  int16_t *p16, buf16[2048];
  int i, j;

  memset(buf32, 0, 2048*sizeof(int32_t));
  memset(buf16, 0, 2048*sizeof(int16_t));
  for (i=0;i<2;i++) {
    context = &midi_contexts[i];
    if (context->fading) {
      float *vol, *fade_vol, fade_rate;
      int ticks, fade_start, fade_len;
      vol = context->vol;
      fade_vol = context->fade_vol;
      fade_start = context->fade_start;
      fade_len = context->fade_len;
      fade_rate = context->fade_rate;
      ticks = GetTicksElapsed() / 34;
      ticks -= fade_start;
      vol[0] = fade_vol[0] + ((float)ticks*fade_rate); 
      vol[1] = fade_vol[1] + ((float)ticks*fade_rate);
      if (ticks > fade_len)
        context->fading = 0;
    }
    SwPlayerProcess(context, ch, amp, freq, len, buf16);
    p16 = buf16;
    p32 = buf32;
    for (j=0;j<len;j++) {
      *(p32++) += (int32_t)*(p16++);
      *(p32++) += (int32_t)*(p16++);
    }
  }
  p16 = data;
  p32 = buf32;
  for (i=0;i<len;i++) {
    *(p16++) = *(p32++)/2;
    *(p16++) = *(p32++)/2;
  }
}

void SwMidiInit() {
  midi_context_t *context;
  fluid_synth_t *synth;
  int i;

  settings = new_fluid_settings();
  loader = new_fluid_defsfloader(settings);
  fluid_sfloader_set_callbacks(loader,
    mem_open,mem_read,mem_seek,mem_tell,mem_close);
  for (i=0;i<2;i++) {
    context = &midi_contexts[i];
    *context = def_context;
    synth = new_fluid_synth(settings);
    fluid_synth_add_sfloader(synth, loader);
    context->synth = synth;
  }
}

void SwMidiKill() {
  midi_context_t *context;
  fluid_synth_t *synth;
  fluid_player_t *player;
  int i, sfid;

  for (i=0;i<2;i++) {
    context = &midi_contexts[i];
    synth = context->synth;
    player = context->player;
    sfid = context->sfid;
    if (player) {
      fluid_player_stop(player);
      delete_fluid_player(player);
    }
    if (sfid != -1)
      fluid_synth_sfunload(synth, sfid, 1);
    delete_fluid_synth(synth);
  }
  delete_fluid_sfloader(loader);
  delete_fluid_settings(settings);
}

int16_t SwSepOpen(uint8_t *sep, int vab_id, int count) {
  midi_context_t *context;
  fluid_synth_t *synth;
  fluid_player_t *player;
  fluid_preset_t *preset;
  mbuf_t buf;
  PThd *pthd;
  size_t sf2_size, seq_size, mid_size;
  size_t midi_sizes[32];
  uint8_t *vab, *sf2, *seq, *mid;
  uint8_t *midi_data;
  int i, sfid;

  for (i=0;i<count;i++) {
    context = &midi_contexts[i];
    synth = context->synth;
    sfid = context->sfid;
    player = context->player;
    if (player) {
      fluid_player_stop(player);
      delete_fluid_player(player);
    }
    if (sfid != -1)
      fluid_synth_sfunload(synth, sfid, 1);
    *context = def_context;
    context->synth = synth;
  }
  vab = vabs[vab_id];
  sf2 = (uint8_t*)malloc(0x400000);
  sf2_size = VabToSf2(vab, sf2);
  strcpy(buf.dummy, "buf");
  buf.data = sf2;
  buf.size = sf2_size;
  for (i=0;i<count;i++) {
    context = &midi_contexts[i];
    synth = context->synth;
    sfid = fluid_synth_sfload(synth, (const char*)&buf, 0);
    context->sfid = sfid;
  }
  midi_data = (uint8_t*)malloc(0x80000);
  mid = midi_data;
  pthd = (PThd*)sep;
  seq = pthd->data;
  for (i=0;i<count;i++) {
    mid_size = SeqToMid(seq, mid, &seq_size);
    midi_sizes[i] = mid_size;
    mid += mid_size;
    seq += seq_size;
  } 
  mid = midi_data;
  for (i=0;i<count;i++) {
    mid_size = midi_sizes[i];
    context = &midi_contexts[i];
    synth = context->synth;
    sfid = context->sfid;
    player = new_fluid_player(synth);
    fluid_player_add_mem(player, mid, mid_size);
    fluid_player_stop(player);
    context->player = player;
    mid += mid_size;
  }
  mid = midi_data;
  free(sf2);
  free(mid);
  return 0;
}

void SwSepClose(int san) {
  midi_context_t *context;
  fluid_synth_t *synth;
  fluid_player_t *player;
  int i, sfid;
  
  for (i=0;i<2;i++) { 
    context = &midi_contexts[i];
    synth = context->synth;
    sfid = context->sfid;
    player = context->player;
    if (player) {
      fluid_player_stop(player);
      delete_fluid_player(player);
    }
    if (sfid != -1)
      fluid_synth_sfunload(synth, sfid, 1);
    *context = def_context;
    context->synth = synth;
  }
}

void SwSepSetVol(int san, int seq_num, uint32_t voll, uint32_t volr) {
  midi_context_t *context;

  context = &midi_contexts[seq_num];
  context->vol[0] = (double)voll/0x7F;
  context->vol[1] = (double)volr/0x7F;
}

void SwSepPlay(int san, int seq_num, int mode, int loops) {
  midi_context_t *context;
  fluid_player_t *player;

  context = &midi_contexts[seq_num];
  player = context->player;
  fluid_player_set_loop(player, mode==1?-1:loops);
  fluid_player_seek(player, 0);
  fluid_player_play(player);
}

void SwSepStop(int san, int seq_num) {
  midi_context_t *context;
  fluid_player_t *player;

  context = &midi_contexts[seq_num];
  player = context->player;
  fluid_player_stop(player);
  fluid_player_seek(player, 0);
}

void SwSepPause(int san, int seq_num) {
  midi_context_t *context;
  fluid_player_t *player;

  context = &midi_contexts[seq_num];
  player = context->player;
  fluid_player_stop(player);
}

void SwSepReplay(int san, int seq_num) {
  midi_context_t *context;
  fluid_player_t *player;

  context = &midi_contexts[seq_num];
  player = context->player;
  fluid_player_play(player);
}

void SwSepSetCrescendo(int san, int seq_num, uint32_t vol, int ticks) {
  midi_context_t *context;
  fluid_player_t *player;
  int base;

  context = &midi_contexts[seq_num];
  player = context->player;
  base = GetTicksElapsed() / 34;
  context->fade_start = base;
  context->fade_len = ticks;
  context->fade_vol[0] = context->vol[0];
  context->fade_vol[1] = context->vol[1];
  context->fade_rate = ((double)vol/0x7F)/(double)ticks;
  context->fading = 1;
}

void SwSepSetDecrescendo(int san, int seq_num, uint32_t vol, int ticks) {
  midi_context_t *context;
  fluid_player_t *player;
  int base;

  context = &midi_contexts[seq_num];
  player = context->player;
  base = GetTicksElapsed() / 34;
  context->fade_start = base;
  context->fade_len = ticks;
  context->fade_vol[0] = context->vol[0];
  context->fade_vol[1] = context->vol[1];
  context->fade_rate = -((double)vol/0x7F)/(double)ticks;
  context->fading = 1;
}
