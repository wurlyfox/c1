#include "ns.h"
#include "geom.h"
#include "title.h"
#include "formats/zdat.h"

/* .rdata */
#ifdef CFLAGS_ORIG_IMPL
#define NS_PATH "c:/src/willie/target/"
#else
#define NS_PATH "./"
#endif
#define NS_SUBPATH "streams/"

/* .data */
const char alpha_map[64] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                            'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
                            'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
                            'u', 'v', 'w', 'x', 'y', 'z', 'A', 'B', 'C', 'D',
                            'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
                            'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                            'Y', 'Z', '_', '!' };
/* .sdata */
rect216 tpage_rect = { .x=0,.y=0,.w=256,.h=128 }; /* 80056420; gp[0x9] */
/* .bss */
lid_t cur_lid;                 /* 80056710 */
lid_t next_lid;                /* 80056714 */
char eid_str_buf[16];          /* 80056718 */
char eid_str_buf2[16];         /* 80056728 */
char ns_filename[128];         /* 80056738 */
page_struct audio_pages[8];    /* 80057F40 */
page_struct texture_pages[16]; /* 800580A0 */
ns_struct ns;                  /* 8005C528 */

extern ns_subsystem subsys[21];
extern int use_cd;
extern entry *cur_zone;
extern eid_t insts[8];

//----- (80012580) --------------------------------------------------------
char *NSEIDToString(eid_t eid) {
  int i; // $v1

  if (eid & 1) {
    eid_str_buf[5] = 0;
    eid >>= 1;
    for (i=4;i>=0;i--) {
      eid_str_buf[i] = alpha_map[eid & 0x3F];
      eid >>= 6;
    }
  }
  else
    sprintf(eid_str_buf, "[%X]", eid);
  return eid_str_buf;
}

//----- (80012604) --------------------------------------------------------
char *NSEIDValToString(eid_t eid) {
  if (eid & 1)
    sprintf(eid_str_buf2, "p%d", eid>>1);
  else
    sprintf(eid_str_buf2, "[%X]", eid);
  return eid_str_buf2;
}

//----- (80012660) --------------------------------------------------------
eid_t NSStringToEID(char* str) {
  char *c;
  int i;
  eid_t eid;

  eid = 0;
  for (c=str;c<str+5;c++) {
    eid <<= 6;
    for (i=0;i<64;i++) {
      if (alpha_map[i]==*c) { break; }
    }
    if (i==64) { continue; }
    eid |= i;
  }
  eid = (eid << 1) | 1;
  return eid;
}

//----- (800126C0) --------------------------------------------------------
int NSEIDType(nsd_pte *pte) {
  entry *en;
  char *eid_str;

  if (!(pte->value & 3)) { /* not a pte? */
    en = (entry*)pte;
    return en->type; /* entry pointer; return type */
  }
  eid_str = NSEIDToString(pte->eid);
  switch (eid_str[4]) {
  case 'V': return 1;
  case 'G': return 2;
  case 'W': return 3;
  case 'S': return 4;
  case 'T': return 5;
  case 'L': return 6;
  case 'Z': return 7;
  case 'C': return 11;
  case 'A': return 12;
  case 'M': return 13;
  case 'N': return 14;
  case 'D': return 15;
  case 'I': return 16;
  case 'P': return 17;
  case 'U': return 18;
  case 'B': return 19;
  case 'X': return 20;
  default:
    return 0;
  }
}

//----- (80012820) --------------------------------------------------------
char NSAlphaNumChar(int idx) {
  return alpha_map[idx];
}

//----- (80012834) --------------------------------------------------------
// unreferenced; exists as inline code elsewhere
int NSAlphaNumIndex(char c) {
  int i;

  for (i=0;i<64;i++) {
    if (alpha_map[i]==c) { break; }
  }
  if (i >= 64)
    return c == '-' ? 99: -14;
  return i;
}

//----- (80012880) --------------------------------------------------------
char *NSGetFilename(int type, uint32_t lid) {
  return NSGetFilename2(type, lid, 0, 0);
}

//----- (800128A4) --------------------------------------------------------
char *NSGetFilename2(int type, uint32_t lid, char *path, char *subdir) {
  /* path and subdir args unused/ignored */
  if (type == 0) { /* type 0? (NSD) */
    if (use_cd)
      sprintf(ns_filename, "\\S%X\\S%07X.NSD", lid >> 4, lid);
    else
      sprintf(ns_filename, "%s%ss%07x.nsd", NS_PATH, NS_SUBPATH, lid);
    return ns_filename;
  }
  else if (type == 1) { /* type 1? (NSF) */
    if (use_cd)
      sprintf(ns_filename, "\\S%X\\S%07X.NSF", lid >> 4, lid);
    else
      sprintf(ns_filename, "%s%ss%07x.nsf", NS_PATH, NS_SUBPATH, lid);
    return ns_filename;
  }
  return (char*)ERROR; /* invalid type */
}

//----- (8001297C) --------------------------------------------------------
int NSAtoi(int type, char *str) {
  switch (type) {
  case 0:
  case 1:
  case 13:
  case 14:
    return NSAlphaNumIndex(str[0]);
  case 2:
    return (2 * atoi(str)) | 1;
  case 3:
    return NSStringToEID(str);
  default:
    return atoi(str);
  }
}

//----- (80012A84) --------------------------------------------------------
char *NSGetFilenameStr(int type, char *str) {
  int res;
  res = NSAtoi(type, str);
  return NSGetFilename(type, res);
}

//----- (80012BAC) --------------------------------------------------------
static inline page *NSMallocPage() {
  page *pg;

  pg = (page*)malloc(sizeof(page));
  if (!pg || ISERRORCODE(pg)) { return (page*)ERROR_MALLOC_FAILED; }
  return pg;
}

//----- (80012C74) --------------------------------------------------------
// unreferenced; possibly does not exist as inline code elsewhere
int NSEntryItemSize(entry *entry, int idx) {
  if (!entry->item_count) { return 0; }
  return entry->items[idx+1] - entry->items[idx];
}

