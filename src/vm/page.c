#include "vm/page.h"
#include <debug.h>
#include <stdio.h>
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
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

/* Destroys entire supplemental page table */
void spt_destroy(struct hash *sup_table) {
  hash_destroy(sup_table, spt_destroy_entry);
}

/* Creates a supplemental page table for a file page
   Takes the user virtual address, file, file offset, whether it is writable,
   the number of bytes to read and its page type 
   Returns false if memory allocation fails or duplicate MMAP created */
bool create_file_page(void *upage, struct file *file, off_t offset,
		      bool writable, size_t read_bytes,
		      enum sup_entry_type type) {
		      	      
  struct thread *t = thread_current ();

  /* Check if virtual page already allocated */
  struct sup_table_entry *spt = spt_find_entry(t, upage);
  
  if(spt != NULL) {
    if (type == MMAPPED_PAGE) {
      return false;	
    }
    
    /* Update metadata if loadsegment is loading same page twice */
    size_t new_read_bytes = spt->read_bytes + read_bytes;
    
    if(new_read_bytes > PGSIZE) {
      spt->read_bytes = PGSIZE;
      
      if(!create_file_page(upage + PGSIZE, file, offset + PGSIZE,
                           writable, new_read_bytes - PGSIZE, type)) {
        return false;
      }
    } else {
      spt->read_bytes = new_read_bytes;
    }

    spt->writable = spt->writable || writable;

    if(spt->type == ZERO_PAGE && type != ZERO_PAGE) {
      spt->type = type;
    }
    return true;
  }

  /* Creates new table entry with given values if one cannot be found */
  spt = malloc(sizeof(struct sup_table_entry));

  if(spt == NULL) {
    return false;
  }
  create_alloc_elem(spt, MALLOC_PTR);

  spt->file = file;
  spt->offset = offset;
  spt->read_bytes = read_bytes;
  spt->ft = NULL;
  spt->upage = pg_round_down(upage);
  spt->owner = thread_current();
  spt->writable = writable;
  spt->modified = false;
  spt->accessed = false;
  spt->pinned = false;
  spt->type = type;
  
  hash_insert(&thread_current()->sup_table, &spt->elem);
  remove_alloc_elem(spt);

  return true;
}

/* Creates a supplemental page table for a stack page
   Takes the user virtual address of the stack page */
void create_stack_page(void *upage) {		      
  struct sup_table_entry *spt = malloc(sizeof(struct sup_table_entry));  
  if(spt == NULL) {
    thread_exit();
  }
  create_alloc_elem(spt, MALLOC_PTR);
  
  spt->file = NULL;
  spt->offset = 0;
  spt->upage = pg_round_down(upage);
  spt->owner = thread_current();
  spt->writable = true;
  spt->modified = false;
  spt->type = NEW_STACK_PAGE;
  spt->ft = NULL;

  hash_insert(&thread_current()->sup_table, &spt->elem);
  remove_alloc_elem(spt);
}

/* Calculates a hash value based on user page address of e */
static unsigned spt_hash_uaddr(const struct hash_elem *e, void *aux UNUSED) {
  struct sup_table_entry *spt = hash_entry(e, struct sup_table_entry, elem);
  
  return hash_int((int) spt->upage);
}

/* Compares entries on numerical value of user page address */ 
static bool spt_cmp_uaddr(const struct hash_elem *a, const struct hash_elem *b,
		   void *aux UNUSED) {
  struct sup_table_entry *spt1 = hash_entry(a, struct sup_table_entry, elem);
  struct sup_table_entry *spt2 = hash_entry(b, struct sup_table_entry, elem);
  
  return spt1->upage < spt2->upage;
}

/* Finds entry corresponding to a given page in the supplemental page table 
   Takes in a thread with sup_table to search and a user page to search for 
   Returns the page entry struct if found or NULL otherwise */
struct sup_table_entry *spt_find_entry(struct thread *t, const void *uaddr) {
  struct sup_table_entry key;
  key.upage = pg_round_down(uaddr);
  
  struct hash_elem *elem = hash_find(&t->sup_table, &key.elem);

  if(elem == NULL) {
    return NULL;
  }
  
  return hash_entry(elem, struct sup_table_entry, elem);
}

/* Removes a supplemental page table entry at given page and frees its memory
   Also clears any swap space allocated to the provided virtual page
   Takes in the user page address of the entry to search for 
   Does nothing if the entry cannot be found */
void spt_remove_entry(void *uaddr) {
  struct thread *t = thread_current();
  struct sup_table_entry *spt = spt_find_entry(thread_current(), uaddr);

  if(spt != NULL) {
    hash_delete(&t->sup_table, &spt->elem);
    spt_destroy_entry(&spt->elem, NULL);
  }
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
    if(ft->modified &&
       (spt->type == FILE_PAGE || spt->type == ZERO_PAGE)) {
      filesys_lock_acquire();
      file_seek(spt->file, spt->offset);
      file_write(spt->file, ft->frame, PGSIZE);
      filesys_lock_release();
    }

    /* Remove self from frame's owners list and free if it has no owners */
    //lock_acquire(&ft->owners_lock);
    list_remove(&spt->frame_elem);
    
    if (list_empty(&ft->owners)) {
      ft_remove_entry(ft->frame);
    } else {
      //lock_release(&ft->owners_lock);
    }
  } else {
    /* If page in swap system, read it back to its file and free swap space */
    if(spt->type == IN_SWAP_FILE) {
      
      filesys_lock_acquire();
      file_seek(spt->file, spt->offset);
      
      swap_lock_acquire();
      swap_read_file(spt->file, spt->block_number, spt->read_bytes);
      swap_lock_release();

      filesys_lock_release();
    }
    
    /* Clear swap space if page is found in swap space */
    if (spt->type == IN_SWAP_FILE || spt->type == STACK_PAGE
	|| (spt->type == MMAPPED_PAGE && spt->modified)) {
      swap_lock_acquire();
      remove_swap_space(spt->block_number, 1);
      swap_lock_release();
    }
  }

  ft_lock_release();

  free(spt);
}
