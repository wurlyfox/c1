/**
 * Type reflection metadata.
 *
 * See /ext/lib/refl.md for an overview of the type reflection functionality.
 */
 #include "./ext/lib/refl.h"

 #include "common.h"
 #include "geom.h"
 #include "gool.h"

int CountPlusOne(void *data, refl_field *field) {
  int count;

  count = ReflGetIntF(data, field);
  return count + 1;
}

size_t EntrySize(void *data, refl_field *field, int idx) {
  refl_type *type;
  size_t size;

  type = field->parent;
  size = ReflGetIntA(data, type, "entry_offsets", idx+1) -
         ReflGetIntA(data, type, "entry_offsets", idx);
  return size;
}

/* here we call this 'subtype' because it refers to the
   'inheriting structure type';
   though, it is based on the entry 'type' field value.
   we also don't want to confuse with the entry type -object- */
refl_type *EntrySubtype(void *data, refl_type *type) {
  int en_type;

  en_type = ReflGetInt(data, type, "type");
  switch (en_type) {
  case 7:
    return ReflGetType(0, "zone");
  }
  return 0; /* no subtype; default to base type */
}

size_t ItemSize(void *data, refl_field *field, int idx) {
  refl_type *type;
  size_t size;
  int count;

  // type = field->parent;
  type = ReflGetType(0, "entry");
  count = ReflGetInt(data, type, "item_count");
  if (idx == count) { return 0; }
  size = ReflGetIntA(data, type, "item_offsets", idx+1) -
         ReflGetIntA(data, type, "item_offsets", idx);
  return size;
}

int ZonePathCount(void *data, refl_field *field) {
  refl_type *header_type;
  refl_value value;
  void *header;
  int count;

  value = ReflGet(data, field->parent, "header");
  if (!value.valid) { return 0; }
  header = value.res;
  header_type = ReflGetType(0, "zone_header");
  count = ReflGetInt(header, header_type, "path_count");
  return count;
}

int ZoneEntityCount(void *data, refl_field *field) {
  refl_type *header_type;
  refl_value value;
  void *header;
  int count;

  value = ReflGet(data, field->parent, "header");
  if (!value.valid) { return 0; }
  header = value.res;
  header_type = ReflGetType(0, "zone_header");
  count = ReflGetInt(header, header_type, "entity_count");
  return count;
}

int GoolCodeCount(void *data, refl_field *field) {
  return ItemSize(data, field, 1) / sizeof(uint32_t);
}

int GoolDataCount(void *data, refl_field *field) {
  return ItemSize(data, field, 2) / sizeof(uint32_t);
}

int GoolStateCount(void *data, refl_field *field) {
  return ItemSize(data, field, 4) / sizeof(gool_state);
}

int GoolEventMapCount(void *data, refl_field *field) {
  refl_type *header_type;
  refl_value value;
  void *header;
  int count;

  value = ReflGet(data, field->parent, "header");
  if (!value.valid) { return 0; }
  header = value.res;
  header_type = ReflGetType(0, "gool_header");
  count = ReflGetInt(header, header_type, "subtype_map_idx")+1;
  return count;
}

int GoolSubtypeMapCount(void *data, refl_field *field) {
  int count;

  count = GoolEventMapCount(data, field);
  count -= (ItemSize(data, field, 3) / sizeof(uint16_t));
  return count;
}

int GoolAnimCount(void *data, refl_field *field) {
  refl_value value;
  refl_type *gool_anim_type;
  size_t size;
  int count;

  size = ItemSize(data, field, 5);
  gool_anim_type = ReflGetType(0, "gool_anim");
  count = 0;
  while (size) {
    value = ReflGet(data, gool_anim_type, 0);
    size -= value.size;
    data += value.size;
    ++count;
  }
  return count;
}

refl_type *GoolAnimSubtype(void *data, refl_type *type) {
  refl_type *subtype;
  int id;
  const char *subtype_map[] = {
   "gool_vertex_anim",
   "gool_sprite_anim",
   "gool_font",
   "gool_text",
   "gool_frag_anim"
  };

  id = ReflGetInt(data, type, "type");
  subtype = ReflGetType(type->subtypes, (char*)subtype_map[id+1]);
  return subtype;
}

int GoolFragCount(void *data, refl_field *field) {
  int count, len;

  len = ReflGetInt(data, field->parent, "length");
  count = ReflGetInt(data, field->parent, "frag_count") * len;
  return count;
}

int OctreeNodeCount(void *data, refl_field *field) {
  refl_value value;
  refl_type *ptype;
  int count;

  ptype = ReflGetType(0, "zone_rect");
  value = ReflGetParent(data, field, ptype, "octree");
  ptype = ReflGetType(0, "zone");
  value = ReflGetParent(value.res, value.field, ptype, "rect");
  count = ReflGetInt(value.res, value.field->type, "unk");
  return count;
}

