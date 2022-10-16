#include "misc.h"
#include "globals.h"
#include "math.h"
#include "ns.h"
#include "geom.h"
#include "gool.h"
#include "level.h"
#include "audio.h"

#ifndef PSX
#include "pc/time.h"
#endif

/**
 * below are various 'light sequences' or precomputed sequences
 * of t values to be used as input to the world transform variants
 * with vertex color interpolation
 *
 * patterns in the second set of tables in particular have
 * many discontinuities, to get a flickering light effect
 * which is necessary for simulating lightning
 */
/* .data */
const int32_t t_table1[84] = { /* hot pipes */
     0,   81,  163,  245,  327,  400,  491,  573,  655,  737,  819,  900,
   982, 1064, 1146, 1228, 1310, 1392, 1474, 1556, 1638, 1719, 1801, 1883,
  1965, 2047, 2129, 2211, 2293, 2375, 2457, 2538, 2620, 2702, 2784,
  2866, /* peak */
  2743, 2620, 2497, 2375, 2252, 2129, 2006, 1883, 1760,
  1638, /* trough */
  1760, 1883, 2006, 2129, 2252, 2375, 2497, 2620, 2743,
  2866, /* peak */
  2743, 2620, 2497, 2375, 2252, 2129, 2006, 1883, 1760, 1638, 1515, 1392,
  1269, 1146, 1023,  941,  859,  778,  696,  614,  532,  450,  368,  286,
   204,  122,   40,   -1
};
const int32_t t_table2a[14] = {
  0xFFF,
      0, 0x7FF, 0xFFF, 0x7FF,     0,
  0x7FF, 0x599,
  0x666, 0x599, 0x3FF, 0x199,  0xA3,    -1
};
const int32_t t_table2b[10] = {
  0xFFF, 0xCCC,     0,
  0xFFF, 0x7FF,
  0xFFF, 0x666, 0x333, 0x199,    -1
};
const int32_t t_table2c[11] = {
  0xFFF, 0xE65, 0xCCC, 0xB32,     0,     0,
  0xFFF,     0,
  0xFFF, 0x7FF,  -1
};
const int32_t t_table2d[14] = {
  0x7FF,     0,
  0xFFF, 0xE65,
  0xFFF, 0xCCC, 0x4CC,     0,     0,
  0x7FF, 0x199,
  0x7FF, 0x599,    -1
};
const int32_t t_table2e[19] ={
  0xFFF,     0,
  0xE65, 0xCCC, 0xB32, 0x999, 0x7FF, 0x666, 0x4CC, 0x333,
  0x7FF, 0x666, 0x4CC, 0x333, 0x199,    -1
};
const int32_t t_table2f[32] ={
  0xFFF, 0xF32, 0xE65, 0xD98, 0xCCC, 0xBFF, 0xB32, 0xA65,
  0x999, 0x8CC, 0x7FF, 0x732, 0x666, 0x599, 0x4CC, 0x3FF,
  0x333,
  0x7FF, 0x732, 0x666, 0x599, 0x4CC, 0x3FF, 0x333, 0x266,
  0x199,  0xCC,    -1
};
const int32_t *t_table2[6] ={ t_table2a, t_table2b, t_table2c,
                              t_table2d, t_table2e, t_table2f };

