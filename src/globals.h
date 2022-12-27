#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#include "common.h"
#include "geom.h"
#include "ns.h"
#include "gool.h"
#include "formats/zdat.h"

/* game state */
#define GAME_STATE_CUTSCENE 0
#define GAME_STATE_PLAYING  0x100
#define GAME_STATE_GAMEOVER 0x200
#define GAME_STATE_CONTINUE 0x300
#define GAME_STATE_NEW_GEM  0x500
#define GAME_STATE_TITLE    0x600

/* gool globals */
typedef struct {
  lid_t cur_lid_ro;
  uint32_t dword_80061890;
  int32_t screen_shake;
  int32_t fog_z;
  uint32_t next_display_flags;
  int respawn_count;
  gool_object *fruit_hud;
  gool_object *life_hud;
  gool_object *ambiance_obj;
  uint32_t cur_display_flags;
  int i_death_cam;
  int32_t dword_800618B8;
  gool_object *pause_obj;
  uint32_t dword_800618C0;
  gool_object *pickup_hud;
  int32_t cam_rot_xz_ro;
  gool_object *doctor;
  int game_state;
  int title_state;
  int saved_title_state;
  int cur_map_level;
  uint32_t dword_800618E0;
  uint32_t dword_800618E4;
  uint32_t dowrd_800618E8;
  int life_count;
  uint32_t health;
  int fruit_count;
  int cortex_count;
  int brio_count;
  int tawna_count;
  uint32_t cur_zone_flags_ro;
  int init_life_count;
  uint32_t dword_8006190C;
  int mono;
  uint32_t sfx_vol;
  uint32_t mus_vol;
  gool_object *cam_spin_obj;
  vec cam_trans_ro;
  ang cam_rot_ro;
  uint32_t ticks_cur_frame;
  vec2 screen_ro;
  int level_count;
  int levels_unlocked;
  uint32_t dword_8006194C;
  int cam_spin_obj_vert;
  uint32_t dword_80061954;
  uint32_t dowrd_80061958;
  uint32_t dword_8006195C;
  uint32_t dword_80061960;
  gool_object *light_src_obj;
  uint32_t dword_80061968;
  int32_t dcam_zoom_speed;
  int32_t dcam_flip_speed;
  uint32_t percent_complete;
  uint32_t card_flags_ro;
  int bonus_round;
  int card_part_count_ro;
  int box_count;
  uint32_t item_pool1;
  uint32_t island_cam_rot_x;
  uint32_t gem_stamp;
  int island_cam_state;
  uint32_t is_first_zone;
  int debug;
  int32_t checkpoint_id;
  uint32_t dword_000619A4;
  uint32_t dword_800619A8;
  uint32_t item_pool2;
  uint32_t map_level_links;
  int title_pause_state;
  uint32_t map_key_links;
  gool_object *caption_obj;
  uint32_t dword_800619C0;
  uint32_t dword_800619C4;
  int draw_count_ro;
  char *card_str;
  void *card_icon;
  uint32_t card_partinfos[15];
  int gem_count;
  int key_count;
  uint32_t dword_80061A18;
  uint32_t saved_item_pool1;
  uint32_t saved_item_pool2;
  vec spawn_trans;
  int pbak_state;
  int fade_counter;
  int fade_step;
  int death_count;
  uint32_t dword_80061A40;
  uint32_t dword_80061A44;
  uint32_t dword_80061A48;
  uint32_t dword_80061A4C;
  int saved_level_count;
  uint32_t dword_80061A54;
  int options_changed;
  gool_object *prev_box;
  uint32_t boxes_y;
  zone_entity *prev_box_entity;
} gool_globals;

extern gool_globals globals;

#define cur_lid_ro globals.cur_lid_ro
#define screen_shake globals.screen_shake
#define fog_z globals.fog_z
#define next_display_flags globals.next_display_flags
#define respawn_count globals.respawn_count
#define fruit_hud globals.fruit_hud
#define life_hud globals.life_hud
#define ambiance_obj globals.ambiance_obj
#define cur_display_flags globals.cur_display_flags
#define i_death_cam globals.i_death_cam
#define dword_800618B8 globals.dword_800618B8
#define pause_obj globals.pause_obj
#define pickup_hud globals.pickup_hud
#define cam_rot_xz_ro globals.cam_rot_xz_ro
#define doctor globals.doctor
#define game_state globals.game_state
#define title_state globals.title_state
#define saved_title_state globals.saved_title_state
#define cur_map_level globals.cur_map_level
#define dword_800618E0 globals.dword_800618E0
#define life_count globals.life_count
#define health globals.health
#define fruit_count globals.fruit_count
#define cortex_count globals.cortex_count
#define brio_count globals.brio_count
#define tawna_count globals.tawna_count
#define cur_zone_flags_ro globals.cur_zone_flags_ro
#define init_life_count globals.init_life_count
#define dword_8006190C globals.dword_8006190C
#define mono globals.mono
#define sfx_vol globals.sfx_vol
#define mus_vol globals.mus_vol
#define cam_spin_obj globals.cam_spin_obj
#define cam_trans_ro globals.cam_trans_ro
#define cam_rot_ro globals.cam_rot_ro
#define ticks_cur_frame globals.ticks_cur_frame
#define screen_ro globals.screen_ro
#define level_count globals.level_count
#define levels_unlocked globals.levels_unlocked
#define dword_8006194C globals.dword_8006194C
#define cam_spin_obj_vert globals.cam_spin_obj_vert
#define light_src_obj globals.light_src_obj
#define dcam_zoom_speed globals.dcam_zoom_speed
#define dcam_flip_speed globals.dcam_flip_speed
#define percent_complete globals.percent_complete
#define card_flags_ro globals.card_flags_ro
#define bonus_round globals.bonus_round
#define card_part_count_ro globals.card_part_count_ro
#define box_count globals.box_count
#define item_pool1 globals.item_pool1
#define map_level_links globals.map_level_links
#define island_cam_rot_x globals.island_cam_rot_x
#define map_key_links globals.map_key_links
#define gem_stamp globals.gem_stamp
#define island_cam_state globals.island_cam_state
#define is_first_zone globals.is_first_zone
#define debug globals.debug
#define checkpoint_id globals.checkpoint_id
#define item_pool2 globals.item_pool2
#define title_pause_state globals.title_pause_state
#define caption_obj globals.caption_obj
#define draw_count_ro globals.draw_count_ro
#define card_str globals.card_str
#define card_icon globals.card_icon
#define card_partinfos globals.card_partinfos
#define gem_count globals.gem_count
#define key_count globals.key_count
#define saved_item_pool1 globals.saved_item_pool1
#define saved_item_pool2 globals.saved_item_pool2
#define pbak_state globals.pbak_state
#define spawn_trans globals.spawn_trans
#define fade_counter globals.fade_counter
#define fade_step globals.fade_step
#define death_count globals.death_count
#define dword_80061A40 globals.dword_80061A40
#define saved_level_count globals.saved_level_count
#define options_changed globals.options_changed
#define prev_box globals.prev_box
#define boxes_y globals.boxes_y
#define prev_box_entity globals.prev_box_entity

#endif /* _GLOBALS_H_ */
