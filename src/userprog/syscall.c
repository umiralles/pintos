#include <stdio.h>
#include <syscall-nr.h>
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/off_t.h"
#include "filesys/file.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"

static void syscall_handler (struct intr_frame *);

/* SYSTEM CALL FUNCTIONS */
static void syscall_halt(struct intr_frame *f UNUSED) {}
static void syscall_exit(struct intr_frame *f);
static void syscall_exec(struct intr_frame *f);
static void syscall_wait(struct intr_frame *f UNUSED) {}
static void syscall_create(struct intr_frame *f UNUSED) {}
static void syscall_remove(struct intr_frame *f UNUSED) {}
static void syscall_open(struct intr_frame *f UNUSED) {}
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

/* Jump table used to call a syscall */
static syscall_func syscalls[MAX_SYSCALLS] = {&syscall_halt, &syscall_exit,
					      &syscall_exec, &syscall_wait,
					      &syscall_create, &syscall_remove,
					      &syscall_open, &syscall_filesize,
					      &syscall_read, &syscall_write,
					      &syscall_seek, &syscall_tell,
					      &syscall_close};

void syscall_init(void) {
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f) {
  syscall_access_memory(f->esp);
  int32_t call_no = *((int32_t *) f->esp);
  syscalls[call_no](f);
}

static void syscall_exit(struct intr_frame *f) {
  int status = GET_ARGUMENT_VALUE(f, int, 1);

  /* Ups the semaphore and exit_status in its tid_elem
     for if its parent calls process_wait on it */
  struct tid_elem *tid_elem = thread_current()->tid_elem;
  tid_elem->exit_status = status;
  sema_up(&tid_elem->child_semaphore);
  
  return_value_to_frame(f, (uint32_t) status);
  process_exit();
}

// something to do with kernel_mode?
static void syscall_exec(struct intr_frame *f) {
  const char *cmd_line = GET_ARGUMENT_VALUE(f, char *, 1);

  int child_tid = process_execute(cmd_line);

  struct tid_elem *curr;
  struct list_elem *curr_elem = list_begin(&thread_current()->child_tid_list);
  bool match = false;
  while (curr_elem != list_end(&thread_current()->child_tid_list) && !match) {
    curr = list_entry(curr_elem, struct tid_elem, elem);
    
    if (curr->tid == child_tid) {
      match = true;
    }

    curr_elem = list_next(curr_elem);
  }

  if (!match) {
    // some error that exits the program?
  } else {
    sema_down(&curr->child_semaphore);
  }
  
  return_value_to_frame(f, (uint32_t) child_tid);
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
    pagedir_destroy(t->pagedir);
    struct list_elem *e;
    for(e = list_begin (&t->held_locks);
	e != list_end (&t->held_locks);
	e = list_next (e)) {
      struct lock_elem *elem = list_entry(e, struct lock_elem, elem);
      lock_release(elem->lock);
    }
    thread_exit();
  }
}

/* HELPER FUNCTIONS */

static void *get_argument(void *esp, int arg_no) {
  syscall_access_memory(esp + arg_no * 4);
  return  esp + arg_no * 4;
}

static void return_value_to_frame(struct intr_frame *f, uint32_t val) {
  syscall_access_memory(&f->eax);
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

