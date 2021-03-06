#include "vm/frame.h"
#include <debug.h>

#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "userprog/exception.h"

/* Frame table */
static struct hash frame_table;
static struct lock frame_table_lock;

extern struct lock allocation_lock;

/* Shared table */
static struct hash shared_table;
static struct lock shared_table_lock;

/* Hash functions for frame_table */
static hash_hash_func hash_frame_address;
static hash_less_func cmp_frame_address;
static hash_action_func reset_reference_bit;

/* Hash functions for shared_table */
static hash_hash_func hash_file;
static hash_less_func cmp_file;

/* Initialise frame_table and frame_table_lock */
void ft_init(void) {
  hash_init(&frame_table, hash_frame_address, cmp_frame_address, NULL);
  lock_init(&frame_table_lock);
}

/* Initialise shared table */
void st_init(void) {
  hash_init(&shared_table, hash_file, cmp_file, NULL);
  lock_init(&shared_table_lock);
}

/* Generates a hash function from the user virtual address of a page 
   hash function is address divided by page size which acts as a page number */
static unsigned hash_frame_address(const struct hash_elem *e,
				   void *aux UNUSED) {
  struct frame_table_entry *ft = hash_entry(e, struct frame_table_entry, elem);

  return ((unsigned) ft->frame) / PGSIZE;
}

/* Compares two frame table elems based on user address */
static bool cmp_frame_address(const struct hash_elem *a,
			     const struct hash_elem *b, void *aux UNUSED) {
  struct frame_table_entry *ft1 = hash_entry(a, struct frame_table_entry, elem);
  struct frame_table_entry *ft2 = hash_entry(b, struct frame_table_entry, elem);

  return ft1->frame < ft2->frame;
}

/* Generates a hash function from the file pointer address of a shared page */
static unsigned hash_file(const struct hash_elem *e, void *aux UNUSED) {
  struct shared_table_entry *st =
    hash_entry(e, struct shared_table_entry, elem);

  return (unsigned) st->file + st->offset;
}

