/*
 * psx seq/vab conversion
 */
#include "util.h"
#include "formats/psx.h"
#include "formats/smf.h"
#include "formats/sf2.h"

static inline int _setnc(char *dst, char *src, int slen) {
  int i;

  for (i=0;i<slen;i++)
    *(dst++) = *(src++);
  if (slen % 2) { /* str must be uint16_t aligned */
    *(dst++) = 0; /* pad with additional null term */
    ++slen;
  }
  return slen;
}

static inline void _padnc(char *dst, char *src, int dlen, int slen) {
  int i;

  for (i=0;i<slen;i++)
    *(dst++) = *(src++);
  for (;i<dlen;i++)
    *(dst++) = 0;
}

static inline void zeromem(void *dst, int size) {
  uint8_t *b;
  int i;

  b = (uint8_t*)dst;
  for (i=0;i<size;i++)
    *(b++) = 0;
}

#define setnc(d,s) _setnc(d,s,sizeof(s))
#define padnc(d,s) _padnc(d,s,sizeof(d),sizeof(s))

#define Next(g,t,d) \
d = (t*)g; \
g += sizeof(t)

int ADPCMToPCM16(uint8_t *adpcm, size_t size, uint8_t *pcm) {
  const int f0[16] ={ 0,60,115, 98,122,0,0,0,0,0,0,0,0,0,0,0 };
  const int f1[16] ={ 0, 0,-52,-55,-60,0,0,0,0,0,0,0,0,0,0,0 };
  VagLine *line;
  double s0, s1, tmp;
  int16_t *ld;
  uint8_t *start;
  int i, j;

  start = pcm;
  s0 = 0; s1 = 0;
  for (i=0;i<size/sizeof(VagLine);i++) {
    Next(adpcm, VagLine, line);
    for (j=0;j<14;j++) {
      Next(pcm, int16_t, ld);
      tmp = s0*(double)f0[line->predict]+s1*(double)f1[line->predict];
      *ld=((int16_t)(line->data[j].adl<<12))>>line->factor;
      s1 = s0;
      s0 = (double)(*ld)+(tmp/64);
      *ld=(int16_t)s0;
      Next(pcm, int16_t, ld);
      tmp = s0*(double)f0[line->predict]+s1*(double)f1[line->predict];
      *ld=((int16_t)(line->data[j].adh<<12))>>line->factor;
      s1 = s0;
      s0 = (double)(*ld)+(tmp/64);
      *ld=(int16_t)s0;
    }
  }
  return (int)(pcm - start);
}

#define SeqNext(s,t,d) Next(s,t,d)
#define MidNext(m,t,d) Next(m,t,d)

int SeqToMid(uint8_t *seq, uint8_t *mid) {
  MThd *mthd; SThd *sthd;
  MTrk *mtrk;
  MMev *mmev; SMev *smev;
  MEvt *mevt; SEvt *sevt;
  MEvr *mevr; SEvr *sevr;
  MMed m;
  int type, length, i;

  type = 7;
  SeqNext(seq, SThd, sthd);
  MidNext(mid, MThd, mthd);
  set4c(mthd->type, "Mthd");
  mthd->length = 6;
  mthd->format = 0;
  mthd->tracks = 1; /* single track */
  mthd->tpqn = sthd->tpqn;
  MidNext(mid, MTrk, mtrk);
  set4c(mtrk->type, "Mtrk");
  MidNext(mid, MMev, mmev);
  mmev->hdr = MMEV_HDR_SET_TEMPO;
  mmev->length = sizeof(MTmp);
  mmev->delta_time = 0;
  MidNext(mid, MTmp, m.tmp);
  set3b(*m.tmp, sthd->init_tempo);
  MidNext(mid, MMev, mmev);
  mmev->hdr = MMEV_HDR_TIME_SIGNATURE;
  mmev->length = sizeof(MTsg);
  mmev->delta_time = 0;
  MidNext(mid, MTsg, m.tsg);
  m.tsg->num = sthd->tsg_num;
  m.tsg->denom = sthd->tsg_denom;
  m.tsg->cpt = 24;
  m.tsg->npc = 8;
  while (1) {
    SeqNext(seq, SEvt, sevt);
    if (!sevt->has_status && type < 7) {
      sevr = (SEvr*)sevt;
      MidNext(mid, MEvr, mevr);
      *mevr = *((MEvr*)sevr);
      length = type != 4 && type != 5 ? 2: 1;
    }
    else if (sevt->type < 7) {
      type = sevt->type;
      MidNext(mid, MEvt, mevt);
      *mevt = *((MEvt*)sevt);
      length = type != 4 && type != 5 ? 2: 1;
    }
    else if (sevt->status == 0xFF) {
      smev = (SMev*)sevt;
      MidNext(mid, MMev, mmev);
      switch (smev->type) {
      case 0x2F:
        length = 0;
        break;
      case 0x51:
        length = sizeof(MTmp);
        break;
      case 0x54:
        length = 5;
        break;
      case 0x58:
        length = sizeof(MTsg);
        break;
      case 0x59:
        length = sizeof(MKsg);
        break;
      }
      mmev->delta_time = smev->delta_time;
      mmev->hdr = smev->hdr;
      mmev->length = length;
    }
    if (smev->type == 0x2F) { break; }
    /* copy data */
    for (i=0;i<length;i++)
      *(mid++) = *(seq++);
  }
  length = mid - mtrk->data;
  mtrk->length = length;
  return length;
}

