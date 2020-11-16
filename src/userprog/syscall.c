#include <stdio.h>
#include <syscall-nr.h>
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "filesys/off_t.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);

/* SYSTEM CALL FUNCTIONS */
static void syscall_halt(struct intr_frame *f UNUSED);
static void syscall_exit(struct intr_frame *f);
static void syscall_exec(struct intr_frame *f);
static void syscall_wait(struct intr_frame *f UNUSED) {}
static void syscall_create(struct intr_frame *f);
static void syscall_remove(struct intr_frame *f);
static void syscall_open(struct intr_frame *f);
static void syscall_filesize(struct intr_frame *f);
static void syscall_read(struct intr_frame *f UNUSED) {}
static void syscall_write(struct intr_frame *f);
static void syscall_seek(struct intr_frame *f);
static void syscall_tell(struct intr_frame *f);
static void syscall_close(struct intr_frame *f UNUSED) {}

/* MEMORY ACCESS FUNCTION */
static void syscall_access_memory(const void *vaddr);

/* HELPER FUNCTIONS */
static void *get_argument(void *esp, int arg_no);
static void return_value_to_frame(struct intr_frame *f, uint32_t val);
static void syscall_acquire_lock(struct lock *);
static void syscall_release_lock(struct lock *);
static struct file_elem* get_file(struct thread *t, int fd);

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

 /* Ups the semaphore and exit_status in its tid_elem
     for if its parent calls process_wait on it */
  lock_acquire(&thread_current()->tid_elem->tid_elem_lock);
  struct tid_elem *tid_elem = thread_current()->tid_elem;
  if(tid_elem->process_dead) {
    lock_release(&thread_current()->tid_elem->tid_elem_lock);
    free(tid_elem);
  } else {
    tid_elem->exit_status = status;
    tid_elem->process_dead = true;
    sema_up(&tid_elem->child_semaphore);
    lock_release(&thread_current()->tid_elem->tid_elem_lock);
  }

  return_value_to_frame(f, (uint32_t) status);
  process_exit();
}

// something to do with kernel_mode?
static void syscall_exec(struct intr_frame *f) {
  const char *cmd_line = GET_ARGUMENT_VALUE(f, char *, 1);

  int child_tid = process_execute(cmd_line);

  // need check for TID_ERROR?

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

/* Returns either the size of a file in bytes 
   or -1 if the file cannot be accessed
 */
static void syscall_filesize(struct intr_frame *f) {
  int fd = GET_ARGUMENT_VALUE(f, int, 1);
  int filesize = -1;
  struct thread *t = thread_current();

  struct file_elem *file = get_file(t, fd);

  /* If a file is found, get its size */
  if(file != NULL) {
    syscall_acquire_lock(&filesys_lock);
    filesize = file_length(file->file);
    syscall_release_lock(&filesys_lock);
  }
  
  return_value_to_frame(f, (uint32_t) filesize);
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

static void syscall_seek(struct intr_frame *f) {
  int fd = GET_ARGUMENT_VALUE(f, int, 1);
  unsigned position = GET_ARGUMENT_VALUE(f, unsigned, 2);

  struct thread *t = thread_current();

  struct file_elem *file = get_file(t, fd);

  /* If a file is found, set its position to the position argument */
  if(file != NULL) {
    syscall_acquire_lock(&filesys_lock);
    file_seek(file->file, (off_t) position);
    syscall_release_lock(&filesys_lock);
  }
}

static void syscall_tell(struct intr_frame *f) {
  int fd = GET_ARGUMENT_VALUE(f, int, 1);
  unsigned position = -1;

  struct thread *t = thread_current();

  struct file_elem *file = get_file(t, fd);

  /* If a file is found, get next byte to be read */
  if(file != NULL) {
    syscall_acquire_lock(&filesys_lock);
    position = (unsigned) file_tell(file->file);
    syscall_release_lock(&filesys_lock);
  }
  
  return_value_to_frame(f, (uint32_t) position);
}

/* MEMORY ACCESS FUNCTION */

static void syscall_access_memory(const void *vaddr) {
  struct thread *t = thread_current();
  if(!(is_user_vaddr(vaddr) && pagedir_get_page(t->pagedir, vaddr))) {
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

/* Takes in a thread and a file descriptor
   Returns the file with file descriptor equal to fd
   Returns NULL if no such file could be found
*/
static struct file_elem* get_file(struct thread *t, int fd) {
  if(fd <= STDOUT_FILENO) {
    return NULL;
  }

  /* fd cannot exist so we short circuit the list traversal and return null */
  if(fd >= t->next_available_fd) {
    return NULL;
  }

  struct file_elem *current;
  struct list_elem *e;

  /* Attempt to find a matching fd in the thread's files list */
  for(e = list_begin(&t->files); e != list_end(&t->files); e = list_next(e)) {
    current = list_entry(e, struct file_elem, elem);
    if (current->fd == fd) {
      return current;
    }
  }

  /* Nothing found */
  return NULL;
  
}
