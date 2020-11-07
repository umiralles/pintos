#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/thread.h"
#include "filesys/off_t.h"
#include "filesys/file.h"
#include "pagedir.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);
static void syscall_acquire_lock(struct lock *);

/* SYSTEM CALL FUNCTIONS */
static void halt(struct intr_frame *f UNUSED) {}
static void exit(struct intr_frame *f);
static void exec(struct intr_frame *f UNUSED) {}
static void wait(struct intr_frame *f UNUSED) {}
static void create(struct intr_frame *f UNUSED) {}
static void remove(struct intr_frame *f UNUSED) {}
static void open(struct intr_frame *f UNUSED) {}
static void filesize(struct intr_frame *f UNUSED){}
static void read(struct intr_frame *f UNUSED) {}
static void write(struct intr_frame *f);
static void seek(struct intr_frame *f UNUSED) {}
static void tell(struct intr_frame *f UNUSED) {}
static void close(struct intr_frame *f UNUSED) {}

/* MEMORY ACCESS FUNCTION */
static void syscall_access_memory(const void *vaddr);

static syscall_func syscalls[MAX_SYSCALLS] = {&halt, &exit, &exec, &wait,
					       &create, &remove, &open,
					       &filesize, &read, &write, &seek,
					       &tell, &close};

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void *get_argument(void *esp, int arg_no) {
  syscall_access_memory(esp + arg_no * 4);
  return  esp + arg_no * 4;
}

static void return_value_to_frame(struct intr_frame *f, uint32_t val) {
  syscall_access_memory(&f->eax);
  f->eax = val;
}

static void
syscall_handler (struct intr_frame *f) 
{
  syscall_access_memory(f->esp);
  int32_t call_no = *((int32_t *) f->esp);
  syscalls[call_no](f);
}

static void exit(struct intr_frame *f) {
  int status = *((int *) get_argument(f->esp, 1));
  return_value_to_frame(f, (uint32_t) status);
  thread_exit();
}

/* One lock in a list. */
struct lock_elem {
  struct list_elem elem;              /* List element. */
  struct lock *lock;         	      /* This lock. */
};

// needs to be thread-safe!
// not sure if this is needed, but we need some way of finding which locks are held by the thread
static void
syscall_acquire_lock(struct lock *lock) {
  // acquire the lock and add it to a list
  lock_acquire(lock);
  struct lock_elem lock_elem;
  lock_elem.lock = lock;
  list_push_back(&thread_current()->held_locks, &lock_elem.elem);
}

static void
syscall_access_memory(const void *vaddr) {
  struct thread *t = thread_current();
  if(!(is_user_vaddr(vaddr) && pagedir_get_page(t->pagedir, vaddr))) {
    pagedir_destroy(t->pagedir);
    struct list_elem *e;
    for (e = list_begin (&t->held_locks);
		    e != list_end (&t->held_locks);
		    e = list_next (e)) {
      struct lock_elem *elem = list_entry(e, struct lock_elem, elem);
      lock_release(elem->lock);
    }
    thread_exit();
  }
}

struct file_elem {
  int fd;
  struct file *file;
  struct list_elem elem;
};

static void write(struct intr_frame *f){
  int fd = GET_ARGUMENT_VALUE(f, int, 1);
  const void *buffer = GET_ARGUMENT_VALUE(f, void *, 2);
  unsigned size = GET_ARGUMENT_VALUE(f, unsigned, 3);
  off_t bytes_written;
  if(fd == STDOUT_FILENO) {
    putbuf(buffer, size);
    bytes_written = (off_t) size;
  } else {
    struct list_elem *e = list_begin(&thread_current()->files);	  
    struct file_elem *file_elem = list_entry(e, struct file_elem, elem);
    while(file_elem->fd != fd) {
      e = list_next(e);
      file_elem = list_entry(e, struct file_elem, elem);
    }
    syscall_access_memory(file_elem->file); 
    bytes_written = file_write(file_elem->file, buffer, (off_t) size);
  }
  f->eax = bytes_written;
}