#ifdef PSX
// unreferenced; exists as inline code elsewhere
static uint8_t *NSTransferFile(ns_filedesc *fd, uint8_t *buf, int flag) {
  CdlLOC p;
  uint8_t res[8];
  int sector_len, size;

  if (!use_cd) { return 0;}
  if (!flag) { return buf; }
  sector_len = shr_ceil(fd->size, 11);
  if (!buf) {
    size = sector_len << 11; /* round up to nearest sector size multiple */
    buf = (uint8_t*)malloc(size);
    if (!buf)
      return (uint8_t*)ERROR_MALLOC_FAILED;
  }
  CdIntToPos(fd->sector_num, &p);
  CdControl(2, &p, res); /* CdlSetLoc */
  if (!CdRead(sector_len, buf, 128) || CdReadSync(0, 0))
    return (uint8_t*)ERROR_READ_FAILED;
  return realloc(buf, fd->size); /* realloc to non sector size multiple */
}
#else

uint8_t *NSFileReadRange(char *filename, int start, int end, size_t *size) {
  FILE *file;
  size_t tmp;
  uint8_t *data;

  if (end == 0 || (end != -1 && start >= end)) { return 0; }
  if (size == 0) { size = &tmp; }
  file = fopen(filename, "r");
  fseek(file, 0, SEEK_END);
  end = end == -1 ? ftell(file) : end;
  *size = end - start;
  data = (uint8_t*)malloc(*size);
  if (!data) {
    fclose(file);
    return (uint8_t*)ERROR_MALLOC_FAILED;
  }
  fseek(file, start, SEEK_SET);
  fread(data, 1, *size, file);
  return data;
}

static uint8_t *NSFileReadFrom(char *filename, int offset, size_t *size) {
  return NSFileReadRange(filename, offset, -1, size);
}

static uint8_t *NSFileRead(char *filename, size_t *size) {
  return NSFileReadRange(filename, 0, -1, size);
}
#endif

int NSCountEntries(ns_struct *nss, int type) {
  nsd *nsd;
  nsd_pte *pte;
  int i, count;

  nsd = nss->nsd;
  count = 0;
  for (i=0;i<nsd->page_table_size;i++) {
    pte = &nsd->page_table[i];
    if (NSEIDType(pte) == type)
      count++;
  }
  return count;
}

//----- (80012E64) --------------------------------------------------------
// unreferenced; exists as inline code elsewhere
static inline int NSPageTranslateOffsets(page* page) {
  int i, ii;
  if (page->type == 1)
    return SUCCESS;
  if (page->magic != MAGIC_PAGE)
    return ERROR_INVALID_MAGIC;
  for (i=page->entry_count-1;i>=0;i--) {
    *(uint32_t*)&page->entries[i] += (uint32_t)page;
    for (ii=page->entries[i]->item_count;ii>=0;ii--)
      *(uint32_t*)&page->entries[i]->items[ii] += (uint32_t)page->entries[i];
  }
  page->entries[page->entry_count] += (uint32_t)page;
  return SUCCESS;
}

#ifndef PSX
page tpagemem[16];
#endif
//----- (80012F10) --------------------------------------------------------
static void NSPageUpdateEntries(int idx) {
  page_struct *ps, *tps;
  page *page;
  entry *entry;
  eid_t eid;
  nsd_pte *pte, *bucket;
  int i, type, hash;
  tpage *tpg;

  ps = &ns.physical_pages[idx];
  if (ps == ns.cur_ps)
    ns.cur_ps = 0;
  page = ps->page;
  ps->state = 30;
  type = page->type;
  if (type == 0) {
    entry = page->entries[0];
    hash = (entry->eid >> 15) & 0xFF;
    pte = ns.pte_buckets[hash];
    if (entry->type == 0) {
      ps->state = 20;
      return/* page*/;
    }
    for (i=0;i<page->entry_count;i++) {
      entry = page->entries[i];
      while ((pte++)->eid != entry->eid);
      --pte;
      pte->entry = entry;
      ps->ref_count++;
      type = entry->type;
      if (subsys[type].on_load)
        (*subsys[type].on_load)(entry);
      ps->ref_count--;
    }
  }
  else if (type == 1) {
    idx = NSTexturePageAllocate();
    if (idx != -12 || (idx = 15, ps->ref_count)) {
      tps = &texture_pages[idx];
#ifdef PSX
      tpage_rect.x = (idx % 4) * 256;
      tpage_rect.y = (idx / 4) * 128;
      LoadImage(tpage_rect, page);
#else
      tpagemem[idx] = *page;
      page = &tpagemem[idx];
#endif
      tpg = (tpage*)page;
      pte = NSProbe(tpg->eid);
      tps->state = 20;
      tps->pgid = ps->pgid;
      tps->eid = tpg->eid;
      tps->pte = pte;
      tps->page = page;
#ifdef PSX
      pte->value = tps->info;
#else
      pte->entry = (struct _entry*)page;
#endif
    }
    ps->state = 1; /* free the original page */
    tps = &texture_pages[idx];
    ns.page_map[ps->pgid >> 1] = tps; /* point to new [texture] page struct */
    return/* (idx << 1)*/;
  }
  else if (type == 3 || type == 4) {
    entry = page->entries[0];
    hash = (entry->eid >> 15) & 0xFF;
    pte = ns.pte_buckets[hash]-1;
    for (i=0;i<page->entry_count;i++) {
      entry = page->entries[i];
      while ((++pte)->eid != entry->eid);
      if (i==0) {
        eid = page->type == 4 ? pte->eid : EID_NONE;
        insts[ps->tail->idx%8] = eid;
      }
#ifdef PSX
      pte->value = (((entry->items[0] - (uint8_t*)page) + ps->tail->addr) << 2) | 0x80000002;
#else
      pte->entry = entry;
#endif
      type = entry->type;
      if (subsys[type].on_load)
        (*subsys[type].on_load)(entry);
    }
    ps->state = 1;
    ps->tail->state = 20;
    ns.page_map[ps->pgid>>1] = ps->tail;
    return/* ps->tail->page_count*/;
  }
  ps->state = 20;
  return/* ps->page*/;
}

#ifdef PSX