/* .sdata */
rgb far_color1 = { 0, 0, 0 };       /* 80056440; gp[0x11] */
int32_t far_t1 = 0x800;             /* 8005644C; gp[0x14] */
rgb far_color2 = { 255, 255, 255 }; /* 80056450; gp[0x15] */
int32_t far_t2 = 0x800;             /* 8005645C; gp[0x18] */
int dark_shamt_add = 0;             /* 80056460; gp[0x19] */
int dark_shamt_sub = 0;             /* 80056464; gp[0x1A] */
int dark_amb_fx0 = 0;               /* 80056468; gp[0x1B] */
int dark_amb_fx1 = 0;               /* 8005646C; gp[0x1C] */
int32_t dark_dist = 0;              /* 80056470; gp[0x1D] */
int32_t ripple_speed = 10;          /* 80056474; gp[0x1E] */
uint32_t ripple_period = 127;       /* 80056478; gp[0x1F] */
int lseq_state = -1;                /* 800564C8; gp[0x33] */
int lseq_idx = 0;                   /* 800564CC; gp[0x34] */
/* .sbss */
int32_t far_t2_tgt;                 /* 800565EC; gp[0x7C] */
int32_t far_t2_rate;                /* 800565F0; gp[0x7D] */
rgb ruins_fc2_1;                    /* 800565F4; gp[0x7E] */
rgb ruins_fc2_0a;                   /* 80056600; gp[0x81] */
rgb ruins_fc2_0b;                   /* 8005660C; gp[0x84] */
gool_object *prev_light_src_obj;    /* 80056610; gp[0x87] */
int32_t dark_afx0_tgt;              /* 80056614; gp[0x88] */
int32_t dark_afx0_step;             /* 80056618; gp[0x89] */
int32_t dark_afx0_next;             /* 8005661C; gp[0x8A] */
int32_t dark_dist_tgt;              /* 80056620; gp[0x8B] */
int32_t dark_dist_step;             /* 80056624; gp[0x8C] */
int32_t dark_dist_next;             /* 80056628; gp[0x8D] */
uint32_t lightning_stamp;           /* 8005662C; gp[0x8E] */
uint32_t prev_lightning_stamp;      /* 80056630; gp[0x8F] */
vec dark_illum;                     /* 800566D4; gp[0xB6] */

extern ns_struct ns;
extern gool_object *crash;
#ifdef PSX
extern int ticks_elapsed;
#endif

//----- (8002EBB4) --------------------------------------------------------
void ShaderParamsUpdateRipple(int init) {
  switch (ns.ldat->lid) {
  case LID_UPSTREAM:
  case LID_UPTHECREEK:
    ripple_speed = 10;
    ripple_period = 127;
    break;
  case LID_RIPPERROO:
    ripple_speed = 10;
    ripple_period = 127;
    break;
  case LID_BONUSTAWNA1:
  case LID_BONUSTAWNA2:
    ripple_speed = 4;
    ripple_period = 127;
    break;
  default:
    ripple_speed = 1;
    ripple_period = 23;
    break;
  }
  if (init)
    GfxTransformWorldsRipple(0);
}

