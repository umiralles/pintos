#include "vm/page.h"
#include <debug.h>
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "vm/frame.h"
#include "vm/swap.h"

struct hash sup_table;

/* Calculates a hash value based on user page address of e */
unsigned sup_table_hash_uaddr(const struct hash_elem *e, void *aux UNUSED) {
  struct sup_table_entry *st = hash_entry(e, struct sup_table_entry, elem);
  
  return (unsigned) st->upage / PGSIZE;
}

/* Compares entries on numerical value of user page address */ 
bool sup_table_cmp_uaddr(const struct hash_elem *a, const struct hash_elem *b,
		   void *aux UNUSED) {
  struct sup_table_entry *st1 = hash_entry(a, struct sup_table_entry, elem);
  struct sup_table_entry *st2 = hash_entry(b, struct sup_table_entry, elem);
  
  return st1->upage < st2->upage;
}

/* Finds entry correesponding to a given page in the supplemental page table 
   Takes in a thread with sup_table to search and  a user page to search for 
   Returns the page entry struct if found or NULL otherwise */
struct sup_table_entry *find_spt_entry(struct thread *t, void *uaddr) {
  struct sup_table_entry key;
  key.upage = pg_round_down(uaddr);
  
  struct hash_elem *elem = hash_find(&t->sup_table, &key.elem);

  if (elem == NULL) {
    return NULL;
  }
  
  return hash_entry(elem, struct sup_table_entry, elem);
}

/* Removes a supplemental page table entry at given page and frees its memory
   Also clears any swap space allocated to the provided virtual page
   Takes in the user page address of the entry to search for 
   Does nothing if the entry cannot be found */
void remove_spt_entry(void *uaddr) {
  struct sup_table_entry *spt = find_spt_entry(thread_current(), uaddr);

  if (spt != NULL) {
    if (!spt->empty) {
      remove_swap_space(spt->block_number, 1);
    }
    
    hash_delete(&thread_current()->sup_table, &spt->elem);
    free(spt);
  }
}


/* Frees a supplemental page table entry at given page
   Clears any swap space allocated to the provided virtual page
   Removes frame table entry at the same user virtual address if it exists
   To be used in hash_destroy to delete all supplemental page table entries */
void destroy_spt_entry(struct hash_elem *e, void *aux UNUSED) {
  struct sup_table_entry *spt = hash_entry(e, struct sup_table_entry, elem);

  if (!spt->empty) {
    remove_swap_space(spt->block_number, 1);
  }

  /* removes frame table entry of the page if it is in physical memory */
  struct frame_table_elem *ft = find_ft_entry(spt->upage);
  if (ft != NULL) {
    remove_ft_entry(spt->upage);
  }

  free(spt);
}
