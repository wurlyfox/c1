#ifndef _EXT_H_
#define _EXT_H_

#include "common.h"
#include <stdarg.h>
#include "geom.h"
#include "../gfx.h"

#include "formats/wgeo.h"
#include "formats/svtx.h"
#include "formats/cvtx.h"
#include "formats/zdat.h"

#include "pc/gfx/pcgfx.h"
#include "pc/gfx/gl.h"

#define SWAP(a,b) { tmp = a; a = b; b = tmp; }

#define EXT_MAX_MANUAL_POLYS 8192
#define EXT_MAX_TEXTURES 4096
#define EXT_MAX_CANDS 4096
#define EXT_MAX_CAM_SAMPLES 512
#define EXT_STATUS_DURATION_TICKS (5.0 * 34.0 * (1000000.0 / 32768.0))

/* global extension configuration */
typedef struct {
  int fps_mode;
  double target_fps;
  int aspect_mode; 
  int fullscreen;
  int enable_hwpersp;
  int enable_textures;
  int enable_gouraud;
  int enable_wireframe;
  int show_octrees;
  int show_objbounds;
  int show_wallmap;
  int show_fps; 
  int use_title_font;
} ext_config_t;

extern ext_config_t ext_config;

/* this draw timestamp (in ticks)
   is nearest in phase to the 30.5 Hz base rate
   when it has just crossed a multiple of 34
   assuming a previous timestamp occured ticks_per_frame ago */
#define IsOrigTick() ((ext_config.fps_mode==0) \
  || ((uint32_t)(context.draw_stamp / 34.0) \
  != ((uint32_t)((context.draw_stamp - ticks_per_frame) / 34.0))))

/* global state for high frame rate support */
typedef struct {
  double draw_count;
  double fade_rem;
  svtx_frame *next_frame;
  cvtx_frame *next_cvtx_frame;
  double interp;
  int is_last_frame;
  int trans;
  int crash_updated;
  int spd_count;
  int32_t spd_seen[8];
  double d_death_cam;
  int i_death_cam_prev;
} hq_context_t;

extern hq_context_t hq;

/* polygon refs use world_eid; 
   edge/vertex refs use world_idx;
   world vertex refs use vert_idx 
   within the world's vertex array and do not use poly_idx */
typedef struct {
  int manual;
  union {
    int world_idx;
    eid_t world_eid;
  };
  int poly_idx;
  union {
    int edge_idx;
    int vert_idx;
  };
} ext_geomref_t;

typedef ext_geomref_t ext_polyref_t;
typedef ext_geomref_t ext_edgeref_t;
typedef ext_geomref_t ext_vertref_t;

typedef struct {
  ext_polyref_t ref;
  dtri2_t proj;
} ext_cand_t;

/* extended unpacked polygon representation 
   this is distinct from poly3i used by the GL driver implementation */
typedef struct {
  uint32_t world_eid;
  int32_t trans_x, trans_y, trans_z;
  vec verts[3];
  fvec uvs[3];
  rgba colors[3];
  int texid;
  int flags;
  uint8_t vertex_locked[3];
  texinfo tinfo;
  int has_tinfo;
} ext_poly_t;

/* a manually created polygon */
typedef struct {
  union { 
    ext_poly_t; 
    ext_poly_t poly; 
  };
  int active;
} ext_mpoly_t;

/* common parameters passed to extension functions */
typedef struct {
  const ext_mpoly_t *mpolys;
  int mpoly_count;
  ddim2 hdim;
  dim2 window_dim;
  double zoom;
} ext_viewparams_t;

/* coverage stats */
typedef struct {
  int zone_samples;
  double zone_avg_coverage;
  double zone_min_coverage;
  int level_zones;
  int level_samples;
  double level_avg_coverage;
  double level_min_coverage;
} ext_stats_t;

/* cache for computing zone boundary edges and void edges */
typedef struct {
  entry *zone; 
  zone_path *path;  
  int32_t progress;
  ext_edgeref_t *edges;  
  ext_edgeref_t *void_edges; 
  int edge_count, void_count;
} ext_cand_cache_t;

/* mouse input state */
typedef struct {
  int ui_capture;
  vec2 pos, prev_pos, delta;
  int held, prev_held;    
  int press, release;
  int wheel;
  int drag;
  vec2 drag_start;
  vec2 drag_end;
} ext_mouse_t;

/* input state */
typedef struct {
  rect2 window;
  ext_mouse_t mouse;
  int keys_ui_capture;
  uint8_t keys[512];
} ext_input_t;

/* inspector/editor state */
typedef struct {
  ext_polyref_t pick;
  int pick_valid;
  int sel_edge;
  int sel_vert;
  int ui_capture;
  int drag_active;
  
  gool_object *obj_sel;
  gool_object *obj_hover;
  gool_object *obj_drag;
  entry *obj_zone;
  vec2 obj_drag_start;
  int obj_drag_moved;

  int hover_mode;
  int hover_kind;
  dtri2_t hover_tri;
  int hover_tri_valid;
  dvec2 hover_vtx;
  int hover_vtx_valid;

  int inherit_target;
  int inherit_color_target;
  int inherit_color_vertex;

  double snap_px;
  dvec2 snap_pt;
  int snap_valid;
} ext_state_t;

/* inspector/editor options */
typedef struct {
  int object_mouse_control;
  int enable_pick;
  int enable_hover;  
  int show_overlay;
  int show_normals;
  int show_vertices;      
  int enable_edit;          
  int enable_snap;          
  int enable_snap_color;     
  int enable_candidates;   
  int enable_auto_edge;     
} ext_options_t;