// helper regexes for conversion from c header to refl_type def.
//
// 1)
// (#.*?.*\n*)|/\*.*\*/|extern .*\n|//.*\n
// <empty>
// 2)
// (?<!typedef) union {\n  ([\s\S]*?)};
// $1
// 3)  repeat as necessary
// (typedef struct (?:[a-zA-Z_]* )?{\n\s*)(.*?) (.*?);\s*\n*  ([\s\S]*?)} (.*);
// REFL_FIELD($5,$2,$3);\n$1$4} $5;
// and
// (typedef union (?:[a-zA-Z_]* )?{\n\s*)(.*?) (.*?);\s*\n*  ([\s\S]*?)} (.*);
// 4)
// ([^;])$
// );
// 5)
// ^(?!REFL_FIELD).*\n
// <empty>
// 6)
// REFL_FIELD\((.*?),(.*?),\*(.*?)\);
// REFL_FIELD($1,$2*,$3);
// 7)
// REFL_FIELD\((.*?),(.*?),(.*?)\[(.+?)\]\);
// REFL_ARRAY_FIELD($1,$2,$3,$4);
//
// python:
//
// import re
//
// repls = [{
//   'regex': "(#.*?.*\n*)|/\*.*\*/|extern .*\n|//.*\n",
//   'sub': "",
//   'times': 1
// },
// {
//   'regex': "(?<!typedef) union {\n  ([\s\S]*?)};",
//   'sub': "\g<1>",
//   'times': 1
// },
// {
//   'regex': "(?<!typedef) struct {\n  ([\s\S]*?)};",
//   'sub': "\g<1>",
//   'times': 3
// },
// {
//   'regex': "(typedef struct (?:[a-zA-Z_]* )?{\n\s*)(.*?) (.*?);\s*\n*  ([\s\S]*?)} (.*);",
//   'sub': "REFL_FIELD(\g<5>,\g<2>,\g<3>),\n\g<1>\g<4>} \g<5>;",
//   'times': 10
// },
// {
//   'regex': "(typedef union (?:[a-zA-Z_]* )?{\s*\n\s*)(.*?) (.*?);\s*\n*  ([\s\S]*?)} (.*);",
//   'sub': "REFL_FIELD(\g<5>,\g<2>,\g<3>),\n\g<1>\g<4>} \g<5>;",
//   'times': 10
// },
// {
//   'regex': "(typedef struct (?:[a-zA-Z_]* )?{\s*\n\s*)(.*?) (.*?);\s*\n*} (.*);",
//   'sub': "REFL_FIELD(\g<4>,\g<2>,\g<3>),",
//   'times': 1
// },
// {
//   'regex': "(typedef union (?:[a-zA-Z_]* )?{\s*\n\s*)(.*?) (.*?);\s*\n*} (.*);",
//   'sub': "REFL_FIELD(\g<4>,\g<2>,\g<3>),",
//   'times': 1
// },
// {
//   'regex': "REFL_FIELD\((.*?),(.*?),\*(.*?)\),",
//   'sub': "REFL_FIELD(\g<1>,\g<2>*,\g<3>),",
//   'times': 1
// },
// {
//   'regex': "REFL_FIELD\((.*?),(.*?),(.*?)\[(.+?)\]\),",
//   'sub': "REFL_ARRAY_FIELD(\g<1>,\g<2>,\g<3>,\g<4>),",
//   'times': 1
// },
// {
//  'regex': "(.*?)\((.*?),(.*?),(.*?)\),\n((?:.*?\(.*?\),\n)*)\n",
//  'sub': 'refl_type \g<2>_type = {\n  .name = "\g<2>",\n  .flags = REFL_FLAGS_STATIC,\n  .fields = {\n    \g<1>(\g<2>,\g<3>,\g<4>)\n    \g<5>  }\n};\n\n',
//  'times': 1
// },
// {
//  'regex': "\nREFL",
//  'sub': "\n    REFL",
//  'times': 1
// }
//]
//
// src = """<header file source here>"""
//
// for repl in repls:
//  for i in range(repl['times']):
//    src = re.sub(repl['regex'], repl['sub'], src)
// print(src)
//

/* geom */
refl_type vec_type = {
  .name = "vec",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(vec,int32_t,x),
    REFL_FIELD(vec,int32_t,y),
    REFL_FIELD(vec,int32_t,z),
    REFL_TERM
  }
};

refl_type vec16_type = {
  .name = "vec16",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(vec16,int16_t,x),
    REFL_FIELD(vec16,int16_t,y),
    REFL_FIELD(vec16,int16_t,z),
    REFL_TERM
  }
};

refl_type ang_type = {
  .name = "ang",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(ang,int32_t,y),
    REFL_FIELD(ang,int32_t,x),
    REFL_FIELD(ang,int32_t,z),
    REFL_TERM
  }
};

refl_type ang2_type = {
  .name = "ang2",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(ang2,int32_t,x),
    REFL_FIELD(ang2,int32_t,y),
    REFL_TERM
  }
};

refl_type rgb8_type = {
  .name = "rgb8",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(rgb8,uint8_t,r),
    REFL_FIELD(rgb8,uint8_t,g),
    REFL_FIELD(rgb8,uint8_t,b),
    REFL_TERM
  }
};

refl_type rgb16_type = {
  .name = "rgb16",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(rgb16,uint16_t,r),
    REFL_FIELD(rgb16,uint16_t,g),
    REFL_FIELD(rgb16,uint16_t,b),
    REFL_TERM
  }
};

refl_type bound_type = {
  .name = "bound",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(bound,vec,p1),
    REFL_FIELD(bound,vec,p2),
    REFL_TERM
  }
};

