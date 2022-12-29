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

size_t ADPCMToPCM16(uint8_t *adpcm, size_t size, uint8_t *pcm, int *loop) {
  const int f0[16] ={ 0,60,115, 98,122,0,0,0,0,0,0,0,0,0,0,0 };
  const int f1[16] ={ 0, 0,-52,-55,-60,0,0,0,0,0,0,0,0,0,0,0 };
  VagLine *line;
  double s0, s1, tmp;
  int16_t *ld;
  uint8_t *start;
  int i, j;

  start = pcm;
  s0 = 0; s1 = 0;
  if (loop) { *loop = -1; }
  for (i=0;i<size/sizeof(VagLine);i++) {
    Next(adpcm, VagLine, line);
    if ((line->flags & 4) && (i>1) && loop) { 
      *loop = (int)(pcm - start);
    }
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
    if (line->flags & 1) { break; }
  }
  size = pcm - start;
  return size;
}

#define MB(d,i) ((d)&(255<<((i)*8)))
#define SB(d,a,b) ((MB(d,a)<<(b*8))|(MB(d,a+b)>>(b*8)))
#define SwapEndian(d,f) \
(sizeof(f)==2?SB(d,0,1): \
 sizeof(f)==4?(SB(d,0,3)|SB(d,1,1)): \
 d)

#define SeqNext(s,t,d) Next(s,t,d)
#define MidNext(m,t,d) Next(m,t,d)

size_t SeqToMid(uint8_t *seq, uint8_t *mid, size_t *seq_size) {
  MThd *mthd; SThd *sthd;
  MTrk *mtrk;
  MMev *mmev; SMev *smev;
  MEvt *mevt; SEvt *sevt;
  MEvr *mevr; SEvr *sevr;
  MMed m;
  size_t size;
  int type, length, i;
 
  type = 7;
  SeqNext(seq, SThd, sthd);
  MidNext(mid, MThd, mthd);
  set4c(mthd->type, "MThd");
  mthd->length = SwapEndian(6, mthd->length);
  mthd->format = SwapEndian(0, mthd->format);
  mthd->tracks = SwapEndian(1, mthd->tracks); /* single track */
  mthd->tpqn = sthd->tpqn;
  MidNext(mid, MTrk, mtrk);
  set4c(mtrk->type, "MTrk");
  MidNext(mid, MMev, mmev);
  mmev->hdr = SwapEndian(MMEV_HDR_SET_TEMPO, mmev->hdr);
  mmev->length = SwapEndian(sizeof(MTmp), mmev->length);
  mmev->delta_time = 0;
  MidNext(mid, MTmp, m.tmp);
  set3b(*m.tmp, sthd->init_tempo);
  MidNext(mid, MMev, mmev);
  mmev->hdr = SwapEndian(MMEV_HDR_TIME_SIGNATURE, mmev->hdr);
  mmev->length = SwapEndian(sizeof(MTsg), mmev->length);
  mmev->delta_time = 0;
  MidNext(mid, MTsg, m.tsg);
  m.tsg->num = sthd->tsg_num;
  m.tsg->denom = sthd->tsg_denom;
  m.tsg->cpt = 24;
  m.tsg->npc = 8;
  length = 0;
  while (1) { 
    if (seq[0]==0 && seq[1]==0 && seq[2]==0xFF && seq[3]==0x2F) {
      *(mid++) = 0;
      seq++;
    }
    SeqNext(seq, SEvt, sevt);
    if (!sevt->has_status && type < 7) {
      sevr = (SEvr*)sevt;
      MidNext(mid, MEvr, mevr);
      *mevr = *((MEvr*)sevr);
      length = (type != 4 && type != 5) ? 2: 1;
      --seq;
    }
    else if (sevt->type < 7) {
      type = sevt->type;
      MidNext(mid, MEvt, mevt);
      *mevt = *((MEvt*)sevt);
      length = (type != 4 && type != 5) ? 2: 1;
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
      if (smev->type == 0x2F) { break; }
    }
    /* copy data */
    for (i=0;i<length;i++)
      *(mid++) = *(seq++);
  }
  
  length = mid - mtrk->data;
  mtrk->length = SwapEndian(length, mtrk->length);
  size = mid - (uint8_t*)mthd;
  if (seq_size)
    *seq_size = (size_t)(seq - (uint8_t*)sthd) + 2;
  return size;
}

