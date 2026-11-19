#include "ext/gfx.h"
#include "ext/math.h"
#include "ext/tex.h"
#include "ext/level.h"

#include "pc/gfx/soft.h"
#include "pc/gfx/tex.h"

const double eps = 1e-9;

extern entry *cur_zone;
extern mat16 ms_cam_rot, mn_cam_rot;

/* fixed-point/float conversion for vectors, angles, and matrices 
   game vectors are Q24.8
   game angles and matrix components are Q4.12 */
void GfxVecToDVec(dvec *out, const vec *in) {
  *out = VecDivS(VecCast(dvec, *in), 256.0);
}

void GfxDVecToVec(vec *out, const dvec *in) {
  out->x = (int32_t)lround(in->x * 256.0);
  out->y = (int32_t)lround(in->y * 256.0);
  out->z = (int32_t)lround(in->z * 256.0);
}

void GfxAngToDAng(dang *out, const ang *in) {
  out->x = (double)(in->x & 0xFFF) * (TWO_PI / 4096.0);
  out->y = (double)(in->y & 0xFFF) * (TWO_PI / 4096.0);
  out->z = (double)(in->z & 0xFFF) * (TWO_PI / 4096.0);
}

void GfxDAngToAng(ang *out, const dang *in) {
  out->x = (int32_t)lround(in->x * (4096.0 / TWO_PI)) & 0xFFF;
  out->y = (int32_t)lround(in->y * (4096.0 / TWO_PI)) & 0xFFF;
  out->z = (int32_t)lround(in->z * (4096.0 / TWO_PI)) & 0xFFF;
}

void GfxMatrix16ToDMatrix(dmat *out, const mat16 *in) {
  int i, j;

  for (i=0;i<3;i++) {
    for (j=0;j<3;j++) {
      out->m[i][j] = (double)in->m[i][j] / 4096.0;
    }
  }
}

void GfxDMatrixToMatrix16(mat16 *out, const dmat *in) {
  int i, j;

  for (i=0;i<3;i++) {
    for (j=0;j<3;j++) {
      out->m[i][j] = (int16_t)(in->m[i][j] * 4096.0);
    }
  }
}

/* floating point variants of vector and matrix operations 
   from soft.c */
int GfxRotD(dvec *in, dvec *out, dmat *m_rot) {
  dvec r_vert;

  r_vert.x = VecDot(m_rot->v[0], *in);
  r_vert.y = VecDot(m_rot->v[1], *in);
  r_vert.z = VecDot(m_rot->v[2], *in);
  *out = r_vert;
  return 1;
}

int GfxRotTransD(dvec *in, dvec *out, dvec *trans, dmat *m_rot) {
  dvec r_vert;

  r_vert.x = trans->x + m_rot->m[0][0]*in->x + m_rot->m[0][1]*in->y + m_rot->m[0][2]*in->z;
  r_vert.y = trans->y + m_rot->m[1][0]*in->x + m_rot->m[1][1]*in->y + m_rot->m[1][2]*in->z;
  r_vert.z = trans->z + m_rot->m[2][0]*in->x + m_rot->m[2][1]*in->y + m_rot->m[2][2]*in->z;
  *out = r_vert;
  return 1;
}

void GfxTransposeDMatrix(dmat *m) {
  double tmp;

  SWAP(m->m[0][1], m->m[1][0]);
  SWAP(m->m[0][2], m->m[2][0]);
  SWAP(m->m[1][2], m->m[2][1]);
}

void GfxMulDMatrix(dmat *m_l, dmat *m_r) {
  dmat_t a,b,t;

  memcpy(a, m_l->m, sizeof(a));
  memcpy(b, m_r->m, sizeof(b));
  t[0][0]=(a[0][0]*b[0][0]+a[0][1]*b[1][0]+a[0][2]*b[2][0]);
  t[0][1]=(a[0][0]*b[0][1]+a[0][1]*b[1][1]+a[0][2]*b[2][1]);
  t[0][2]=(a[0][0]*b[0][2]+a[0][1]*b[1][2]+a[0][2]*b[2][2]);
  t[1][0]=(a[1][0]*b[0][0]+a[1][1]*b[1][0]+a[1][2]*b[2][0]);
  t[1][1]=(a[1][0]*b[0][1]+a[1][1]*b[1][1]+a[1][2]*b[2][1]);
  t[1][2]=(a[1][0]*b[0][2]+a[1][1]*b[1][2]+a[1][2]*b[2][2]);
  t[2][0]=(a[2][0]*b[0][0]+a[2][1]*b[1][0]+a[2][2]*b[2][0]);
  t[2][1]=(a[2][0]*b[0][1]+a[2][1]*b[1][1]+a[2][2]*b[2][1]);
  t[2][2]=(a[2][0]*b[0][2]+a[2][1]*b[1][2]+a[2][2]*b[2][2]);
  memcpy(m_l->m, t, sizeof(t));
}

void GfxRotDMatrixYXY(dmat *m_rot, dang *rot) {
  double sx, sy, sz, cx, cy, cz;
  double sxsy, sxsz, sysz, cxsy, cxsz, sxcy, sxcz, sycz, cxcz;
  double sxcysz, cxcysz, sxcycz, cxcycz;

  sx=sin(rot->x);sy=sin(rot->y);sz=sin(rot->z);
  cx=cos(rot->x);cy=cos(rot->y);cz=cos(rot->z);
  sxsy=(sx*sy);
  sxsz=(sx*sz);
  sysz=(sy*sz);
  cxsy=(cx*sy);
  cxsz=(cx*sz);
  sxcy=(sx*cy);
  sxcz=(sx*cz);
  sycz=(sy*cz);
  cxcz=(cx*cz);
  sxcysz=(sxcy*sz);
  sxcycz=(sxcy*cz);
  cxcysz=(cxsz*cy);
  cxcycz=(cxcz*cy);
  m_rot->m[0][0]=cxcz-sxcysz;
  m_rot->m[0][1]=sysz;
  m_rot->m[0][2]=sxcz+cxcysz;
  m_rot->m[1][0]=sxsy;
  m_rot->m[1][1]=cy;
  m_rot->m[1][2]=-cxsy;
  m_rot->m[2][0]=-cxsz-sxcycz;
  m_rot->m[2][1]=sycz;
  m_rot->m[2][2]=-sxsz+cxcycz;
}

void GfxRotDMatrixZXY(dmat *m_rot, dang *rot) {
  double sx, sy, sz, cx, cy, cz;
  double sxsy, sxsz, cxsz, cysz, sxcy, sxcz, cxcy, cxcz, cycz;
  double sxsysz, cxsysz, sxsycz, cxsycz;

  sx=sin(rot->x);sy=sin(rot->y);sz=sin(rot->z);
  cx=cos(rot->x);cy=cos(rot->y);cz=cos(rot->z);
  sxsy=(sx*sy);
  sxsz=(sx*sz);
  cxsz=(cx*sz);
  cysz=(cy*sz);
  sxcy=(sx*cy);
  sxcz=(sx*cz);
  cxcy=(cx*cy);
  cxcz=(cx*cz);
  cycz=(cy*cz);
  sxsysz=(sxsy*sz);
  cxsysz=(cxsz*sy);
  sxsycz=(sxsy*cz);
  cxsycz=(cxcz*sy);
  m_rot->m[0][0]=cxcz-sxsysz;
  m_rot->m[0][1]=-cysz;
  m_rot->m[0][2]=sxcz+cxsysz;
  m_rot->m[1][0]=cxsz+sxsycz;
  m_rot->m[1][1]=cycz;
  m_rot->m[1][2]=sxsz-cxsycz;
  m_rot->m[2][0]=-sxcy;
  m_rot->m[2][1]=sy;
  m_rot->m[2][2]=cxcy;
}

/* rodrigues rotation matrix */
void GfxRotDMatrixAxisAngle(dmat *m_rot, dvec *axis, double angle) {
  double s, c, t, x, y, z;

  s = sin(angle);
  c = cos(angle);
  t = 1.0 - c;
  x = axis->x;
  y = axis->y;
  z = axis->z;
  m_rot->m[0][0] = t*x*x + c;
  m_rot->m[0][1] = t*x*y - s*z;
  m_rot->m[0][2] = t*x*z + s*y;
  m_rot->m[1][0] = t*x*y + s*z;
  m_rot->m[1][1] = t*y*y + c;
  m_rot->m[1][2] = t*y*z - s*x;
  m_rot->m[2][0] = t*x*z - s*y;
  m_rot->m[2][1] = t*y*z + s*x;
  m_rot->m[2][2] = t*z*z + c;
}

/* composite conversion between ang and dmat */
void GfxDMatrixFromAng(dmat *m_rot, const ang *rot) {
  dang d;

  GfxAngToDAng(&d, rot);
  d = VecMulS(d, -1.0);
  GfxRotDMatrixZXY(m_rot, &d);
}