refl_type mat16_type = {
  .name = "mat16",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_ARRAY_FIELD(mat16,vec16,v,3),
    REFL_FIELD(mat16,vec16,t),
    REFL_TERM
  }
};

refl_type texinfo_type = {
  .name = "texinfo",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    { .name = "f1", .typename = "uint32_t", .offset = 0 },
    { .name = "f2", .typename = "uint32_t", .offset = 4 },
    { .name = "f3", .typename = "uint32_t", .offset = 8 },
    REFL_TERM
  }
};

/* page */
refl_type pgid_type = {
  .name = "pgid_t",
  .flags = REFL_FLAGS_STATIC | REFL_FLAGS_PRIM,
  .size = 4
};

refl_type page_type = {
  .name = "page",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(page,uint16_t,magic),
    REFL_FIELD(page,uint16_t,type),
    REFL_FIELD(page,pgid_t,pgid),
    REFL_FIELD(page,uint32_t,entry_count),
    REFL_FIELD(page,uint32_t,checksum),
    REFL_DYNARRAY_FIELD_F(page,uint32_t,entry_offsets,entry_count,CountPlusOne),
    REFL_DYNARRAY_FIELD_F(page,entry*,entries,entry_count,CountPlusOne),
    REFL_DYNARRAY_FIELD_A(page,entry,entries2,entry_offsets,entry_count),
    REFL_ARRAY_FIELD(page,uint8_t,data,PAGE_SIZE),
    REFL_TERM
  }
};

refl_type page_compressed_type = {
  .name = "page_compressed",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(page_compressed,uint16_t,magic),
    REFL_FIELD(page_compressed,uint16_t,pad),
    REFL_FIELD(page_compressed,uint32_t,length),
    REFL_FIELD(page_compressed,uint32_t,skip),
    // REFL_FIELD(page_compressed,uint8_t,data[]);
    REFL_TERM
  }
};

refl_type tpage_type = {
  .name = "tpage",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(tpage,uint16_t,magic),
    REFL_FIELD(tpage,uint16_t,type),
    REFL_FIELD(tpage,eid_t,eid),
    REFL_FIELD(tpage,uint32_t,entry_type),
    REFL_FIELD(tpage,uint32_t,checksum),
    REFL_ARRAY_FIELD(tpage,uint8_t,data,PAGE_SIZE),
    REFL_TERM
  }
};

refl_type page_ref_type = {
  .name = "page_ref",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(page_ref,pgid_t,pgid),
    REFL_FIELD(page_ref,page*,pg),
    REFL_FIELD(page_ref,uint32_t,value),
    REFL_TERM
  }
};

/* entry */
refl_type eid_type = {
  .name = "eid_t",
  .flags = REFL_FLAGS_STATIC | REFL_FLAGS_PRIM,
  .size = 4
};

refl_type entry_type = {
  .name = "entry",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(entry,uint32_t,magic),
    REFL_FIELD(entry,eid_t,eid),
    REFL_FIELD(entry,int,type),
    REFL_FIELD(entry,int,item_count),
    REFL_DYNARRAY_FIELD_F(entry,uint32_t,item_offsets,item_count,CountPlusOne),
    REFL_DYNARRAY_FIELD_F(entry,uint8_t*,items,item_count,CountPlusOne),
    REFL_DYN2DARRAY_FIELD_A(entry,uint8_t,items2,item_offsets,item_count,ItemSize),
    REFL_TERM
  },
  .subtype_fn = EntrySubtype
};

/* nsd */
refl_type lid_type = {
  .name = "lid_t",
  .flags = REFL_FLAGS_STATIC | REFL_FLAGS_PRIM,
  .size = 4
};

refl_type nsd_ldat_type = {
  .name = "nsd_ldat",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(nsd_ldat, uint32_t, magic),
    REFL_FIELD(nsd_ldat, lid_t, lid),
    REFL_FIELD(nsd_ldat, eid_t, zone_spawn),
    REFL_FIELD(nsd_ldat, int, path_idx_spawn),
    REFL_FIELD(nsd_ldat, uint32_t, unk_10),
    REFL_ARRAY_FIELD(nsd_ldat, eid_t, exec_map, NSD_EXEC_COUNT),
    REFL_FIELD(nsd_ldat, uint32_t, fov),
    REFL_ARRAY_FIELD(nsd_ldat, uint8_t, image_data, 0xF5F8),
    REFL_TERM
  }
};

refl_type nsd_pte_type = {
  .name = "nsd_pte",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(nsd_pte,pgid_t,pgid),
    // REFL_FIELD(nsd_pte,entry*,entry_c1),
    REFL_FIELD(nsd_pte,uint32_t,value),
    REFL_FIELD(nsd_pte,eid_t,eid),
    REFL_FIELD(nsd_pte,entry*,entry),
    REFL_FIELD(nsd_pte,uint32_t,key),
    REFL_TERM
  }
};