#define Sf2Next(s,t,d) Next(s,t,d)

#define Sf2NextCk(s,c,n) \
Sf2Next(s,CK,c); \
set4c(c->ckID,n)

#define Sf2NextCd(s,c,n,t,d) \
Sf2NextCk(s,c,n); \
c->ckSize = sizeof(t); \
Sf2Next(s,t,d)

#define Sf2NextCs(s,c,n,a) \
Sf2NextCk(s,c,n); \
c->ckSize = setnc((char*)s,a); \
s += c->ckSize

#define VabNext(v,t,d) Next(v,t,d)

/*
VAB file        => bank
ProgAtr/Program => preset
VagAtr/Tone     => zone

mapping progress:
prior    todo
mode     todo (need more info)
vol      todo
vibW     not going to map for now
vibT     not going to map for now
porW     not mappable?
porT     not mappable?
adsr     todo (difficult)
*/

typedef struct {
  ProgAtr *atr;
  VagAtr *vagatr[16];
  VagBody *waves[16];
} VabProg;

int VabToSf2(uint8_t *vab, uint8_t *sf2) {
  VabProg *prog, progs[128];
  VagBody *waves[254], *wave;
  VagAtr *atr;
  VabHead *vh; VabBody *vb;
  CK *ck0, *ck1, *ck2, *ck3;
  iRC irc; hRC hrc;
  sfPresetHeader *presets[128];
  sfPresetBag *pbags[128], *tpbag;
  sfInst *insts[128];
  sfInstBag *ibags[128][17], *tibag;
  uint32_t wave_offs;
  int i, j, prog_count, zone_count, sample_count;

  vh = &((Vab*)vab)->head;
  vb =  ((Vab*)vab)->body;
  wave_offs = 0;
  for (i=0;i<vh->hdr.vs;j++) {
    waves[i] = (VagBody*)&vb[wave_offs];
    wave_offs += vh->wave_sizes[i] << 4;
  }
  for (i=0;i<128;j++) {
    prog = &progs[i];
    prog->atr = &vh->atrs[j];
    for (j=0;j<prog->atr->tones;j++) {
      prog->vagatr[j] = &vh->vagatrs[i][j];
      prog->waves[j] = waves[prog->vagatr[j]->vag];
    }
  }
  Sf2NextCk(sf2, ck0, "sfbk");
  Sf2NextCk(sf2, ck1, "INFO");
  Sf2NextCd(sf2, ck2, "ifil", sfVersionTag, irc.ifil);
  irc.ifil->wMajor = 2;
  irc.ifil->wMinor = 8; /* 2.08 */
  Sf2NextCs(sf2, ck2, "isng", "EMU8080");
  Sf2NextCs(sf2, ck2, "INAM", "General MIDI");
  ck1->ckSize = (sf2 - (uint8_t*)ck1) - 6;
  Sf2NextCk(sf2, ck1, "sdta");
  for (i=0;i<vh->hdr.vs;i++) {
    wave = waves[i];
    Sf2NextCk(sf2, ck2, "smpl");
    sf2 += ADPCMToPCM16(wave, vh->wave_sizes[i], sf2);
    ck2->ckSize = (sf2 - (uint8_t*)ck2) - 6;
  }
  ck1->ckSize = (sf2 - (uint8_t*)ck1) - 6;
  Sf2NextCk(sf2, ck1, "pdta");
  Sf2NextCk(sf2, ck2, "PHDR");
  prog_count = 0;
  for (i=0;i<128;j++) {
    prog = &progs[i];
    if (prog->atr->tones == 0) { continue; }
    Sf2Next(sf2, sfPresetHeader, hrc.phdr);
    padnc(hrc.phdr->achPresetName, "preset");
    hrc.phdr->achPresetName[6] = '0'+(i/100);
    hrc.phdr->achPresetName[7] = '0'+((i/10)%10);
    hrc.phdr->achPresetName[8] = '0'+(i%10);
    hrc.phdr->wPreset = i;
    hrc.phdr->wBank = 0;
    hrc.phdr->wPresetBagNdx = prog_count;
    hrc.phdr->dwLibrary = 0;
    hrc.phdr->dwGenre = 0;
    hrc.phdr->dwMorphology = 0;
    presets[i] = hrc.phdr;
    ++prog_count;
  }
  ck2->ckSize = (sf2 - (uint8_t*)ck2) - 6;
  Sf2NextCk(sf2, ck2, "PBAG");
  for (i=0;i<128;i++) {
    prog = &progs[i];
    if (prog->atr->tones == 0) { continue; }
    Sf2Next(sf2, sfPresetBag, hrc.pbag);
    pbags[i] = hrc.pbag;
  }
  Sf2Next(sf2, sfPresetBag, hrc.pbag);
  tpbag = hrc.pbag;
  ck2->ckSize = (sf2 - (uint8_t*)ck2) - 6;
  Sf2NextCk(sf2, ck2, "PMOD");
  Sf2Next(sf2, sfModList, hrc.pmod);
  *((uint16_t*)hrc.pmod) = 0;
  ck2->ckSize = (sf2 - (uint8_t*)ck2) - 6;
  Sf2NextCk(sf2, ck2, "PGEN");
  prog_count = 0;
  for (i=0;i<128;j++) {
    prog = &progs[i];
    if (prog->atr->tones == 0) { continue; }
    pbags[i]->wModNdx = 0;
    pbags[i]->wGenNdx = ((sf2 - (uint8_t*)ck2) - 6) / sizeof(sfGenList);
    Sf2Next(sf2, sfGenList, hrc.pgen);
    hrc.pgen->sfGenOper = instrument;
    hrc.pgen->genAmount.shAmount = prog_count; /* instr idx */
  }
  Sf2Next(sf2, sfGenList, hrc.pgen);
  *((uint16_t*)hrc.pgen) = 0;
  ck2->ckSize = (sf2 - (uint8_t*)ck2) - 6;
  Sf2NextCk(sf2, ck2, "INST");
  prog_count = 0;
  for (i=0;i<128;i++) {
    prog = &progs[i];
    if (prog->atr->tones == 0) { continue; }
    Sf2Next(sf2, sfInst, hrc.inst);
    padnc(hrc.inst->achInstName, "inst");
    hrc.inst->achInstName[4] = '0'+(i/100);
    hrc.inst->achInstName[5] = '0'+((i/10)%10);
    hrc.inst->achInstName[6] = '0'+(i%10);
    insts[i] = hrc.inst;
    ++prog_count;
  }
  Sf2Next(sf2, sfInst, hrc.inst);
  zeromem(hrc.inst, sizeof(sfInst));
  hrc.inst->wInstBagNdx = prog_count;
  ck2->ckSize = (sf2 - (uint8_t*)ck2) - 6;
  zone_count = 0;
  Sf2NextCk(sf2, ck2, "IBAG");
  for (i=0;i<128;i++) {
    prog = &progs[i];
    if (prog->atr->tones == 0) { continue; }
    insts[i]->wInstBagNdx = zone_count;
    for (j=0;j<prog->atr->tones+1;j++) {
      Sf2Next(sf2, sfInstBag, hrc.ibag);
      ibags[i][j] = hrc.ibag;
      ++zone_count;
    }
  }
  Sf2Next(sf2, sfInstBag, hrc.ibag);
  hrc.ibag->wInstGenNdx = zone_count;
  hrc.ibag->wInstModNdx = 0;
  ck2->ckSize = (sf2 - (uint8_t*)ck2) - 6;
  Sf2NextCk(sf2, ck2, "IMOD");
  for (i=0;i<128;i++) {
    prog = &progs[i];
    if (prog->atr->tones == 0) { continue; }
    for (j=0;j<prog->atr->tones;j++) {
      atr = prog->vagatr[j];
      ibags[i][j]->wInstModNdx = ((sf2 - (uint8_t*)ck2) - 6) / sizeof(sfInstModList);
      /* pbmin 0-127, 127=1 octave */
      /* 1 = 12/127 semitone */
      /* (pbmin*12)/127 = semitones
      *  (((pbmin*12)%127)*100)/127) = cents */
      Sf2Next(sf2, sfInstModList, hrc.imod);
      setsfm(hrc.imod->sfModSrcOper, pitchWheel, 0, 1, 0, linear); /* - unipolar */
      hrc.imod->sfModDestOper = coarseTune;
      hrc.imod->modAmount = (atr->pbmin*12)/127; /* semitones */
      setsfm(hrc.imod->sfModAmtSrcOper, pitchWheelSens, 0, 0, 0, linear);
      hrc.imod->sfModTransOper = linear;
      Sf2Next(sf2, sfInstModList, hrc.imod);
      setsfm(hrc.imod->sfModSrcOper, pitchWheel, 0, 1, 0, linear); /* - unipolar */
      hrc.imod->sfModDestOper = fineTune;
      hrc.imod->modAmount = (((atr->pbmin*12)%127)*100)/127; /* cents */
      setsfm(hrc.imod->sfModAmtSrcOper, pitchWheelSens, 0, 0, 0, linear);
      hrc.imod->sfModTransOper = linear;
      Sf2Next(sf2, sfInstModList, hrc.imod);
      setsfm(hrc.imod->sfModSrcOper, pitchWheel, 0, 0, 0, linear); /* + unipolar */
      hrc.imod->sfModDestOper = coarseTune;
      hrc.imod->modAmount = (atr->pbmax*12)/127; /* semitones */
      setsfm(hrc.imod->sfModAmtSrcOper, pitchWheelSens, 0, 0, 0, linear);
      hrc.imod->sfModTransOper = linear;
      Sf2Next(sf2, sfInstModList, hrc.imod);
      setsfm(hrc.imod->sfModSrcOper, pitchWheel, 0, 0, 0, linear); /* + unipolar */
      hrc.imod->sfModDestOper = fineTune;
      hrc.imod->modAmount = (((atr->pbmax*12)%127)*100)/127; /* cents */
      setsfm(hrc.imod->sfModAmtSrcOper, pitchWheelSens, 0, 0, 0, linear);
      hrc.imod->sfModTransOper = linear;
    }
  }
  Sf2Next(sf2, sfInstModList, hrc.imod);
  zeromem(hrc.imod, sizeof(sfInstModList));
  ck2->ckSize = (sf2 - (uint8_t*)ck2) - 6;
  Sf2NextCk(sf2, ck2, "IGEN");
  for (i=0;i<128;i++) {
    prog = &progs[i];
    for (j=0;j<prog->atr->tones;j++) {
      atr = prog->vagatr[j];
      ibags[i][j]->wInstGenNdx = ((sf2 - (uint8_t*)ck2) - 6) / sizeof(sfInstGenList);
      Sf2Next(sf2, sfInstGenList, hrc.igen);
      hrc.igen->sfGenOper = keyRange;
      hrc.igen->genAmount.ranges.byLo = atr->min;
      hrc.igen->genAmount.ranges.byHi = atr->max;
      Sf2Next(sf2, sfInstGenList, hrc.igen);
      hrc.igen->sfGenOper = pan;
      hrc.igen->genAmount.shAmount = ((atr->pan-64)*500)/64;
      Sf2Next(sf2, sfInstGenList, hrc.igen);
      hrc.igen->sfGenOper = overridingRootKey;
      hrc.igen->genAmount.shAmount = atr->center;
      Sf2Next(sf2, sfInstGenList, hrc.igen);
      hrc.igen->sfGenOper = fineTune;
      hrc.igen->genAmount.shAmount = -(atr->shift*99)/127; /* is this correct? */
      Sf2Next(sf2, sfInstGenList, hrc.igen);
      hrc.igen->sfGenOper = sampleID;
      hrc.igen->genAmount.shAmount = atr->vag;
    }
  }
  Sf2Next(sf2, sfInstGenList, hrc.igen);
  zeromem(hrc.igen, sizeof(sfInstGenList));
  ck2->ckSize = (sf2 - (uint8_t*)ck2) - 6;
  Sf2NextCk(sf2, ck2, "SHDR");
  for (i=0;i<vh->hdr.vs;i++) {
    Sf2Next(sf2, sfSample, hrc.shdr);
    hrc.shdr->dwStart = 0; /* ? */
    hrc.shdr->dwEnd = vh->wave_sizes[i]*28; /* 4:7 */
    hrc.shdr->dwStartloop = 0;
    hrc.shdr->dwEndloop = vh->wave_sizes[i]*28;
    hrc.shdr->dwSampleRate = 44100;
    hrc.shdr->byOriginalKey = 60;
    hrc.shdr->chCorrection = 0;
    hrc.shdr->sfSampleType = monoSample;
    hrc.shdr->wSampleLink = 0;
  }
  Sf2Next(sf2, sfSample, hrc.shdr);
  zeromem(hrc.shdr, sizeof(sfSample));
  ck2->ckSize = (sf2 - (uint8_t*)ck2) - 6;
  ck1->ckSize = (sf2 - (uint8_t*)ck1) - 6;
  ck0->ckSize = (sf2 - (uint8_t*)ck0) - 6;
  return ck0->ckSize + 6;
}