void GfxAngFromDMatrix(ang *rot, dmat *m_rot) {
  dang d;

  d.y = asin(m_rot->m[2][1] > 1.0 ? 1.0 : (m_rot->m[2][1] < -1.0 ? -1.0 : m_rot->m[2][1]));
  d.x = atan2(-m_rot->m[2][0], m_rot->m[2][2]);
  d.z = atan2(-m_rot->m[0][1], m_rot->m[1][1]);
  d = VecMulS(d, -1.0);
  GfxDAngToAng(rot, &d);
}


vec GfxRoundVec(const dvec *pos) {
  vec r;

  r.x = (int32_t)lround(pos->x);
  r.y = (int32_t)lround(pos->y);
  r.z = (int32_t)lround(pos->z);
  return r;
}

/**
 * Transform a local point into view space.
 */
int GfxTransformLocalToView(const vec *local, const vec *trans, vec *view_out) {
  int res;

  res = SwRotTrans((vec *)local, view_out, (vec *)trans, &ms_cam_rot);
  view_out->y = -view_out->y;
  if (!res && view_out->z <= MIN_Z) 
    return 0;
  return 1;
}

/**
 * Transform a world point into view space.
 * 
 * Returns 0 if the transformed point is behind the near plane.
 */
int GfxTransformWorldToView(const vec *world, vec *view_out) {
  vec rel;
  int res;

  rel = VecSub(*world, mn_cam_rot.t);
  res = SwRot(&rel, view_out, &ms_cam_rot);
  view_out->y = -view_out->y;
  return res && view_out->z > MIN_Z;
}

/**
 * Transform a world point into view space using double precision.
 */
void GfxTransformWorldToViewD(const dvec *world, dvec *view_out) {
  dmat rot;
  dvec rel;

  rel = VecSub(*world, mn_cam_rot.t);
  GfxMatrix16ToDMatrix(&rot, &ms_cam_rot);
  GfxRotD(&rel, view_out, &rot);
  view_out->y = -view_out->y;
}

/**
 * Transform a local point into view space 
 * using the specified camera matrix.
 */
void GfxTransformLocalToCam(const vec *local, const mat16 *cam, const vec *trans, vec *view_out) {
  vec rel;

  rel.x = (trans->x - cam->t.x) + local->x;
  rel.y = (trans->y - cam->t.y) + local->y;
  rel.z = (trans->z - cam->t.z) + local->z;
  SwRot(&rel, view_out, (mat16 *)&cam->m);
  view_out->y = -view_out->y;
}

static int GfxTransformViewDirToWorldD(const dvec *view, dvec *world_out) {
  dmat rot;
  dvec rel;
  
  rel = *view;
  rel.y = -rel.y;
  GfxMatrix16ToDMatrix(&rot, &ms_cam_rot);
  return GfxMatSolve(rot.m, &rel, world_out);
}

int GfxTransformViewToWorldD(const dvec *view, dvec *world_out) {
  if (!GfxTransformViewDirToWorldD(view, world_out)) 
    return 0;
  *world_out = VecAdd(*world_out, mn_cam_rot.t);
  return 1;
}

/**
 * Transform a view space point into world space.
 */
int GfxTransformViewToWorld(const vec *view, vec *world_out) {
  dvec view_d, world_d;
  
  view_d = VecCast(dvec, *view);
  if (!GfxTransformViewToWorldD(&view_d, &world_d)) 
    return 0;
  *world_out = GfxRoundVec(&world_d);
  return 1;
}

/**
 * Project a 3D point in view space to 2D normalized device coordinates (NDC).
 */
int GfxProjectViewToNDC(const vec *view, const ddim2 *hdim, dvec2 *ndc_out) {
  dvec view_d;
  
  view_d = VecCast(dvec, *view);
  return GfxProjectViewToNDCD(&view_d, hdim, ndc_out);
}

/**
 * Project a 3D point in view space to 2D normalized device coordinates (NDC).
 * Input point specified in double precision.
 */
int GfxProjectViewToNDCD(const dvec *view, const ddim2 *hdim, dvec2 *ndc_out) {
  if (view->z <= MIN_Z) 
    return 0;
  ndc_out->x = (view->x / view->z) / hdim->w;
  ndc_out->y = (-view->y / view->z) / hdim->h;
  return (!isnan(ndc_out->x) && !isnan(ndc_out->y) && !isinf(ndc_out->x) && !isinf(ndc_out->y));
}

/**
 * Transform a point from screen space to normalized device coordinates (NDC). 
 */
int GfxTransformPxToNDC(const vec2 *px, const ext_viewparams_t *params, dvec2 *ndc_out) {
  if (params->window_dim.w <= 0 || params->window_dim.h <= 0) 
    return 0;
  ndc_out->x = (((double)px->x / (double)params->window_dim.w) * 2.0 - 1.0) * params->zoom;
  ndc_out->y = (((double)px->y / (double)params->window_dim.h) * 2.0 - 1.0) * params->zoom; /* projection NDC is y-down */
  return 1;
}

/**
 * Transform a point from normalized device coordinates (NDC) to screen space.
 * Output screen coordinates are double precision.
 */
void GfxTransformNDCToDPx(const dvec2 *ndc, const ext_viewparams_t *params, dvec2 *px_out) {
  px_out->x = (ndc->x / params->zoom * 0.5 + 0.5) * (double)params->window_dim.w;
  px_out->y = (ndc->y / params->zoom * 0.5 + 0.5) * (double)params->window_dim.h;
}

/**
 * Deproject a point from normalized device coordinates (NDC) to view space.
 * The z-coordinate specifies the depth in view space.
 */
int GfxDeprojectNDCToView(const dvec2 *ndc, const ext_viewparams_t *params, double z,
                          dvec *view_out) {
  view_out->x = ndc->x * params->hdim.w * z;
  view_out->y = -ndc->y * params->hdim.h * z;
  view_out->z = z;
  return 1;
} 

/**
 * Deproject a point from normalized device coordinates (NDC) to local space.
 * The base and trans vectors are in local space
 * and define the local-to-view transformation.
 */
int GfxDeprojectNDCToLocal(const dvec2 *ndc, const ext_viewparams_t *params,
                           const vec *trans, const vec *base,
                           vec *local_out) {
  dmat_t mat;
  vec local, view, base_view;
  dvec d, dl;
  int i;

  if (!GfxTransformLocalToView(base, trans, &base_view)) 
    return 0;
  /* deproject to the position 
     that would preserve the z of base_view */
  GfxDeprojectNDCToView(ndc, params, base_view.z, &d);
  /* get the delta */
  d = VecSub(d, base_view);
  /* get the effective local to view transformation matrix
     for the current state, such that positions the origin 
     at "base";
     i.e. transform bases originating from "base" */
  for (i=0;i<3;i++) {
      local = *base;
      local.c[i] += 256;
      if (!GfxTransformLocalToView(&local, trans, &view)) 
        return 0;
      mat[0][i] = (double)(view.x - base_view.x) / 256.0;
      mat[1][i] = (double)(view.y - base_view.y) / 256.0;
      mat[2][i] = (double)(view.z - base_view.z) / 256.0;
  }
  /* use that matrix to inverse transform the delta
     from view space to local space */
  if (!GfxMatSolve(mat, &d, &dl))
    return 0;
  /* apply transformed delta to get new local position */
  dl = VecAdd(VecCast(dvec, *base), dl);
  *local_out = GfxRoundVec(&dl);
  return 1;
}

/**
 * Clip an edge against the near plane in view space.
 * Returns the number of points in the clipped edge (0, 1, or 2).
 * The points are stored in the array verts_out.
 */
int GfxEdgeNearClip(const vec *a, const vec *b, dvec *verts_out) {
  double t, near_z;
  int ain, bin, n;

  near_z = (double)MIN_Z + 1.0;
  ain = (double)a->z > near_z;
  bin = (double)b->z > near_z;
  n = 0;
  if (ain) /* a is past the near plane */
    verts_out[n++] = VecCast(dvec, *a); /* add it to vert set */
  if (ain != bin) { /* edge intersects the near plane */
    /* solve for the point of intersection
       and add it to the vert set */
    t = (near_z - (double)a->z) / ((double)b->z - (double)a->z);
    verts_out[n++] = VecInterp(VecCast(dvec, *a), VecCast(dvec, *b), t);
  }
  return n;
}

#define GfxPoints2dCW(a,b,c) \
  (Vec2Cross(Vec2Sub(b,a),Vec2Sub(c,a))<=0)

double GfxTriArea(tri_t tri) {
  dvec cross, u, v;

  u = VecSub(VecCast(dvec, tri[1]), VecCast(dvec, tri[0]));
  v = VecSub(VecCast(dvec, tri[2]), VecCast(dvec, tri[0]));
  cross = VecCross(u, v);
  return 0.5 * VecMag(cross);
}

