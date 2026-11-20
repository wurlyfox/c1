#include "ext/ns.h"

static int NSReadPage(FILE *file, const nsd *dir, pgid_t pgid, page *page_out) {
  uint32_t offset;

  if (!(pgid & 1) || (int)(pgid>>1) >= dir->page_count) 
    return 0;
  offset = dir->pages_offset * SECTOR_SIZE
         + (pgid>>1) * PAGE_SIZE;
  if (fseek(file, offset, SEEK_SET) != 0)
    return 0;
  if (fread(page_out, 1, PAGE_SIZE, file) != PAGE_SIZE) 
    return 0;
  return 1;
}

/**
 * Read a font texture page and definition directly 
 * from an NSF/NSD file, independently of the paging system. 
 * 
 * @param lid Level ID for the NSF/NSD file
 * @param gool_eid EID for the GOOL entry with the font definition
 * @param font_eid EID for the font texture page
 * @param font (output) Font definition
 * @param font_page (output) Font texture page
 * @return 1 on success, 0 on failure
 */
int NSReadFontAssets(lid_t lid, eid_t gool_eid, eid_t font_eid,
                     gool_font *font, tpage *font_page) {
  FILE *file;
  nsd dir;
  nsd_pte pte;
  gool_anim *anim;
  page gool_page;
  entry *en, *gool_entry;
  pgid_t font_pgid, gool_pgid;
  uint8_t *item, *last;
  size_t size;
  int i, offset;

  font_pgid = 0;
  gool_pgid = 0;
  file = fopen(NSGetFilename(0, lid), "rb");
  if (!file) 
    return 0;
  if (fread(&dir, 1, sizeof(dir), file) != sizeof(dir)) {
    fclose(file);
    return 0;
  }
  /* we could use the hash table, but for now this is simpler */
  for (i=0;i<(int)dir.page_table_size;i++) {
    if (fread(&pte, sizeof(pte), 1, file) != 1) break;
    if (pte.eid == font_eid) 
      font_pgid = pte.pgid;
    if (pte.eid == gool_eid) 
      gool_pgid = pte.pgid;
  }
  fclose(file);
  if (!font_pgid || !gool_pgid) 
    return 0;
  file = fopen(NSGetFilename(1, lid), "rb");
  if (!file) 
    return 0;
  if (NSReadPage(file, &dir, font_pgid, (page*)font_page) == 0
   || NSReadPage(file, &dir, gool_pgid, &gool_page) == 0) {
    fclose(file);
    return 0;
  }
  fclose(file);
  /* now search the gool page for the entry */
  gool_entry = 0;
  for (i=0;i<(int)gool_page.entry_count;i++) {
    offset = gool_page.entry_offsets[i];
    en = (entry*)(gool_page.data + offset);
    if (en->magic == MAGIC_ENTRY && en->eid == gool_eid) {
      gool_entry = en;
      break;
    }
  }
  if (!gool_entry || gool_entry->item_count < 6)
    return 0;
  item = (uint8_t*)gool_entry + gool_entry->item_offsets[5];
  last = (uint8_t*)gool_entry + gool_entry->item_offsets[6];
  size = last - item;
  while (size >= sizeof(gool_font)) {
    anim = (gool_anim*)item;
    if (anim->type == GOOL_ANIM_TYPE_FONT && anim->eid == font_eid) {
      memcpy(font, anim, sizeof(gool_font));
      return 1;
    }
    item += sizeof(uint32_t);
    size -= sizeof(uint32_t);
  }
  return 0;
}