//----- (8001331C) --------------------------------------------------------
int NSPageFree(page_struct* ps) {
  page_struct* ps;
  page* page;
  entry* entry;
  ns_pte* pte, *bucket;
  int pgid, type, hash;

  if (ps == cur_ps)
    cur_ps = 0;
  if (ps->state == 20 || ps->state == 21) {
    page = ps->page;
    if (ps->type == 0) {
      entry = page->entries[0];
      pgid = ps->pgid;
      hash = (entry->eid >> 15) & 0xFF;
      bucket = ns.pte_buckets[hash];
      for (i=0;i<page->entry_count;i++) {
        entry = page->entries[i];
        eid = entry->eid;
        type = entry->type;
        pte = bucket;
        while ((pte++)->eid != eid);
        (*subsys[type].kill)(entry)
        pte->pgid = pgid;
      }
    }
    ns.page_map[ps->pgid>>1] = NULL_PAGE;
  }
  ps->state = 1;
  return SUCCESS;
}

//----- (800134C8) --------------------------------------------------------
void NSUpdate(pgid_t pgid) {
  page_struct* ps, *cur_ps;
  int i, idx, count, state;
  int pgid_min, start, end;

  /* TODO: check control flow at bottom of this func */
  if (ns.page_count == 0) return;
  cur_ps = ns.cur_ps;
  if (cur_ps && pgid != -1) { /* update any pages in 'currently paging' group */
    page_count = cur_ps->page_count;
    ps = cur_ps - (page_count + 1);
    for (i=0;i<page_count;i++,ps++) {
      state = ps->state;
      if (state == 3 || state == 13)
        NS_PageUpdate(ps, pgid);
    }
  }
  if (!cur_ps && pgid == -1) { /* clone next sequence of contiguous virtual pages */
    for (i=0;i<ns.virtual_page_count;i++) { /* find first virtual page waiting to be processed */
      ps = &ns.virtual_pages[i];
      if (ps->state == 2 && (idx == -1 || ps->pgid < pgid_min)) {
        pgid_min = ps->pgid;
        idx = i;
      }
    }
    if (idx != -1) { /* count number of following virtual pages */
      count = 1;
      if (use_cd) {
        start = (pgid_min >> 1) + 1;
        end = ns.page_count - 1;
        for (i=start;i<end;i++) {
          ps = ns.page_map[i];
          if (ISERRORCODE(ps) && ps->type == 0)
            count++;
          else
            break;
        }
      }
      ps = &virtual_pages[idx];
      NSPageCreate(ps, ps->pgid, EID_NONE, count, 0); /* copy virtual pages to physical pages */
    }
  }
  for (i=0;i<ns.physical_page_count;i++) { /* update all [other] physical pages */
    NSPageUpdate(ns.physical_pages[i], pgid);
  }
}

//----- (80013748) --------------------------------------------------------
void NSUpdate2() {
  dword_8005CFC0 = 1;
  while (ns.page_count)
    NSUpdate(-1);
}

//----- (80013798) --------------------------------------------------------
int NSCdSeek(int idx) {
  ns_filedesc *fd;
  uint8_t res[8];

  if (!use_cd || cur_ps) { return 0; }
  if (!PadChkVsync(1, 0)) { return 0; }
  fd = &filedescs[idx];
  CdIntToPos(fd->sector_num, res);
  return CdControlF(0x15, res); /* CdlSeekL: logical seek */
}

// unreferenced; exists as inline code elsewhere
static int NSCdRead(page_struct *ps, int sector_count) {
  int size, sector_offset;
  uint8_t *dst;
  char res[16]; // [sp+10h] [-10h] BYREF

  if (ps->compressed) {
    size = 32*sector_count - ps->savings;
    dst = (uint8_t*)ps->page + (ps->savings << 11);
    sector_offset = ns.nsf_loc + ns.nsd->compressed_page_offsets[ps->pgid >> 1] >> 6;
  }
  else {
    size = 32*sector_count;
    dst = (uint8_t*)ps->page;
    sector_offset = ns.nsf_loc + ns.nsd->pages_offset + 32*(ps->pgid>>1);
  }
  CdIntToPos(sector_offset, res);
  CdControlF(2, res);
  if (CdRead(size, dst, 128) != 0)
    return ERROR_READ_FAILED;
  return SUCCESS;
}

//----- (80013970) --------------------------------------------------------
void NSPageDecompress(page_compressed *page_src, page *page_dst) {
  int i, size;
  uint16_t seek, length, skip, remainder, data;
  uint8_t span, prefix;
  uint8_t *src, *dst;

  src = page_src->data;
  dst = (uint8_t*)page_dst;
  if (page_src->magic == MAGIC_PAGE) {
    if ((uint8_t*)page_dst == ((uint8_t*)page_src)+0xC) { return; }
    for (i=0;i<0x10000;i++)
      *(dst++) = *(src++);
    return;
  }
  while (dst - (uint8_t*)page_dst < page_src->length) {
    prefix = *(src++);
    if (prefix & 0x80) {
      data = (prefix << 8) | *(src++);
      span = data & 7;
      seek = (data >> 3) & 0xFFF;
      if (span == 7)
        span = 0x40;
      else
        span += 3;
      for (i=0;i<span;i++,dst++)
        *(dst) = *(dst-seek);
    }
    else {
      for (i=0;i<prefix;i++)
        *(dst++) = *(src++);
    }
  }
  src += page_src->skip;
  remainder = 0x10000 - page_src->length;
  for (i=0;i<remainder;i++)
    *(dst++) = *(src++);
}

//----- (80013B30) --------------------------------------------------------
page_struct *NSPageTranslate(page_struct *ps) {
  int i, idx, count, state;
  int compressed, savings, pagesize;
  int clen, dlen;
  uint8_t *src, *dst;

  state = ps->state;
  if (state == 3 || state == 13) {
    if (ps->compressed) {
      dst = ps->page;
      src = dst + (ps->savings << 11);
      NSPageDecompress(src, dst);
    }
    ps->state = 4;
    NSPageTranslateOffsets(ps->page);
    return 0;
  }
  count = ns.nsd->count_compressed;
  idx = ps->page_idx;
  compressed = (idx < count) && use_compressed;
  count = ps->page_count;
  for (i=0;i<count;i++,ps--) {
    if (state != 2)
      continue;
    idx = ps->page_idx >> 1;
    clen = ns.nsd->compressed_lengths[idx] & 0x3F;
    ps->compressed = compressed;
    if (compressed) {
      savings += (32 - clen);
      ps->savings = savings;
    }
    if (ps->state == 2) {
      ps->state = 3;
      continue;
    }
    ps->state = 13;
    if (compressed) {
      dlen = clen + ps->savings;
      pagesize = dlen * 2048;
      ps->next = ps->page + pagesize;
    }
    else
      ps->next = ps[1].page;
    ps->next->magic = MAGIC_DECOMPRESSED;
  }
  return ps+(count>0);
}
// 8005C540: using guessed type int dword_8005C540;
// 8005CFC0: using guessed type int dword_8005CFC0;

