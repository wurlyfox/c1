/*
   c ports of the asm funcs;
   here solely for documentation purposes (sake of readability)
   these can be linked instead of the asm funcs but may not perform as well
*/

void RGteTransformSvtx(
  svtx_frame *frame,
  void *ot,
  tgeo_polygon *polys,
  tgeo_header *header,
  uint32_t cull_mask,
  int far,
  void **prims_tail,
  rect28 *regions,
  int flag) {
  int i;
  vec u_vert;
  svtx_vertex *verts, *vert, *poly_verts[3];
  tgeo_texinfo *texinfos, info;
  rgb8 color;

  verts = &frame->vertices;
  texinfos = &header->texinfos;
  for (i=0;i<header->poly_count;i++) {
    poly=&polys[i];
    info=texinfos[poly->texinfo_idx];
    poly_verts[0]=(svtx_vertex*)((uint8_t*)verts+poly->a_idx);
    poly_verts[1]=(svtx_vertex*)((uint8_t*)verts+poly->b_idx);
    poly_verts[2]=(svtx_vertex*)((uint8_t*)verts+poly->c_idx);
    for (ii=0;ii<3;ii++) {
      vert=poly_verts[ii];
      u_vert.x=(frame->x+(vert->x-128))*4;
      u_vert.y=(frame->y+(vert->y-128))*4;
      u_vert.z=(frame->z+(vert->z-128))*4;
      $T0=(u_vert.y<<16)|u_vert.x;
      $T1=u_vert.z;
      if (ii == 0) {
        __asm {
          mtc2    $t0, $0
          mtc2    $t1, $1
        }
      }
      else if (ii == 1) {
        __asm {
          mtc2    $t0, $2
          mtc2    $t1, $3
        }
      }
      else if (ii == 2) {
        __asm {
          mtc2    $t0, $4
          mtc2    $t1, $5
        }
      }
    }
    __asm {
      cop2    0x280030
      cfc2    $t0, $31
    }
    if ($T0 < 0) { continue; }
    if (!info.no_cull) {
      __asm {
        cop2    0x1400006
        mfc2    $t0
      }
      if ($T0 == 0 || ($T0 ^ cull_mask) < 0) { continue; }
    }
    if (poly->is_flat_shaded) {
      $T0=info.rgb;
      __asm {
        mtc2    $t0, $20
        mtc2    $t1, $21
        mtc2    $t2, $22
      }
    }
    else {
      for (ii=0;ii<3;ii++) {
        vert=poly_verts[ii];
        $S0=((vert->normal_y*256)<<16)|(vert->normal_x*256);
        $S1=vert->normal_z*256;
        if (ii == 0) {
          __asm {
            mtc2    $t0, $0
            mtc2    $t1, $1
          }
        }
        else if (ii == 1) {
          __asm {
            mtc2    $t0, $2
            mtc2    $t1, $3
          }
        }
        else if (ii == 2) {
          __asm {
            mtc2    $t0, $4
            mtc2    $t1, $5
          }
        }
      }
      __asm {
        cop2    0x118043F
      }
    }
    if (info.type == 1) { /* textured poly? */
      POLY_GT3 *prim;

      prim=(POLY_GT3*)*prims_tail;
      $T0=prim;
      __asm {
        swc2    $12, 8($t0)
        swc2    $13, 0x14($t0)
        swc2    $14, 0x20($t0)
        mfc2    $t1, $20
        swc2    $21, 0x10($t0)
        swc2    $22, 0x1C($t0)
      }
      color=(rgb8)$T1;
      setRGB0(prim,color.r,color.g,color.b);
      setPolyGT3(prim);
      setSemiTrans(prim,info.semi_trans!=3);
      color_mode=info.color_mode;
      offs_x=info.offs_x<<(3-color_mode);
      offs_y=info.offs_y<<2|(pginfo.yh<<7);
      offs_xy=(offs_y<<8)|offs_x;
      prim->clut=(pginfo.yid<<6)|(pginfo.xid<<4)|(info.cluty<<6)|info.clutx;
      prim->tpage=(color_mode<<7)|(info.segment<<5)|(pginfo.yid<<2)|info.semi_trans;
      idx=info.region_idx;
      region=&regions[idx];
      *(uint16_t*)&poly->u0=region->verts[0]+offs_xy;
      *(uint16_t*)&poly->u1=region->verts[1]+offs_xy;
      *(uint16_t*)&poly->u2=region->verts[2]+offs_xy;
      *(uint16_t*)&poly->u3=region->verts[3]+offs_xy;
      __asm {
        mfc2    $t0, $17
        mfc2    $t1, $18
        mfc2    $t2, $19
      }
      if (flag) {
        z_min=min(min($_T0,$_T1),_$T2);
        z_max=max(max($_T0,$_T1),_$T2);
        z_sum=(2*(z_min+z_max))/3;
      }
      else
        z_sum=_$T0+_$T1+_$T2;
      z_idx=far-(z_sum/32);
      z_idx=limit(z_idx, 0, 0x7FF); /* ?????? orig impl uses and, would be more like modulus */
                                    /* also orig impl has no lower limit by 0? */
      prev=ot[z_idx];
      catPrim(prim, prev);
      ot[z_idx]=prim;
      *prims_tail+=sizeof(POLY_GT3);
    }
    else {
      POLY_G3 *prim;

      prim=(POLY_G3*)*prims_tail;
      $T0=prim;
      __asm {
        swc2    $12, 8($t0)
        swc2    $13, 0x10($t0)
        swc2    $14, 0x18($t0)
        mfc2    $t1, $20
        swc2    $21, 0xC($t0)
        swc2    $22, 0x14($t0)
      }
      color=(rgb8)$T1;
      setRGB0(prim,color.r,color.g,color.b);
      setPolyG3(prim);
      setSemiTrans(prim,info.semi_trans!=3);
      __asm {
        mfc2    $t0, $17
        mfc2    $t1, $18
        mfc2    $t2, $19
      }
      if (flag) {
        z_min=min(min($_T0,$_T1),_$T2);
        z_max=max(max($_T0,$_T1),_$T2);
        z_sum=(2*(z_min+z_max))/3;
      }
      else
        z_sum=_$T0+_$T1+_$T2;
      z_idx=far-(z_sum/32);
      z_idx=limit(z_idx,0,0x7FF); /* ?????? orig impl uses and, would be more like modulus */
      prev=ot[z_idx];
      catPrim(prim,prev);
      ot[z_idx]=prim;
      *prims_tail+=sizeof(POLY_G3);
    }
  }
}