refl_type nsd_type = {
  .name = "nsd",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_ARRAY_FIELD(nsd, uint32_t, ptb_offsets, NSD_PTB_COUNT),
    REFL_ARRAY_FIELD(nsd, nsd_pte*, page_table_buckets, NSD_PTB_COUNT),
    REFL_FIELD(nsd, int, page_count),
    REFL_FIELD(nsd, size_t, page_table_size),
    REFL_FIELD(nsd, eid_t, ldat_eid),
    REFL_FIELD(nsd, int, has_loading_image),
    REFL_FIELD(nsd, uint32_t, loading_image_width),
    REFL_FIELD(nsd, uint32_t, loading_image_height),
    REFL_FIELD(nsd, uint32_t, pages_offset),
    REFL_FIELD(nsd, page*, pages),
    REFL_FIELD(nsd, int32_t, compressed_page_count),
    REFL_ARRAY_FIELD(nsd, uint32_t, compressed_page_offsets, NSD_COMPRESSED_PAGE_COUNT),
    REFL_ARRAY_FIELD(nsd, uint8_t*, compressed_pages, NSD_COMPRESSED_PAGE_COUNT),
    REFL_DYNARRAY_FIELD(nsd, nsd_pte, page_table, page_table_size),
    REFL_FIELD_A(nsd, nsd_ldat, ldat, page_table),
    REFL_TERM
  }
};

refl_type entry_ref_type = {
  .name = "entry_ref",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(entry_ref,eid_t,eid),
    REFL_FIELD(entry_ref,entry*,en),
    REFL_FIELD(entry_ref,uint32_t,value),
    REFL_TERM
  }
};

refl_type ns_loadlist_type = {
  .name = "ns_loadlist",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(ns_loadlist,int,entry_count),
    REFL_FIELD(ns_loadlist,int,page_count),
    REFL_ARRAY_FIELD(ns_loadlist,eid_t,eids,8),
    REFL_ARRAY_FIELD(ns_loadlist,entry*,entries,8),
    REFL_ARRAY_FIELD(ns_loadlist,pgid_t,pgids,32),
    REFL_ARRAY_FIELD(ns_loadlist,page*,pages,32),
    REFL_TERM
  }
};

/* zone */
refl_type zone_world_type = {
  .name = "zone_world",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(zone_world,eid_t,eid),
    REFL_FIELD(zone_world,entry*,entry),
    REFL_FIELD(zone_world,uint32_t,key),
    REFL_FIELD(zone_world,uint32_t,tag),
    REFL_FIELD(zone_world,vec,trans),
    //REFL_FIELD(zone_world,wgeo_header*,header),
    //REFL_FIELD(zone_world,wgeo_polygon*,polygons),
    //REFL_FIELD(zone_world,wgeo_vertex*,vertices),
    //REFL_FIELD(zone_world,wgeo_texinfo*,texinfos),
    REFL_FIELD(zone_world,uint8_t*,header),
    REFL_FIELD(zone_world,uint8_t*,polygons),
    REFL_FIELD(zone_world,uint8_t*,vertices),
    REFL_FIELD(zone_world,uint8_t*,texinfos),
    REFL_ARRAY_FIELD(zone_world,entry*,tpages,8),
    REFL_TERM
  }
};

refl_type zone_gfx_type = {
  .name = "zone_gfx",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(zone_gfx,uint32_t,vram_fill_height),
    REFL_FIELD(zone_gfx,uint32_t,unknown_a),
    REFL_FIELD(zone_gfx,uint32_t,visibility_depth),
    REFL_FIELD(zone_gfx,uint32_t,unknown_b),
    REFL_FIELD(zone_gfx,uint32_t,unknown_c),
    REFL_FIELD(zone_gfx,uint32_t,unknown_d),
    REFL_FIELD(zone_gfx,uint32_t,unknown_e),
    REFL_FIELD(zone_gfx,uint32_t,flags),
    REFL_FIELD(zone_gfx,int32_t,water_y),
    REFL_FIELD(zone_gfx,eid_t,midi),
    REFL_FIELD(zone_gfx,uint32_t,unknown_g),
    REFL_FIELD(zone_gfx,rgb8,unknown_h),
    REFL_FIELD(zone_gfx,rgb8,vram_fill),
    REFL_FIELD(zone_gfx,uint8_t,unused_a),
    REFL_FIELD(zone_gfx,rgb8,far_color),
    REFL_FIELD(zone_gfx,uint8_t,unused_b),
    REFL_FIELD(zone_gfx,gool_colors,object_colors),
    REFL_FIELD(zone_gfx,gool_colors,player_colors),
    REFL_TERM
  }
};

refl_type zone_header_type = {
  .name = "zone_header",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(zone_header,uint32_t,world_count),
    REFL_ARRAY_FIELD(zone_header,zone_world,worlds,8),
    REFL_FIELD(zone_header,uint32_t,paths_idx),
    REFL_FIELD(zone_header,uint32_t,path_count),
    REFL_FIELD(zone_header,uint32_t,entity_count),
    REFL_FIELD(zone_header,uint32_t,neighbor_count),
    REFL_ARRAY_FIELD(zone_header,eid_t,neighbors,8),
    REFL_FIELD(zone_header,ns_loadlist,loadlist),
    REFL_FIELD(zone_header,uint32_t,display_flags),
    REFL_FIELD(zone_header,zone_gfx,gfx),
    REFL_TERM
  }
};