//----- (80013D48) --------------------------------------------------------
int NSPageUpdate(page_struct *ps, pgid_t pgid) {

  while (1) {
    switch (ps->state) {
    case 2:
      if (ns.cur_ps) { return; }
      tail = ps->tail;
      idx = ps->pgid >> 1;
      tail_idx = tail->pgid >> 1;
      head_idx = ps->page_count;
      if (pgid == -1 || (idx >= head_idx && idx <= tail_idx)) {
        ns.cur_pgid = ns.cur_ps->pgid;
        if (use_cd) {
          if (CDSync(1, 0) != 2 && pgid == -1)
            return;
          head = NSPageTranslate(ps);
          ns.cur_ps = ps;
          NSCdRead(head, ps->page_count);
        }
        else {
#if defined(NS_TRANSFER_PC)
          head = NSPageTranslate(ps);
          NSTransferPage(pagesys, head, ps->page_count);
          head->state = 4;
          goto case_4;
#endif
        }
      }
    case 3:
      if (use_cd && ps == ns.cur_ps) {
        status = CdReadSync((pgid == -1), &buf);
        if (status <= 0)
          return;
        NSPageTranslate(ps);
        cur_ps = 0;
      }
      else {
        NSPageDelete(ps);
        page_count--;
        ns.cur_ps = 0;
        return;
      }
    }
    case 13:
      if (pgid != -1) {
        res = sub_80043984(0, 0);
        if (res != 0) { return; }
      }
      else if (ps->page->magic != MAGIC_DECOMPRESSED)
        return;
      NSPageTranslate(ps);
      if (pgid != -1 && ps != ps->tail)
        NSPageUpdate(ps+1, pgid); /* update next ps in the list */
    case 4:
      page = ps->page;
      type = page->type;
      if (type != 3 && type != 4) {
        NSPageUpdateEntries(ps->idx);
        page_count--;
        return;
      }
      else
        ps->state = 10;
    case 10:
      if (!ns.cur_audio_ps) { return; }
      if (pgid != -1 && pgid != ps->pgid) { return; }
      arg = -1;
      page = ps->page;
      type = page->type;
      if (type == 4)
        arg = ((uint32_t*)page->entries[0]->items[0])[0] + dword_8005CFB0;
      idx = NSAudioPageAllocate(type, arg);
      if (idx == -12) {
        NSPageDelete(ps);
        ns.page_count--;
        return;
      }
      aps = &dword_800574F0[idx];
      ps->state = 11;
      ps->tail = aps;
      ns.cur_audio_ps = ps;
      aps->state = 11;
      aps->pgid = ps->pgid;
      SpuSetTransferStartAddr(aps->addr);
      if (type == 3)
        SpuWrite(&page, 0x10000);
      else if (type == 4) {
        entry = &page->entries[0];
        item1 = entry->items[0];
        item2 = entry->items[1];
        size = ((uint32_t*)item1)[1];
        SpuWrite(item2, size);
      }
    case 11:
      if (ps == ns.cur_audio_ps) {
        res = SpuIsTransferCompleted(pgid == ps->pgid);
        if (res) {
          NSPageUpdateEntries(ps->idx);
          ns.cur_audio_ps = 0;
          ns.page_count--;
        }
      }
      break; /* !!! */
    case 31:
      ps->state = 1;
    }
  }
}

//----- (800141F4) --------------------------------------------------------
int NSPageAllocate(int* count, int flag) {
  int i, cur, rem, replace;
  int idx, len, age;
  int idx_max, len_max, age_max;
  page_struct *ps;

  for (i=0;i<ns.physical_page_count;i++) {
    rem = ns.physical_page_count - i;
    if (cur < min(rem, *count)) {
      ps = &ns.physical_pages[i + len];
      replace = ((ps->state == 20 || ps->state == 21) && ps->ref_count <= 0);
      if (ps->state == 1 || replace) {
        if (replace)
          age += 1 + ps->timestamp;
        len++;
        if (idx_max == -1 || len > len_max || (len == len_max && age >= age_max)) {
          idx_max = i;
          len_max = len;
          age_max = age;
        }
        continue;
      }
    }
    len = 0;
    age = 0;
  }
  if (idx_max != -1) {
    for (i=idx_max;i<idx_max+len_max;i++) {
      NSPageFree(ns.physical_pages[i]);
    }
    *count = len_max;
    return idx_max;
  }
  *count = 0;
  return -1;
}

#endif

//----- (8001439C) --------------------------------------------------------
int NSTexturePageAllocate() {
  int i, ii;
  page_struct *ps;
  entry_ref *ref;
  zone_header *header;
  ns_loadlist *loadlist;

  for (i=15;i>=0;i--) {
    ps = &texture_pages[i];
    if (ps->state == 1) { return i; }
  }
  for (i=15;i>=0;i--) {
    ps = &texture_pages[i];
    if (ps->state == 21) { return i; }
  }
  if (!cur_zone) {
    NSTexturePageFree(15);
    return 15;
  }
  /* steal the first slot occupied by a texture page
     that is not in the current zone load list, if any */
  header = (zone_header*)cur_zone->items[0];
  loadlist = &header->loadlist;
  for (i=15;i>=0;i--) {
    ps = &texture_pages[i];
    if (ps->state != 20 && ps->state != 21) { continue; }
    for (ii=0;ii<loadlist->entry_count;ii++) {
      ref = (entry_ref*)&loadlist->entries[ii];
      if (ref->is_eid) {
        if (ref->eid == ps->eid) { break; }
      }
      else if (ref->en->eid == ps->eid) { break; }
    }
    if (ii==loadlist->entry_count) {
      NSTexturePageFree(i);
      return i;
    }
  }
  return ERROR_NO_FREE_PAGES;
}

