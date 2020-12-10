#include "vm/mmap.h"

#include <debug.h>

#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "filesys/off_t.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "vm/page.h"

/* Hash functions for mmap_table */
static hash_hash_func mmap_hash_mapid;
static hash_less_func mmap_cmp_mapid;
static hash_action_func mmap_destroy_entry;

/* Initialise sup_table */
void mmap_init(struct hash *mmap_table) {
  hash_init(mmap_table, mmap_hash_mapid, mmap_cmp_mapid, NULL);
}

void mmap_destroy(struct hash *mmap_table) {
  hash_destroy(mmap_table, mmap_destroy_entry);
}

/* Calculates a hash value based on mmap_entry's identifier */
static unsigned mmap_hash_mapid(const struct hash_elem *e, void *aux UNUSED) {
  struct mmap_entry *mm = hash_entry(e, struct mmap_entry, elem);
  
  return hash_int(mm->map_id);
}

/* Compares entries on their map identifiers */ 
static bool mmap_cmp_mapid(const struct hash_elem *a, const struct hash_elem *b,
		   void *aux UNUSED) {
  struct mmap_entry *mm1 = hash_entry(a, struct mmap_entry, elem);
  struct mmap_entry *mm2 = hash_entry(b, struct mmap_entry, elem);
  
  return mm1->map_id < mm2->map_id;
}

/* Inserts hash_elem elem into the mmap_table */
mapid_t mmap_create_entry(struct file *file, void *addr) {
  struct mmap_entry *mmap_entry = malloc(sizeof(struct mmap_entry));

  if(mmap_entry == NULL) {
    return ERROR_CODE;
  }
  create_alloc_elem(mmap_entry, MALLOC_PTR);

  struct thread *t = thread_current();
  
  mmap_entry->map_id = t->next_map_id;
  mmap_entry->file = file;
  mmap_entry->addr = addr;
  hash_insert(&t->mmap_table, &mmap_entry->elem);
  remove_alloc_elem(mmap_entry);

  t->next_map_id++;	

  return mmap_entry->map_id;
}

/* Finds entry corresponding to a given map id in the mmap table; 
   Takes in a map_id to search for; 
   Returns the mmap entry struct if found or NULL otherwise */
struct mmap_entry *mmap_find_entry(mapid_t map_id) {
  struct mmap_entry key;
  key.map_id = map_id;
  
  struct hash_elem *elem = hash_find(&thread_current()->mmap_table, &key.elem);

  if(elem == NULL) {
    return NULL;
  }
  
  return hash_entry(elem, struct mmap_entry, elem);
}

/* Removes a mmap entry from the table and frees it 
   Takes in the map_id of the entry to remove 
   Does nothing if the entry doesn't exist */
void mmap_remove_entry(struct mmap_entry *mmap, bool destroy) {
  
  if(mmap == NULL) {
    return;
  }

  struct thread *t = thread_current();

  filesys_lock_acquire();
  off_t length = file_length(mmap->file);
  filesys_lock_release();
  
  size_t page_read_bytes;
  off_t ofs = 0;
  void *addr = mmap->addr;
  while(length > 0) {
    page_read_bytes = length < PGSIZE ? length : PGSIZE;	

    if(pagedir_is_dirty(t->pagedir, addr)){
      filesys_lock_acquire();
      file_seek(mmap->file, ofs);
      file_write(mmap->file, addr, page_read_bytes);
      filesys_lock_release();
    }    

    spt_remove_entry(mmap->addr);   	

    length -= PGSIZE;
    ofs += PGSIZE;
    addr += PGSIZE;
  }

  filesys_lock_acquire();
  file_close(mmap->file);
  filesys_lock_release();

  if(!destroy) {
    hash_delete(&thread_current()->mmap_table, &mmap->elem);
  }
  
  free(mmap);
}

static void mmap_destroy_entry(struct hash_elem *e, void *aux UNUSED) {
  struct mmap_entry *mmap = hash_entry(e, struct mmap_entry, elem);
  
  mmap_remove_entry(mmap, true);

}