int GfxTriNearClip(const tri_t tri, dvec *verts_out) {
  const vec *a, *b;
  int i, n;

  n = 0;
  for (i=0;i<3;i++) {
    a = &tri[i]; 
    b = &tri[(i + 1) % 3];
    n += GfxEdgeNearClip(a, b, &verts_out[n]);
  }
  if (n < 3) return 0; /* 0 point result if degenerate */
  return n;
}

/**
 * Clip a triangle in NDC space against the (-1,-1)-(1,1) box
 * and fan the result into triangles.
 * The output triangles are stored in the array tris_out.
 * Returns the triangle count (0 if fully outside).
 */
int GfxTri2dClip(const dtri2_t tri, dtri2_t tris_out[5]) {
  dvec2 in[8], res[16], a, b, v; /* a clipped triangle has at most 7 vertices */
  double t, da, db;
  int i, j, n, m;

  in[0] = tri[0];
  in[1] = tri[1];
  in[2] = tri[2];
  n = 3;

  for (i=0;i<4;i++) {
    m = 0;
    for (j=0;j<n;j++) {
      a = in[j];
      b = in[(j + 1) % n];
      da = (i < 2) ? a.x : a.y;
      db = (i < 2) ? b.x : b.y;
      da = (i & 1) ? 1.0 - da : da + 1.0;
      db = (i & 1) ? 1.0 - db : db + 1.0;
      if ((da >= 0.0) != (db >= 0.0) && da != 0.0 && db != 0.0) {
        t = da / (da - db);
        v.x = a.x + (b.x - a.x) * t;
        v.y = a.y + (b.y - a.y) * t;
        if (i < 2) v.x = (i & 1) ? 1.0 : -1.0;
        else       v.y = (i & 1) ? 1.0 : -1.0;
        res[m++] = v;
      }
      if (db >= 0.0) res[m++] = b;
    }
    if (m < 3) return 0;
    if (m > 7) m = 7; 
    memcpy(in, res, m * sizeof(dvec2));
    n = m;
  }

  for (i=1;i<n-1;i++) {
    tris_out[i-1][0] = in[0];
    tris_out[i-1][1] = in[i];
    tris_out[i-1][2] = in[i+1];
  }
  return n - 2;
}

/**
 * Test if a point is inside a 2D triangle in NDC space.
 * Returns 1 if the point is inside or on the edge, 0 otherwise.
 */
int GfxPointInTri2d(const dtri2_t tri, const dvec2 *ndc) {
  double c0, c1, c2;

  c0 = Vec2Cross(Vec2Sub(tri[1], tri[0]), Vec2Sub(*ndc, tri[0]));
  c1 = Vec2Cross(Vec2Sub(tri[2], tri[1]), Vec2Sub(*ndc, tri[1]));
  c2 = Vec2Cross(Vec2Sub(tri[0], tri[2]), Vec2Sub(*ndc, tri[2]));
  return (c0 >= -eps && c1 >= -eps && c2 >= -eps)
      || (c0 <= eps && c1 <= eps && c2 <= eps);
}

/**
 * Test if a point is inside a 2D polygon in NDC space.
 * Returns 1 if the point is inside or on the edge, 0 otherwise.
 */
int GfxPointInPoly2d(const dvec2 *verts, const dvec2 *ndc, int count) {
  double cross, x, y, xi, yi, xj, yj;
  int i, j, is_in;

  if (!verts || count < 3) return 0;
  is_in = 0;
  x = ndc->x; y = ndc->y;
  for (i=0,j=count-1;i<count;i++) {
    xi = verts[i].x; yi = verts[i].y;
    xj = verts[j].x; yj = verts[j].y;
    cross = Vec2Cross(Vec2Sub(verts[i], verts[j]), Vec2Sub(*ndc, verts[j]));
    if (fabs(cross)<eps
     && x>=min(xi,xj)-eps && x<=max(xi,xj)+eps
     && y>=min(yi,yj)-eps && y<=max(yi,yj)+eps) 
       return 1;
    if (((yi<y && yj>=y) || (yj<y && yi>=y))
     && (xi<=x || xj<=x))
      is_in ^= (xi+(y-yi)/(yj-yi)*(xj-xi)<x);
    j = i;
  }
  return is_in;
}

/**
 * Deproject a point from NDC space to the face of a triangle 
 * in view space.
 * The deprojected point is returned in `view_out`
 * and the ray parameter in `depth_out`.
 * Returns 1 if the point can be deprojected onto the triangle, 
 *         0 otherwise.
 */
int GfxDeprojectNDCToTri(const tri_t tri, const dvec2 *ndc, const ddim2 *hdim,
                         dvec *view_out, double *depth_out) {
  ext_viewparams_t params;
  dmat mat;
  dvec o, e1, e2, ray, rhs, sol;
  
  if (!view_out || !depth_out || hdim->w <= 0 || hdim->h <= 0) 
    return 0;
  params = (ext_viewparams_t){ .hdim = *hdim };
  /* a ray cast from the origin 
     through a point R = (view_x, view_y, 1.0) on the viewport 
     intersects the plane determined by a triangle
     wherever the values of barycentric coordinates u, v
     and value of ray parameter t yield the same point:
     0 + t*R = o + u*e1 + v*e2
     =>
     -t*R + u*e1 + v*e2 = -o
     =>
     [ -R, e1, e2 ] * [ t, u, v ]^T = -o
     =>
     solve for [ t, u, v ]^T 
     =>
     t*R gives the intersection point on the ray */
  o = VecCast(dvec, tri[0]);
  e1 = VecSub(VecCast(dvec, tri[1]), o);
  e2 = VecSub(VecCast(dvec, tri[2]), o);
  GfxDeprojectNDCToView(ndc, &params, 1.0, &ray);
  mat.v[0] = VecMulS(ray, -1.0);
  mat.v[1] = e1;
  mat.v[2] = e2;
  rhs = VecMulS(o, -1.0);
  /* change v from rows to columns */
  GfxTransposeDMatrix(&mat);
  /* solve */
  if (!GfxMatSolve(mat.m, &rhs, &sol)) return 0;
  /* if the point on the ray is outside of the triangle
     [or behind the near plane] 
     then it does not intersect the triangle */
  if (sol.y < -eps || sol.z < -eps 
   || sol.y + sol.z > 1.0 + eps 
   || sol.x <= MIN_Z + 1.0)
    return 0;
  /* compute t*R */
  *view_out = VecMulS(ray, sol.x);
  *depth_out = sol.x;
  return 1;
}

/**
 * Deproject a point from NDC space to the nearest face of a 
 * bounding box in view space.
 * The deprojected point is returned in `view_out`
 * and the ray parameter in `depth_out`.
 * Returns 1 if the point can be deprojected onto the bounding box, 
 *         0 otherwise.
 */
int GfxDeprojectNDCToBound(const bound *bound, const dvec2 *ndc, const ddim2 *hdim,
                           vec *view_out, double *depth_out) {
  ext_viewparams_t params = { .hdim = *hdim };
  dvec ray, dir, view;
  double lo, hi, t1, t2, enter, exit, tmp;
  int i;

  if (!bound || !view_out 
   || hdim->w <= 0 || hdim->h <= 0) 
    return 0;
  GfxDeprojectNDCToView(ndc, &params, 1.0, &ray);
  if (!GfxTransformViewDirToWorldD(&ray, &dir)) 
    return 0;
  enter = MIN_Z + 1.0;
  exit = 1e300;
  for (i=0;i<3;i++) {
    lo = min(bound->p1.c[i], bound->p2.c[i]) / 256.0;
    hi = max(bound->p1.c[i], bound->p2.c[i]) / 256.0;
    if (fabs(dir.c[i]) < 1e-12) {
      if (mn_cam_rot.t.c[i] < lo || mn_cam_rot.t.c[i] > hi) return 0;
      continue;
    }
    t1 = (lo - mn_cam_rot.t.c[i]) / dir.c[i];
    t2 = (hi - mn_cam_rot.t.c[i]) / dir.c[i];
    if (t1 > t2) { SWAP(t1, t2); }
    if (t1 > enter) enter = t1;
    if (t2 < exit) exit = t2;
    if (enter > exit) return 0;
  }
  if (depth_out) *depth_out = enter;
  GfxDeprojectNDCToView(ndc, &params, enter, &view);
  *view_out = GfxRoundVec(&view);
  return 1;
}

