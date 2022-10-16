#ifndef _SMF_H_
#define _SMF_H_

#include "common.h"

typedef uint8_t uint24_t[3];

typedef struct { /* smf header chunk */
  char type[4];
  int length;
  uint16_t format;
  uint16_t tracks;
  union {
    struct {
      uint16_t :1;
      uint16_t tpqn:15;
    };
    struct {
      uint16_t fmt:1;
      uint16_t fs:7;
      uint16_t tf:8;
    } division;
  };
} MThd;

typedef struct { /* smf track chunk */
  char type[4];
  int length;
  uint8_t data[];
} MTrk;

typedef struct {
  uint8_t delta_time;
  union {
    uint8_t status;
    struct {
      uint8_t has_status:1;
      uint8_t type:3;
      uint8_t chan:4;
    };
  };
  uint8_t data[];
} MEvt;

typedef struct { /* event with running status */
  uint8_t delta_time;
  uint8_t data[];
} MEvr;

#define MMEV_HDR_END_OF_TRACK   0xFF2F
#define MMEV_HDR_SET_TEMPO      0xFF51
#define MMEV_HDR_SMPTE_OFFSET   0xFF54
#define MMEV_HDR_TIME_SIGNATURE 0xFF58
#define MMEV_HDR_KEY_SIGNATURE  0xFF59

typedef struct { /* meta-event */
  uint8_t delta_time;
  union {
    uint16_t hdr;
    struct {
      uint8_t status; /* 0xFF */
      uint8_t type;
    };
  };
  uint8_t length;
  uint8_t data[];
} MMev;

typedef uint24_t MTmp;

typedef struct {
  uint8_t num;
  uint8_t denom;
  uint8_t cpt;
  uint8_t npc;
} MTsg;

typedef struct {
  uint8_t sf;
  uint8_t mi;
} MKsg;

typedef union { /* meta-event data */
  MTmp *tmp;
  MTsg *tsg;
  MKsg *ksg;
} MMed;

#endif /* _SMF_H_ */
