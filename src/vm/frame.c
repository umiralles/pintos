#include "vm/frame.h"
#include <debug.h>

#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/palloc.h"

/* Frame table */
static struct hash frame_table;
static struct lock frame_table_lock;

/* Shared table */
static struct hash shared_table;
static struct lock shared_table_lock;

/* Hash functions for frame_table */
static hash_hash_func hash_user_address;
static hash_less_func cmp_user_address;

/* Hash functions for shared_table */
static hash_hash_func hash_file;
static hash_less_func cmp_file;

/* Initialise frame_table and frame_table_lock */
void ft_init(void) {
  hash_init (&frame_table, hash_user_address, cmp_user_address, NULL);
  lock_init(&frame_table_lock);
}

/* Initialise shared table */
void st_init(void) {
  hash_init(&shared_table, hash_file, cmp_file, NULL);
  lock_init(&shared_table_lock);
}

/* Generates a hash function from the user virtual address of a page 
   hash function is address divided by page size which acts as a page number */
static unsigned hash_user_address(const struct hash_elem *e, void *aux UNUSED) {
  struct frame_table_entry *ft = hash_entry(e, struct frame_table_entry, elem);

  return ((unsigned) ft->uaddr) / PGSIZE;
}

/* Compares two frame table elems based on user address */
static bool cmp_user_address(const struct hash_elem *a,
			     const struct hash_elem *b, void *aux UNUSED) {
  struct frame_table_entry *ft1 = hash_entry(a, struct frame_table_entry, elem);
  struct frame_table_entry *ft2 = hash_entry(b, struct frame_table_entry, elem);

  return ft1->uaddr < ft2->uaddr;
}

/* Generates a hash function from the file pointer address of a shared page */
static unsigned hash_file(const struct hash_elem *e, void *aux UNUSED) {
  struct shared_table_entry *st =
    hash_entry(e, struct shared_table_entry, elem);

  return (unsigned) st->file;
}

/* Compares two shared table elems based on file pointer address */
static bool cmp_file(const struct hash_elem *a,
			     const struct hash_elem *b, void *aux UNUSED) {
  struct shared_table_entry *st1 =
    hash_entry(a, struct shared_table_entry, elem);
  struct shared_table_entry *st2 =
    hash_entry(b, struct shared_table_entry, elem);

  return st1->file < st2->file;
}

/* Inserts hash_elem elem into the frame_table 
   SHOULD BE CALLED WITH THE FRAME TABLE LOCK ACQUIRED */
void ft_insert_entry(struct hash_elem *elem) {
  hash_insert(&frame_table, elem);
}

/* Inserts hash_elem elem into the shared_table 
   SHOULD BE CALLED WITH THE SHARED TABLE LOCK ACQUIRED */
void st_insert_entry(struct hash_elem *elem) {
  hash_insert(&shared_table, elem);
}

/* Gets a frame table entry from its hash table with a matching user address
   Takes in a user virtual address to search for 
   Returns pointer to the table entry or NULL if unsuccessful 
   SHOULD BE CALLED WITH THE FRAME TABLE LOCK ACQUIRED */
struct frame_table_entry *ft_find_entry(const void *uaddr) {
  struct frame_table_entry key;
  
  key.uaddr = pg_round_down(uaddr);

  struct hash_elem *elem = hash_find(&frame_table, &key.elem);

  if(elem == NULL) {
    return NULL;
  }

  return hash_entry(elem, struct frame_table_entry, elem);
}

/* Gets a shared table entry from its hash table with a matching file address
   Takes in a file address to search for 
   Returns pointer to the table entry or NULL if unsuccessful 
   SHOULD BE CALLED WITH THE SHARED TABLE LOCK ACQUIRED */
struct shared_table_entry *st_find_entry(const struct file *file) {
  struct shared_table_entry key;
  
  key.file = file;

  struct hash_elem *elem = hash_find(&shared_table, &key.elem);

  if(elem == NULL) {
    return NULL;
  }

  return hash_entry(elem, struct shared_table_entry, elem);
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

/* Removes a shared table entry from the table and frees it 
   Takes in the file address of the entry to remove 
   Does nothing if the entry doesn't exist 
   SHOULD BE CALLED WITH THE SHARED TABLE LOCK ACQUIRED */
void st_remove_entry(struct file *file) {
  struct shared_table_entry *st = st_find_entry(file);

  if(st != NULL) {
    hash_delete(&shared_table, &st->elem);
    free(st);
  }
}

/* Functions for accessing frame_table_lock */
void ft_lock_acquire(void) {
  lock_acquire(&frame_table_lock);
}

void ft_lock_release(void) {
  lock_release(&frame_table_lock);
}

/* Functions for accessing shared_table_lock */
void st_lock_acquire(void) {
  lock_acquire(&shared_table_lock);
}

void st_lock_release(void) {
  lock_release(&shared_table_lock);
}