refl_type zone_octree_type = {
  .name = "zone_octree",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(zone_octree,uint16_t,root),
    REFL_FIELD(zone_octree,uint16_t,max_depth_x),
    REFL_FIELD(zone_octree,uint16_t,max_depth_y),
    REFL_FIELD(zone_octree,uint16_t,max_depth_z),
    REFL_DYNARRAY_FIELD_F(zone_octree,uint16_t,nodes,,OctreeNodeCount),
    REFL_TERM
  }
};

refl_type zone_rect_type = {
  .name = "zone_rect",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(zone_rect,int32_t,x),
    REFL_FIELD(zone_rect,int32_t,y),
    REFL_FIELD(zone_rect,int32_t,z),
    REFL_FIELD(zone_rect,uint32_t,w),
    REFL_FIELD(zone_rect,uint32_t,h),
    REFL_FIELD(zone_rect,uint32_t,d),
    REFL_FIELD(zone_rect,uint32_t,unk),
    REFL_FIELD(zone_rect,zone_octree,octree),
    REFL_TERM
  }
};

refl_type zone_neighbor_path_type = {
  .name = "zone_neighbor_path",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(zone_neighbor_path,uint8_t,relation),
    REFL_FIELD(zone_neighbor_path,uint8_t,neighbor_zone_idx),
    REFL_FIELD(zone_neighbor_path,uint8_t,path_idx),
    REFL_FIELD(zone_neighbor_path,uint8_t,goal),
    REFL_TERM
  }
};

refl_type zone_path_point_type = {
  .name = "zone_path_point",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(zone_path_point,int16_t,x),
    REFL_FIELD(zone_path_point,int16_t,y),
    REFL_FIELD(zone_path_point,int16_t,z),
    REFL_FIELD(zone_path_point,int16_t,rot_x),
    REFL_FIELD(zone_path_point,int16_t,rot_y),
    REFL_FIELD(zone_path_point,int16_t,rot_z),
    REFL_TERM
  }
};

refl_type zone_path_type = {
  .name = "zone_path",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(zone_path,eid_t,slst),
    REFL_FIELD(zone_path,entry*,parent_zone),
    REFL_FIELD(zone_path,uint32_t,neighbor_path_count),
    REFL_ARRAY_FIELD(zone_path,zone_neighbor_path,neighbor_paths,4),
    REFL_FIELD(zone_path,uint8_t,entrance_index),
    REFL_FIELD(zone_path,uint8_t,exit_index),
    REFL_FIELD(zone_path,uint16_t,length),
    REFL_FIELD(zone_path,uint16_t,cam_mode),
    REFL_FIELD(zone_path,uint16_t,avg_node_dist),
    REFL_FIELD(zone_path,uint16_t,cam_zoom),
    REFL_FIELD(zone_path,uint16_t,unknown_a),
    REFL_FIELD(zone_path,uint16_t,unknown_b),
    REFL_FIELD(zone_path,uint16_t,unknown_c),
    REFL_FIELD(zone_path,int16_t,direction_x),
    REFL_FIELD(zone_path,int16_t,direction_y),
    REFL_FIELD(zone_path,int16_t,direction_z),
    REFL_DYNARRAY_FIELD(zone_path,zone_path_point,points,length),
    REFL_TERM
  }
};

refl_type zone_entity_path_point_type = {
  .name = "zone_entity_path_point",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(zone_entity_path_point,int16_t,x),
    REFL_FIELD(zone_entity_path_point,int16_t,y),
    REFL_FIELD(zone_entity_path_point,int16_t,z),
    REFL_TERM
  }
};

refl_type zone_entity_type = {
  .name = "zone_entity",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(zone_entity,entry*,parent_zone),
    REFL_FIELD(zone_entity,uint16_t,spawn_flags),
    REFL_FIELD(zone_entity,uint16_t,group),
    REFL_FIELD(zone_entity,uint16_t,id),
    REFL_FIELD(zone_entity,uint16_t,path_length),
    REFL_FIELD(zone_entity,uint16_t,init_flags_a),
    REFL_FIELD(zone_entity,uint16_t,init_flags_b),
    REFL_FIELD(zone_entity,uint16_t,init_flags_c),
    REFL_FIELD(zone_entity,uint8_t,type),
    REFL_FIELD(zone_entity,uint8_t,subtype),
    REFL_DYNARRAY_FIELD(zone_entity,zone_entity_path_point,path_points,path_length),
    REFL_TERM
  }
};

refl_type zone_type = {
  .name = "zone",
  .basename = "entry",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD_A(entry,zone_header,header,item_offsets),
    REFL_FIELD_A(entry,zone_rect,rect,header),
    REFL_DYNARRAY_FIELD_AF(entry,zone_path,paths,rect,0,ZonePathCount),
    REFL_DYNARRAY_FIELD_AF(entry,zone_entity,entities,paths,0,ZoneEntityCount),
    REFL_TERM
  }
};

/* gool (entry) */
refl_type gool_header_type = {
  .name = "gool_header",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_header,uint32_t,type),
    REFL_FIELD(gool_header,uint32_t,category),
    REFL_FIELD(gool_header,uint32_t,unk_0x8),
    REFL_FIELD(gool_header,uint32_t,init_sp),
    REFL_FIELD(gool_header,uint32_t,subtype_map_idx),
    REFL_FIELD(gool_header,uint32_t,unk_0x14),
    REFL_TERM
  }
};

