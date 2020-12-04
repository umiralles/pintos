#include "vm/frame.h"
#include <debug.h>

#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/palloc.h"

/* Frame table */
static struct hash frame_table;
static struct lock frame_table_lock;

/* Hash functions for frame_table */
static hash_hash_func hash_user_address;
static hash_less_func cmp_timestamp;

void ft_init(void) {
  hash_init (&frame_table, hash_user_address, cmp_timestamp, NULL);
  lock_init(&frame_table_lock);
}

/* Generates a hash function from the user virtual address of a page 
   hash function is address divided by page size which acts as a page number */
static unsigned hash_user_address(const struct hash_elem *e, void *aux UNUSED) {
  struct frame_table_entry *ft = hash_entry(e, struct frame_table_entry, elem);

  return ((unsigned) ft->frame) / PGSIZE;
}

/* Compares two frame table elems based on timestamp they were added in */
static bool cmp_timestamp(const struct hash_elem *a, const struct hash_elem *b,
		   void *aux UNUSED) {
  struct frame_table_entry *ft1 = hash_entry(a, struct frame_table_entry, elem);
  struct frame_table_entry *ft2 = hash_entry(b, struct frame_table_entry, elem);

  return ft1->timestamp < ft2->timestamp;
}

/* Inserts hash_elem elem into the frame_table */
void ft_insert_entry(struct hash_elem *elem) {
  hash_insert(&frame_table, elem);
}

/* Gets a frame table entry from its hash table with a matching user address
   Takes in a user virtual address to search for 
   Returns pointer to the table entry or NULL if unsuccessful 
   SHOULD BE CALLED WITH THE FRAME TABLE LOCK ACQUIRED */
struct frame_table_entry *ft_find_entry(void *uaddr) {
  struct frame_table_entry key;
  
  key.uaddr = pg_round_down(uaddr);

  struct hash_elem *elem = hash_find(&frame_table, &key.elem);

  if(elem == NULL) {
    return NULL;
  }

  return hash_entry(elem, struct frame_table_entry, elem);
}

/* Removes a frame table entry from the table and frees it 
   Takes in the user virtual address of the entry to remove 
   Does nothing if the entry doesn't exist 
   SHOULD BE CALLED WITH THE FRAME TABLE LOCK ACQUIRED */
void ft_remove_entry(void *uaddr) {
  struct frame_table_entry *ft = ft_find_entry(uaddr);

  if(ft != NULL) {
    hash_delete(&frame_table, &ft->elem);
    free(ft);
  }
}

/* Functions for accessing frame_table_lock */
void ft_lock_acquire(void) {
  lock_acquire(&frame_table_lock);
}

void ft_lock_release(void) {
  lock_release(&frame_table_lock);
}