/**
 * Test if a point in NDC is inside a triangle specified in view space
 * (after clipping against the near plane and projecting).
 * 
 * @param tri_out The 'adjusted triangle' projected to NDC
 *                such that any vertices behind the near plane are 
 *                adjusted forward to the near plane.
 *                (the near-clipped triangle on the other-hand is not necessarily a triangle)
 * @param depth_out The average z of the vertices
 *                  of the triangular section of the near-clipped polygon 
 *                  containing the point.
*/
int GfxPointInNearClippedTri(
  const tri_t tri, const dvec2 *ndc, const ddim2 *hdim, double *depth_out, dtri2_t tri_out) {
  dtri2_t dtri;
  dvec p[4];
  dvec2 ps[4];
  double z, near_z;
  int i, j, n;

  near_z = (double)MIN_Z + 1.0;
  n = GfxTriNearClip(tri, p);
  if (n < 3) return 0; /* return if degenerate */
  /* project near-clipped polygon to NDC */
  for (i=0;i<n;i++) {
    ps[i].x = (p[i].x / p[i].z) / hdim->w;
    ps[i].y = -(p[i].y / p[i].z) / hdim->h;
  }
  /* test disjoint triangular sections 
      of the near-clipped polygon */
  for (i=1;i+1<n;i++) {
    dtri[0] = ps[0];
    dtri[1] = ps[i];
    dtri[2] = ps[i+1];
    if (!GfxPointInTri2d(dtri, ndc)) continue;
    /* get average depth of vertices in this section */
    *depth_out = (p[0].z + p[i].z + p[i+1].z) / 3.0;
    /* project adjusted triangle to NDC */
    for (j=0;j<3;j++) {
      z = (double)tri[j].z > near_z ? (double)tri[j].z : near_z;
      tri_out[j].x = ((double)tri[j].x / z) / hdim->w;
      tri_out[j].y = -((double)tri[j].y / z) / hdim->h;
    }
    return 1;
  }
  return 0;
}

/**
 * Solve m * x = b for x by Cramer's rule; returns 0 if m is singular.
 */
int GfxMatSolve(double m[3][3], const dvec *b, dvec *result_out) {
  double t[3][3], det;
  dvec res;
  int i, j, k;

  det = GfxMatDet(m);
  if (fabs(det) < eps) return 0;
  for (j=0;j<3;j++) {
    for (i=0;i<3;i++)
      for (k=0;k<3;k++) t[i][k] = m[i][k];
    for (i=0;i<3;i++) t[i][j] = b->c[i];
    res.c[j] = GfxMatDet(t) / det;
  }
  *result_out = res;
  return 1;
}

/*
 * Get the outward extrusion vector for a triangle edge.
 * 
 * Specifically, it is the component of the adjacent edge that is
 * perpendicular to the edge itself, negated. 
 * This vector points away from the source triangle and its length
 * is the source's altitude over the edge, so that each extruded
 * triangle has the same area as the source.
 */
int GfxTriEdgeExtrusionVector(tri_t tri, int idx, dvec *offset_out) {
  dvec edge, w, perp;
  vec verts[3];
  double len, k, alt;
  int i;

  for (i=0;i<3;i++) 
    verts[i]=tri[(idx+i)%3];
  edge = VecSub(VecCast(dvec, verts[1]), VecCast(dvec, verts[0]));
  w = VecSub(VecCast(dvec, verts[2]), VecCast(dvec, verts[0]));
  len = VecDot(edge, edge);
  if (len < 1.0) return 0;
  k = VecDot(w, edge) / len;
  perp = VecSub(w, VecMulS(edge, k));
  alt = VecMag(perp);
  if (alt < 1.0) return 0; /* sliver; no usable plane direction */
  *offset_out = VecMulS(perp, -1.0);
  return 1;
}

/**
 * Get the index of the vertex of a 2D triangle in NDC space 
 * that is nearest to a given point.
 * Returns the index of the nearest vertex (0, 1, or 2).
 */
int GfxTri2dNearestVertex(const dtri2_t tri, const dvec2 *ndc, const ext_viewparams_t *params) {
  dvec2 mpx, px;
  double best, dist;
  int i, bi;

  best = 12.0 * 12.0;
  GfxTransformNDCToDPx(ndc, params, &mpx);
  bi = -1;
  for (i=0;i<3;i++) {
    GfxTransformNDCToDPx(&tri[i], params, &px);
    dist = Vec2SqrDist(px, mpx);
    if (dist <= best) { 
      best = dist; 
      bi = i; 
    }
  }
  return bi;
}

/**
 * Test if a 2D triangle in NDC space is completely offscreen.
 * Returns 1 if the triangle is offscreen, 0 otherwise.
 */
int GfxTri2dOffscreen(const dtri2_t ndc) {
  return (ndc[0].x < -1.0 && ndc[1].x < -1.0 && ndc[2].x < -1.0)
      || (ndc[0].x >  1.0 && ndc[1].x >  1.0 && ndc[2].x >  1.0)
      || (ndc[0].y < -1.0 && ndc[1].y < -1.0 && ndc[2].y < -1.0)
      || (ndc[0].y >  1.0 && ndc[1].y >  1.0 && ndc[2].y >  1.0);
}

/**
 * Test if an edge of a 2D triangle in NDC space is completely offscreen.
 * Returns 1 if the edge is offscreen, 0 otherwise.
 */
int GfxTri2dEdgeOffscreen(const dtri2_t ndc, int idx) {
  dvec2 a = ndc[idx];
  dvec2 b = ndc[(idx+1)%3];
  return (a.x < -1.0 && b.x < -1.0)
      || (a.x >  1.0 && b.x >  1.0)
      || (a.y < -1.0 && b.y < -1.0)
      || (a.y >  1.0 && b.y >  1.0);
}

/** 
 * Get the triangles and edges that make up a bounding box 
 * in view coordinates. 
 */
void GfxBoundGeom(const bound *box, tri_t tris_out[GFX_BOUND_FACE_COUNT],
                  line_t edges_out[GFX_BOUND_EDGE_COUNT]) {
  static const int face_idxes[12][3] = {
    {0,1,3},{0,3,2},{4,6,7},{4,7,5},
    {0,4,5},{0,5,1},{2,3,7},{2,7,6},
    {0,2,6},{0,6,4},{1,5,7},{1,7,3}
  };       
  static const int edge_idxes[12][2] =  {
    {0,1},{2,3},{4,5},{6,7}, 
    {0,2},{1,3},{4,6},{5,7},
    {0,4},{1,5},{2,6},{3,7}
  };
  vec world, verts[8];
  int i, j;

  for (i=0;i<8;i++) {
    world.x = ((i & 1) ? box->p1.x : box->p2.x) >> 8;
    world.y = ((i & 2) ? box->p1.y : box->p2.y) >> 8;
    world.z = ((i & 4) ? box->p1.z : box->p2.z) >> 8;
    GfxTransformWorldToView(&world, &verts[i]);
  }
  for (i=0;i<12;i++) {
    for (j=0;j<3;j++) {
      tris_out[i][j] = verts[face_idxes[i][j]];
    }
  }
  for (i=0;i<12;i++) {
    for (j=0;j<2;j++) {
      edges_out[i][j] = verts[edge_idxes[i][j]];
    }
  }
}

/**
 * Get the [packed array] set of polygons and edges that make up
 * a near plane clipped bounding box in view coordinates.
 */
int GfxBoundGeomClipped(const bound *box, dvec verts_out[GFX_BOUND_POLY_VERT_MAX],
                        int *poly_count_out, int vert_counts_out[GFX_BOUND_FACE_COUNT],
                        dline_t edges_out[GFX_BOUND_EDGE_COUNT], int *edge_count_out) {
  tri_t tris[GFX_BOUND_FACE_COUNT];
  line_t lines[GFX_BOUND_EDGE_COUNT];
  dvec clipped[4], reverse[2];
  int pc, vc, ec;
  int i, j, n;

  pc = vc = ec = 0;
  *poly_count_out = 0;
  *edge_count_out = 0;
  GfxBoundGeom(box, tris, lines);
  for (i=0;i<GFX_BOUND_FACE_COUNT;i++) {
    n = GfxTriNearClip(tris[i], clipped);
    if (n < 3) continue;
    for (j=0;j<n;j++) 
      verts_out[vc++] = clipped[j];
    vert_counts_out[pc++] = n;
  }
  for (i=0;i<GFX_BOUND_EDGE_COUNT;i++) {
    n = GfxEdgeNearClip(&lines[i][0], &lines[i][1], clipped);
    if (n == 1 && GfxEdgeNearClip(&lines[i][1], &lines[i][0], reverse))
      clipped[n++] = reverse[0];
    if (n < 2) continue;
    for (j=0;j<n;j++)
      edges_out[ec][j] = clipped[j];
    ec++;
  }
  *poly_count_out = pc;
  *edge_count_out = ec;
  return pc > 0 || ec > 0;
}

/**
 * Get the polygons and edges that make up a near plane 
 * clipped bounding box projected to NDC coordinates.
 */
