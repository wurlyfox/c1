#ifndef _NS_H_
#define _NS_H_

#include "common.h"

/* eid */
#define EID_NONE 0x6396347F
#define EID_NULL EID_NONE

typedef uint32_t eid_t;

/* entry */
#define MAGIC_ENTRY     0x100FFFF
#define ENTRY_ITEMS_MAX 128

typedef struct _entry {
  uint32_t magic;
  eid_t eid;
  int type;
  int item_count;
  union {
    uint32_t item_offsets[0];
    uint8_t *items[0];
  };
} entry;

/* pgid */
typedef uint32_t pgid_t;

/* page */
#define MAGIC_PAGE        0x1234
#define MAGIC_COMPRESSED  0x1235
#define MAGIC_DECOMPESSED 0x8765

#define PAGE_SIZE         0x10000
#define PAGE_ENTRIES_MAX  256

#define SECTOR_SIZE       0x800

#define align_to_sectors(offset) \
  (((((uint32_t)(offset)-1)>>11)+1)<<11)
#define sector_offset(offset) \
  ((uint32_t)(offset)%(1<<11))

typedef union {
  struct {
    uint16_t magic;
    uint16_t type;
    pgid_t pgid;
    uint32_t entry_count;
    uint32_t checksum;
    union {
      uint32_t entry_offsets[0];
      entry *entries[0];
    };
  };
  uint8_t data[PAGE_SIZE];
} page;

typedef struct {
  uint16_t magic;
  uint16_t pad;
  uint32_t length;
  uint32_t skip;
  uint8_t data[];
} page_compressed;

typedef struct _tpage {
  union {
    struct {
      uint16_t magic;
      uint16_t type;
      eid_t eid;
      uint32_t entry_type;
      uint32_t checksum;
    };
    uint8_t data[PAGE_SIZE];
  };
} tpage;

/* lid */
typedef uint32_t lid_t;

/* ldat */
#define MAGIC_LDAT     1
#define NSD_EXEC_COUNT 64

typedef struct {
  uint32_t magic;
  lid_t lid;
  eid_t zone_spawn;
  int path_idx_spawn;
  uint32_t unk_10;
  eid_t exec_map[NSD_EXEC_COUNT];
  uint32_t fov;
  uint8_t image_data[0xF5F8];
} nsd_ldat;

/* nsd */
#define NSD_COMPRESSED_PAGE_COUNT 64
#define NSD_PTB_COUNT             256

typedef struct {
  union {
    pgid_t pgid;
    entry *entry;
    uint32_t value;
  };
  union {
    eid_t eid;
    uint32_t key;
  };
} nsd_pte;

typedef struct _nsd {
  union {
    uint32_t ptb_offsets[NSD_PTB_COUNT];
    nsd_pte* page_table_buckets[NSD_PTB_COUNT];
  };
  int page_count;
  size_t page_table_size;
  eid_t ldat_eid; /* does not actually exist as an entry, just a name */
  int has_loading_image;
  uint32_t loading_image_width;
  uint32_t loading_image_height;
  union {
    uint32_t pages_offset;
    page *pages;
  };
  int32_t compressed_page_count;
  union {
    uint32_t compressed_page_offsets[NSD_COMPRESSED_PAGE_COUNT];
    uint8_t *compressed_pages[NSD_COMPRESSED_PAGE_COUNT];
  };
  nsd_pte page_table[];
} nsd;

#define nsd_ldat(_nsd) \
  (nsd_ldat*)((uint8_t*)_nsd \
    + sizeof(nsd) \
    + sizeof(nsd_pte)*_nsd->page_table_size)

/* page struct */
typedef struct _page_struct {
  page *page;
  short state;
  int16_t type;
  short idx;
  short ref_count;
  uint16_t savings;
  short compressed;
  uint32_t info;
  pgid_t pgid;
  eid_t eid;
  union {
    int page_count;
    uint32_t addr;
  };
  nsd_pte *pte;
  struct _page_struct *tail;
  uint32_t timestamp;
} page_struct;

/* ns struct */
#ifdef PSX
#define NS_PHYSICAL_PAGE_COUNT 22
#define NS_VIRTUAL_PAGE_COUNT  38
#else
#define NS_PHYSICAL_PAGE_COUNT 128
#define NS_VIRTUAL_PAGE_COUNT  0
#endif