void RGteProjectBound(bound *lbound, vec *col, vec *trans, vec *_cam_trans) {
  vec216 verts[3];
  bound2 extents;
  bound bound;

  extents.p1.x = 0x7FFFFFFF; /* INT_MAX */
  extents.p1.y = 0x7FFFFFFF; /* INT_MAX */
  extents.p2.x = 0x80000000; /* INT_MIN */
  extents.p2.y = 0x80000000; /* INT_MIN */
  bound.p1.x=lbound->p1.x+trans->x+(col->x-_cam_trans->x))>>8;
  bound.p1.y=lbound->p1.y+trans->y+(col->y-_cam_trans->y))>>8;
  bound.p1.z=lbound->p1.z+trans->z+(col->z-_cam_trans->z))>>8;
  bound.p2.x=lbound->p2.x+trans->x+(col->x-_cam_trans->x))>>8;
  bound.p2.y=lbound->p2.y+trans->y+(col->y-_cam_trans->y))>>8;
  bound.p2.z=lbound->p2.z+trans->z+(col->z-_cam_trans->z))>>8;
  _$T0=(bound.p1.y<<16)|bound.p1.x;
  _$T1=(bound.p2.y<<16)|bound.p1.x;
  _$T2=bound.p1.z;
  _$T3=bound.p2.z;
  __asm {
    mtc2    $t0, $0
    mtc2    $t0, $2
    mtc2    $t1, $4
    mtc2    $t2, $1
    mtc2    $t3, $3
    mtc2    $t2, $5
    nop
    nop
    cop2    0x280030
    nop
    mfc2    $t0, $12
    mfc2    $t1, $13
    mfc2    $t4, $14
  }
  verts[0]=(vec216)_$T0;
  verts[1]=(vec216)_$T1;
  verts[2]=(vec216)_$T4;
  sub_80035370(verts[0],verts[1],verts[2],&extents);
  _$T0=(bound.p2.y<<16)|bound.p1.x;
  _$T1=(bound.p1.y<<16)|bound.p2.x;
  __asm {
    mtc2    $t0, $0
    mtc2    $t1, $2
    mtc2    $t1, $4
    mtc2    $t3, $1
    mtc2    $t2, $3
    mtc2    $t3, $5
    nop
    nop
    cop2    0x280030
    nop
    mfc2    $t0, $12
    mfc2    $t1, $13
    mfc2    $t4, $14
  }
  verts[0]=(vec216)_$T0;
  verts[1]=(vec216)_$T1;
  verts[2]=(vec216)_$T4;
  sub_80035370(verts[0],verts[1],verts[2],&extents);
  _$T0=(bound.p2.y<<16)|bound.p2.x;
  __asm {
    mtc2    $t0, $2
    mtc2    $t0, $4
    mtc2    $t2, $3
    mtc2    $t3, $5
    nop
    nop
    cop2    0x280030
    nop
    mfc2    $t0, $12
    mfc2    $t1, $13
  }
  verts[0]=(vec216)_$T0;
  verts[1]=(vec216)_$T1;
  sub_800353B0(verts[0],verts[1],&extents);
  scratch.obj_extents = extents;
  return 0;
}

