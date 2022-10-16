#ifndef _SF2_H_
#define _SF2_H_

/* based on sf2 2.04 specifications */

typedef struct { /* RIFF chunk */
  char ckID[4];
  uint16_t ckSize;
  union {
    char fccType[4];
    uint8_t ckData[0];
  };
} CK;

typedef struct {
  uint16_t wMajor;
  uint16_t wMinor;
} sfVersionTag;

typedef struct {
  char achPresetName[20];
  uint16_t wPreset;
  uint16_t wBank;
  uint16_t wPresetBagNdx;
  uint32_t dwLibrary;
  uint32_t dwGenre;
  uint32_t dwMorphology;
} sfPresetHeader;

typedef struct {
  uint16_t wGenNdx;
  uint16_t wModNdx;
} sfPresetBag;

typedef enum {
  startAddrsOffset            = 0,
  endAddrsOffset              = 1,
  startloopAddrsOffset        = 2,
  endloopAddrsOffset          = 3,
  startAddrsCoarseOffset      = 4,
  modLfoToPitch               = 5,
  vibLfoToPitch               = 6,
  modEnvToPitch               = 7,
  initialFilterFc             = 8,
  initialFilterQ              = 9,
  modLfoToFilterFc            = 10,
  modEnvToFilterFc            = 11,
  endAddrsCoarseOffset        = 12,
  modLfoToVolume              = 13,
  unused1                     = 14,
  chorusEffectsSend           = 15,
  reverbEffectsSend           = 16,
  pan                         = 17,
  unused2                     = 18,
  unused3                     = 19,
  unused4                     = 20,
  delayModLFO                 = 21,
  freqModLFO                  = 22,
  delayVibLFO                 = 23,
  freqVibLFO                  = 24,
  delayModEnv                 = 25,
  attackModEnv                = 26,
  holdModEnv                  = 27,
  decayModEnv                 = 28,
  sustainModEnv               = 29,
  releaseModEnv               = 30,
  keynumToModEnvHold          = 31,
  keynumToModEnvDecay         = 32,
  delayVolEnv                 = 33,
  attackVolEnv                = 34,
  holdVolEnv                  = 35,
  decayVolEnv                 = 36,
  sustainVolEnv               = 37,
  releaseVolEnv               = 38,
  keynumToVolEnvHold          = 39,
  keynumToVolEnvDecay         = 40,
  instrument                  = 41,
  reserved1                   = 42,
  keyRange                    = 43,
  velRange                    = 44,
  startloopAddrsCoarseOffset  = 45,
  keynum                      = 46,
  velocity                    = 47,
  initialAttenuation          = 48,
  reserved2                   = 49,
  endloopAddrsCoarseOffset    = 50,
  coarseTune                  = 51,
  fineTune                    = 52,
  sampleID                    = 53,
  sampleModes                 = 54,
  reserved3                   = 55,
  scaleTuning                 = 56,
  exclusiveClass              = 57,
  overridingRootKey           = 58,
  initialPitch                = 59 /* not listed in sf2 spec */
} SFGenerator;

typedef enum {
  noController   = 0,
  noteOnVel      = 1,
  noteOnKeynum   = 2,
  polyPressure   = 10,
  chanPressure   = 13,
  pitchWheel     = 14,
  pitchWheelSens = 18,
  link           = 127
} SFCtrlPalette;

typedef enum {
  linear  = 0,
  concave = 1,
  convex  = 2,
  _switch = 3
} SFCtrlType;

typedef struct {
  SFCtrlPalette index:7;
  uint16_t cc:1;
  uint16_t direction:1;
  uint16_t polarity:1;
  SFCtrlType type:6;
} SFModulator;

#define setsfm(m,i,c,d,p,t) \
m.index = i; \
m.cc = c; \
m.direction = d; \
m.polarity = p; \
m.type = t

typedef enum {
  _linear   = 0,
  absolute = 2
} SFTransform;

typedef struct {
  SFModulator sfModSrcOper;
  SFGenerator sfModDestOper;
  uint16_t modAmount;
  SFModulator sfModAmtSrcOper;
  SFTransform sfModTransOper;
} sfModList;

typedef struct {
  uint8_t byLo;
  uint8_t byHi;
} rangesType;

typedef union {
  rangesType ranges;
  int16_t shAmount;
  uint16_t wAmount;
} genAmountType;

typedef struct {
  SFGenerator sfGenOper;
  genAmountType genAmount;
} sfGenList;

typedef struct {
  char achInstName[20];
  uint16_t wInstBagNdx;
} sfInst;

typedef struct {
  uint16_t wInstGenNdx;
  uint16_t wInstModNdx;
} sfInstBag;

typedef sfModList sfInstModList;
typedef sfGenList sfInstGenList;

typedef enum {
  monoSample      = 1,
  rightSample     = 2,
  leftSample      = 4,
  linkedSample    = 8,
  RomMonoSample   = 0x8001,
  RomRightSample  = 0x8002,
  RomLeftSample   = 0x8004,
  RomLinkedSample = 0x8008
} SFSampleLink;

typedef struct {
  char achSampleName[20];
  uint32_t dwStart;
  uint32_t dwEnd;
  uint32_t dwStartloop;
  uint32_t dwEndloop;
  uint32_t dwSampleRate;
  uint8_t byOriginalKey;
  char chCorrection;
  uint16_t wSampleLink;
  SFSampleLink sfSampleType;
} sfSample;

typedef union { /* info sub-record */
  sfVersionTag *ifil;
  char *isng;
  char *inam;
  char *irom;
  sfVersionTag *iver;
  char *icrd;
  char *ieng;
  char *iprd;
  char *icop;
  char *icmt;
  char *isft;
} iRC;

typedef union { /* hydra sub-record */
  sfPresetHeader *phdr;
  sfPresetBag *pbag;
  sfModList *pmod;
  sfGenList *pgen;
  sfInst *inst;
  sfInstBag *ibag;
  sfInstModList *imod;
  sfInstGenList *igen;
  sfSample *shdr;
} hRC;

#endif /* _SF2_H_ */
