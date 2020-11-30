#include "vm/page.h"
#include <debug.h>
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "vm/frame.h"
#include "vm/swap.h"

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


struct sup_table_entry *find_spt_entry(void *uaddr) {
  struct sup_table_entry key;
  key.upage = pg_round_down(uaddr);
  
  struct hash_elem *elem = hash_find(&thread_current()->sup_table, &key.elem);

  if (elem == NULL) {
    return NULL;
  }
  
  return hash_entry(elem, struct sup_table_entry, elem);
}

void remove_spt_entry(void *uaddr) {
  struct sup_table_entry *spt = find_spt_entry(uaddr);

  if (spt != NULL) {
    if (!spt->empty) {
      remove_swap_space(spt->location.block_number, 1);
    }
    
    hash_delete(&thread_current()->sup_table, &spt->elem);
    free(spt);
  }
}

void destroy_spt_entry(struct hash_elem *e, void *aux UNUSED) {
  struct sup_table_entry *spt = hash_entry(e, struct sup_table_entry, elem);

  if (!spt->empty) {
    remove_swap_space(spt->location.block_number, 1);
  }

  /* removes frame table entry of the page if it is in physical memory */
  struct frame_table_elem *ft = find_ft_elem(spt->upage);
  if (ft != NULL) {
    remove_ft_elem(spt->upage);
  }

  free(spt);
}
