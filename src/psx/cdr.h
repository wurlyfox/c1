#ifndef _CDR_H_
#define _CDR_H_

#include "common.h"
#include <LIBCD.H>

typedef struct {
  uint32_t nsd_num; /* sector location of nsd file */
  uint32_t nsd_len; /* length of nsd file in sectors */
  uint32_t nsf_num; /* sector location of nsf file */
} ns_fileinfo;

extern int CdReadFileSys();

#endif /* _CDR_H_ */