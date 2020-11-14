#include <stdio.h>
#include <syscall-nr.h>
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "filesys/off_t.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);

/* SYSTEM CALL FUNCTIONS */
static void syscall_halt(struct intr_frame *f UNUSED);
static void syscall_exit(struct intr_frame *f);
static void syscall_exec(struct intr_frame *f UNUSED) {}
static void syscall_wait(struct intr_frame *f UNUSED) {}
static void syscall_create(struct intr_frame *f);
static void syscall_remove(struct intr_frame *f);
static void syscall_open(struct intr_frame *f);
static void syscall_filesize(struct intr_frame *f UNUSED) {}
static void syscall_read(struct intr_frame *f UNUSED) {}
static void syscall_write(struct intr_frame *f);
static void syscall_seek(struct intr_frame *f UNUSED) {}
static void syscall_tell(struct intr_frame *f UNUSED) {}
static void syscall_close(struct intr_frame *f UNUSED) {}

/* MEMORY ACCESS FUNCTION */
static void syscall_access_memory(const void *vaddr);

/* HELPER FUNCTIONS */
static void *get_argument(void *esp, int arg_no);
static void return_value_to_frame(struct intr_frame *f, uint32_t val);
static void syscall_acquire_lock(struct lock *);
static void syscall_release_lock(struct lock *);

/* Jump table used to call a syscall */
static syscall_func syscalls[MAX_SYSCALLS] = {&syscall_halt, &syscall_exit,
					      &syscall_exec, &syscall_wait,
					      &syscall_create, &syscall_remove,
					      &syscall_open, &syscall_filesize,
					      &syscall_read, &syscall_write,
					      &syscall_seek, &syscall_tell,
					      &syscall_close};

/* Lock used to control access to file system */
static struct lock filesys_lock;

void syscall_init(void) {
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}

static void syscall_handler(struct intr_frame *f) {
  syscall_access_memory(f->esp);
  int32_t call_no = *((int32_t *) f->esp);
  syscalls[call_no](f);
}

static void syscall_halt(struct intr_frame *f UNUSED) {
  shutdown_power_off();  
}

static void syscall_exit(struct intr_frame *f) {
  int status = GET_ARGUMENT_VALUE(f, int, 1);
  return_value_to_frame(f, (uint32_t) status);
  thread_exit();
}

static void syscall_create(struct intr_frame *f) {
  const char *file = GET_ARGUMENT_VALUE(f, char *, 1);
  uint32_t initial_size = GET_ARGUMENT_VALUE(f, uint32_t, 2);

  syscall_acquire_lock(&filesys_lock);
  bool res = filesys_create(file, (off_t) initial_size); 
  syscall_release_lock(&filesys_lock);
  
  return_value_to_frame(f, (uint32_t) res);
}

static void syscall_remove(struct intr_frame *f) {
  const char *file = GET_ARGUMENT_VALUE(f, char *, 1);

  syscall_acquire_lock(&filesys_lock);
  bool res = filesys_remove(file);
  syscall_release_lock(&filesys_lock);

  return_value_to_frame(f, (uint32_t) res);
}

static void syscall_open(struct intr_frame *f) {
  const char *name = GET_ARGUMENT_VALUE(f, char *, 1);
  int fd = -1;

  syscall_acquire_lock(&filesys_lock);
  struct file *file = filesys_open(name);

  if(file != NULL) {
    struct thread *t = thread_current();

    /* TODO: free this memory! 
       will be done in process_exit when we merge with other team 
    */
    struct file_elem *current_file = malloc(sizeof(struct file_elem));

    /* If process runs out of memory, kill it */
    if(current_file == NULL) {
      thread_exit();
    }
    
    fd = t->next_available_fd;
    t->next_available_fd++;

    current_file->fd = fd;
    current_file->file = file;

    list_push_back(&t->files, &current_file->elem);
  }

  syscall_release_lock(&filesys_lock);

  return_value_to_frame(f, (uint32_t) fd);  
}

static void syscall_write(struct intr_frame *f) {
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

/* MEMORY ACCESS FUNCTION */

static void syscall_access_memory(const void *vaddr) {
  struct thread *t = thread_current();
  if(!(is_user_vaddr(vaddr) && pagedir_get_page(t->pagedir, vaddr))) {
    struct list_elem *e;
    for(e = list_begin (&t->held_locks);
	e != list_end (&t->held_locks);
	e = list_next (e)) {
      struct lock_elem *elem = list_entry(e, struct lock_elem, elem);
      lock_release(elem->lock);
    }
    pagedir_destroy(t->pagedir);  
    thread_exit();
  }
}

/* HELPER FUNCTIONS */

static void *get_argument(void *esp, int arg_no) {
  syscall_access_memory(esp + arg_no * 4);
  return  esp + arg_no * 4;
}

static void return_value_to_frame(struct intr_frame *f, uint32_t val) {
  f->eax = val;
}

/* Needs to be thread-safe!
   Not sure if this is needed, but we need some way of finding
   which locks are held by the thread */
static void syscall_acquire_lock(struct lock *lock) {
  // acquire the lock and add it to a list
  lock_acquire(lock);
  struct lock_elem lock_elem;
  lock_elem.lock = lock;
  list_push_back(&thread_current()->held_locks, &lock_elem.elem);
}

//TODO: make this edit the list of thread's held locks
static void syscall_release_lock(struct lock *lock) {
  lock_release(lock);
}