//----- (80014514) --------------------------------------------------------
int NSTexturePageFree(int idx) {
  page_struct *ps;

  ps = &texture_pages[idx];
  if (ps->state == 20) {
    ps->pte->pgid = ps->pgid;
#ifdef PSX
    ns.page_map[ps->pgid >> 1] = (page_struct*)NULL_PAGE;
#else
    /* page contents are kept in memory on pc,
       so keep the ps for the tpage data */
#endif
    ps->state = 1;
  }
  else if (ps->state == 21)
    ps->state = 1;
  return ERROR;
}

#ifdef PSX
//----- (800145C8) --------------------------------------------------------
int NSAudioPageAllocate(int type, int idx) {
  int i;
  page_struct *ps;

  for (i=7;i>=0;i--) {
    ps = &audio_pages[i];
    if (ps->state == 1 && ps->type == type) {
      if (idx == -1 || ps->idx == idx)
        break;
    }
  }
  if (i < 0) { return ERROR_NO_FREE_PAGES; }
  return i;
}

//----- (80014644) --------------------------------------------------------
// unreferenced; exists as inline code elsewhere
page_struct* NSPageCreateVirtual(int pgid, eid_t eid) {
  int i, count; // $v1
  page_struct* ps; // $a2

  count = ns.virtual_page_count;
  for (i=0,ps=&ns.virtual_pages;i<count;i++,ps++) {
    if (ps->state == 1) {
      ps->state = 2;
      ps->pgid = pgid;
      ps->entries_to_process = 0;
      ps->eid = eid;
      ps->timestamp = 0; /* ??????? */
      ns.page_map[pgid>>1] = ps;
      ns.page_count++;
      return ps;
    }
  }
  return 0;
}

//----- (800146F0) --------------------------------------------------------
page_struct *NSPageCreatePhysical(page_struct* ps, int pgid, eid_t eid, int *count, int flag) {
  int idx_head, idx_tail;
  page_struct *head, *tail, *src, *dst;

  idx_head = NSPageAllocate(count, flag);
  if (idx_head == -1)
    return (page_struct*)ERROR_NO_FREE_PAGES;
  idx_tail = idx_head + *count;
  tail = &physical_pages_pre[idx_tail];
  if (ps) { /* if there is a source group to copy */
    pgid = ps->pgid;
    dst = tail;
    for (i=*count-1;i>=0;i--,dst--) {
      src = ns.page_map[(pgid>>1)+i];
      dst->pgid = src->pgid;
      dst->eid = (eid == EID_NONE) ? src->eid : eid;
      dst->timestamp = src->timestamp;
      dst->page_count = src->page_count;
      dst->tail = tail;
      if (i == (*count - 1))
        dst->state = 2; /* tail page, ready for transfer */
      else
        dst->state = 12; /* other pages, ready for transfer */
      ns.page_map[dst->pgid>>1] = dst;
      src->state = 1;
    }
    return tail;
  }
  else { /* single new page */
    *count = 1;
    dst = tail;
    dst->pgid = pgid;
    dst->ref_count = 0;
    dst->timestamp = ticks_elapsed;
    dst->page_count = 1;
    dst->tail = tail;
    dst->state = 2;
    ns.page_map[dst->pgid >> 1] = dst;
    return dst
  }
}

//----- (800148C4) --------------------------------------------------------
void NSPageDecRef(page_struct *ps) {
  if (ps->ref_count <= 0) { return; }
  ps->ref_count--;
  if (ps->ref_count <= 0 && ps->type == 0) {
    NSPageDelete(ps);
    ns->page_count--;
  }
}

//----- (80014930) --------------------------------------------------------
page_struct *NSPageDelete(page_struct *ps) {
  ps->state = 1;
  ns.page_map[ps->pgid>>1] = NULL_PAGE;
  return ps;
}

//----- (8001495C) --------------------------------------------------------
void NSZoneUnload(ns_loadlist* loadlist) {
  int i;
  page_struct* ps;
  if (!loadlist)
    return;
  for (i=0;i<loadlist->entry_count;i++) {
    ps = NSPageStruct(&loadlist->eids[i]);
    if (ISERRORCODE(ps))
      NSPageDecRef(ps);
  }
  for (i=0;i<loadlist->page_count;i++)
    NSPageClose(loadlist->pgids[i], 1);
}

//----- (80014B20) --------------------------------------------------------
// unreferenced; exists as inline code elsewhere
static inline page_struct* NSPageVirtual(int pgid, int eid) {
  page_struct* ps; // $v0
  ps = ns->virtual_pages[pgid >> 1];
  if (ps == ERROR_INVALID_MAGIC)
    ps = NSPageCreateVirtual(pgid, eid); /* inlined */
  else if (ISERRORCODE(ps))
    ps->eid = eid;
  return ps;
}

//----- (80014C08) --------------------------------------------------------
static page_struct* NSPagePhysical(int pgid, eid_t eid) {
  page_struct* ps; // $a0
  ps = ns->page_map[pgid >> 1];
  if (ISERRORCODE(ps)) { /* page does not exist as physical or virtual */
    ps = NSPageCreate(0, pgid, eid, 1, 1);
    if (ISERRORCODE(ps))
      return ps;
  }
  else if (ps == SUCCESS) /* previously succeeded */
    return ps;
  else if (ps->type == 0) /* virtual */
    ps = NSPageCreate(ps, pgid, eid, 1, 1);
  else if (ps->type == 1) { /* physical */
    ps->eid = eid;
    if (ps->state == 20)
      return ps;
  }
  else if (ps->type == 2 || ps->type == 3) /* texture/audio */
    return ps;
  NSUpdate(pgid);
  return ns->page_map[pgid>>1];
}
#else

static page_struct* NSPageTranslate(page_struct *ps) {
  int state, type;

  state = ps->state;
  type = ps->page->type;
  if (state == 3) {
    ps->state = 4;
    NSPageTranslateOffsets(ps->page);
    return 0;
  }
  return ps;
}

void NSPageDecRef(page_struct *ps) {
  if (ps->ref_count <= 0) { return; }
  ps->ref_count--;
}

void NSZoneUnload(ns_loadlist* loadlist) {
  int i;
  page_struct* ps;
  if (!loadlist)
    return;
  for (i=0;i<loadlist->entry_count;i++) {
    ps = NSPageStruct(&loadlist->eids[i]);
    if (ISERRORCODE(ps))
      NSPageDecRef(ps);
  }
  for (i=0;i<loadlist->page_count;i++)
    NSPageClose(loadlist->pgids[i], 1);
}

