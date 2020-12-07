#include "threads/synch.h"
#include "vm/mmap.h"
#include <debug.h>

/* MMAP table*/
static struct hash mmap_table;
static struct lock mmap_table_lock;

/* Hash functions for mmap_table */
static hash_hash_func mmap_hash_mapid;
static hash_less_func mmap_cmp_mapid;

/* Initialise sup_table */
void mmap_init(void) {
  hash_init(&mmap_table, mmap_hash_mapid, mmap_cmp_mapid, NULL);
  lock_init(&mmap_table_lock);
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

/* Finds entry corresponding to a given map id in the mmap table; 
   Takes in a map_id to search for; 
   Returns the mmap entry struct if found or NULL otherwise */
struct mmap_entry *mmap_find_entry(mapid_t map_id) {
  struct mmap_entry key;
  key.map_id = map_id;
  
  struct hash_elem *elem = hash_find(&mmap_table, &key.elem);

  if(elem == NULL) {
    return NULL;
  }
  
  return hash_entry(elem, struct mmap_entry, elem);
}


