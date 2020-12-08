#include "vm/page.h"
#include <debug.h>

#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "userprog/syscall.h"
#include "vm/frame.h"
#include "vm/swap.h"

/* Hash Functions */
static hash_hash_func spt_hash_uaddr;
static hash_less_func spt_cmp_uaddr;
static hash_action_func spt_destroy_entry;

/* Initialise sup_table */
void spt_init(struct hash *sup_table) {
  hash_init(sup_table, spt_hash_uaddr, spt_cmp_uaddr, NULL);
}

void spt_destroy(struct hash *sup_table) {
  hash_destroy(sup_table, spt_destroy_entry);
}

/* Gets a user page to be written to swap space on eviction
   Creates sup_table_entry for it
   Takes user page pointer, file pointer, offset within file
   and whether file is writable */
void create_file_page(void *upage, struct file *file, off_t offset,
		      bool writable, size_t read_bytes,
		      enum sup_entry_type type) {
//TODO: FREE ON EXIT		      
  struct sup_table_entry *spt = malloc(sizeof(struct sup_table_entry));  
  if(spt == NULL) {
    thread_exit();
  }
  
  overwrite_file_page(spt, upage, file, offset, writable, read_bytes, type);
  	
  hash_insert(&thread_current()->sup_table, &spt->elem);
}

void overwrite_file_page(struct sup_table_entry *spt, void *upage,
			  struct file *file, off_t offset, bool writable, 
			  size_t read_bytes, enum sup_entry_type type){
  spt->file = file;
  spt->offset = offset;
  spt->read_bytes = read_bytes;
  spt->upage = upage;
  spt->writable = writable;
  spt->type = type;
  spt->ft = NULL;
			  
}

void create_stack_page(void *upage) {
  struct sup_table_entry *spt = malloc(sizeof(struct sup_table_entry));  
  if(spt == NULL) {
    thread_exit();
  }
  
  spt->file = NULL;
  spt->offset = 0;
  spt->upage = upage;
  spt->writable = true;
  spt->type = NEW_STACK_PAGE;
  spt->ft = NULL;

  hash_insert(&thread_current()->sup_table, &spt->elem);
}

/* Calculates a hash value based on user page address of e */
static unsigned spt_hash_uaddr(const struct hash_elem *e, void *aux UNUSED) {
  struct sup_table_entry *st = hash_entry(e, struct sup_table_entry, elem);
  
  return (unsigned) st->upage;
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

/* Frees a supplemental page table entry at given page
   Clears any swap space allocated to the provided virtual page
   Removes frame table entry at the same user virtual address if it exists
   To be used in hash_destroy to delete all supplemental page table entries */
static void spt_destroy_entry(struct hash_elem *e, void *aux UNUSED) {
  struct sup_table_entry *spt = hash_entry(e, struct sup_table_entry, elem);

  /* Removes frame table entry of the page if it is in physical memory */
  ft_lock_acquire();
  struct frame_table_entry *ft = spt->ft;
  
  if(ft != NULL) {
    /* If page is a modified file in frame, read it back to the file */
    if(spt->type == IN_SWAP || (spt->type == ZERO_PAGE && ft->modified)) {
      filesys_lock_acquire();
      file_seek(spt->file, spt->offset);
      file_write(spt->file, ft->frame, PGSIZE);
      filesys_lock_release();
    }

    lock_acquire(&ft->owners_lock);
    list_remove(&spt->frame_elem);
    
    if (list_empty(&ft->owners)) {
      ft_remove_entry(ft->frame);
    }
    lock_release(&ft->owners_lock);
  } else {
    /* If page in swap system, read it back to its file and free swap space */
    if(spt->type == IN_SWAP) {
      
      filesys_lock_acquire();
      file_seek(spt->file, spt->offset);
      
      swap_lock_acquire();
      swap_read_file(spt->file, spt->block_number);
      swap_lock_release();

      filesys_lock_release();
    }
    
    if (spt->type == IN_SWAP || spt->type == STACK_PAGE) {
      swap_lock_acquire();
      remove_swap_space(spt->block_number, 1);
      swap_lock_release();
    }
  }

  ft_lock_release();
  
  free(spt);
}
