#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);
static void syscall_acquire_lock(struct lock);
static void syscall_access_memory(const void *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  thread_exit ();
}

/* One lock in a list. */
struct lock_elem {
  struct list_elem elem;              /* List element. */
  struct lock *lock;         		/* This lock. */
};

// needs to be thread-safe!
static void
syscall_acquire_lock(struct lock *lock) {
  // acquire the l;ock somehow and add it to some list
  acquire_lock(lock);
  struct lock_elem lock_elem;
  lock_elem.lock = lock;
  list_push_back(&thread_current()->held_locks, &lock_elem.elem);
}

static void
syscall_access_memory(const void *vaddr) {
  if(!(is_user_vaddr(vaddr) && pagedir_get_page(vaddr))) {
    pagedir_destroy(thread_current()->pagedir);
    struct list_elem *e;
    for (e = list_begin (&thread_current()->held_locks);
		    e != list_end (&thread_current()->held_locks);
		    e = list_next (e)) {
      struct lock_elem *elem = list_entry(e, struct lock_elem, elem);
      lock_release(elem->lock);
    }
    thread_exit();
  }
}

static void
syscall_write() {

}
