#ifndef _GOOL_H_
#define _GOOL_H_

#include "common.h"
#include "geom.h"
#include "ns.h"

#include "formats/gool.h"
#include "formats/zdat.h"

/* events */
#define GOOL_EVENT_JUMPED_ON       0
#define GOOL_EVENT_HIT             0x300
#define GOOL_EVENT_SPIN_HIT        0x400
#define GOOL_EVENT_TRIGGERED       0x800
#define GOOL_EVENT_FALL_KILL       0x900
#define GOOL_EVENT_BOX_STACK_BREAK 0x900
#define GOOL_EVENT_HIT_INVINCIBLE  0xA00
#define GOOL_EVENT_STATUS          0xF00
#define GOOL_EVENT_COMBO           0x1000
#define GOOL_EVENT_RESPAWN         0x1300
#define GOOL_EVENT_EAT             0x1400
#define GOOL_EVENT_BOUNCE          0x1500
#define GOOL_EVENT_WARP            0x1600
#define GOOL_EVENT_SQUASH          0x1900
#define GOOL_EVENT_TERMINATE       0x1A00
#define GOOL_EVENT_WIN_BOSS        0x1D00
#define GOOL_EVENT_FLING           0x1D00
#define GOOL_EVENT_EXPLODE         0x1E00
#define GOOL_EVENT_BURN            0x1F00
#define GOOL_EVENT_DROWN           0x2100
#define GOOL_EVENT_SHOCK           0x2300
#define GOOL_EVENT_HIT_FENCE       0x2300
#define GOOL_EVENT_BOULDER_SQUASH  0x2500
#define GOOL_EVENT_LEVEL_END       0x2900
#define GOOL_EVENT_PLAYER_DAMAGE   0x2B00

/* status a flags */
#define GOOL_FLAG_GROUNDLAND        0x1
#define GOOL_FLAG_TOWARD_GOAL       0x4
#define GOOL_FLAG_TROT_DIR          0x8
#define GOOL_FLAG_CHANGE_PATH_DIR   0x10
#define GOOL_FLAG_FIRST_FRAME       0x20
#define GOOL_FLAG_HIT_AT_BOTTOM     0x80
#define GOOL_FLAG_INVALID_PATH      0x200
#define GOOL_FLAG_DYING             0x400
#define GOOL_FLAG_REACHED_TROT      0x800
#define GOOL_FLAG_PLAYER_D_COLLIDER 0x1000
#define GOOL_FLAG_LBOUND_INVALID    0x8000
#define GOOL_FLAG_KEEP_EVENT_STACK  0x20000
#define GOOL_FLAG_ON_FLOOR          0x40000
#define GOOL_FLAG_ATOP_OBJECT       0x200000

/* status b flags */
#define GOOL_FLAG_ROT_Y             0x1
#define GOOL_FLAG_TRACK_PATH_ROT    0x2
#define GOOL_FLAG_TRACK_PATH_SIGN   0x4
#define GOOL_FLAG_STOPPED_BY_SOLID  0x8
#define GOOL_FLAG_COLLIDABLE        0x10
#define GOOL_FLAG_COLLIDEABLE       0x10
#define GOOL_FLAG_GRAVITY           0x20
#define GOOL_FLAG_TRANS_MOTION      0x40
#define GOOL_FLAG_DPAD_CONTROL      0x80
#define GOOL_FLAG_INVISIBLE         0x100
#define GOOL_FLAG_2D                0x200
#define GOOL_FLAG_ROT_X             0x2000
#define GOOL_FLAG_SOLID_GROUND      0x4000
#define GOOL_FLAG_ORIENT_ON_PATH    0x8000
#define GOOL_FLAG_SOLID_SIDES       0x10000
#define GOOL_FLAG_SOLID_TOP         0x20000
#define GOOL_FLAG_ROT_Y2            0x80000
#define GOOL_FLAG_FORCE_UPDATE      0x2000000
#define GOOL_FLAG_HAS_SHADOW        0x4000000
#define GOOL_FLAG_STRING_CENTER     0x4000000
#define GOOL_FLAG_SOLID_BOTTOM      0x8000000
#define GOOL_FLAG_STALL             0x10000000

/* state/status c flags */
#define GOOL_FLAG_GROUND_STATE      0x4
#define GOOL_FLAG_AIR_STATE         0x8
#define GOOL_FLAG_FLING_STATE       0x10  /* rotate state? */
#define GOOL_FLAG_COLOVERRIDE_STATE 0x800
#define GOOL_FLAG_MENUTEXT_STATE    0x20000
#define GOOL_FLAG_NOTERM_STATE      0x40000
#define GOOL_FLAG_EXPENDABLE_STATE  0x80000