/* Compares two shared table elems based on file pointer address */
static bool cmp_file(const struct hash_elem *a,
			     const struct hash_elem *b, void *aux UNUSED) {
  struct shared_table_entry *st1 =
    hash_entry(a, struct shared_table_entry, elem);
  struct shared_table_entry *st2 =
    hash_entry(b, struct shared_table_entry, elem);

  return ((off_t) st1->file) + st1->offset < ((off_t)st2->file) + st2->offset;
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
struct frame_table_entry *ft_find_entry(const void *frame) {
  struct frame_table_entry key;
  
  key.frame = pg_round_down(frame);

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
struct shared_table_entry *st_find_entry(const struct file *file,
					 off_t offset) {
  struct shared_table_entry key;
  
  key.file = file;
  key.offset = offset;

  struct hash_elem *elem = hash_find(&shared_table, &key.elem);

  if(elem == NULL) {
    return NULL;
  }

  return hash_entry(elem, struct shared_table_entry, elem);
}

/* Finds a frame to evict and returns it 
   MUST BE CALLED WITH THE FRAME TABLE LOCK */
struct frame_table_entry *ft_get_victim(void) {
  struct hash_iterator iterator;
  struct hash_elem *cur;
  struct frame_table_entry *ft;
  
  hash_first(&iterator, &frame_table);

  bool victim_found = false;
  
  while (!victim_found && hash_next(&iterator)) {
    cur = hash_cur(&iterator);
    ft = hash_entry(cur, struct frame_table_entry, elem);
    victim_found = !ft->reference_bit && !ft->pinned;
  }

  /* Now check again for any unpinned page */
  if (!victim_found) {
    hash_first(&iterator, &frame_table);

    while (!victim_found && hash_next(&iterator)) {
      cur = hash_cur(&iterator);
      ft = hash_entry(cur, struct frame_table_entry, elem);
      victim_found = !ft->pinned;
    }

    /* All frames are pinned, swap cannot happen */
    if (!victim_found) {
      ft_lock_release();
      thread_exit();
    }
    return hash_entry(cur, struct frame_table_entry, elem);
  } else {
    return ft;
  }
}

/* Hash action func that resets a reference bit to 0 */
static void reset_reference_bit(struct hash_elem *e, void *aux UNUSED) {
  struct frame_table_entry *ft = hash_entry(e, struct frame_table_entry, elem);

  ft->reference_bit = 0;
}

/* Resets all reference bits in the frame table to 0 */
void ft_reset_reference_bits(void) {
  hash_apply(&frame_table, reset_reference_bit);
}

/* Removes a frame table entry from the table and frees it 
   Takes in the user virtual address of the entry to remove 
   Does nothing if the entry doesn't exist 
   SHOULD BE CALLED WITH THE FRAME TABLE
   AND OWNERS LIST LOCKS ACQUIRED */
void ft_remove_entry(void *frame) {
  struct frame_table_entry *ft = ft_find_entry(frame);
  
  //ASSERT(lock_held_by_current_thread(&ft->owners_lock));
  
  if(ft != NULL) {
    struct hash_iterator iterator;
    struct shared_table_entry *st;
    
    hash_first(&iterator, &shared_table);
    bool found = false;
    
    st_lock_acquire();
    while (!found && hash_next(&iterator)) {
      st = hash_entry(hash_cur(&iterator), struct shared_table_entry, elem);
      if (ft == st->ft) {
	hash_delete(&shared_table, &st->elem);
	free(st);
	found = true;
      }
    }
    st_lock_release(); 
    hash_delete(&frame_table, &ft->elem);
    //lock_release(&ft->owners_lock);
    free(ft);
  }
}

/* Removes a shared table entry from the table and frees it 
   Takes in the file address of the entry to remove 
   Does nothing if the entry doesn't exist 
   SHOULD BE CALLED WITH THE SHARED TABLE LOCK ACQUIRED */
void st_remove_entry(struct file *file, off_t offset) {
  struct shared_table_entry *st = st_find_entry(file, offset);

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

bool ft_lock_held_by_current_thread(void) {
  return lock_held_by_current_thread(&frame_table_lock);
}

/* Functions for accessing shared_table_lock */
void st_lock_acquire(void) {
  lock_acquire(&shared_table_lock);
}

void st_lock_release(void) {
  lock_release(&shared_table_lock);
}

bool st_lock_held_by_current_thread(void) {
  return lock_held_by_current_thread(&shared_table_lock);
}

/* Pins a buffer at uaddr of size size so its frames cannot be evicted 
   Takes the start address of the buffer and its size */
void ft_pin(const void *uaddr, unsigned size) {
  ASSERT (is_user_vaddr(uaddr));
    
  struct thread *t = thread_current();
  struct sup_table_entry *spt;
  while(size > 0) {
    
    spt = spt_find_entry(t, uaddr);
    spt = grow_stack(uaddr, spt);
    
    if(spt == NULL) {
      thread_exit();
    }

    if(spt->ft) {
      spt->ft->pinned = true;
    }

    spt->pinned = true;
    uaddr += PGSIZE;
    if(size < PGSIZE){
      size = 0;
    } else {
      size -= PGSIZE;
    }
  }
}

/* Unpins a buffer at uaddr of size size so its frames can be evicted 
   Takes the start address of the buffer and its size */
void ft_unpin(const void *uaddr, unsigned size) {
  ASSERT (is_user_vaddr(uaddr));

  struct thread *t = thread_current();
  struct sup_table_entry *spt;
  while(size > 0) {
    
    spt = spt_find_entry(t, uaddr);
    spt = grow_stack(uaddr, spt);

    if(spt == NULL) {
      thread_exit();
    }

    if(spt->ft) {
      spt->ft->pinned = false;
    }
    
    spt->pinned = false;
    uaddr += PGSIZE;
    if(size < PGSIZE){
      size = 0;
    } else {
      size -= PGSIZE;
    }
  }
}