typedef struct {
  int inited;               /* 8005C528 */
  lid_t lid;                /* 8005C52C */
  nsd_pte **pte_buckets;    /* 8005C530 */
  nsd_pte *page_table;      /* 8005C534 */
  eid_t *ldat_eid;          /* 8005C538 */
  nsd_ldat *ldat;           /* 8005C53C */
  nsd *nsd;                 /* 8005C540 */
  uint32_t unk_8005C544;    /* 8005C544 */
  uint32_t nsf_loc;         /* 8005C548 */
  int draw_skip_counter;    /* 8005C54C */
  int physical_page_count;
  union {
    page_struct physical_pages[NS_PHYSICAL_PAGE_COUNT];
    page *pagemem;
  };
  int virtual_page_count;
  page_struct virtual_pages[NS_VIRTUAL_PAGE_COUNT];
  int page_count;           /* 8005CFA8 */
  page_struct *cur_ps;      /* 8005CFAC */
  int wavebank_page_count;  /* 8005CFB0 */
  page_struct *cur_audio_ps;/* 8005CFB4 */
  pgid_t cur_pgid;          /* 8005CFB8 */
  page_struct **page_map;   /* 8005CFBC */
  int level_update_pending; /* 8005CFC0 */
} ns_struct;

typedef struct {
  int entry_count;
  int page_count;
  union {
    eid_t eids[8];
    entry *entries[8];
  };
  union {
    pgid_t pgids[32];
    page *pages[32];
  };
} ns_loadlist;

/* reference types */
typedef union {
  pgid_t pgid;
  page *pg;
  struct {
    uint32_t is_pgid:1;
    uint32_t idx:31;
  };
  uint32_t value;
} page_ref;

typedef union {
  eid_t eid;
  entry *en; /* ref->is_entry == (!ref->is_eid && !ref->is_value && ref->en->magic == MAGIC_ENTRY) */
  struct {
    uint32_t is_eid:1;
    uint32_t is_value:1;
    uint32_t:13;
    uint32_t hash:10;
    uint32_t :7;
  };
  uint32_t value;
  nsd_pte *pte;
} entry_ref;

typedef union {
  entry *en;
  nsd_pte pte;
  uint32_t value;
} nsd_pte_enp;

typedef struct {
  const char name[8];
  int (*init)(void);
  int (*init2)(void);
  int (*on_load)(entry*);
  int (*unused)(void);
  int (*kill)(void);
} ns_subsystem;

extern char *NSEIDToString(eid_t eid);
extern char *NSEIDValToString(eid_t eid);
extern eid_t NSStringToEID(char *str);
extern char NSAlphaNumChar(int idx);
extern int NSAlphaNumIndex(char c);
extern char *NSGetFilename(int type, uint32_t lid);
extern char *NSGetFilename2(int type, uint32_t lid, char *path, char *subdir);
extern int NSAtoi(int type, char *str);
extern char *NSGetFilenameStr(int type, char *str);

extern int NSEntryItemSize(entry *entry, int idx);
extern int NSCountEntries(ns_struct *nss, int type);
#ifdef PSX
/* psx needs to actually 'page' the pages from cd
   so the per-frame handler is needed
   as well as allocate, free, and delete functions */
extern void NSUpdate(pgid_t pgid);
extern void NSUpdate2(pgid_t pgid);
extern int NSCdSeek(int idx);
extern int NSPageUpdate(page_struct *ps, pgid_t pgid);
extern int NSPageAllocate(int* count, int flag);
extern int NSPageFree(page_struct* ps);
extern int NSAudioPageAllocate(int type, int idx);
extern page_struct *NSPageDelete(page_struct *ps);
#endif
extern int NSTexturePageAllocate();
extern int NSTexturePageFree(int idx);
extern void NSPageDecRef(page_struct *ps);
extern void NSZoneUnload(ns_loadlist *loadlist);
extern page_struct *NSPageStruct(void *ref);
extern int NSPageOpen(pgid_t pgid, int flag, int count, eid_t eid);
extern int NSPageClose(pgid_t pgid, int count);
extern entry *NSOpen(void *ref, int flag, int count);
extern int NSClose(void *ref, int count);
extern int NSCountAvailablePages();
extern int NSCountAvailablePages2(void *list, int len);
extern nsd_pte *NSProbe(eid_t eid);
// extern nsd_pte *NSProbeSafe(eid_t eid); /* proto only */
// extern entry *NSResolve(nsd_pte *pte); /* proto only */
extern entry *NSLookup(void *ref);
extern void NSInit(ns_struct *nss, uint32_t lid);
extern int NSKill(ns_struct *nss);
extern void NSKillPage(ns_struct *nss, int idx);
#ifndef PSX
extern uint8_t *NSFileReadRange(char *filename, int start, int end, size_t *size);
#endif
#endif /* _NS_H_ */