/* spawn flags */
#define GOOL_FLAG_SPAWNED           0x1
#define GOOL_FLAG_ACTIVE_SPAWN      0x8

/* display flags */
#define GOOL_FLAG_DISPLAY_WORLDS    0x1
#define GOOL_FLAG_CAM_UPDATE        0x2
#define GOOL_FLAG_DISPLAY           0x4
#define GOOL_FLAG_ANIMATE           0x8
#define GOOL_FLAG_DISPLAY_C1        0x10
#define GOOL_FLAG_ANIMATE_C1        0x20
#define GOOL_FLAG_DISPLAY_C356      0x40
#define GOOL_FLAG_ANIMATE_C356      0x80
#define GOOL_FLAG_ANIMATE_C2        0x100
#define GOOL_FLAG_DISPLAY_C2        0x200
#define GOOL_FLAG_ANIMATE_C4        0x400
#define GOOL_FLAG_DISPLAY_C4        0x800
#define GOOL_FLAG_COUNT_DRAWS       0x1000
#define GOOL_FLAG_DISPLAY_SOLID_BGC 0x2000
#define GOOL_FLAG_FORCE_DISP_MENUS  0x4000
#define GOOL_FLAG_FORCE_ANIM_MENUS  0x8000
#define GOOL_FLAG_SPIN_DEATH        0x10000
#define GOOL_FLAG_DISPLAY_IMAGES    0x20000
#define GOOL_FLAG_SPIN_ACCEL        0x40000
#define GOOL_FLAG_DISPLAY_UNK       0x200000

#define GOOL_FLAG_DISPANIM_ALL      0x3C00F
#define GOOL_FLAG_DISPANIM_OBJECTS  0xFFFC

/* interpreter flags */
#define GOOL_FLAG_SUSPEND_ON_RET    0x1
#define GOOL_FLAG_SUSPEND_ON_RETLNK 0x2
#define GOOL_FLAG_SUSPEND_ON_ANIM   0x4
#define GOOL_FLAG_EVENT_SERVICE     0x8
#define GOOL_FLAG_STATUS_PRESERVE   0x10
#define GOOL_FLAG_RETURN_EVENT      0x20

/* constants */
#define GOOL_OBJECT_COUNT           96
#define GOOL_LEVEL_SPAWN_COUNT      3592
#define GOOL_SPAWN_COUNT            304

/* macros */
#define OBJECT(obj)       ((gool_object*)(obj))
#define HANDLE(obj)       (OBJECT(obj)->handle)
#define ISFREEOBJECT(obj) (HANDLE(obj).type==0)
#define ISHANDLE(obj)     (HANDLE(obj).type==2)

#define GoolObjectGetChildren(obj) \
(ISHANDLE(obj) ? HANDLE(obj).children : OBJECT(obj)->children)
#define GoolObjectSetChildren(obj,c) \
if (ISHANDLE(obj)) { HANDLE(obj).children = c; } \
else               { OBJECT(obj)->children = c; }

#define GoolCalcBound(b,t,o) \
(o)->p1.x = (b)->p1.x + (t)->x; \
(o)->p1.y = (b)->p1.y + (t)->y; \
(o)->p1.z = (b)->p1.z + (t)->z; \
(o)->p2.x = (b)->p2.x + (t)->x; \
(o)->p2.y = (b)->p2.y + (t)->y; \
(o)->p2.z = (b)->p2.z + (t)->z
#define GoolObjectCalcBound(o,b) \
GoolCalcBound(&(o)->bound, &(o)->trans, (b))

/* object structure definitions */
typedef struct {
  struct _gool_object *self;
  struct _gool_object *parent;
  struct _gool_object *sibling;
  struct _gool_object *children;
  struct _gool_object *creator;
  struct _gool_object *player;
  struct _gool_object *collider;
  struct _gool_object *interrupter;
} gool_links;

typedef struct {
  struct _gool_object *prev;
  struct _gool_object *next;
} gool_link;

typedef struct {
  vec trans;
  ang rot;
  vec scale;
  union {
    vec velocity;
    vec misc_a;
    gool_link box_link;
  };
  union {
    ang misc_b;
    struct {
      int32_t ang_velocity_x;
      ang2 target_rot;
    };
  };
  union {
    vec misc_c;
    struct {
      uint32_t mode_flags_a;
      uint32_t mode_flags_b;
      uint32_t mode_flags_c;
    };
  };
} gool_vectors;

