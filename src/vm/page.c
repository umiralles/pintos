#include "vm/page.h"
#include <debug.h>
#include "threads/vaddr.h"

struct hash sup_table;

unsigned sup_table_hash_uaddr(const struct hash_elem *e, void *aux UNUSED) {
  struct sup_table_entry *st = hash_entry(e, struct sup_table_entry, elem);
  
  return (unsigned) st->upage / PGSIZE;
}

bool sup_table_cmp_uaddr(const struct hash_elem *a, const struct hash_elem *b,
		   void *aux UNUSED) {
  struct sup_table_entry *st1 = hash_entry(a, struct sup_table_entry, elem);
  struct sup_table_entry *st2 = hash_entry(b, struct sup_table_entry, elem);
  
  return st1->upage < st2->upage;
}