refl_type gool_state_maps_type = {
  .name = "gool_state_maps",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_DYNARRAY_FIELD_F(gool_state_maps,uint16_t,event_map,0,GoolEventMapCount),
    REFL_DYNARRAY_FIELD_AF(gool_state_maps,uint16_t,subtype_map,event_map,0,GoolSubtypeMapCount),
    REFL_TERM
  }
};

refl_type gool_state_type = {
  .name = "gool_state",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_state,uint32_t,flags),
    REFL_FIELD(gool_state,uint32_t,status_c),
    REFL_FIELD(gool_state,uint16_t,extern_idx),
    REFL_FIELD(gool_state,uint16_t,pc_event),
    REFL_FIELD(gool_state,uint16_t,pc_trans),
    REFL_FIELD(gool_state,uint16_t,pc_code),
    REFL_TERM
  }
};

refl_type gool_vertex_anim_type = {
  .name = "gool_vertex_anim",
  .basename = "gool_anim",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_vertex_anim,eid_t,eid),
    REFL_TERM
  }
};

refl_type gool_sprite_anim_type = {
  .name = "gool_sprite_anim",
  .basename = "gool_anim",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_sprite_anim,eid_t,tpage),
    REFL_DYNARRAY_FIELD(gool_sprite_anim,texinfo,texinfos,SpriteTexinfoCount),
    REFL_TERM
  }
};

refl_type gool_glyph_type = {
  .name = "gool_glyph",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_glyph,texinfo,texinfo),
    REFL_FIELD(gool_glyph,uint16_t,width),
    REFL_FIELD(gool_glyph,uint16_t,height),
    REFL_TERM
  }
};

refl_type gool_frag_type = {
  .name = "gool_frag",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_frag,texinfo,texinfo),
    REFL_FIELD(gool_frag,uint16_t,x1),
    REFL_FIELD(gool_frag,uint16_t,y1),
    REFL_FIELD(gool_frag,uint16_t,x2),
    REFL_FIELD(gool_frag,uint16_t,y2),
    REFL_TERM
  }
};

refl_type gool_font_type = {
  .name = "gool_font",
  .basename = "gool_anim",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_font,eid_t,tpage),
    REFL_ARRAY_FIELD(gool_font,gool_glyph,glyphs,63),
    REFL_FIELD(gool_font,gool_frag,backdrop),
    REFL_FIELD(gool_font,int,has_backdrop),
    REFL_TERM
  }
};

refl_type gool_text_type = {
  .name = "gool_text",
  .basename = "gool_anim",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_text,uint32_t,unk_0x4),
    REFL_FIELD(gool_text,uint32_t,glyphs_offset),
    REFL_DYN2DARRAY_FIELD_F(gool_text,uint8_t,strings,unk_0x4,0,(refl_sizefn_t)ReflStrlen),
    REFL_TERM
  }
};

refl_type gool_frag_anim_type = {
  .name = "gool_frag_anim",
  .basename = "gool_anim",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_frag_anim,eid_t,tpage),
    REFL_FIELD(gool_frag_anim,uint32_t,frag_count),
    REFL_DYNARRAY_FIELD_F(gool_frag_anim,gool_frag,frags,0,GoolFragCount),
    REFL_TERM
  }
};

refl_type gool_anim_type = {
  .name = "gool_anim",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_anim,uint8_t,type),
    REFL_FIELD(gool_anim,uint8_t,unused),
    REFL_FIELD(gool_anim,uint8_t,length),
    REFL_FIELD(gool_anim,uint8_t,unused_2),
    REFL_FIELD(gool_anim,eid_t,eid),
    REFL_ARRAY_FIELD(gool_anim,uint8_t,data,0),
    REFL_TERM
  },
  .subtype_fn = GoolAnimSubtype
};

refl_type gool_colors_type = {
  .name = "gool_colors",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_colors,mat16,light_mat),
    REFL_FIELD(gool_colors,rgb16,color),
    REFL_ARRAY_FIELD(gool_colors,uint16_t,l,12),
    REFL_FIELD(gool_colors,mat16,color_mat),
    REFL_FIELD(gool_colors,rgb16,intensity),
    REFL_ARRAY_FIELD(gool_colors,rgb16,vert_colors,4),
    REFL_ARRAY_FIELD(gool_colors,uint16_t,c,12),
    REFL_ARRAY_FIELD(gool_colors,uint16_t,a,24),
    REFL_TERM
  }
};

refl_type gool_type = {
  .name = "gool",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD_A(gool,gool_header,header,item_offsets),
    REFL_DYNARRAY_FIELD_AF(gool,uint32_t,code,header,0,GoolCodeCount),
    REFL_DYNARRAY_FIELD_AF(gool,uint32_t,data,code,0,GoolDataCount),
    REFL_FIELD_A(gool,gool_state_maps,state_maps,data),
    REFL_DYNARRAY_FIELD_AF(gool,gool_state,states,state_maps,0,GoolStateCount),
    REFL_DYNARRAY_FIELD_AF(gool,gool_anim,anims,states,0,GoolAnimCount),
    REFL_TERM
  }
};