static page_struct *NSPagePhysical(int pgid, eid_t eid) {
  page_struct *ps;

  ps = ns.page_map[pgid >> 1];
  if ((int)ps == SUCCESS) { return ps; }
  if (ps->state == 3)
    NSPageTranslate(ps);
  if (ps->state == 4) {
    NSPageUpdateEntries(ps->idx);
    ns.page_count--;
  }
  return ps;
}

static page_struct *NSPageVirtual(int pgid, eid_t eid) {
  return NSPagePhysical(pgid, eid);
}

#endif

//----- (80014D00) --------------------------------------------------------
int NSPageOpen(pgid_t pgid, int flag, int count, eid_t eid) {
  page_struct* ps;
  ps = flag ? NSPagePhysical(pgid, eid)
            : NSPageVirtual(pgid, eid); /* inlined */
  if ((int)ps == SUCCESS)
    return SUCCESS;
  ps->ref_count += count;
  return ps->ref_count;
}

//----- (80014E34) --------------------------------------------------------
int NSPageClose(pgid_t pgid, int count) {
  page_struct* ps;
  int i;
  ps = ns.page_map[pgid >> 1];
  if (ISERRORCODE(ps))
    return 0;
  else if (ISSUCCESSCODE(ps) || ps->type == 2 || ps->type == 3)
    return 1;
  for (i=0;i<count;i++)
    NSPageDecRef(ps);
  return ps->ref_count + 1;
}

//----- (80014F30) --------------------------------------------------------
// unreferenced; exists as inline code elsewhere
page_struct* NSEntryPageStruct(entry *entry) {
  int i;
  uint8_t *addr_page, *addr_entry;
  if ((uint32_t)entry & 2) /* ??????? */
    return 0; /* ??????? */
  addr_entry = (uint8_t*)entry;
  for (i=0;i<ns.physical_page_count;i++) {
    addr_page = (uint8_t*)&ns.physical_pages[i].page;
    if (addr_entry > addr_page && addr_entry < addr_page + 0x10000)
      return &ns.physical_pages[i];
  }
  return 0;
}

//----- (80014FA8) --------------------------------------------------------
// unreferenced; exists as inline code in previous routine
page* NSEntryPage(entry *entry) {
  page_struct* ps; // $a1

  ps = NSEntryPageStruct(entry);
  if (ps)
    return ps->page;
  return 0;
}

//----- (80015034) --------------------------------------------------------
page_struct *NSPageStruct(void *en_ref) {
  nsd_pte* pte;
  page_struct* ps;
  entry_ref *ref;

  ref = (entry_ref*)en_ref;
  if (ref->is_eid)
    ref->pte = NSProbe(ref->eid);
  pte = ref->pte;
  if (pte->pgid & 1)
    ps = ns.page_map[pte->pgid>>1];
  else {
    ps = NSEntryPageStruct(pte->entry);
    if (ps == 0)
      return (page_struct*)SUCCESS;
  }
  return ps;
}

//----- (80015118) --------------------------------------------------------
entry *NSOpen(void *en_ref, int flag, int count) {
  entry_ref *ref;
  page_struct* ps;
  nsd_pte* pte;
  entry* entry;
  int pgid;
  eid_t eid;

  ref = (entry_ref*)en_ref;
  if (ref->value == EID_NONE) { return (struct _entry*)ERROR_INVALID_REF; }
  if (ref->is_eid)
    pte = ref->pte = NSProbe(ref->eid);
  else
    pte = ref->pte;
  if (pte->pgid & 1) { /* unresolved page? */
    pgid = pte->pgid;
    eid = pte->eid;
    NSPageOpen(pgid, flag, count, eid);
    if (pte->pgid & 1) /* still a page id? (i.e. page could not be opened and resolved) */
      return 0;
  }
  else if (count && !(pte->value & 2)) {
    entry = pte->entry;
    ps = NSEntryPageStruct(entry);
    if (ps) {
      pgid = ps->pgid;
      eid = ps->eid;
      NSPageOpen(pgid, flag, count, eid);
    }
  }
  return pte->entry;
}

//----- (80015458) --------------------------------------------------------
int NSClose(void *en_ref, int count) {
  entry_ref *ref;
  page_struct* ps;
  nsd_pte* pte;
  int pgid;
  eid_t eid;

  ref = (entry_ref*)en_ref;
  if (ref->is_eid)
    pte = ref->pte = NSProbe(ref->eid);
  else
    pte = ref->pte;
  if (pte->pgid & 1)
    pgid = pte->pgid;
  else if (!count)
    return 1;
  else if (pte->value & 2)
    return 0;
  else {
    ps = NSEntryPageStruct(pte->entry);
    if (!ps || ISERRORCODE(ps)) { return 0; }
    if ((int)ps == SUCCESS) { return 1; }
    pgid = ps->pgid;
  }
  return NSPageClose(pgid, count);
}

//----- (800156D4) --------------------------------------------------------
int NSCountAvailablePages() {
  page_struct* ps;
  int i, count;

  count = ns.physical_page_count;
  for (i=0;i<ns.virtual_page_count;i++) {
    ps = &ns.virtual_pages[i];
    if (ps->state != 1 && ps->ref_count)
      count--;
  }
  for (i=0;i<ns.physical_page_count;i++) {
    ps = &ns.physical_pages[i];
    if (ps->state != 1 && ps->ref_count)
      count--;
  }
  return count;
}

//----- (8001579C) --------------------------------------------------------
int NSCountAvailablePages2(void *list, int len) {
  entry_ref **ref;
  page_struct *ps;
  nsd_pte *pte;
  pgid_t pgid, cache[44];
  eid_t eid;
  int cache_size;
  int i, ii, count;

  ref = (entry_ref**)list;
  for (i=0;i<len;i++) {
    if ((*ref)->is_eid)
      pte = (*ref)->pte = NSProbe((*ref)->eid);
    else
      pte = (*ref)->pte;
    if ((pte->pgid & 1) || (pte->value & 2)) {
      pgid = pte->pgid;
      ps = ns.page_map[pgid >> 1];
      if ((int)ps == NULL_PAGE) {
        for (ii=0;ii<cache_size && pgid!=cache[ii];ii++);
        if (ii==cache_size) {
          count++;
          cache[cache_size++] = pgid;
        }
      }
      else if (ISERRORCODE(ps) && !ps->ref_count)
        count++;
    }
    else {
      ps = NSEntryPageStruct(pte->entry);
      if (ps && (ps->type == 1 || !ps->ref_count))
        count++;
    }
    ++ref;
  }
  return count;
}