#define Sf2Next(s,t,d) Next(s,t,d)

#define Sf2NextCc(s,c,t,n) \
Sf2Next(s,CK,c); \
set4c(c->ckID,t); \
set4c(c->fccType,n); \
s += sizeof(char)*4; 

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

typedef struct {
  ProgAtr *atr;
  VagAtr *vagatr[16];
  VagBody *waves[16];
} VabProg;

size_t VabToSf2(uint8_t *vab, uint8_t *sf2) {
  VabProg *prog, progs[128];
  VagBody *wave;
  VagAtr *atr;
  VabA *va; VabB *vb; VabBody *vbd;
  CK *ck0, *ck1, *ck2, *ck3;
  iRC irc; hRC hrc;
  sfPresetHeader *presets[128];
  sfPresetBag *pbags[128], *tpbag;
  sfInst *insts[128], *tinst;
  sfInstBag *ibags[128][17], *tibag;
  size_t size;
  uint32_t wave_offs;
  uint8_t *waves[255], *wave_ends[255];
  int i, j, prog_count, zone_count, sample_count;

  va = (VabA*)vab;
  vb = (VabB*)&va->vagatrs[va->hdr.ps];
  wave_offs = 0;
  for (i=0;i<va->hdr.vs;i++) {
    waves[i] = &vb->body[wave_offs];
    wave_offs += vb->wave_sizes[i] << 3;
  }
  prog_count = 0;
  for (i=0;i<128;i++) { 
    prog = &progs[i];
    prog->atr = &va->atrs[i];
    if (prog->atr->tones == 0) { continue; }
    for (j=0;j<prog->atr->tones;j++) {
      prog->vagatr[j] = &va->vagatrs[prog_count][j];
      prog->waves[j] = waves[prog->vagatr[j]->vag];
    }
    ++prog_count;
  }
  Sf2NextCc(sf2, ck0, "RIFF", "sfbk");
  Sf2NextCc(sf2, ck1, "LIST", "INFO");
  Sf2NextCd(sf2, ck2, "ifil", sfVersionTag, irc.ifil);
  irc.ifil->wMajor = 2;
  irc.ifil->wMinor = 8; /* 2.08 */
  Sf2NextCs(sf2, ck2, "isng", "EMU8080");
  Sf2NextCs(sf2, ck2, "INAM", "General MIDI");
  ck1->ckSize = (sf2 - (uint8_t*)ck1) - 8;
  Sf2NextCc(sf2, ck1, "LIST", "sdta");
  Sf2NextCk(sf2, ck2, "smpl");
  for (i=0;i<va->hdr.vs;i++) {
    wave = waves[i];
    waves[i] = sf2;
    sf2 += ADPCMToPCM16(wave, vb->wave_sizes[i]<<3, sf2, 0);
    wave_ends[i] = sf2;
    sf2 += (sf2 - waves[i]) % 46;
  }
  ck2->ckSize = (sf2 - (uint8_t*)ck2) - 8;
  ck1->ckSize = (sf2 - (uint8_t*)ck1) - 8;
  Sf2NextCc(sf2, ck1, "LIST", "pdta");
  Sf2NextCk(sf2, ck2, "phdr");
  prog_count = 0;
  for (i=0;i<128;i++) {
    prog = &progs[i];
    prog->atr = &va->atrs[i];
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
  Sf2Next(sf2, sfPresetHeader, hrc.phdr);
  hrc.phdr->wPreset = -1;
  hrc.phdr->wBank = -1;
  hrc.phdr->wPresetBagNdx = prog_count;
  ck2->ckSize = (sf2 - (uint8_t*)ck2) - 8;
  Sf2NextCk(sf2, ck2, "pbag");
  for (i=0;i<128;i++) {
    prog = &progs[i];
    if (prog->atr->tones == 0) { continue; }
    Sf2Next(sf2, sfPresetBag, hrc.pbag);
    pbags[i] = hrc.pbag;
  }
  Sf2Next(sf2, sfPresetBag, hrc.pbag);
  tpbag = hrc.pbag;
  ck2->ckSize = (sf2 - (uint8_t*)ck2) - 8;
  Sf2NextCk(sf2, ck2, "pmod");
  Sf2Next(sf2, sfModList, hrc.pmod);
  *((uint16_t*)hrc.pmod) = 0;
  ck2->ckSize = (sf2 - (uint8_t*)ck2) - 8;
  Sf2NextCk(sf2, ck2, "pgen");
  prog_count = 0;
  for (i=0;i<128;i++) {
    prog = &progs[i];
    if (prog->atr->tones == 0) { continue; }
    pbags[i]->wModNdx = 0;
    pbags[i]->wGenNdx = ((sf2 - (uint8_t*)ck2) - 8) / sizeof(sfGenList);
    Sf2Next(sf2, sfGenList, hrc.pgen);
    hrc.pgen->sfGenOper = instrument;
    hrc.pgen->genAmount.shAmount = prog_count; /* instr idx */
    ++prog_count;
  }
  tpbag->wModNdx = 0;
  tpbag->wGenNdx = ((sf2 - (uint8_t*)ck2) - 8) / sizeof(sfGenList);
  Sf2Next(sf2, sfGenList, hrc.pgen);  
  *((uint16_t*)hrc.pgen) = 0; 
  ck2->ckSize = (sf2 - (uint8_t*)ck2) - 8;
  Sf2NextCk(sf2, ck2, "inst");
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
  tinst = hrc.inst;
  ck2->ckSize = (sf2 - (uint8_t*)ck2) - 8;
  zone_count = 0;
  Sf2NextCk(sf2, ck2, "ibag");
  for (i=0;i<128;i++) {
    prog = &progs[i];
    if (prog->atr->tones == 0) { continue; }
    insts[i]->wInstBagNdx = zone_count;
    for (j=0;j<prog->atr->tones;j++) {
      Sf2Next(sf2, sfInstBag, hrc.ibag);
      ibags[i][j] = hrc.ibag;
      ++zone_count;
    }
  }
  tinst->wInstBagNdx = zone_count;
  Sf2Next(sf2, sfInstBag, hrc.ibag);
  tibag = hrc.ibag;
  ck2->ckSize = (sf2 - (uint8_t*)ck2) - 8;
  Sf2NextCk(sf2, ck2, "imod");
  for (i=0;i<128;i++) {
    prog = &progs[i];
    if (prog->atr->tones == 0) { continue; }
    for (j=0;j<prog->atr->tones;j++) {
      atr = prog->vagatr[j];
      ibags[i][j]->wInstModNdx = ((sf2 - (uint8_t*)ck2) - 8) / sizeof(sfInstModList);
      /* pbmin 0-127, 127=1 octave
         1 = 12/127 semitone
         (pbmin*12)/127 = semitones
         (((pbmin*12)%127)*100)/127) = cents */
      // Sf2Next(sf2, sfInstModList, hrc.imod);
      // setsfm(hrc.imod->sfModSrcOper, pitchWheel, 0, 1, 0, linear); /* - unipolar */
      // hrc.imod->sfModDestOper = coarseTune;
      // hrc.imod->modAmount = (atr->pbmin*12)/127; /* semitones */
      // setsfm(hrc.imod->sfModAmtSrcOper, pitchWheelSens, 0, 0, 0, linear);
      // hrc.imod->sfModTransOper = linear;
      // Sf2Next(sf2, sfInstModList, hrc.imod);
      // setsfm(hrc.imod->sfModSrcOper, pitchWheel, 0, 1, 0, linear); /* - unipolar */
      // hrc.imod->sfModDestOper = fineTune;
      // hrc.imod->modAmount = (((atr->pbmin*12)%127)*100)/127; /* cents */
      // setsfm(hrc.imod->sfModAmtSrcOper, pitchWheelSens, 0, 0, 0, linear);
      // hrc.imod->sfModTransOper = linear;
      // Sf2Next(sf2, sfInstModList, hrc.imod);
      // setsfm(hrc.imod->sfModSrcOper, pitchWheel, 0, 0, 0, linear); /* + unipolar */
      // hrc.imod->sfModDestOper = coarseTune;
      // hrc.imod->modAmount = (atr->pbmax*12)/127; /* semitones */
      // setsfm(hrc.imod->sfModAmtSrcOper, pitchWheelSens, 0, 0, 0, linear);
      // hrc.imod->sfModTransOper = linear;
      // Sf2Next(sf2, sfInstModList, hrc.imod);
      // setsfm(hrc.imod->sfModSrcOper, pitchWheel, 0, 0, 0, linear); /* + unipolar */
      // hrc.imod->sfModDestOper = fineTune;
      // hrc.imod->modAmount = (((atr->pbmax*12)%127)*100)/127; /* cents */
      // setsfm(hrc.imod->sfModAmtSrcOper, pitchWheelSens, 0, 0, 0, linear);
      // hrc.imod->sfModTransOper = linear;
    }
  }
  tibag->wInstModNdx = ((sf2 - (uint8_t*)ck2) - 8) / sizeof(sfInstModList);
  Sf2Next(sf2, sfInstModList, hrc.imod);
  zeromem(hrc.imod, sizeof(sfInstModList));
  ck2->ckSize = (sf2 - (uint8_t*)ck2) - 8;
  Sf2NextCk(sf2, ck2, "igen");
  for (i=0;i<128;i++) {
    prog = &progs[i];
    for (j=0;j<prog->atr->tones;j++) {
      atr = prog->vagatr[j];
      ibags[i][j]->wInstGenNdx = ((sf2 - (uint8_t*)ck2) - 8) / sizeof(sfInstGenList);
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
      hrc.igen->genAmount.shAmount = atr->vag-1;
    }
  }
  tibag->wInstGenNdx = ((sf2 - (uint8_t*)ck2) - 8) / sizeof(sfInstGenList);
  Sf2Next(sf2, sfInstGenList, hrc.igen);
  zeromem(hrc.igen, sizeof(sfInstGenList));
  ck2->ckSize = (sf2 - (uint8_t*)ck2) - 8;
  Sf2NextCk(sf2, ck2, "shdr");
  for (i=0;i<va->hdr.vs;i++) {
    Sf2Next(sf2, sfSample, hrc.shdr);
    padnc(hrc.shdr->achSampleName, "sample");
    hrc.shdr->achSampleName[6] = '0'+(i/100);
    hrc.shdr->achSampleName[7] = '0'+((i/10)%10);
    hrc.shdr->achSampleName[8] = '0'+(i%10);
    hrc.shdr->dwStart = (waves[i] - waves[0]) / sizeof(int16_t);
    hrc.shdr->dwEnd = (wave_ends[i] - waves[0]) / sizeof(int16_t); 
    hrc.shdr->dwStartloop = hrc.shdr->dwStart;
    hrc.shdr->dwEndloop = hrc.shdr->dwStart;
    hrc.shdr->dwSampleRate = 44100;
    hrc.shdr->byOriginalKey = 60;
    hrc.shdr->chCorrection = 0;
    hrc.shdr->sfSampleType = monoSample;
    hrc.shdr->wSampleLink = 0;
  }
  Sf2Next(sf2, sfSample, hrc.shdr);
  zeromem(hrc.shdr, sizeof(sfSample));
  ck2->ckSize = (sf2 - (uint8_t*)ck2) - 8;
  ck1->ckSize = (sf2 - (uint8_t*)ck1) - 8;
  ck0->ckSize = (sf2 - (uint8_t*)ck0) - 8;
  size = ck0->ckSize + 8;  
  return size;
}