typedef struct {
  union { gool_links; struct _gool_object *links[8]; };
  union {
    gool_vectors;
    gool_vectors vectors;
    vec vectors_v[6];
    ang vectors_a[6];
  };
  uint32_t status_a;
  uint32_t status_b;
  uint32_t status_c;
  uint32_t subtype;
  uint32_t pid_flags;
  uint32_t *sp;
  uint32_t *pc;
  uint32_t *fp;
  uint32_t *tp;
  uint32_t *ep;
  uint32_t *once_p;
  union {
    uint32_t misc_flag;
    struct _gool_object *misc_child;
    uint32_t misc_node;
    entry *misc_entry;
    uint32_t misc_memcard;
  };
  uint32_t ack;
  uint32_t anim_stamp;
  uint32_t state_stamp;
  uint32_t anim_counter;
  gool_anim *anim_seq;
  uint32_t anim_frame;
  zone_entity *entity;
  int32_t path_progress;
  uint32_t path_length;
  uint32_t floor_y;
  uint32_t state_flags;
  int32_t speed;
  uint32_t invincibility_state;
  uint32_t invincibility_stamp;
  uint32_t floor_impact_stamp;
  int32_t floor_impact_velocity;
  uint32_t size;
  uint32_t event;
  int32_t cam_zoom;
  int32_t ang_velocity_y;
  int32_t hotspot_size;
  uint32_t voice_id;
  uint32_t _150;
  uint32_t _154;
  uint32_t node;
  uint32_t memory[64];
} gool_process;

typedef struct {
  int type;
  union {
    struct _gool_object *children;
    int subtype;
  };
} gool_handle;

typedef struct _gool_object {
  gool_handle handle;
  bound bound;
  entry *global;
  entry *external;
  entry *zone;
  uint32_t state;
  union {
    gool_colors;
    gool_colors colors;
    uint16_t colors_i[24];
  };
  union {
    gool_process;
    gool_process process;
    uint32_t regs[0x1FC];
  };
} gool_object;

typedef union {
  gool_object *obj;
  uint32_t node;
  uint32_t value;
} gool_objnode;

/* query structure definitions */
typedef struct {
  gool_object *sender;
  uint32_t event;
  int argc;
  int type;
  int count;
  uint32_t *argv;
} gool_event_query;

typedef struct {
  uint32_t categories;
  gool_object *obj;
  gool_object *nearest_obj;
  int32_t dist;
  uint32_t event;
} gool_nearest_query;

/* misc structure definitions */
typedef struct {
  bound;
  gool_object *obj;
} gool_bound;

typedef struct {
  int dir;
  int32_t angle;
  int32_t speed_scale;
} gool_move_state;

typedef struct {
  int32_t accel;
  int32_t max_speed;
  uint32_t unk;
  int32_t decel;
} gool_accel_state;

typedef struct {
  uint32_t *buf;
  int idx;
} gool_const_buf;

typedef struct {
  uint32_t state;
  int guard;
} gool_state_ref;

/* function pointer typedefs */
typedef int (*gool_ifnptr_t)(gool_object*,int);
typedef int (*gool_ifnptr2_t)(gool_object*,int,int);

/* exported funcs */
extern int GoolInitAllocTable();
extern int GoolKillAllocTable();
extern int GoolInitLid();
extern void GoolInitLevelSpawns(lid_t);
extern int GoolObjectHandleTraverseTreePostorder(
  gool_object*,
  int (*func)(gool_object*,int),
  int);
extern void GoolForEachObjectHandle(
  int (*func)(gool_object*,int,int),
  int, int);
extern gool_object *GoolObjectSpawn(entry*,int);
extern gool_object *GoolObjectCreate(void*,int,int,int,uint32_t*,int);
extern int GoolObjectTerminate(gool_object*,int);
extern void GoolZoneObjectsTerminate(entry*);
extern void GoolUpdateObjects(int);
extern int GoolSendEvent(gool_object*,gool_object*,uint32_t,int,uint32_t*);
extern int16_t GoolObjectRotate(int16_t,int16_t,int32_t,gool_object*);
extern int16_t GoolObjectRotate2(int16_t,int16_t,int32_t,gool_object*);
extern int16_t GoolAngDiff(int16_t,int16_t);
extern int32_t GoolSeek(int32_t,int32_t,int32_t);
extern int GoolTransform(vec*,vec*,ang*,vec*,vec*);
extern int GoolTransform2(vec*,vec*,int);
extern int GoolCollide(gool_object*,bound*,gool_object*,bound*);
extern int GoolSendToColliders(gool_object*,uint32_t,int,int,uint32_t*);

#endif /* _GOOL_H_ */