int GfxBoundGeomClippedProject(const bound *box, const ext_viewparams_t *params,
                               dvec2 verts_out[GFX_BOUND_POLY_VERT_MAX], int *poly_count_out,
                               int vert_counts_out[GFX_BOUND_FACE_COUNT],
                               dline2_t edges_out[GFX_BOUND_EDGE_COUNT], int *edge_count_out) {
  dvec pv[GFX_BOUND_POLY_VERT_MAX];
  dline_t ev[GFX_BOUND_EDGE_COUNT];
  int pcv, vcv[GFX_BOUND_FACE_COUNT], tcv, ecv;
  int pc, tc, ec, cvc, i, j;

  pc = 0; tc = 0; ec = 0; tcv = 0;
  *poly_count_out = 0;
  *edge_count_out = 0;
  if (!GfxBoundGeomClipped(box, pv, &pcv, vcv, ev, &ecv))
    return 0;
  for (i=0;i<pcv;i++) {
    cvc = vcv[i];
    for (j=0;j<cvc;j++) {
      if (!GfxProjectViewToNDCD(&pv[tcv+j], &params->hdim, &verts_out[tc+j]))
        break;
    }
    tcv += cvc;
    if (j != cvc) continue;
    tc += cvc;
    vert_counts_out[pc] = cvc;
    ++pc;
  }
  for (i=0;i<ecv;i++) {
    for (j=0;j<2;j++) {
      if (!GfxProjectViewToNDCD(&ev[i][j], &params->hdim, &edges_out[ec][j])) 
        break;
    }
    if (j != 2) continue;
    ++ec;
  }
  *poly_count_out = pc;
  *edge_count_out = ec;
  return pc > 0 || ec > 0;
}

/** 
 * Unpack a world vertex into local world coordinates
 * (matches logic in SwTransformAndShadeWorlds).
 */
void GfxWorldVertUnpack(const wgeo_vertex *vert, vec *local_out, rgba *col_out) {
  local_out->x = (int16_t)(vert->x * 8);
  local_out->y = (int16_t)(vert->y * 8);
  local_out->z = (int16_t)((vert->z8 + (vert->z2 << 8) + (vert->z3 << 10)) * 8);
  *col_out = GfxRgb8ToA32(vert->r, vert->g, vert->b);
}

rgba GfxRgb8ToA32(uint8_t r, uint8_t g, uint8_t b) {
  rgba col;

  col.r = (uint32_t)r * 16843009U;
  col.g = (uint32_t)g * 16843009U;
  col.b = (uint32_t)b * 16843009U;
  col.a = ~0U;
  return col;
}

/* WGEO tpag refs are rewritten to entry pointers at load time; persist/compare the stable eid instead. */
static uint32_t GfxTpageEid(uint32_t raw) {
  entry_ref r;
  r.value = raw;
  return r.is_eid ? (uint32_t)r.eid : (uint32_t)r.en->eid;
}

/**
 * Transform a world vertex into view space.
 * 
 * Returns 1 if the transformation is successful, 0 otherwise.
 */
int GfxWorldVertTransform(const zone_world *world, const wgeo_vertex *vert, const mat16 *cam,
                          const vec *trans, vec *view_out) {
  vec local;
  rgba col;

  GfxWorldVertUnpack(vert, &local, &col);
  if (cam)
    GfxTransformLocalToCam(&local, cam, trans, view_out);
  else if (!GfxTransformLocalToView(&local, &world->trans, view_out)) 
    return 0;
  return 1; 
}

/**
 * Project a world vertex into normalized device coordinates (NDC) space.
 * 
 * Returns 1 if the projection is successful, 0 otherwise.
 */
int GfxWorldVertProject(const zone_world *world, const wgeo_vertex *vert, const ddim2 *hdim,
                        const mat16 *cam, const vec *trans, dvec2 *ndc_out) {
  vec view;

  if (!GfxWorldVertTransform(world, vert, cam, trans, &view))
    return 0;
  if (!GfxProjectViewToNDC(&view, hdim, ndc_out))
    return 0;
  return 1;
}

/**
 * Get the base texinfo for a World polygon.
 * 
 * World polygons with animated textures change texinfo dynamically,
 * so this ensures retrieval of the fixed base texinfo.
 * 
 * Returns 1 if the polygon is textured, 0 otherwise.
*/
int GfxWorldPolyTexinfo(const zone_world *world, const wgeo_polygon *poly, texinfo *tinfo_out) {
  wgeo_texinfo *wtinfo;

  memset(tinfo_out, 0, sizeof(*tinfo_out));
  if (!world->texinfos) return 0;
  wtinfo = (wgeo_texinfo *)(&((uint32_t *)world->texinfos)[poly->tinf_idx]);
  tinfo_out->colinfo = wtinfo->colinfo;
  if (tinfo_out->type == 1) {
    tinfo_out->rgninfo = poly->anim_mask ? wtinfo->rgninfos[0] : wtinfo->rgninfo;
    tinfo_out->tpage = GfxTpageEid(world->header->tpags[poly->tpag_idx]);
    return 1;
  }
  return 0;
}

/**
 * Load a world polygon's vertex positions, UVs, colors, and texture info.
 * 
 * Equivalent to the logic in the polygon loop in SwTransformAndShadeWorlds
 * excluding pre and post transforms (shaders) and the projection to screen space
 * [and insertion into the OT since it does not apply here].
 */
void GfxWorldPolyLoad(const zone_world *world, const wgeo_polygon *poly,
                      vec verts_out[3], fvec uvs_out[3], rgba cols_out[3],
                      texinfo *tinfo_out, int *is_textured_out, int *texid_out, int *flags_out) {
  wgeo_vertex *world_verts;
  texinfo tinfo;
  fvec uvs[4];
  int i, texid;
  
  world_verts = world->vertices;
  /* unpack vert x,y,zs and convert vert colors to 32-bit rgba */
  GfxWorldVertUnpack(&world_verts[poly->a_idx], &verts_out[0], &cols_out[0]);
  GfxWorldVertUnpack(&world_verts[poly->b_idx], &verts_out[1], &cols_out[1]);
  GfxWorldVertUnpack(&world_verts[poly->c_idx], &verts_out[2], &cols_out[2]);
  /* get texinfo and load texture if textured */
  *is_textured_out = GfxWorldPolyTexinfo(world, poly, tinfo_out);
  memset(uvs, 0, sizeof(uvs));
  texid = -1;
  if (*is_textured_out) {
    tinfo = *tinfo_out;
    texid = TextureLoad(&tinfo, &uvs);
  }
  for (i=0;i<3;i++) uvs_out[i] = uvs[i];
  *texid_out = texid;
  *flags_out = tinfo_out->semi_trans;
}

/**
 * Transform the vertices of a world polygon to view space.
 *
 * With a cam matrix the polygon is transformed by trans/cam,
 * and otherwise by game's current transform. 
 * 
 * Returns 1 if the transformation was successful, 0 otherwise.
 */
int GfxWorldPolyTransform(const zone_world *world, const wgeo_polygon *poly, const mat16 *cam,
                          const vec *trans, int permute, vec verts_out[3]) {
  wgeo_vertex *verts, *pv[3];
  vec local;
  rgba col;
  int i, ok;
  
  ok = 1;
  verts = world->vertices;
  pv[0] = &verts[poly->a_idx];
  pv[1] = &verts[poly->b_idx];
  pv[2] = &verts[poly->c_idx];
  for (i=0;i<3;i++) {
    GfxWorldVertUnpack(pv[(i + permute) % 3], &local, &col);
    if (cam)
      GfxTransformLocalToCam(&local, cam, trans, &verts_out[i]);
    else if (!GfxTransformLocalToView(&local, &world->trans, &verts_out[i])) 
      ok = 0; 
  }
  return ok;
}

/*
 * Project the vertices of a world polygon to NDC. 
 *
 * With a cam matrix the polygon is transformed by trans/cam,
 * and otherwise by game's current transform. 
 * 
 * 'permute' rotates the vertex order:
 * 0 = 012, 1 = 120, 2 = 201 
 * (so that vertices 0 and 1 constitute the edge with that index).
 */
int GfxWorldPolyProject(const zone_world *world, const wgeo_polygon *poly, const ddim2 *hdim,
                        const mat16 *cam, const vec *trans, int permute, dtri2_t tri_out) {
  vec verts[3];
  int i;
 
  if (!GfxWorldPolyTransform(world, poly, cam, trans, permute, verts))
    return 0;
  for (i=0;i<3;i++) {
    if (!GfxProjectViewToNDC(&verts[i], hdim, &tri_out[i]))
      return 0;
  }
  return 1;
}

/**
 * Project a world-space point to NDC (and optionally to view space).
 */
int GfxWorldPointProject(const zone_world *world, const vec *local, const ddim2 *hdim,
                         vec *view_out, dvec2 *ndc_out) {
  vec view;
  if (!SwRotTrans((vec*)local, &view, (vec*)&world->trans, &ms_cam_rot)) {
    if (view.z <= MIN_Z) return 0;
  }
  view.y = -view.y;
  if (!GfxProjectViewToNDC(&view, hdim, ndc_out)) return 0;
  if (view_out) *view_out = view;
  return 1;
}