//----- (80015978) --------------------------------------------------------
nsd_pte* NSProbe(eid_t eid) {
  int hash;
  nsd_pte *pte;

  hash = (eid >> 15) & 0xFF;
  pte = ns.pte_buckets[hash];
  while ((pte++)->eid != eid);
  return --pte;
}

//----- (800159C4) --------------------------------------------------------
// unreferenced; exists as inline code elsewhere
static inline nsd_pte *NSProbeSafe(eid_t eid) {
  int hash;
  nsd_pte *pte;

  hash = (eid >> 15) & 0xFF;
  pte = ns.pte_buckets[hash];
  do {
    if (pte - ns.page_table >= ns.nsd->page_table_size)
      break;
  } while ((pte++)->eid != eid);
  if (pte - ns.page_table >= ns.nsd->page_table_size)
    return (nsd_pte*)ERROR_INVALID_REF;
  return --pte;
}

static inline entry *NSResolve(nsd_pte *pte) {
  if (pte->pgid & 1) {
    NSPagePhysical(pte->pgid, pte->eid);
    if (pte->pgid & 1) { return (entry*)ERROR_INVALID_REF; }
  }
  return pte->entry;
}

//----- (80015A98) --------------------------------------------------------
entry *NSLookup(void *en_ref) {
  entry_ref *ref;
  nsd_pte *pte;
  ref = (entry_ref*)en_ref;
  if (ref->is_eid)
    pte = ref->pte = NSProbe(ref->eid);
  else
    pte = ref->pte; /* ??? */
  return NSResolve(pte);
}

//----- (80015F18) inline -------------------------------------------------
static inline void NSInitTexturePages(ns_struct *nss, uint32_t lid) {
  /*
  there are 2 ways to view the contents of vram (1024x512)

  - as an 4x4 array of 16 256x128 blocks, where each block holds
    the contents of a "texture page"
  - as 4 512x256 quadrants, where each quadrant is consisted of
    4 texture pages
    - upper left quadrant = onscreen display buffer = tex pages 0-3
    - upper right quadrant = offscreen draw buffer = tex pages 4-7
    - lower left & lower right quadrants = offscreen texture window = tex pages 8-15

  for each texture page we compute a bitfield with information
  including data and clut location of the corresponding block
  */
  page_struct *ps;
  nsd_pte *pte;
  uint32_t info;
  int blx, bly, clutx, cluty, pagexy;
  int i, idx, hash;

  for (i=0;i<16;i++) {
    ps = &texture_pages[i];
    ps->type = 2; /* texture page */
    blx=i%4;bly=i/4;
    pagexy=blx<<2;
    if (i>8)
      pagexy|=0x10;
    clutx=blx<<8;
    cluty=bly<<17;
    info=0x80000002|cluty|clutx|pagexy;
    if (bly & 1)
      info |= 0x80;
    ps->info = info;
    if (i<8) {
      ps->state = 30; /* upper half of vram "inaccessible" */
      continue;
    }
    else if (ps->state != 20 && ps->state != 21) { /* this slot not used in previous level? */
      ps->state = 1; /* freshly reserved; ready for transfer */
      continue;
    }
    /* slot was used in previous level */
    hash = (ps->eid >> 15) & 0xFF;
    pte = nss->pte_buckets[hash];
    while (ps->eid != pte->eid) {
      idx = (++pte - nss->page_table);
      if (idx >= nss->nsd->page_table_size) {
        pte = (nsd_pte*)-10; /* texture not in the page table for this level */
        break;
      }
    }
    if (!ISERRORCODE(pte)) { /* loaded texture also in page table for this level? */
      ps->state = 20; /* loaded */
      ps->pte = pte;
      ps->pgid = pte->pgid;
      ns.page_map[pte->pgid >> 1] = ps; /* reuse this ps */
    }
    else
      ps->state = 21; /* loaded, but should be replaced */
  }
}

//----- (800160F8) inline -------------------------------------------------
static inline void NSInitAudioPages(ns_struct *nss, uint32_t lid) {
  page_struct *aps;
  nsd_pte *pte;
  eid_t eid;
  uint32_t addr;
  int i, j, count, type;

  count=0;
  for (i=0;i<nss->nsd->page_table_size;i++) {
    pte = &nss->nsd->page_table[i];
    type = NSEIDType(pte);
    if (type == 14) { count++; }
  }
  switch (cur_lid) {
  case 3: case 0x22: case 0x29: case 0x2B: case 0x2E: case 0x38:
    ns.wavebank_page_count = max(8-count, 2);
    break;
  default:
    ns.wavebank_page_count = limit(8-count, 2, 4);
    break;
  }
  for (i=0;i<8;i++) {
    aps = &audio_pages[i];
    aps->idx = i;
    if (i < ns.wavebank_page_count) {
      aps->state = 1;
      aps->type = 3;
      aps->addr = addr;
    }
    else {
      aps->state = 1;
      aps->type = 4;
      aps->addr = 0x2000 + (ns.wavebank_page_count << 16) + 0xFF90*(i - ns.wavebank_page_count);
    }
    addr += 0x10000;
  }
  for (i=0;i<8;i++) {
    aps = &audio_pages[i];
    eid = insts[i];
    if (eid != EID_NONE && aps->type == 4) {
      pte = NSProbeSafe(eid);
      if (!ISERRORCODE(pte)) {

#ifdef PSX
        nss->page_map[pte->pgid >> 1] = aps;
#endif
        aps->state = 20;
        aps->pgid = pte->pgid;
        aps->eid = eid;
#ifdef PSX
        pte->value = 0x80000002;
#endif

/*#else
        page *page;
        entry *entry;
        page_struct *ps;
        ps = ns.page_map[pte->pgid>>1];
        page = ps->page;
        entry = 0;
        for (j=0;j<page->entry_count;j++) {
          entry = page->entries[i];
          if (pte->eid == entry->eid) { break; }
        }
        pte->entry = entry;
#endif*/
      }
      else
        insts[i] = EID_NONE;
    }
  }
}