void sub_80035370(vec216 v1, vec216 v2, vec216 v3, bound2 *extents) {
  extents->p1.x = min(v1.x, extents->p1.x);
  extents->p2.x = max(v1.x, extents->p2.x);
  extents->p1.y = min(v1.y, extents->p1.y);
  extents->p2.y = max(v1.y, extents->p2.y);
  sub_800353B0(v2, v3, extents);
}

void sub_800353B0(vec216 v1, vec216 v2, bound2 *extents) {
  extents->p1.x = min(v1.x, extents->p1.x);
  extents->p2.x = max(v1.x, extents->p2.x);
  extents->p1.y = min(v1.y, extents->p1.y);
  extents->p2.y = max(v1.y, extents->p2.y);
  extents->p1.x = min(v2.x, extents->p1.x);
  extents->p2.x = max(v2.x, extents->p2.x);
  extents->p1.y = min(v2.y, extents->p1.y);
  extents->p2.y = min(v2.y, extents->p2.y);
}

void RGpuResetOT(void *ot, int len) {
  int i;
  uint32_t tag;

  for (i=0;i<len;i++) {
    tag = (uint32_t)((uint32_t*)ot + 1) & 0xFFFFFF;
    ot[i] = tag;
  }
}

extern int ZoneQueryOctree(zone_rect*,bound*,zone_query_results*);
extern int PlotQueryWalls(zone_query*,bound*,int,int,int,int,int,int);
extern void PlotWall(int,int);
extern void FindFloorY(gool_object*,zone_query*,bound*,bound*,int32_t,int32_t,
  int(*func)(gool_object*,uint16_t,int,int),zone_query_summary*);
extern void FindCeilY(gool_object*,zone_query*,bound*,bound*,int,int,int32_t,int32_t);

int RZoneQueryOctree(zone_rect *rect, bound *bound, zone_query_results *results) {
  return ZoneQueryOctree(rect, bound, results);
}

int RPlotQueryWalls(
  zone_query *query,
  bound *nodes_bound,
  int flag,
  int test_y1_t1,
  int test_y1,
  int test_y2,
  int trans_x,
  int trans_z) {
  return PlotQueryWalls(query, nodes_bound, flag, test_y1_t1,
    test_y1, test_y2, trans_x, trans_z);
}

void RPlotWall(int x, int z) {
  PlotWall(x,z);
}

void RFindFloorY(
  gool_object *obj,
  zone_query *query,
  bound *nodes_bound,
  bound *collider_bound,
  int16_t max_y,
  int16_t default_y,
  int (*func)(gool_object*, uint16_t, int, int),
  zone_query_summary *summary) {
  FindFloorY(obj, query, nodes_bound, collider_bound, max_y, default_y, func, summary);
}

int RFindCeilY(
  gool_object *obj,
  zone_query *query,
  bound *nodes_bound,
  bound *collider_bound
  int type_a,
  int type_b,
  int16_t default_y) {
  return FindCeilY(obj, query, nodes_bound, collider_bound, type_a, type_b, default_y);
}
