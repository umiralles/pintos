#include "vm/mmap.h"

#include <debug.h>

#include "threads/thread.h"
#include "threads/malloc.h"
#include "userprog/syscall.h"
#include "vm/page.h"

/* Hash functions for mmap_table */
static hash_hash_func mmap_hash_mapid;
static hash_less_func mmap_cmp_mapid;

/* Initialise sup_table */
void mmap_init(struct hash *mmap_table) {
  hash_init(mmap_table, mmap_hash_mapid, mmap_cmp_mapid, NULL);
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

  struct thread *t = thread_current();
  
  mmap_entry->map_id = t->next_map_id;
  mmap_entry->file = file;
  mmap_entry->addr = addr;
  hash_insert(&t->mmap_table, &mmap_entry->elem);

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
void mmap_remove_entry(mapid_t map_id) {
  struct mmap_entry *mmap = mmap_find_entry(map_id);

  if(mmap != NULL) {
    filesys_lock_acquire();
    file_close(mmap->file);
    filesys_lock_release();
    
    spt_remove_entry(mmap->addr);    
    
    hash_delete(&thread_current()->mmap_table, &mmap->elem);
    free(mmap);
  }
}