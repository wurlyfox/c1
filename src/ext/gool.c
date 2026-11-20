#include "ext/gool.h"
#include "ext/gfx.h"

gool_handle handles_root = { .type = 3 }; // dummy object

extern gool_handle handles[8];
extern gool_object *objects;
extern gool_object *player;
extern gool_object *crash;
extern ns_struct ns;

/**
 * Get the descriptive name of a GOOL object.
 */
int GoolObjectName(const gool_object *obj, char *name) {
  zone_entity *entity;
  entry *exec;
  char *eid_str;
  int id, idx, type, subtype;

  if (obj == 0) {
    sprintf(name, "None");
    return 1;
  }
  if (!(((int)obj >= (int)objects &&
         (int)obj <= (int)(objects+(GOOL_OBJECT_COUNT-1))) 
      ||((int)obj == (int)player)
      ||((int)obj >= (int)&handles[0] &&
         (int)obj <= (int)&handles[7])
      ||((int)obj == (int)&handles_root))) {
    sprintf(name, "Non-object pointer");
    return 1;
  }
  if (obj->handle.type == 2) {
    idx = (gool_handle*)obj - handles;
    switch (idx) {
    case 1: strcpy(name, "HUD and playback"); break;
    case 3: strcpy(name, "Interactive"); break;
    case 4: strcpy(name, "Boxes and enemies"); break;
    case 6: strcpy(name, "Main objects"); break;
    case 7: strcpy(name, "Pause menu"); break;
    default: sprintf(name, "Handle %i", idx); break;
    }
    return 1;
  }
  if (obj->handle.type == 3) {
    sprintf(name, "Root");
    return 1;
  }
  entity = obj->entity;
  if ((int)entity >= (int)ns.pagemem 
   && (int)entity <= (int)(ns.pagemem+ns.physical_page_count)) {
    id = entity->id;
    type = entity->type;
    eid_str = NSEIDToString(((entry*)ns.ldat->exec_map[type])->eid);
    sprintf(name, "%i (%s)", id, eid_str);
  }
  else if ((exec = obj->external) || (exec = obj->global)) {
    eid_str = NSEIDToString(exec->eid);
    subtype = obj->subtype;
    sprintf(name, "%s (subtype %i)", eid_str, subtype);
  }
  else {
    sprintf(name, "Unknown @%08x", (uint32_t)obj);
  }
  return 1;
}

/**
 * Get the bounding box of a GOOL object.
 */
int GoolObjectBound2(gool_object *obj, gool_bound *out) {
  bound bound;
  vec scale;
  uint32_t status_a;

  if (!obj || !out || !obj->anim_seq) 
    return 0;
  bound = obj->bound;
  scale = obj->scale;
  status_a = obj->status_a;
  obj->status_a |= GOOL_FLAG_LBOUND_INVALID;
  GoolObjectGetBound(obj, out);
  if (obj == crash) {
    out->p1 = VecAdd(obj->trans, obj->bound.p1);
    out->p2 = VecAdd(obj->trans, obj->bound.p2);
  }
  obj->bound = bound;
  obj->scale = scale;
  obj->status_a = status_a;
  return 1;
}

/**
 * Get the GOOL object located at a given 2D point 
 * in normalized device coordinates.
 */
gool_object *GoolObjectByPoint2d(dvec2 ndc, ddim2 hdim) {
  gool_object *obj, *nearest = 0;
  gool_bound bound;
  vec view;
  double depth, min_depth = 1e300;
  int i;

  for (i=0;i<=GOOL_OBJECT_COUNT;i++) {
    obj = i==GOOL_OBJECT_COUNT ? crash : (objects ? &objects[i] : 0);
    if (!obj || obj->handle.type != 1) continue;
    if (!GoolObjectBound2(obj, &bound)) 
      continue;
    if (GfxDeprojectNDCToBound(&bound.bound, &ndc, &hdim, &view, &depth)
     && depth < min_depth) {
      min_depth = depth;
      nearest = obj;
    }
  }
  return nearest;
}
