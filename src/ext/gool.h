#ifndef _EXT_GOOL_H_
#define _EXT_GOOL_H_

#include "common.h"
#include "ext/ext.h"

extern gool_handle handles_root;

int GoolObjectName(const gool_object *obj, char *name_out);
int GoolObjectBound2(gool_object *obj, gool_bound *out);
gool_object *GoolObjectByPoint2d(dvec2 point, ddim2 hdim);

#endif /* _EXT_GOOL_H_ */