//----- (80015B58) --------------------------------------------------------
void NSInit(ns_struct *nss, uint32_t lid) {
  nsd *nsd;
  page *pagemem;
  nsd_pte *pte, **buckets;
  page_struct *ps;
  size_t nsd_size, pagemem_size;
  char *nsf_filename, *nsd_filename;
  char filename[0x100];
  int i, idx, page_count, hash;
  int physical_page_count, virtual_page_count;
  int pgid, *offsets;

  cur_lid = lid;
  next_lid = -1;
  nss->inited = 1;
  nss->lid = lid;
  nss->page_count = 0;
  nss->cur_ps = 0;
  nss->cur_audio_ps = 0;
  nss->cur_pgid = 0;
  nss->level_update_pending = 1;
  nsd_filename = NSGetFilename(0, lid);
  strcpy(filename, nsd_filename);
#ifdef PSX
#ifdef NS_FILEMAP
  nsd = NSTransferFile(filemap[lid], nsd, 1, 0);
#else
  nsd = NSTransferFile(filename, nsd, 1, 0);
#endif
#else
  nsd = (struct _nsd*)NSFileRead(filename, 0);
#endif
  nss->nsd = nsd;
  /* record pointers to nsd data */
  nss->ldat_eid = &nsd->ldat_eid;
  nss->pte_buckets = nsd->page_table_buckets;
  nss->page_table = nsd->page_table;
  nss->ldat = (nsd_ldat*)&nsd->page_table[nsd->page_table_size];
  /* display loading screen */
  TitleLoading(lid, nss->ldat->image_data, nsd);
  nsd_size = sizeof(nsd) + sizeof(nsd_pte)*nsd->page_table_size + sizeof(nsd_ldat);
#ifdef PSX
  /* note: psy-q realloc always shrinks in-place */
  nss->nsd = realloc(nss->nsd, nsd_size); /* trim splash screen image */
#endif
  /* convert pte bucket relative offsets to absolute pointers */
  buckets = nss->pte_buckets;
  offsets = nss->nsd->ptb_offsets;
  for (i=0;i<256;i++)
    buckets[i] = &nss->page_table[offsets[i]];
  nsf_filename = NSGetFilename(1, lid);
  strcpy(filename, nsf_filename);
#ifdef PSX
  /* record position of nsf file */
  if (use_cd) { /* cd-rom read? */
#ifdef NS_FILEMAP
    ns->nsf_loc = mapping->nsf_loc; /* TODO: mapping? */
#else
    CdlFile fp;
    if (CDSearchFile(&fp, filename) == 0)
      return ERROR_READ_FAILED;
    nss->nsf_loc = fp.loc;
#endif
  }
  else {
#ifdef NS_TRANSFER_PC
    uint32_t handle = PCopen(filename);
    ns->nsf_loc = handle;
    if (handle == -1) {
      PCclose(-1);
      return -16;
    }
#endif
  }
#endif
  /* allocate and init the page map */
  page_count = nss->nsd->page_count;
  nss->page_map = (page_struct**)malloc(page_count*sizeof(page_struct*));
  for (i=0;i<page_count;i++)
    nss->page_map[i] = (page_struct*)NULL_PAGE; /* null page struct */

  /* call pre-page init subsystem funcs */
  for (i=0;i<21;i++) {
    if (subsys[i].init)
      (*subsys[i].init)();
  }
#ifdef PSX
  /* allocate and init 22 64 kb physical pages (max) */
  physical_page_count = 22;
  do {
    pagemem = malloc(physical_page_count*PAGE_SIZE);
    if (pagemem) {
      nss->pagemem = pagemem;
      break;
    }
  } while (physical_page_count > 0);
  nss->physical_page_count = physical_page_count;
  ps = &nss->physical_pages[0];
  for (i=0;i<physical_page_count;i++,ps++) {
    ps->state = 1;
    ps->type = 1;
    ps->idx = i;
    ps->ref_count = 0;
    ps->page = &pagemem[i];
  }

  /* init 38 virtual pages */
  virtual_page_count = 38;
  nss->virtual_page_count = virtual_page_count;
  ps = &nss->virtual_pages[0];
  for (i=0;i<virtual_page_count;i++,ps++) {
    ps->state = 1;
    ps->type = 0;
    /* no idx recorded for virtual pages */
    ps->ref_count = 0;
    ps->page = 0;
  }
#else
  /* load all uncompressed pages in nsf into pagemem */
  physical_page_count = page_count;
  pagemem = (page*)NSFileReadFrom(filename, nsd->pages_offset*SECTOR_SIZE, &pagemem_size);
  pagemem = (page*)realloc(pagemem, physical_page_count*PAGE_SIZE);
  nss->physical_page_count = physical_page_count;
  /* init physical page structs */
  ps = &nss->physical_pages[0];
  for (i=0;i<nss->physical_page_count;i++, ps++) {
    ps->type = 1;
    ps->idx = i;
    ps->page = &pagemem[i];
    ps->pgid = (i << 1) | 1;
    ps->ref_count = 0;
    ps->page_count = 1;
    ps->tail = ps;
    ps->state = 3; /* loaded */
  }
  /* no need for virtual pages */
  nss->virtual_page_count = 0;
  /* set page map */
  for (i=0;i<page_count;i++)
    nss->page_map[i] = &nss->physical_pages[i];
#endif
  printf("Inited and Allocated %d pages\n", physical_page_count);
  NSInitTexturePages(nss, lid);
  NSInitAudioPages(nss, lid);

  /* call post-page init subsystem funcs */
  for (i=0;i<21;i++) {
    if (subsys[i].init2)
      (*subsys[i].init2)();
  }
}

//----- (80016420) --------------------------------------------------------
int NSKill(ns_struct *nss) {
  int i;
  if (!nss->inited) { return SUCCESS; }
  for (i=0;i<21;i++) {
    if (subsys[i].kill)
      (*subsys[i].kill)();
  }
  for (i=0;i<nss->physical_page_count;i++)
    NSKillPage(nss, i);
  free(nss->pagemem);
  free(nss->page_map);
  free(nss->nsd);
  nss->inited = 0;
  return SUCCESS;
}

//----- (800164F8) --------------------------------------------------------
void NSKillPage(ns_struct *nss, int idx) {
  nss->physical_pages[idx].state = 0;
}