//----- (8002EC68) --------------------------------------------------------
void ShaderParamsUpdate(int init) {
  eid_t *eid;
  lid_t lid;
  char buf[6];
  rgb c, ca, cb;
  vec *trans;
  uint32_t val, elapsed_since;
  uint32_t pitch, vol;
  int32_t t;
#ifndef PSX
  int ticks_elapsed;
#endif

  if (init) {
    far_t1 = 2048;
    far_color1.r = 0;
    far_color1.g = 0;
    far_color1.b = 0;
    far_t2 = 2048;
    far_color2.r = 255;
    far_color2.g = 255;
    far_color2.b = 255;
    lseq_idx = 0;
    lseq_state = -1;
    switch (ns.ldat->lid) {
    case LID_CORTEXPOWER:
    case LID_HEAVYMACHINERY:
    case LID_TOXICWASTE:
    case LID_CASTLEMACHINERY:
      far_t1 = 0;
      far_color2.r = 255;
      far_color2.g = 43;
      far_color2.b = 11;
      return;
    case LID_GENERATORROOM:
      far_t1 = 0;
      far_color2.r = 238;
      far_color2.g = 255;
      far_color2.b = 60;
      return;
    case LID_PAPUPAPU:
    case LID_TEMPLERUINS:
    case LID_JAWSOFDARKNESS:
      far_t1 = 0;
      far_color2.r = 255;
      far_color2.g = 100;
      far_color2.b = 0;
      break;
    case LID_BOULDERDASH:
      far_t1 = 0;
      far_color2.r = 0;
      far_color2.g = 240;
      far_color2.b = 255;
      return;
    case LID_THELOSTCITY:
    case LID_SUNSETVISTA:
      far_t1 = 0;
      far_t2 = 1600;
      ruins_fc2_1.r = 165;
      ruins_fc2_1.g = 90;
      ruins_fc2_1.b = 100;
      ruins_fc2_0a.r = 165;
      ruins_fc2_0a.g = 90;
      ruins_fc2_0a.b = 100;
      ruins_fc2_0b.r = 255;
      ruins_fc2_0b.g = 75;
      ruins_fc2_0b.b = 0;
      return;
    case LID_KOALAKONG:
      far_t1 = 0;
      far_color2.r = 200;
      far_color2.g = 80;
      far_color2.b = 0;
      break;
    case LID_LIGHTSOUT:
    case LID_FUMBLINGINTHEDARK:
      far_t2 = 0;
      dark_shamt_add = 10;
      dark_amb_fx0 = -14000;
      prev_light_src_obj = 0;
      dark_afx0_next = 4095;
      dark_dist_next = 2000;
      break;
    default:
      return;
    }
  }
  switch (ns.ldat->lid) {
  case LID_CORTEXPOWER:
  case LID_HEAVYMACHINERY:
  case LID_TOXICWASTE:
  case LID_CASTLEMACHINERY:
    if (t_table1[lseq_idx] == -1) /* at end of light sequence? */
      lseq_idx = 0; /* restart the sequence */
    far_t2 = t_table1[lseq_idx++];
    return;
  case LID_GENERATORROOM:
  case LID_PAPUPAPU:
  case LID_TEMPLERUINS:
  case LID_JAWSOFDARKNESS: /* approach a random t2 value from 0-1500 every 2 steps */
    if (lseq_idx==0) {
      far_t2_tgt = randb(1500);
      far_t2 += ((far_t2_tgt - far_t2) / 2);
    }
    if (lseq_idx==1)
      far_t2 = far_t2_tgt;
    lseq_idx = (lseq_idx+1) % 2;
    return;
  case LID_BOULDERDASH:
    if (t_table1[lseq_idx] == -1) /* at end of light sequence? */
      lseq_idx = 0; /* restart the sequence */
    far_t2 = t_table1[lseq_idx++] >> 1;
    return;
  case LID_DRNBRIO:
  case LID_STORMYASCENT:
  case LID_SLIPPERYCLIMB:
    if (lseq_state == -1) { /* light sequence in ? state? */
      far_t1 = 0;
      far_t2 = 0;
      if (randb(1000) >= 25) { return; }
      lid = ns.ldat->lid;
      if (lid != LID_DRNBRIO && lid != LID_STORMYASCENT && lid != LID_SLIPPERYCLIMB) { return; }
      if (lid == LID_STORMYASCENT || lid == LID_SLIPPERYCLIMB) {
        val = randb(3);
        if (val == 0)
          lseq_state = 5;
        else if (val == 1)
          lseq_state = 4;
        else
          lseq_state = randb(4);
      }
      else
        lseq_state = randb(6);
      lseq_idx = 0;
#ifndef PSX
      ticks_elapsed = GetTicksElapsed();
#endif
      elapsed_since = ticks_elapsed - prev_lightning_stamp;
      if (elapsed_since < 6145) { return; }
      strcpy(buf, "lt1rA");
      prev_lightning_stamp = lightning_stamp;
#ifndef PSX
      ticks_elapsed = GetTicksElapsed();
#endif
      lightning_stamp = ticks_elapsed;
      buf[2] += randb(3);
      *eid = NSStringToEID(buf);
      pitch = (randb(0x4CC) + 0xD99) >> 3;
      AudioControl(0, 1, (generic*)&pitch, 0); /* set pitch */
      val = randb(15) + 1;
      AudioControl(0, 7, (generic*)&val, 0); /* trigger note on create */
      vol = randb(100);
      if (vol > 20) { vol += randb(50); }
      AudioVoiceCreate(0, eid, 0x3FFF*vol/100);
    }
    else {
      t = t_table2[lseq_state][lseq_idx];
      if (t == -1) {
        far_t1 = 0;
        far_t2 = 0;
        lseq_state = -1;
      }
      else {
        far_t1 = t;
        far_t2 = t;
        lseq_idx++;
      }
    }
    return;
  case LID_THELOSTCITY:
  case LID_SUNSETVISTA:
    if (lseq_idx == 0) {
      /* (1-t)*a + t*b = a - ta + tb = a + t(b-a) */
      ca = ruins_fc2_0a;
      cb = ruins_fc2_0b;
      t = randb(100); /* choose random [numerator] value for t */
      c.r = (ca.r+(cb.r-ca.r)*t)/100;
      c.g = (ca.g+(cb.g-ca.g)*t)/100;
      c.b = (ca.b+(cb.b-ca.b)*t)/100;
      far_color2.r = ((c.r - 255) / 2) + 255; /* reverse brightness level and halve */
      far_color2.g = ((c.g - 255) / 2) + 255;
      far_color2.b = ((c.b - 255) / 2) + 255;
    }
    else if (lseq_idx == 1) {
      far_color2.r = ruins_fc2_1.r;
      far_color2.g = ruins_fc2_1.g;
      far_color2.b = ruins_fc2_1.b;
    }
    lseq_idx = (lseq_idx+1)%2;
    return;
  case LID_KOALAKONG: /* approach a random t2 value from 0-1000 every 4 steps */
    if (lseq_idx == 0) {
      far_t2_tgt = randb(1000);
      far_t2_rate = (far_t2_tgt - far_t2) / 4;
    }
    if (lseq_idx >= 0 && lseq_idx < 4)
      far_t2 += far_t2_rate;
    lseq_idx = (lseq_idx+1)%4;
    return;
  case LID_LIGHTSOUT:
  case LID_FUMBLINGINTHEDARK:
    trans = doctor ? &doctor->trans : &crash->trans;
    dark_illum = *trans;
    if (!light_src_obj) {
      if (prev_light_src_obj) {
        dark_afx0_tgt = 4095;
        dark_afx0_step = 100;
        dark_dist_tgt = 2000;
        dark_dist_step = 20;
      }
    }
    else if (light_src_obj != prev_light_src_obj) {
      dark_afx0_tgt = -8000;
      dark_afx0_step = -500;
      dark_dist_tgt = 75;
      dark_dist_step = -75;
    }
    prev_light_src_obj = light_src_obj;
    if (dark_afx0_next != dark_afx0_tgt) {
      if (dark_afx0_tgt > dark_afx0_next && (dark_afx0_next + dark_afx0_step >= dark_afx0_tgt)
        || dark_afx0_next < dark_afx0_tgt && (dark_afx0_tgt >= dark_afx0_step + dark_afx0_next)) {
        dark_afx0_next = dark_afx0_tgt;
        dark_afx0_step = 0;
      }
      else
        dark_afx0_next = dark_afx0_next + dark_afx0_step;
    }
    if (dark_dist_tgt != dark_dist_next) {
      if (dark_dist_tgt > dark_dist_next && (dark_dist_next + dark_dist_step >= dark_dist_tgt)
        || dark_dist_next < dark_dist_tgt && (dark_dist_tgt >= dark_dist_step + dark_dist_next)) {
        dark_dist_next = dark_dist_tgt;
        dark_dist_step = 0;
      }
      else
        dark_dist_next = dark_dist_next + dark_dist_step;
    }
    dark_amb_fx0 = dark_afx0_next;
    dark_dist = dark_dist_next;
    return;
  default:
    return;
  }
}
