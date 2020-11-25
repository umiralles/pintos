#include "vm/frame.h"
#include <debug.h>

#include "threads/vaddr.h"

/* frame table */
struct hash frame_table;

/* Generates a hash function from the user virtual address of a page 
   hash function is address divided by page size which acts as a page number */
unsigned hash_user_address(const struct hash_elem *e, void *aux UNUSED) {
  struct frame_table_elem *ft = hash_entry(e, struct frame_table_elem, elem);

  return ((unsigned) ft->frame) / PGSIZE;
}

/* Compares two frame table elems based on timestamp they were added in */
bool cmp_timestamp(const struct hash_elem *a, const struct hash_elem *b,
		   void *aux UNUSED) {
  struct frame_table_elem *ft1 = hash_entry(a, struct frame_table_elem, elem);
  struct frame_table_elem *ft2 = hash_entry(b, struct frame_table_elem, elem);

  return ft1->timestamp < ft2->timestamp;
}
