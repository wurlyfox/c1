#ifndef _EXT_NS_H_
#define _EXT_NS_H_

#include "common.h"
#include "../ns.h"
#include "formats/gool.h"

int NSReadFontAssets(lid_t lid, eid_t gool_eid, eid_t font_eid,
                     gool_font *font, tpage *font_page);

#endif /* _EXT_NS_H_ */
