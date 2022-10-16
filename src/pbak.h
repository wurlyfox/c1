#ifndef _PBAK_H_
#define _PBAK_H_

#include "common.h"
#include "ns.h"
#include "formats/pbak.h"

extern int PbakInit();
extern int PbakKill();
extern void PbakPlay(eid_t*);
extern void PbakChoose(eid_t*);

#endif /* _PBAK_H_ */