refl_type gool_links_type = {
  .name = "gool_links",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_links,gool_object*,self),
    REFL_FIELD(gool_links,gool_object*,parent),
    REFL_FIELD(gool_links,gool_object*,sibling),
    REFL_FIELD(gool_links,gool_object*,children),
    REFL_FIELD(gool_links,gool_object*,creator),
    REFL_FIELD(gool_links,gool_object*,player),
    REFL_FIELD(gool_links,gool_object*,collider),
    REFL_FIELD(gool_links,gool_object*,interrupter),
    REFL_TERM
  }
};

refl_type gool_link_type = {
  .name = "gool_link",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_link,gool_object*,prev),
    REFL_FIELD(gool_link,gool_object*,next),
    REFL_TERM
  }
};

refl_type gool_vectors_type = {
  .name = "gool_vectors",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_vectors,vec,trans),
    REFL_FIELD(gool_vectors,ang,rot),
    REFL_FIELD(gool_vectors,vec,scale),
    REFL_FIELD(gool_vectors,vec,velocity),
    REFL_FIELD(gool_vectors,vec,misc_a),
    REFL_FIELD(gool_vectors,gool_link,box_link),
    REFL_FIELD(gool_vectors,ang,misc_b),
    REFL_FIELD(gool_vectors,int32_t,ang_velocity_x),
    REFL_FIELD(gool_vectors,ang2,target_rot),
    REFL_FIELD(gool_vectors,vec,misc_c),
    REFL_FIELD(gool_vectors,uint32_t,mode_flags_a),
    REFL_FIELD(gool_vectors,uint32_t,mode_flags_b),
    REFL_FIELD(gool_vectors,uint32_t,mode_flags_c),
    REFL_TERM
  }
};

refl_type gool_process_type = {
  .name = "gool_process",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_process,gool_object*,self),
    REFL_FIELD(gool_process,gool_object*,parent),
    REFL_FIELD(gool_process,gool_object*,sibling),
    REFL_FIELD(gool_process,gool_object*,children),
    REFL_FIELD(gool_process,gool_object*,creator),
    REFL_FIELD(gool_process,gool_object*,player),
    REFL_FIELD(gool_process,gool_object*,collider),
    REFL_FIELD(gool_process,gool_object*,interrupter),
    REFL_FIELD(gool_process,gool_vectors,vectors),
    REFL_ARRAY_FIELD(gool_process,vec,vectors_v,6),
    REFL_ARRAY_FIELD(gool_process,ang,vectors_a,6),
    REFL_FIELD(gool_process,uint32_t,status_a),
    REFL_FIELD(gool_process,uint32_t,status_b),
    REFL_FIELD(gool_process,uint32_t,status_c),
    REFL_FIELD(gool_process,uint32_t,subtype),
    REFL_FIELD(gool_process,uint32_t,pid_flags),
    REFL_FIELD(gool_process,uint32_t*,sp),
    REFL_FIELD(gool_process,uint32_t*,pc),
    REFL_FIELD(gool_process,uint32_t*,fp),
    REFL_FIELD(gool_process,uint32_t*,tp),
    REFL_FIELD(gool_process,uint32_t*,ep),
    REFL_FIELD(gool_process,uint32_t*,once_p),
    REFL_FIELD(gool_process,uint32_t,misc_flag),
    REFL_FIELD(gool_process,gool_object*,misc_child),
    REFL_FIELD(gool_process,uint32_t,misc_node),
    REFL_FIELD(gool_process,entry*,misc_entry),
    REFL_FIELD(gool_process,uint32_t,misc_memcard),
    REFL_FIELD(gool_process,uint32_t,ack),
    REFL_FIELD(gool_process,uint32_t,anim_stamp),
    REFL_FIELD(gool_process,uint32_t,state_stamp),
    REFL_FIELD(gool_process,uint32_t,anim_counter),
    REFL_FIELD(gool_process,gool_anim*,anim_seq),
    REFL_FIELD(gool_process,uint32_t,anim_frame),
    REFL_FIELD(gool_process,zone_entity*,entity),
    REFL_FIELD(gool_process,int32_t,path_progress),
    REFL_FIELD(gool_process,uint32_t,path_length),
    REFL_FIELD(gool_process,uint32_t,floor_y),
    REFL_FIELD(gool_process,uint32_t,state_flags),
    REFL_FIELD(gool_process,int32_t,speed),
    REFL_FIELD(gool_process,uint32_t,invincibility_state),
    REFL_FIELD(gool_process,uint32_t,invincibility_stamp),
    REFL_FIELD(gool_process,uint32_t,floor_impact_stamp),
    REFL_FIELD(gool_process,int32_t,floor_impact_velocity),
    REFL_FIELD(gool_process,uint32_t,size),
    REFL_FIELD(gool_process,uint32_t,event),
    REFL_FIELD(gool_process,int32_t,cam_zoom),
    REFL_FIELD(gool_process,int32_t,ang_velocity_y),
    REFL_FIELD(gool_process,int32_t,hotspot_size),
    REFL_FIELD(gool_process,uint32_t,voice_id),
    REFL_FIELD(gool_process,uint32_t,_150),
    REFL_FIELD(gool_process,uint32_t,_154),
    REFL_FIELD(gool_process,uint32_t,node),
    REFL_ARRAY_FIELD(gool_process,uint32_t,memory,64),
    REFL_TERM
  }
};