void GfxEdgeRefVerts(const zone_world *world, const ext_edgeref_t *e, const ext_mpoly_t *mpolys, vec verts_out[3]) {
  const wgeo_polygon *poly;
  vec verts[3];
  rgba col;
  int i;

  if (e->manual) {
    for (i=0;i<3;i++) verts_out[i] = mpolys[e->poly_idx].verts[(e->edge_idx + i) % 3];
    return;
  }
  poly = &world->polygons[e->poly_idx];
  GfxWorldVertUnpack(&world->vertices[poly->a_idx], &verts[0], &col);
  GfxWorldVertUnpack(&world->vertices[poly->b_idx], &verts[1], &col);
  GfxWorldVertUnpack(&world->vertices[poly->c_idx], &verts[2], &col);
  for (i=0;i<3;i++) verts_out[i] = verts[(e->edge_idx + i) % 3];
}

int GfxEdgeRefProject(const zone_world *world, const ext_edgeref_t *e, const ext_viewparams_t *params, dtri2_t tri_out) {
  vec verts[3];
  int i;

  if (!e->manual)
    return GfxWorldPolyProject(world, &world->polygons[e->poly_idx], &params->hdim, 0, 0, e->edge_idx, tri_out);
  GfxEdgeRefVerts(world, e, params->mpolys, verts);
  for (i=0;i<3;i++)
    if (!GfxWorldPointProject(world, &verts[i], &params->hdim, 0, &tri_out[i])) return 0;
  return 1;
}

/**
 * Reorder the vertices of an unpacked polygon 
 * (positions, uvs, colors, locked status)
 * according to the specified winding.
 *
 * The output polygon is written to `poly_out`.
 */
void GfxPolyPermute(const ext_poly_t *poly, int v0, int v1, int v2, ext_poly_t *poly_out) {
  poly_out->world_eid = poly->world_eid;
  poly_out->trans_x = poly->trans_x;
  poly_out->trans_y = poly->trans_y;
  poly_out->trans_z = poly->trans_z;
  poly_out->verts[0] = poly->verts[v0];
  poly_out->verts[1] = poly->verts[v1];
  poly_out->verts[2] = poly->verts[v2];
  poly_out->uvs[0] = poly->uvs[v0];
  poly_out->uvs[1] = poly->uvs[v1];
  poly_out->uvs[2] = poly->uvs[v2];
  poly_out->colors[0] = poly->colors[v0];
  poly_out->colors[1] = poly->colors[v1];
  poly_out->colors[2] = poly->colors[v2];
  poly_out->vertex_locked[0] = poly->vertex_locked[v0];
  poly_out->vertex_locked[1] = poly->vertex_locked[v1];
  poly_out->vertex_locked[2] = poly->vertex_locked[v2];
  poly_out->tinfo = poly->tinfo;
  poly_out->has_tinfo = poly->has_tinfo;
  poly_out->texid = poly->texid;
  poly_out->flags = poly->flags;
  if (poly->has_tinfo) {
    poly_out->tinfo.uv_idx = UvPermuteWinding(poly->tinfo.uv_idx, v0, v1, v2);
  }
}

/**
 * Get the native unpacked polygon for the given polygon reference.
 * 
 * (If only positions and colors are needed, use GfxPolyRefVerts instead).
 */
int GfxPolyFromRef(const ext_polyref_t *ref, const ext_viewparams_t *params, int rotate, ext_poly_t *poly_out) {
  const zone_world *world;
  ext_poly_t src;
  int i, e;

  memset(&src, 0, sizeof(src));
  if (ref->manual) {
    if (ref->poly_idx < 0 || ref->poly_idx >= params->mpoly_count
     || !params->mpolys[ref->poly_idx].active) return 0;
    src = params->mpolys[ref->poly_idx].poly;
  } 
  else {
    world = ZoneGetWorld(cur_zone, ref->world_eid);
    if (!world || !world->header || !world->polygons || !world->vertices 
     || ref->poly_idx < 0 || ref->poly_idx >= (int)world->header->poly_count) 
      return 0;
    src.world_eid = ref->world_eid;
    src.trans_x = world->header->trans_x; 
    src.trans_y = world->header->trans_y; 
    src.trans_z = world->header->trans_z;
    GfxWorldPolyLoad(world, &world->polygons[ref->poly_idx], 
      src.verts, src.uvs, src.colors, &src.tinfo, &src.has_tinfo, &src.texid, &src.flags);
    for (i=0;i<3;i++) 
      src.vertex_locked[i] = 1;
  }
  e = ((ref->edge_idx % 3) + 3) % 3;
  *poly_out = src;
  if (rotate) {
    if (e == 1) GfxPolyPermute(&src, 1, 2, 0, poly_out);
    if (e == 2) GfxPolyPermute(&src, 2, 0, 1, poly_out);
  }
  return 1;
}

/**
 * Gets the vertex position and color for the given vertex reference.
 * 
 * Note that for world vertex references, the vertex index is relative
 * to the world vertex array instead of the polygon's local vertex array.
 */
int GfxVertFromRef(const ext_vertref_t *ref, const ext_viewparams_t *params, vec *pos_out, rgba *col_out) {
  const ext_mpoly_t *mp;
  zone_header *zh;
  zone_world *world;
  
  if (ref->manual) {
    if (ref->poly_idx < 0 || ref->poly_idx >= params->mpoly_count)
      return 0;
    mp = &params->mpolys[ref->poly_idx];
    *pos_out = mp->verts[ref->vert_idx];
    *col_out = mp->colors[ref->vert_idx];
  }
  else {
    if (ref->world_idx < 0)
      return 0;
    zh = (zone_header *)cur_zone->items[0];
    world = &zh->worlds[ref->world_idx];
    GfxWorldVertUnpack(&world->vertices[ref->vert_idx], pos_out, col_out);
  }
  return 1;
}

/**
 * Convert a native unpacked polygon to a primitive triangle
 * for the rendering pipeline.
 * 
 * Returns 1 if the conversion is successful, 0 otherwise.
 */
int GfxPolyToPrim(ext_poly_t *poly, poly3i *tri_out) {
  zone_world *world;
  int i;

  memset(tri_out, 0, sizeof(poly3i));
  world = ZoneGetWorld(cur_zone, poly->world_eid);
  if (!world) return 0;
  poly->world_eid = WorldEID(world);
  
  tri_out->type = 1;
  tri_out->texid = poly->texid;
  tri_out->flags = poly->flags;
  for (i=0;i<3;i++) {
    if (!GfxTransformLocalToView(&poly->verts[i], &world->trans, &tri_out->verts[i])) 
      return 0;
    tri_out->colors[i] = poly->colors[i];
    tri_out->uvs[i] = poly->uvs[i];
  }
  return 1;
}

/**
 * Extrude the first edge of a polygon outward by its own altitude, 
 * resulting in two new polygons that reuse its texture, colors and world.
 * 
 * Writes the new polygons to out and returns how many there are 
 * (0 if src is degenerate).
 */
int GfxPolyExtrude(const ext_poly_t *src, ext_poly_t polys_out[2]) {
  tri_t tmp;
  ext_poly_t *a, *b;
  vec cur_l, cur_r;
  dvec t, extruded;

  memset(&tmp, 0, sizeof(tmp));
  tmp[0] = src->verts[0];
  tmp[1] = src->verts[1];
  tmp[2] = src->verts[2];
  /* get the extrusion vector for edge 0 */
  if (!GfxTriEdgeExtrusionVector(tmp, 0, &t)) 
    return 0;
  /* v0 and v1 together with v0 + t form the first extruded triangle
     v0 + t and v1 together with v1 + t form a complementary second extruded triangle
     such that together the 2 form a rectangle */
  /* so get v0+t and v1+t */
  extruded = VecAdd(VecCast(dvec, tmp[0]), t);
  cur_l = GfxRoundVec(&extruded);
  extruded = VecAdd(VecCast(dvec, tmp[1]), t);
  cur_r = GfxRoundVec(&extruded);
  /* the first new poly is such that uvs/colors
     are inherited from the source poly in *reverse* winding order
     i.e. v0 => v0, v1 => v2, v2 => v1
     and likewise for vertex positions, except that v1 is replaced with v0+t */
  a = &polys_out[0];
  GfxPolyPermute(src, 0, 2, 1, a);
  a->verts[1] = cur_l;
  a->vertex_locked[1] = 0; /* v1 is unshared so it begins unlocked */
  /* v2 of this poly is also equal to v1 of the source poly 
     but the winding direction must be the same as the source poly
     hence the order of the inherited vertices must be some rotation of 012
     the rotation that moves v1 to v2 is 201
     positions of v0 and v1 are replaced with v0+t and v1+t (from the source poly) 
     respectively */
  b = &polys_out[1];
  GfxPolyPermute(src, 2, 0, 1, b);
  b->verts[0] = cur_l; 
  b->verts[1] = cur_r; 
  b->vertex_locked[0] = 0; 
  b->vertex_locked[1] = 0; 
  return 2;
}

/**
 * Get the vertex positions and colors of the polygon referenced by ref.
 * 
 * (For usage when only the vertex positions and colors are needed. Otherwise,
 * use GfxPolyFromRef instead).
 */
