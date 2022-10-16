#include "cdr.h"

/* .sdata */
char cd_name[8]           = "CD001"; /* 800564E0; gp[0x39] */
char iso_dot_rec_name[4]  =     "."; /* 800564E8; gp[0x3B] */
char iso_ddot_rec_name[8] =    ".."; /* 800564EC; gp[0x3C] */
/* .bss */
ns_fileinfo file_map[65];            /* 8005E03C */

//----- (8002F858) --------------------------------------------------------
static inline int CdReadSectors(int count, int sector_num, uint8_t *buf) {
  CdlLOC loc;
  int res;
 
  CdIntToPos(sector_num, &loc);
  CdControl(2, &loc, 0);
  CdRead(count, buf, 128);
  res = CdReadSync(0, 0);
  return res;
}

//----- (8002F8C4) --------------------------------------------------------
int CdReadFileSys() {
  uint8_t *buf, *ibuf;
  uint32_t dir_sec_nums[4];
  char name[32];
  int i, ii, idx, res, len;
  int sector_num, sector_len;

  buf = (uint8_t*)malloc(2048);
  res = CdReadSectors(1, 16, buf);
  /* 1b type 5b CD001 1b ver 2041b data */
  if (!res || strncmp(buf+1, "CD001", 5)) {
    free(buf);
    return 0;
  }
  printf("reading file system\n");
  sector_num = *((uint32_t*)(buf+0x8C)); /* location of the path table */
  res = CdReadSectors(1, sector_num, buf);
  if (res != 0) {
    free(buf);
    return 0;
  }
  for (i=0;i<4;i++)
    dir_sec_nums[i] = -1;
  ibuf = buf;
  for (i=0;i<128;i++) {
    if (ibuf >= buf+0x800) { break; }
    len = ibuf[0]; /* get length of directory name */
    if (len == 0) { break; }
    memcpy(name, buf+8, len); /* get the directory name */
    name[len] = 0;
    sector_num = *((uint32_t*)(ibuf+2));
    /* is it an S0, S1, S2, etc. dir? */
    if (strlen(name) == 2 && (name[0] == 'S' || name[0] == 's')) 
      dir_sec_nums[(name[1]-0xB0)>>2] = sector_num; /* map extended attribute to sector location */
    ibuf += (8+len+(len&1)); /* get next directory identifier */
  }
  for (i=0;i<4;i++) {
    sector_num = dir_sec_nums[i];
    if (sector_num != -1) {
      CdReadSectors(1, sector_num, buf); /* read records for this dir into the buffer */
      if (res != 0) {
        free(buf);
        return 0;
      }
    }
    ibuf = buf;
    for (ii=0;ii<64;ii++) {
      len = ibuf[0]; 
      if (len == 0) { break; }
      sector_num = *((uint32_t*)(ibuf+2));   /* get sector location of directory record */
      sector_len = *((uint32_t*)(ibuf+10));  /* get sector length of directory record */
      if (ii==0) { /* first directory record? */
        name[0] = iso_dot_rec_name[0]; /* name is always empty string, describing the "." entry */
        name[1] = iso_dot_rec_name[1];
      }
      else if (ii==1) { /* second directory record? */
        name[0] = iso_ddot_rec_name[0]; /* name is always "\1", describing the .. entry */
        name[1] = iso_ddot_rec_name[1];
        name[2] = iso_ddot_rec_name[2];
      }
      else { /* normal file or subdirectory record */
        len = ibuf[32]; /* get filename length */
        memcpy(name, ibuf+33, len); /* copy filename into buffer */
        name[len] = 0; /* put null term at end */
      }
      if (name[0] == 'S' || name[0] == 's') { /* filename is S*******.NS* or s*******.ns*? */
        /* convert the last 2 hex chars before period to an index */
        idx = (name[6]-'0')*16+name[7]-(name[7]<'A'?'0':('A'-10));           
        if (name[11] == 'D' || name[11] == 'd') { /* filename is S*******.NSD or s*******.nsd? */
          file_map[idx].nsd_num = (file_map[idx].nsd_num & 0xFF800000) | (sector_num & 0x7FFFFF);
          file_map[idx].nsd_len = (file_map[idx].nsd_len & 0xFFFE0000) | (sector_len & 0x1FFFF); /* put sector num and len in table */
        }
        else /* filename is S*******.NSF or s*******.nsf */
          file_map[idx].nsf_num = (file_map[idx].nsf_num & 0xFF000000) | (sector_num & 0xFFFFFF); /* put sector num in table */
      }
    }
  }
  free(buf);
  return 1;
}