refl_type gool_handle_type = {
  .name = "gool_handle",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_handle,int,type),
    REFL_FIELD(gool_handle,gool_object*,children),
    REFL_FIELD(gool_handle,int,subtype),
    REFL_TERM
  }
};

refl_type gool_object_type = {
  .name = "gool_object",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_object,gool_handle,handle),
    REFL_FIELD(gool_object,bound,bound),
    REFL_FIELD(gool_object,entry*,global),
    REFL_FIELD(gool_object,entry*,external),
    REFL_FIELD(gool_object,entry*,zone),
    REFL_FIELD(gool_object,uint32_t,state),
    REFL_FIELD(gool_object,gool_colors,colors),
    REFL_ARRAY_FIELD(gool_object,uint16_t,colors_i,24),
    REFL_FIELD(gool_object,gool_process,process),
    REFL_ARRAY_FIELD(gool_object,uint32_t,regs,0x1FC),
    REFL_TERM
  }
};

refl_type gool_objnode_type = {
  .name = "gool_objnode",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_objnode,gool_object*,obj),
    REFL_FIELD(gool_objnode,uint32_t,node),
    REFL_FIELD(gool_objnode,uint32_t,value),
    REFL_TERM
  }
};


refl_type gool_event_query_type = {
  .name = "gool_event_query",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_event_query,gool_object*,sender),
    REFL_FIELD(gool_event_query,uint32_t,event),
    REFL_FIELD(gool_event_query,int,argc),
    REFL_FIELD(gool_event_query,int,type),
    REFL_FIELD(gool_event_query,int,count),
    REFL_FIELD(gool_event_query,uint32_t*,argv),
    REFL_TERM
  }
};

refl_type gool_nearest_query_type = {
  .name = "gool_nearest_query",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_nearest_query,uint32_t,categories),
    REFL_FIELD(gool_nearest_query,gool_object*,obj),
    REFL_FIELD(gool_nearest_query,gool_object*,nearest_obj),
    REFL_FIELD(gool_nearest_query,int32_t,dist),
    REFL_FIELD(gool_nearest_query,uint32_t,event),
    REFL_TERM
  }
};


refl_type gool_bound_type = {
  .name = "gool_bound",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_bound,bound,bound),
    REFL_FIELD(gool_bound,gool_object*,obj),
    REFL_TERM
  }
};

refl_type gool_move_state_type = {
  .name = "gool_move_state",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_move_state,int,dir),
    REFL_FIELD(gool_move_state,int32_t,angle),
    REFL_FIELD(gool_move_state,int32_t,speed_scale),
    REFL_TERM
  }
};

refl_type gool_accel_state_type = {
  .name = "gool_accel_state",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_accel_state,int32_t,accel),
    REFL_FIELD(gool_accel_state,int32_t,max_speed),
    REFL_FIELD(gool_accel_state,uint32_t,unk),
    REFL_FIELD(gool_accel_state,int32_t,decel),
    REFL_TERM
  }
};

refl_type gool_const_buf_type = {
  .name = "gool_const_buf",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_const_buf,uint32_t*,buf),
    REFL_FIELD(gool_const_buf,int,idx),
    REFL_TERM
  }
};

refl_type gool_state_ref_type = {
  .name = "gool_state_ref",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    REFL_FIELD(gool_state_ref,uint32_t,state),
    REFL_FIELD(gool_state_ref,int,guard),
    REFL_TERM
  }
};

refl_type gool_handles = {
  .name = "gool_handles",
  .flags = REFL_FLAGS_STATIC,
  .fields = {
    { .name = "h", .typename="gool_handle", .count=8, .offset=0 },
    REFL_TERM
  }
};

refl_type *types[] = {
  &pgid_type, &page_type, &page_compressed_type, &tpage_type, &page_ref_type,
  &eid_type, &entry_type, &entry_ref_type,
  &lid_type, &nsd_ldat_type, &nsd_pte_type, &nsd_type, &ns_loadlist_type,
  &zone_world_type, &zone_gfx_type, &zone_header_type,
  &zone_octree_type, &zone_rect_type,
  &zone_neighbor_path_type, &zone_path_point_type, &zone_path_type,
  &zone_entity_path_point_type, &zone_entity_type,
  &zone_type,
  &gool_header_type, &gool_state_maps_type, &gool_state_type, &gool_anim_type,
  &gool_vertex_anim_type, &gool_sprite_anim_type, &gool_font_type, &gool_text_type, &gool_frag_anim_type,
  &gool_glyph_type, &gool_frag_type,
  &gool_type,
  &gool_handle_type, &gool_colors_type, &gool_links_type, &gool_vectors_type,
  &gool_process_type, &gool_object_type,
  &gool_link_type, &gool_objnode_type, &gool_event_query_type, &gool_nearest_query_type,
  &gool_bound_type, &gool_move_state_type, &gool_accel_state_type,
  &gool_const_buf_type, &gool_state_ref_type,
  &vec_type, &vec16_type, &ang_type, &ang2_type, &rgb8_type, &rgb16_type, 
  &mat16_type, &bound_type, &texinfo_type,
  &gool_handles, 
  0
};

void ReflInitA() {
  ReflInit(types);
}