int GfxPolyRefVerts(const ext_polyref_t *ref, const ext_viewparams_t *params, vec verts_out[3], rgba cols_out[3]) {
  const ext_mpoly_t *mp;
  zone_world *world;
  wgeo_polygon *wpoly;
  wgeo_vertex *wverts[3];
  int i;

  if (ref->manual) {
    if (ref->poly_idx < 0 || ref->poly_idx >= params->mpoly_count)
      return 0;
    world = ZoneGetWorld(cur_zone, params->mpolys[ref->poly_idx].world_eid);
  }
  else
    world = ZoneGetWorld(cur_zone, ref->world_eid);
  if (!world) 
    return 0;
  if (!ref->manual) {
    if (!world->header || !world->polygons || !world->vertices ||
        ref->poly_idx < 0 || ref->poly_idx >= (int)world->header->poly_count)
      return 0;
    wpoly = &world->polygons[ref->poly_idx];
    wverts[0] = &world->vertices[wpoly->a_idx];
    wverts[1] = &world->vertices[wpoly->b_idx];
    wverts[2] = &world->vertices[wpoly->c_idx];
    for (i=0;i<3;i++) {
      GfxWorldVertUnpack(wverts[i], &verts_out[i], &cols_out[i]);
    }
  }
  else {
    mp = &params->mpolys[ref->poly_idx];
    for (i=0;i<3;i++) {
      verts_out[i] = mp->verts[i];
      cols_out[i] = mp->colors[i];
    }
  }
  return 1;
}

/**
 * Get the texinfo of the polygon referenced by ref.
 * 
 * (For usage when only the texinfo is needed. Otherwise, use GfxPolyFromRef).
 * Returns 1 if the texinfo was successfully retrieved, 0 otherwise.
 */
int GfxPolyRefTexinfo(const ext_polyref_t *ref, const ext_viewparams_t *params, texinfo *tinfo_out) {
  const ext_mpoly_t *mp;
  zone_world *world;
  wgeo_polygon *wpoly;

  if (ref->manual) {
    if (ref->poly_idx < 0 || ref->poly_idx >= params->mpoly_count)
      return 0;
    mp = &params->mpolys[ref->poly_idx];
    if (!mp->has_tinfo)
      return 0;
    *tinfo_out = mp->tinfo;
    return 1;
  }
  world = ZoneGetWorld(cur_zone, ref->world_eid);
  if (!world || !world->header || !world->polygons || !world->vertices)
    return 0;
  if (ref->poly_idx < 0 || ref->poly_idx >= (int)world->header->poly_count)
    return 0;
  wpoly = &world->polygons[ref->poly_idx];
  return GfxWorldPolyTexinfo(world, wpoly, tinfo_out);
}

/**
 * Transform the vertices of the polygon referenced by ref into view space.
 * 
 * Works for references to both original world polygons and manual polygons.
 */
int GfxPolyRefTransform(const ext_polyref_t *ref, const ext_viewparams_t *params, const mat16 *cam,
                        const vec *trans, vec verts_out[3]) {
  zone_world *world;                              
  vec verts[3];
  rgba cols[3];
  eid_t eid;
  int i, ok;

  if (!cam) {
    eid = ref->world_eid;
    if (ref->manual) {
      if (ref->poly_idx < 0 || ref->poly_idx >= params->mpoly_count)
        return 0;
      eid = params->mpolys[ref->poly_idx].world_eid;
    }
    world = ZoneGetWorld(cur_zone, eid);
    if (!world)
      return 0;
  }
  if (!GfxPolyRefVerts(ref, params, verts, cols))
    return 0;
  ok = 1;
  for (i=0;i<3;i++) {
    if (cam)
      GfxTransformLocalToCam(&verts[i], cam, trans, &verts_out[i]);
    else if (!GfxTransformLocalToView(&verts[i], &world->trans, &verts_out[i]))
      ok = 0;
  }
  return ok;
}

/**
 * Project the vertices of the polygon referenced by ref into 
 * normalized device coordinates (NDC).
 * 
 * Works for references to both original world polygons and manual polygons.
 */
int GfxPolyRefProject(const ext_polyref_t *ref, const ext_viewparams_t *params, const mat16 *cam,
                      const vec *trans, dtri2_t tri_out) {
  vec verts[3];
  int i;

  if (!GfxPolyRefTransform(ref, params, cam, trans, verts))
    return 0;
  for (i=0;i<3;i++) {
    if (!GfxProjectViewToNDC(&verts[i], &params->hdim, &tri_out[i]))
      return 0;
  }
  return 1;
}

/**
 * Get the index of the vertex of the polygon referenced by ref 
 * that is nearest to the given mouse position in screen space.
 */
int GfxPolyNearestVert(const ext_polyref_t *ref, const vec2 *px, const ext_viewparams_t *params) {
  dtri2_t tri;
  dvec2 mouse_px, vert_px, ndc;
  int i;

  if (!GfxPolyRefProject(ref, params, 0, 0, tri))
    return -1;
  if (!GfxTransformPxToNDC(px, params, &ndc))
    return -1;
  GfxTransformNDCToDPx(&ndc, params, &mouse_px);
  for (i=0;i<3;i++) {
    GfxTransformNDCToDPx(&tri[i], params, &vert_px);
    if (Vec2SqrDist(vert_px, mouse_px) <= 12.0 * 12.0)
      return i;
  }
  return -1;
}

/* coverage rasterizer */

static inline int CvCell(double v) {
  int c = (int)floor((v + CV_RANGE) / (2.0 * CV_RANGE) * CV_N);
  return c < 0 ? 0 : (c >= CV_N ? CV_N - 1 : c);
}

static inline void GfxCvFillRow(cv_grid_t *grid, int r, int c0, int c1) {
  int len;

  len = c1 - c0 + 1;
  memset(&grid->map[r * CV_N + c0], 1, len);
}

static inline void GfxCvFillTriRows(cv_grid_t *grid, int r0, int r1, double x0, double x1, double d0, double d1) {
  int r, ri, c0, c1, len;
  double tmp;

  if (x0 > x1 || (x0 == x1 && d0 > d1)) {
    SWAP(x0, x1);
    SWAP(d0, d1);
  }

  ri = r0 * CV_N;
  for (r=r0;r<=r1;r++) {
    c0 = (int)x0;
    c1 = (int)x1;
    len = c1 - c0 + 1;
    memset(&grid->map[ri + c0], 1, len);
    x0 += d0;
    x1 += d1;
    ri += CV_N;
  }
}

static inline void GfxCvFillTriClipped(cv_grid_t *grid, const dtri2_t tri) {
  dvec2 q0, q1, q2, tmp;
  double x0, x1, xa, xb, xc, ya, yb, lo, hi;
  double scale, dx02, dx01, dx12;
  int r0, r1, rc0, rc1, rm;

  /* transform vertices from NDC to grid coordinates */
  scale = CV_N / (2.0 * CV_RANGE);
  q0 = Vec2MulS(Vec2AddS(tri[0], CV_RANGE), scale);
  q1 = Vec2MulS(Vec2AddS(tri[1], CV_RANGE), scale);
  q2 = Vec2MulS(Vec2AddS(tri[2], CV_RANGE), scale);
  if (q0.y > q1.y) SWAP(q0, q1);
  if (q0.y > q2.y) SWAP(q0, q2);
  if (q1.y > q2.y) SWAP(q1, q2);

  /* sample at row centers */
  rc0 = (int)ceil(q0.y - 0.5);
  rc1 = (int)floor(q2.y - 0.5);
  rm = (int)floor(q1.y - 0.5);

  /* get edge slopes */
  dx02 = (q2.y > q0.y) ? (q2.x - q0.x) / (q2.y - q0.y) : 0.0;
  dx01 = (q1.y > q0.y) ? (q1.x - q0.x) / (q1.y - q0.y) : 0.0;
  dx12 = (q2.y > q1.y) ? (q2.x - q1.x) / (q2.y - q1.y) : 0.0;

  if (rc0 <= rc1) {
    if (rm >= rc0) { /* top part */
      x0 = q0.x + dx02 * ((double)rc0 + 0.5 - q0.y);
      x1 = q0.x + dx01 * ((double)rc0 + 0.5 - q0.y);
      GfxCvFillTriRows(grid, rc0, rm < rc1 ? rm : rc1,
        x0, x1, dx02, dx01);
    }
    if (rm < rc1) { /* bottom part */
      if (rm < rc0 - 1) rm = rc0 - 1;
      x0 = q0.x + dx02 * ((double)rm + 1.5 - q0.y);
      x1 = q1.x + dx12 * ((double)rm + 1.5 - q1.y);
      GfxCvFillTriRows(grid, rm + 1, rc1, 
        x0, x1, dx02, dx12);
    }
  }

  /* the top and bottom rows may be missed by center sampling;
     fill them from the exact x-extent of the triangle within the row */
  r0 = (int)q0.y;
  r1 = (int)q2.y;
  if (r0 < rc0 || r0 > rc1) { 
    yb = r0 + 1 < q2.y ? (double)(r0 + 1) : q2.y;
    xc = yb >= q2.y ? q2.x : q0.x + dx02 * (yb - q0.y);
    if (yb < q1.y)
      xa = xb = q0.x + dx01 * (yb - q0.y); 
    else { 
      xa = q1.x; 
      xb = yb >= q2.y ? q2.x : q1.x + dx12 * (yb - q1.y); 
    }
    lo = min(min(q0.x, xc), min(xa, xb));
    hi = max(max(q0.x, xc), max(xa, xb));
    GfxCvFillRow(grid, r0, (int)lo, (int)hi);
  }
  if (r1 != r0 && (r1 < rc0 || r1 > rc1)) { 
    ya = r1 > q0.y ? (double)r1 : q0.y;
    xc = ya <= q0.y ? q0.x : q0.x + dx02 * (ya - q0.y);
    if (ya > q1.y)
        xa = xb = q1.x + dx12 * (ya - q1.y);
    else { 
        xa = q1.x; 
        xb = ya <= q0.y ? q0.x : q0.x + dx01 * (ya - q0.y); 
    }
    lo = min(min(q2.x, xc), min(xa, xb));
    hi = max(max(q2.x, xc), max(xa, xb));
    GfxCvFillRow(grid, r1, (int)lo, (int)hi);
  }
}

