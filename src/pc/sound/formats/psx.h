#ifndef _F_PSX_H_
#define _F_PSX_H_

#include "common.h"
#include "pc/sound/formats/smf.h"

typedef struct { /* sep header */
  char type[4];
  uint16_t version;
  uint8_t data[];
} PThd;

typedef struct { /* seq header (sep internal) */
  uint16_t seq_num;
  uint16_t tpqn;
  uint32_t init_tempo:24;
  uint32_t tsg_num:8;
  uint8_t tsg_denom;
  uint32_t length;
  uint8_t data[];
} SThd;

/* other seq subchunk structures are identical to those of SMF... */
typedef MEvt SEvt;
typedef MEvr SEvr;
typedef MMed SMed;
/* ...except for meta-event which does not include length */
typedef struct {
  uint8_t delta_time;
  union {
    uint16_t hdr;
    struct {
      uint8_t status;
      uint8_t type;
    };
    /* no length */
  };
  uint8_t data[];
} SMev;

#define set3b(d,v) \
d[0]=(v&0xFF); \
d[1]=(v&0xFF00)>>8; \
d[2]=(v&0xFF0000)>>16

static inline void set4c(char *dst, const char *src) {
  *((uint32_t*)dst) = *((uint32_t*)src);
}

typedef struct {
  uint8_t tones;         /* total number of effective tones which compose the program */
  uint8_t mvol;          /* program volume */
  uint8_t prior;         /* program priority */
  uint8_t mode;          /* program mode */
  uint8_t mpan;          /* program pan */
  int8_t reserved0;
  int16_t attr;          /* program attribute */
  uint32_t reserved1;
  uint32_t reserved2;
} ProgAtr;

typedef struct {
  uint8_t prior;         /* tone priority */
  uint8_t mode;          /* tone mode */
  uint8_t vol;           /* tone volume */
  uint8_t pan;           /* tone pan */
  uint8_t center;        /* center note (0-127) */
  uint8_t shift;         /* pitch correction (0-127, cent units) */
  uint8_t min;           /* minimum note limit (0-127) */
  uint8_t max;           /* maximum note limit (0-127, provided min < max) */
  uint8_t vibW;          /* vibrato width (1/128 rate, 0-127) */
  uint8_t vibT;          /* 1 cycle time of vibrato (tick units) */
  uint8_t porW;          /* portamento width (1/128 rate, 0-127) */
  uint8_t porT;          /* portamento holding time (tick units) */
  uint8_t pbmin;         /* pitch bend (-0-127, 127 = 1 octave) */
  uint8_t pbmax;         /* pitch bend (+0-127, 127 = 1 octave) */
  uint8_t reserved1;
  uint8_t reserved2;
  union {
    uint16_t adsr1;      /* ADSR1 */
    struct {
      uint16_t sl:4;     /* sustain level (0-15) */
      uint16_t dr:4;     /* decay rate (0-15, actual value=15-dr) */
      uint16_t ar:7;     /* attack rate (0-127, actual value=127-ar) */
      uint16_t a_mode:1; /* attack mode (0=linear, 1=exponential) */
    };
  };
  union {
    uint16_t adsr2;      /* ADSR2 */
    struct {
      uint16_t rr:5;     /* release rate (0-31, actual value=31-rr) */
      uint16_t r_mode:1; /* release mode (0=linear, 1=exponential) */
      uint16_t sr:7;     /* sustain rate (0-127, actual value +/-(127-sr) */
      uint16_t unused1:1;
      uint16_t sr_sign:1;/* sustain rate sign */
      uint16_t s_mode:1; /* sustain mode (0=linear, 1=exponential) */
    };
  };
  int16_t prog;          /* parent program */
  int16_t vag;           /* waveform (VAG) used */
  int16_t reserved[4];
} VagAtr;

typedef struct {
  char form[4];          /* always "VABp" */
  int32_t ver;           /* format version number */
  int32_t id;            /* bank ID */
  uint32_t fsize;        /* file size */
  uint16_t reserved0;
  uint16_t ps;           /* total number of programs in this bank */
  uint16_t ts;           /* total number of effective tones */
  uint16_t vs;           /* number of waveforms (VAG) */
  uint8_t mvol;          /* master volume */
  uint8_t pan;           /* master pan */
  uint8_t attr1;         /* bank attribute */
  uint8_t attr2;         /* bank attribute */
  uint32_t reserved1;
} VabHdr;

typedef struct {
  VabHdr hdr;
  ProgAtr atrs[128];
  VagAtr vagatrs[128][16];
  uint16_t unk;
  uint16_t wave_sizes[254]; /* multiply by 16 for actual size */
} VabHead;

typedef uint8_t VabBody;

typedef struct {
  VabHead head;
  VabBody body[];
} Vab;

typedef struct {
  char form[4];
  int32_t ver;
  uint32_t offs;
  uint32_t size;
  uint32_t srate;
  uint32_t reserved1[3];
  char name[16];
  uint32_t reserved2[4];
} VagHdr;

typedef uint8_t VagBody;

typedef struct {
  uint8_t factor:4;
  uint8_t predict:4;
  uint8_t flags;
  struct {
    uint8_t adl:4;
    uint8_t adh:4;
  } data[14];
} VagLine;

#endif /* _F_PSX_H_ */
