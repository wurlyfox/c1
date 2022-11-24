/*
 * fluidsynth pc audio backend
 */
#include "audio.h"

#ifdef CFLAGS_SFX_FLUIDSYNTH

#include <fluidsynth.h>

fluid_settings_t *asettings;
fluid_synth_t *synth;
fluid_sfloader_t *aloader;
fluid_audio_driver_t *adriver;
fluid_preset_t *presets[24] = { 0 };
fluid_sample_t *samples[24] = { 0 };
fluid_voice_t *fluid_voices[24] = { 0 };
uint32_t sample_lengths[24] = { 0 };
uint32_t keys_on[24] = { 0 };

typedef struct {
  int bank;
  int num;
  const char *name;
} fluid_preset_info_t;

fluid_preset_info_t preset_infos[24] ={
  {.bank = 0, .num =  0, .name = "Preset 01" },
  {.bank = 0, .num =  1, .name = "Preset 02" },
  {.bank = 0, .num =  2, .name = "Preset 03" },
  {.bank = 0, .num =  3, .name = "Preset 04" },
  {.bank = 0, .num =  4, .name = "Preset 05" },
  {.bank = 0, .num =  5, .name = "Preset 06" },
  {.bank = 0, .num =  6, .name = "Preset 07" },
  {.bank = 0, .num =  7, .name = "Preset 08" },
  {.bank = 0, .num =  8, .name = "Preset 09" },
  {.bank = 0, .num =  9, .name = "Preset 10" },
  {.bank = 0, .num = 10, .name = "Preset 11" },
  {.bank = 0, .num = 11, .name = "Preset 12" },
  {.bank = 0, .num = 12, .name = "Preset 13" },
  {.bank = 0, .num = 13, .name = "Preset 14" },
  {.bank = 0, .num = 14, .name = "Preset 15" },
  {.bank = 0, .num = 15, .name = "Preset 16" },
  {.bank = 0, .num = 16, .name = "Preset 17" },
  {.bank = 0, .num = 17, .name = "Preset 18" },
  {.bank = 0, .num = 18, .name = "Preset 19" },
  {.bank = 0, .num = 19, .name = "Preset 20" },
  {.bank = 0, .num = 20, .name = "Preset 21" },
  {.bank = 0, .num = 21, .name = "Preset 22" },
  {.bank = 0, .num = 22, .name = "Preset 23" },
  {.bank = 0, .num = 23, .name = "Preset 24" }
};

static const char *preset_get_name(fluid_preset_t *preset) {
  fluid_preset_info_t *preset_info;

  preset_info = fluid_preset_get_data(preset);
  return preset_info->name;
}

static int preset_get_bank(fluid_preset_t *preset) {
  fluid_preset_info_t *preset_info;

  preset_info = fluid_preset_get_data(preset);
  return preset_info->bank;
}

static int preset_get_num(fluid_preset_t *preset) {
  fluid_preset_info_t *preset_info;

  preset_info = fluid_preset_get_data(preset);
  return preset_info->num;
}

static int preset_noteon(fluid_preset_t *preset, fluid_synth_t *synth, int chan, int key, int vel) {
  fluid_sample_t *sample;
  fluid_voice_t *voice;

  sample = samples[chan];
  voice = fluid_synth_alloc_voice(synth, sample, chan, key, 127);
  fluid_synth_start_voice(synth, voice);
  fluid_voices[chan] = voice;
}

static const char *sfont_get_name(fluid_sfont_t *sfont) {
  return "General MIDI";
}

static fluid_preset_t *create_preset(int prenum, fluid_sfont_t *sfont) {
  fluid_preset_t *preset;

  preset = new_fluid_preset(sfont,
    preset_get_name,
    preset_get_bank,
    preset_get_num,
    preset_noteon,
    delete_fluid_preset);
  fluid_preset_set_data(preset, &preset_infos[prenum]);
  presets[prenum] = preset;
  return preset;
}

static fluid_preset_t *sfont_get_preset(fluid_sfont_t *sfont, int bank, int prenum) {
  fluid_preset_t *preset;

  preset = presets[prenum];
  if (!preset)
    preset = create_preset(prenum, sfont);
  return preset;
}

static fluid_sfont_t *sfloader_load(fluid_sfloader_t *loader, const char *filename) {
  fluid_sfont_t *sfont;

  sfont = new_fluid_sfont(sfont_get_name,
    sfont_get_preset,
    NULL,
    NULL,
    delete_fluid_sfont);
  return sfont;
}