/**
 * Fill a triangle in NDC space into the specified coverage grid.
 */
void GfxCvFillTri(cv_grid_t *grid, const dtri2_t tri) {
  dtri2_t clip[5];
  int i, n;

  n = GfxTri2dClip(tri, clip);
  for (i=0;i<n;i++) 
    GfxCvFillTriClipped(grid, clip[i]);
}

/**
 * Check if the given point in NDC space is covered by the 
 * specified coverage grid.
 */
int GfxCvPointCovered(const cv_grid_t *grid, const dvec2 *ndc) {
  return grid->map[CvCell(ndc->y) * CV_N + CvCell(ndc->x)];
}

/**
 * Determine the amount of empty space outside of a triangle edge 
 * in the coverage grid.
 * 
 * @param grid The coverage grid to check against.
 * @param tri The triangle in NDC space.
 * @param idx The index of the edge to check (0, 1, or 2).
 * @param flags Flags controlling the behavior 
 *              (1 for considering points outside the NDC range as empty).
 * @param total_out Optional pointer to an integer to store
 *              the total number of sample points checked.
 * @return The amount of empty space outside of the specified triangle edge.
 */
int GfxTriEdgeVoidAmt(const cv_grid_t *grid, const dtri2_t tri, int idx, int flags, int *total_out) {
  dvec2 ea, eb, dir, s;
  double side, len, t;
  int i, j, k, b, o, empty;

  if (total_out) (*total_out) = 0;
  i = idx;
  b = (i + 1) % 3;
  o = (i + 2) % 3;
  /* get the normalized perpendicular to edge A 
     [2 possibilities: left or right oriented perpendicular: 
     choose the one that is oriented furthest from a triangle edge;
     that is-the one that points "away" from the triangle] */
  ea = Vec2Sub(tri[b], tri[i]); /* edge A is from vi to v_((i+1)%3) */
  eb = Vec2Sub(tri[o], tri[i]); /* edge B is from vi to v_((i+2)%3) */
  side = Vec2Cross(ea, eb);
  dir = (side >= 0.0) ? Vec2(dvec2, ea.y, -ea.x) 
                      : Vec2(dvec2, -ea.y, ea.x);
  len = Vec2Mag(dir);
  if (len < eps) return 0;
  dir = Vec2DivS(dir, len);
  empty = 0;
  /* sample a 3x3 grid of points oriented along edge A
     and positioned "outside" of the triangle [in the perp direction]; 
     space points width-wise so they evenly distribute over edge A */
  for (j=0;j<3;j++) {
    t = 0.2 + 0.3 * j; /* 0.2, 0.5, 0.8 */
    for (k=1;k<=3;k++) {
      /* TODO: should we use better spacing for the heights, i.e. 0.01, 0.03, 0.08? */
      s = Vec2Add(tri[i], Vec2Add(Vec2MulS(ea, t), Vec2MulS(dir, 0.03 * k)));
      /* points outside of the screen/NDC range are considered empty 
         when flag 1 is set */
      if (s.x < -1.0 || s.x > 1.0 
       || s.y < -1.0 || s.y > 1.0) {
        if (flags & 1) empty++; 
        continue; 
      }
      if (total_out) (*total_out)++;
      /* points uncovered by any other polygon are also considered empty */
      if (!GfxCvPointCovered(grid, &s)) empty++;
    }
  }
  /* return amount of empty space outside of the triangle 
     at this edge */
  return empty;
}

/**
 * Determine the edge of the triangle with the most empty space 
 * outside of it.
 * 
 * @param grid The coverage grid to check against.
 * @param tri The triangle in NDC space.
 * @return The index of the edge with the most empty space outside of it.
 */
int GfxTriBestVoidEdge(const cv_grid_t *grid, const dtri2_t tri) {
  int i, best, best_empty, empty;

  best = 0;
  best_empty = -1;
  for (i=0;i<3;i++) {
    empty = GfxTriEdgeVoidAmt(grid, tri, i, 1, 0);
    /* update the best void edge */
    if (empty > best_empty) { 
      best_empty = empty; 
      best = i; 
    }
  }
  /* the best void edge is the one with the most empty space
     near it and outside of the triangle */
  return best;
}

/**
 * Fill a world polygon into the coverage grid.
 *
 * @param grid The coverage grid to fill.
 * @param world The world containing the polygon.
 * @param poly The polygon to fill.
 * @param cam The camera matrix.
 * @param trans The camera translation vector.
 * @param hdim The viewport dimensions.
 */
void GfxCvFillWorldPoly(cv_grid_t *grid, const zone_world *world,
                        const wgeo_polygon *poly, const ddim2 *hdim, const mat16 *cam, const vec *trans) {
  dtri2_t tri;
  vec vv[3];
  dvec c[4];
  dvec2 pn[4];
  int i, n;

  GfxWorldPolyTransform(world, poly, cam, trans, 0, vv);
  n = GfxTriNearClip(vv, c);
  if (n < 3) return;
  /* project to NDC */
  for (i=0;i<n;i++) {
    pn[i].x = c[i].x / c[i].z / hdim->w;
    pn[i].y = -c[i].y / c[i].z / hdim->h;
  }
  for (i=1;i+1<n;i++) {
    /* TODO: tesselation should be auto in GfxTriNearClip */
    tri[0] = pn[0]; 
    tri[1] = pn[i]; 
    tri[2] = pn[i + 1];
    if (GfxTri2dOffscreen(tri)) continue;
    GfxCvFillTri(grid, tri);
  }
}

/**
 * Fill a manual polygon into the coverage grid.
 *
 * @param grid The coverage grid to fill.
 * @param world The world associated with the polygon.
 * @param mp The manual polygon to fill.
 * @param cam The camera matrix (optional).
 * @param hdim The viewport dimensions.
 */
void GfxCvFillManualPoly(cv_grid_t *grid, const zone_world *world,
                         const ext_mpoly_t *mp, const ddim2 *hdim, const mat16 *cam) {
  wgeo_header *wh;
  vec view, trans;
  dvec2 ndc[3];
  int i, valid;
  
  valid = 1;
  for (i=0;i<3;i++) {
    if (cam) { /* cam matrix/pose provided? */
      wh = world->header;
      trans = (vec){ wh->trans_x, wh->trans_y, wh->trans_z };
      GfxTransformLocalToCam(&mp->verts[i], cam, &trans, &view);
      if (!GfxProjectViewToNDC(&view, hdim, &ndc[i])) {
        valid = 0; 
        break; 
      }
    } 
    else { /* no cam matrix/pose provided */
      /* project using world trans and current cam rot */
      if (!GfxWorldPointProject(world, &mp->verts[i], hdim, &view, &ndc[i])) {
        valid = 0; 
        break; 
      }
    }
  }
  /* skip invalid triangles and triangles behind the frustum */
  if (!valid) return;
  /* trivially reject triangles 
     completely to the left, right, top, or bottom of the frustum */
  if (GfxTri2dOffscreen(ndc)) return;
  GfxCvFillTri(grid, ndc);
}

/**
 * Get the coverage ratio of the coverage grid.
 *
 * @param grid The coverage grid to check.
 * @return The fraction of the grid that is covered.
 */
double GfxCvGetCoverage(const cv_grid_t *grid) {
  int covered, total;
  int i;

  total = CV_N*CV_N;
  covered = 0;
  for (i=0;i<total;i++) {
    if (grid->map[i]) covered++;
  }
  return (double)covered / (double)total;
}