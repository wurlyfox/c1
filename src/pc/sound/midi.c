/*
 * fluidsynth pc midi backend
 */
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <fluidsynth.h>

#include "midi.h"
#include "util.h"
#include "formats/smf.h"
#include "formats/psx.h"

Mix_Chunk *seq_chunks[2];
int seq_fade_ctrs[2];
extern int seq_count;
extern uint8_t *vabs[2];

/*
  fluidsynth allows soundfont (sf2) data to be loaded via file or through
  the use of a loader. it does not directly support loading from memory.
  however, a loader can be configured to do this.
  to use a loader, we define a number of callbacks. to configure
  a loader to load sf2 data from memory, we simply return a pointer directly
  to the memory location from the open callback. (the other callbacks do
  not require any implementation beyond returning appropriate codes)
  (this is based on the example from fluidsynth.org)
*/
fluid_settings_t *settings;
fluid_sfloader_t *loader;
void *(*FLUIDSYNTH_CreateFromRW)(SDL_RWops*, int);
int FLUIDSYNTH_patched = 0;
void *FLUIDSYNTH_sfmem;

static void *mem_open(const char *filename) {
  return FLUIDSYNTH_sfmem;
}
static int mem_read(void *buf, int count, void *handle) {
  return FLUID_OK;
}
static int mem_seek(void *handle, long offset, int origin) {
  return FLUID_OK;
}
static int mem_close(void *handle) {
  return FLUID_OK;
}
static long mem_tell(void *handle) {
  return 0;
}

/*
  unfortunately SDL_Mixer does not include the callback-based sf2
  loading functionality from fluidsynth, so we have to patch it
  in ourselves.

  note: this is probably bad. on the other hand we could just
  duplicate the streaming that SDL_Mixer does but this is
  considerably less code.
*/

typedef enum { MIX_MUSIC_FLUIDSYNTH=3 } Mix_MusicAPI;
typedef struct {
  const char *tag;
  Mix_MusicAPI api;
  int unused1[5];
  void *(*CreateFromRW)(SDL_RWops *src, int freesrc);
  uint8_t unused2[];
} Mix_MusicInterface;
typedef struct {
  fluid_synth_t *synth;
  fluid_settings_t *settings;
  fluid_player_t *player;
  uint8_t unused1[];
} FLUIDSYNTH_Music;

static void *FLUIDSYNTH_CreateFromRW_WithSfMem(SDL_RWops *src, int freesrc) {
  FLUIDSYNTH_Music *music;

  music = (*FLUIDSYNTH_CreateFromRW)(src, freesrc);
  fluid_synth_add_sfloader(music->synth, loader);
  fluid_synth_sfload(music->synth, "unused", 0);
  return (void*)music;
}

extern int get_num_music_interfaces();
extern Mix_MusicInterface *get_music_interface(int);
static Mix_Chunk *Mix_LoadWAV_WithSfMem(SDL_RWops *src, int freesrc, void *sf2) {
  Mix_Chunk *chunk;
  Mix_MusicInterface *interface;
  int i;

  FLUIDSYNTH_sfmem = sf2;
  if (!FLUIDSYNTH_patched) {
    for (i=0;i<get_num_music_interfaces();i++) {
      interface = get_music_interface(i);
      if (interface->api == MIX_MUSIC_FLUIDSYNTH)
        break;
    }
    FLUIDSYNTH_CreateFromRW = interface->CreateFromRW;
    interface->CreateFromRW = FLUIDSYNTH_CreateFromRW_WithSfMem;
    FLUIDSYNTH_patched = 1;
  }
  chunk = Mix_LoadWAV_RW(src, SDL_TRUE); // /* works with midi data! /
  return chunk;
}

void SwMidiInit() {
  Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 512);
  Mix_AllocateChannels(2);
  settings = new_fluid_settings();
  loader = new_fluid_defsfloader(settings);
  fluid_sfloader_set_callbacks(loader,
    mem_open,mem_read,mem_seek,mem_tell,mem_close);
}

void SwMidiKill() {
  delete_fluid_settings(settings);
  Mix_CloseAudio();
  FLUIDSYNTH_patched = 0;
}

int16_t SwSepOpen(uint8_t *sep, int vab_id, int count) {
  SDL_RWops *rw;
  Mix_Chunk *chunk;
  PThd *pthd;
  uint8_t *vab, *sf2, *seq, *mid;
  uint8_t *midi_data;
  int i, length;

  vab = vabs[vab_id];
  VabToSf2(vab, sf2);
  midi_data = (uint8_t*)malloc(0x10000);
  mid = midi_data;
  pthd = (PThd*)sep;
  seq = pthd->data;
  for (i=0;i<count;i++) {
    length = SeqToMid(seq, mid);
    mid += sizeof(MThd) + sizeof(MTrk) + length;
    seq += sizeof(SThd) + ((SThd*)seq)->length;
  }
  midi_data = realloc(midi_data, mid - midi_data);
  mid = midi_data;
  for (i=0;i<count;i++) {
    length = ((MTrk*)((MThd*)mid+1))->length;
    rw = SDL_RWFromMem(mid, length);
    chunk = Mix_LoadWAV_WithSfMem(rw, SDL_TRUE, sf2); /* works with midi data */
    seq_chunks[i] = chunk;
    seq_fade_ctrs[i] = 0;
    mid += sizeof(MThd) + sizeof(MTrk) + length;
  }
  mid = midi_data;
  free(sf2);
  free(mid);
  return 0;
}

void SwSepClose() {
  int i;

  for (i=0;i<seq_count;i++) {
    Mix_FreeChunk(seq_chunks[i]);
    seq_fade_ctrs[i] = 0;
  }
}

void SwSepSetVol(int san, int seq_num, uint32_t voll, uint32_t volr) {
  uint32_t vol;

  vol = max(voll, volr);
  Mix_Volume(seq_num, vol/128);
  Mix_SetPanning(seq_num, (voll*255)/16384, (volr*255)/16384);
}

void SwSepPlay(int san, int seq_num, int mode, int loops) {
  Mix_Chunk *chunk;
  int fade_ms;

  /* mode ignored for now */
  chunk = seq_chunks[seq_num];
  fade_ms = seq_fade_ctrs[seq_num];
  if (!fade_ms)
    Mix_PlayChannel(seq_num, chunk, loops-1);
  else {
    Mix_FadeInChannel(seq_num, chunk, loops-1, fade_ms);
    seq_fade_ctrs[seq_num] = 0;
  }
}

void SwSepStop(int san, int seq_num) {
  Mix_HaltChannel(seq_num);
}

void SwSepPause(int san, int seq_num) {
  Mix_Pause(seq_num);
}

void SwSepReplay(int san, int seq_num) {
  Mix_Resume(seq_num);
}

void SwSepSetCrescendo(int san, int seq_num, uint32_t vol, int ticks) {
  seq_fade_ctrs[seq_num] = (ticks*1000000)/16667;
  Mix_Volume(seq_num, vol/128);
}

void SwSepSetDecrescendo(int san, int seq_num, uint32_t vol, int ticks) {
  if (seq_fade_ctrs[seq_num]) { return; } /* for now */
  Mix_Volume(seq_num, vol/128);
  Mix_FadeOutChannel(seq_num, (ticks*1000000)/16667);
}