void SwAudioInit() {
  int sfont_id, i;

  asettings = new_fluid_settings();
  fluid_settings_setstr(asettings, "audio.driver", "sdl2");
  fluid_settings_setint(asettings, "audio.periods", 4);
  fluid_settings_setint(asettings, "audio.period-size", 2048);
  fluid_settings_setint(asettings, "synth.midi-channels", 24);
  synth = new_fluid_synth(asettings);
  aloader = new_fluid_sfloader(sfloader_load, delete_fluid_sfloader);
  fluid_synth_add_sfloader(synth, aloader);
  sfont_id = fluid_synth_sfload(synth, "unused", 0);
  adriver = new_fluid_audio_driver(asettings, synth);
  for (i=0;i<24;i++) {
    // fluid_synth_program_change(synth, i, i);
    fluid_synth_program_select(synth, i, sfont_id, 0, i);
  }
}

void SwAudioKill() {
  int i;

  for (i=0;i<24;i++) {
    if (samples[i]) {
      delete_fluid_sample(samples[i]);
      samples[i] = 0;
    }
  }
  delete_fluid_audio_driver(adriver);
  delete_fluid_synth(synth);
  delete_fluid_settings(asettings);
}

void SwSetMVol(uint32_t vol) {
  /* TODO: linear or log? */
  fluid_synth_set_gain(synth, (float)(vol/(127)));
}

uint8_t pcm_buf[0x100000];
void SwLoadSample(int voice_idx, uint8_t *data, size_t size) {
  fluid_sample_t *sample;

  if (samples[voice_idx])
    SwUnloadSample(voice_idx);
  size = ADPCMToPCM16(data, size, pcm_buf);
  /* assuming 16 bit sample points, nbframes = size / 2 */
  sample = new_fluid_sample();
  fluid_sample_set_sound_data(sample, (short*)pcm_buf, NULL, size/2, 11025, 0);
  samples[voice_idx] = sample;
  sample_lengths[voice_idx] = (size*17)/11025;
}

void SwUnloadSample(int voice_idx) {
  delete_fluid_sample(samples[voice_idx]);
  samples[voice_idx] = 0;
}

void SwNoteOn(int voice_idx) {
  if (keys_on[voice_idx]) { return; }
  keys_on[voice_idx] = sample_lengths[voice_idx];
  fluid_synth_noteon(synth, voice_idx, 1, 127);
}

void SwNoteOff(int voice_idx) {
  if (!keys_on[voice_idx]) { return; }
  keys_on[voice_idx] = 0;
  fluid_synth_noteoff(synth, voice_idx, 1);
}

void SwVoiceSetVolume(int voice_idx, uint32_t voll, uint32_t volr) {
  int32_t pan;
  uint32_t volume, sum;

  sum = volr+voll;
  if (sum != 0) {
    pan = 500*(volr-voll)/sum;
    volume = ((1000*voll)/(500-pan)) >> 7;
  }
  else {
    pan = 0;
    volume = 0;
  }
  /* attenuation is mapped to cc 7 by default */
  fluid_synth_set_gen(synth, voice_idx, GEN_PAN, (float)pan);
  fluid_synth_cc(synth, voice_idx, 7, volume);
}

void SwVoiceSetPitch(int voice_idx, uint32_t pitch) {
  fluid_synth_pitch_bend(synth, voice_idx, pitch);
}

void SwGetAllKeysStatus(uint8_t *status) {
  fluid_voice_t *voice;
  int i, on;
  for (i=0;i<24;i++) {
    if (keys_on[i] == 1) {
      keys_on[i] = 0;
      status[i] = 0;
      SwNoteOff(i);
    }
    else if (keys_on[i]) {
      --keys_on[i];
      status[i] = 1;
    }
  }
}

#else

void SwAudioInit() {}
void SwAudioKill() {}
void SwSetMVol(uint32_t vol) {}
void SwLoadSample(int voice_idx, uint8_t *data, size_t size) {}
void SwUnloadSample(int voice_idx) {}
void SwNoteOn(int voice_idx) {}
void SwNoteOff(int voice_idx) {}
void SwVoiceSetVolume(int voice_idx, uint32_t voll, uint32_t volr) {}
void SwVoiceSetPitch(int voice_idx, uint32_t pitch) {}
void SwGetAllKeysStatus(uint8_t *status) {}

#endif