/* main extension context */
typedef struct {
  ext_options_t options;
  char json_path[128];
  char status[256];
  ticks_t status_time;
  ext_state_t state;
  ext_input_t input;
  /* texture blocks */
  texinfo texblocks[EXT_MAX_TEXTURES];
  int texblock_count;
  entry *texblock_zone;
  /* manual polygons */
  ext_mpoly_t mpolys[EXT_MAX_MANUAL_POLYS];
  int mpoly_count;
  /* candidate edges */
  ext_cand_cache_t cache;
  ext_cand_t candidates[EXT_MAX_CANDS];
  int candidate_count;
  /* computed statistics */
  ext_stats_t stats;
  entry *stats_zone;
  int stats_zone_dirty, stats_level_dirty;
  mat16 cam_samples[EXT_MAX_CAM_SAMPLES];
} ext_context_t;

extern ext_context_t ext_ctxt;

/* inspector/editor interface */
double ExtViewZoom(void);

int ExtWorldName(eid_t world_eid, char *name_out);

const ext_state_t *ExtGetState(const ext_context_t *ctxt);
const char *ExtGetStatus(ext_context_t *ctxt);
int ExtStatus(ext_context_t *ctxt, const char *fmt, ...);
int ExtStatusV(ext_context_t *ctxt, const char *fmt, va_list args);

int ExtGetSelectedPoly(ext_context_t *ctxt, ext_polyref_t *ref, ext_poly_t *poly);
int ExtGetSelectionCenter(ext_context_t *ctxt, dvec *out);
int ExtSelectPoly(ext_context_t *ctxt, const ext_polyref_t *ref);
int ExtExtrudeSelected(ext_context_t *ctxt);
int ExtDeleteSelected(ext_context_t *ctxt);

int ExtGetSelectedVertex(ext_context_t *ctxt);
void ExtSelectVertex(ext_context_t *ctxt, int idx);
int ExtGetVertexPos(ext_context_t *ctxt, int idx, vec *out);
int ExtGetVertexColor(ext_context_t *ctxt, int idx, rgba *out);
int ExtSetVertexPos(ext_context_t *ctxt, int idx, const vec *pos);
int ExtSetVertexColor(ext_context_t *ctxt, int idx, const rgba *color);

int ExtGetSelectedEdge(ext_context_t *ctxt);
int ExtSelectEdge(ext_context_t *ctxt, int e);

int ExtGetSelectedTexture(ext_context_t *ctxt, texinfo *tex);
const texinfo *ExtGetTexture(const ext_context_t *ctxt, int idx);
int ExtGetTextureIndex(const ext_context_t *ctxt, const texinfo *tex);
int ExtGetTextureCount(const ext_context_t *ctxt);
int ExtApplyTexture(ext_context_t *ctxt, int idx);
int ExtSetFlipWinding(ext_context_t *ctxt, int flip_winding);
int ExtSetWindingParts(ext_context_t *ctxt, int xflip, int yflip, int winding);

gool_object *ExtGetSelectedObject(const ext_context_t *ctxt);
void ExtSelectObject(ext_context_t *ctxt, gool_object *obj);

const ext_polyref_t *ExtGetCandidate(const ext_context_t *ctxt, int idx);
int ExtGetCandidateCount(const ext_context_t *ctxt);
int ExtGetSelectedCandidate(ext_context_t *ctxt);
int ExtSelectCandidate(ext_context_t *ctxt, int idx);

int ExtInheritTexToggle(ext_context_t *ctxt);
int ExtInheritColorToggle(ext_context_t *ctxt);

void ExtLoadJson(ext_context_t *ctxt);
void ExtSaveJson(ext_context_t *ctxt);

void ExtRecomputeStats(ext_context_t *ctxt, int level);
int ExtZoneStatsValid(const ext_context_t *ctxt);
const ext_stats_t *ExtGetStats(const ext_context_t *ctxt);

/* camera extension */
enum { CAM_MODE_GAME = 0, CAM_MODE_CHASE = 1, CAM_MODE_FREE = 2 };

/* camera pose structure */
typedef struct {
  dvec trans;
  ang rot;
} cam_pose;

/* extended camera state */
typedef struct {
  cam_state cam;
  cam_state cam_prev;
  int applied;
  cam_pose last_eff, free_base;
  double orbit_phi;
  double orbit_dir;
} ext_cam_state_t;

/* extended camera options */
typedef struct {
  int enable_input;
  int enable_orbit;
  int show_paths;
  float cam_zoom;     
} ext_cam_options_t;

/* extended camera (state, options, control) */
typedef struct {
  ext_cam_options_t options;
  int mode;
  dvec offs_trans;
  dang offs_rot;
  double speed; 
  double sens;
  double orbit_dir;
  double orbit_radius;
  double orbit_speed;
  ext_cam_state_t state;
} ext_cam_t;

extern ext_cam_t ext_cam;

/* extended camera functions */
void CamExApply(void);
void CamExRestore(void);
void CamExSetMode(int mode);
void CamExResetOffsets(void);
void CamExUpdate(const ext_input_t *xinput);
int CamExModifierHeld(const uint8_t *keys);

/* main extension functions */
void ExtAppendTris(ext_context_t *ctxt, poly3i *tris, int *tri_count);
void ExtUpdate(ext_context_t *ctxt, const gl_input *input);
int ExtSetFullscreen(ext_context_t *ctxt, int fullscreen);

void GLDrawCamOverlay(void);
void GLDrawExtOverlay(ext_context_t *ctxt);
void GLDrawObjectHighlights(ext_context_t *ctxt);

#endif /* _EXT_H_ */
