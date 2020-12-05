#include "vm/page.h"
#include <debug.h>

#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "vm/frame.h"
//#include "vm/swap.h"

static unsigned spt_hash_uaddr(const struct hash_elem *e, void *aux UNUSED);
static bool spt_cmp_uaddr(const struct hash_elem *a, const struct hash_elem *b,
				void *aux UNUSED);

/* Initialise sup_table */
void spt_init(struct hash *sup_table) {
  hash_init(sup_table, spt_hash_uaddr, spt_cmp_uaddr, NULL);
}

/* Gets a user page to be written to swap space on eviction
   Creates sup_table_entry for it
   Takes user page pointer, file pointer, offset within file
   and whether file is writable */
void create_file_page(void *upage, struct file *file, off_t offset,
		      bool writable, enum sup_entry_type type) {
//TODO: FREE ON EXIT		      
  struct sup_table_entry *spt = malloc(sizeof(struct sup_table_entry));  
  if(spt == NULL) {
    thread_exit();
  }
  
  spt->file = file;
  spt->offset = offset;
  spt->upage = upage;
  spt->writable = writable;
  spt->type = type;

  hash_insert(&thread_current()->sup_table, &spt->elem);
}

void create_stack_page(void *upage) {
//TODO: FREE ON EXIT
  struct sup_table_entry *spt = malloc(sizeof(struct sup_table_entry));  
  if(spt == NULL) {
    thread_exit();
  }
  
  spt->file = NULL;
  spt->offset = 0;
  spt->upage = upage;
  spt->writable = true;
  spt->type = ZERO_PAGE;

  hash_insert(&thread_current()->sup_table, &spt->elem);
}

/* Calculates a hash value based on user page address of e */
static unsigned spt_hash_uaddr(const struct hash_elem *e, void *aux UNUSED) {
  struct sup_table_entry *st = hash_entry(e, struct sup_table_entry, elem);
  
  return (unsigned) st->upage / PGSIZE;
}

/* Compares entries on numerical value of user page address */ 
static bool spt_cmp_uaddr(const struct hash_elem *a, const struct hash_elem *b,
		   void *aux UNUSED) {
  struct sup_table_entry *st1 = hash_entry(a, struct sup_table_entry, elem);
  struct sup_table_entry *st2 = hash_entry(b, struct sup_table_entry, elem);
  
  return st1->upage < st2->upage;
}

/* Finds entry correesponding to a given page in the supplemental page table 
   Takes in a thread with sup_table to search and  a user page to search for 
   Returns the page entry struct if found or NULL otherwise */
struct sup_table_entry *spt_find_entry(struct thread *t, void *uaddr) {
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
void spt_remove_entry(void *uaddr) {
  struct sup_table_entry *spt = spt_find_entry(thread_current(), uaddr);

  if (spt != NULL) {
    if (spt->type == ZERO_PAGE) {
      //remove_swap_space(spt->block_number, 1);
    }
    
    hash_delete(&thread_current()->sup_table, &spt->elem);
    free(spt);
  }
}


/* Frees a supplemental page table entry at given page
   Clears any swap space allocated to the provided virtual page
   Removes frame table entry at the same user virtual address if it exists
   To be used in hash_destroy to delete all supplemental page table entries */
void spt_destroy_entry(struct hash_elem *e, void *aux UNUSED) {
  struct sup_table_entry *spt = hash_entry(e, struct sup_table_entry, elem);

  if (spt->type == ZERO_PAGE) {
    //remove_swap_space(spt->block_number, 1);
  }

  /* Removes frame table entry of the page if it is in physical memory */
  ft_lock_acquire();
  struct frame_table_entry *ft = ft_find_entry(spt->upage);
  
  if (ft != NULL) {
    ft_remove_entry(spt->upage);
  }

  ft_lock_release();

  free(spt);
}
