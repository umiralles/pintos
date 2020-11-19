#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "filesys/off_t.h"
#include "filesys/filesys.h"
#include "filesys/directory.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "devices/shutdown.h"
#include "devices/input.h"

static void syscall_handler (struct intr_frame *);

/* SYSTEM CALL FUNCTIONS */
static void syscall_halt(struct intr_frame *f UNUSED);
static void syscall_exit(struct intr_frame *f);
static void syscall_exec(struct intr_frame *f);
static void syscall_wait(struct intr_frame *f);
static void syscall_create(struct intr_frame *f);
static void syscall_remove(struct intr_frame *f);
static void syscall_open(struct intr_frame *f);
static void syscall_filesize(struct intr_frame *f);
static void syscall_read(struct intr_frame *f);
static void syscall_write(struct intr_frame *f);
static void syscall_seek(struct intr_frame *f);
static void syscall_tell(struct intr_frame *f);
static void syscall_close(struct intr_frame *f);

/* MEMORY ACCESS FUNCTION */
static void syscall_access_memory(const void *vaddr);
static void syscall_access_string(const char *str);
static bool check_filename(const char *name);


/* HELPER FUNCTIONS */
static void *get_argument(void *esp, int arg_no);
static void return_value_to_frame(struct intr_frame *f, uint32_t val);
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

 /* Puts exit_status in its tid_elem for if its
    parent calls process_wait on it */
  thread_current()->tid_elem->exit_status = status;

  return_value_to_frame(f, (uint32_t) status);
  thread_exit();
}

static void syscall_exec(struct intr_frame *f) {
  const char *cmd_line = GET_ARGUMENT_VALUE(f, char *, 1);

  syscall_access_string(cmd_line);
  int child_tid = process_execute(cmd_line);

  /* Returns -1 if child process failed to execute due to an error */
  if(child_tid == TID_ERROR) { 
    child_tid = -1;
  }

  return_value_to_frame(f, (uint32_t) child_tid);
}

static void syscall_wait(struct intr_frame *f) {
  tid_t tid = GET_ARGUMENT_VALUE(f, tid_t, 1);
  
  int res = process_wait(tid);
  return_value_to_frame(f, (uint32_t) res);
}

static void syscall_create(struct intr_frame *f) {
  const char *name = GET_ARGUMENT_VALUE(f, char *, 1);
  uint32_t initial_size = GET_ARGUMENT_VALUE(f, uint32_t, 2);
  bool res = false;
  
  if(check_filename(name)) {
    lock_acquire(&filesys_lock);
    res = filesys_create(name, (off_t) initial_size); 
    lock_release(&filesys_lock);
  }
  
  return_value_to_frame(f, (uint32_t) res);
}

static void syscall_remove(struct intr_frame *f) {
  const char *name = GET_ARGUMENT_VALUE(f, char *, 1);
  bool res = false;
  
  if(check_filename(name)) {
    lock_acquire(&filesys_lock);
    res = filesys_remove(name);
    lock_release(&filesys_lock);
  }
  
  return_value_to_frame(f, (uint32_t) res);
}

static void syscall_open(struct intr_frame *f) {
  const char *name = GET_ARGUMENT_VALUE(f, char *, 1);
  int fd = -1;

  if(check_filename(name)) {
    lock_acquire(&filesys_lock);
    struct file *file = filesys_open(name);
    
    if(file != NULL) {
      struct thread *t = thread_current();

      struct file_elem *current_file = malloc(sizeof(struct file_elem));

      /* If process runs out of memory, kill it */
      if(current_file == NULL) {
	lock_release(&filesys_lock);
	thread_exit();
      }      
      fd = t->next_available_fd;
      t->next_available_fd++;

      current_file->fd = fd;
      current_file->file = file;

      list_push_back(&t->files, &current_file->elem);
    } 
    lock_release(&filesys_lock);
  }
  
  return_value_to_frame(f, (uint32_t) fd);  
}

/* Returns either the size of a file in bytes 
   or -1 if the file cannot be accessed */
static void syscall_filesize(struct intr_frame *f) {
  int fd = GET_ARGUMENT_VALUE(f, int, 1);
  int filesize = -1;
  struct thread *t = thread_current();

  struct file_elem *file = get_file(t, fd);

  /* If a file is found, get its size */
  if(file != NULL) {
    lock_acquire(&filesys_lock);
    filesize = file_length(file->file);
    lock_release(&filesys_lock);
  }
  
  return_value_to_frame(f, (uint32_t) filesize);
}

static void syscall_read(struct intr_frame *f) {
  int fd = GET_ARGUMENT_VALUE(f, int, 1);
  uint8_t *buffer = (uint8_t *) GET_ARGUMENT_VALUE(f, void *, 2);
  unsigned size = GET_ARGUMENT_VALUE(f, unsigned, 3);
  struct thread *t = thread_current();
  
  int bytes_read = -1;

  /* Check entire buffer is in valid memory */
  syscall_access_memory(buffer);
  syscall_access_memory(buffer + size);
  
  if(fd == STDIN_FILENO) {
    uint8_t key;
    bytes_read = 0;
    for(unsigned i = 0; i < size; i++) {
      key = input_getc();
      *buffer = key;
      
      buffer++;
      bytes_read++;
    }
  } else {
    struct file_elem *file = get_file(t, fd);
    if(file != NULL) {
      lock_acquire(&filesys_lock);
      bytes_read = (int) file_read(file->file, buffer, (off_t) size);
      lock_release(&filesys_lock);
    }
  }

  return_value_to_frame(f, (uint32_t) bytes_read);
}

static void syscall_write(struct intr_frame *f) {
  int fd = GET_ARGUMENT_VALUE(f, int, 1);
  const void *buffer = GET_ARGUMENT_VALUE(f, void *, 2);
  unsigned size = GET_ARGUMENT_VALUE(f, unsigned, 3);
  int bytes_written = -1;

  struct thread *t = thread_current();

  /* Checks entire buffer is in valid memory */
  syscall_access_memory(buffer);
  syscall_access_memory(buffer + size);
  
  if(fd == STDOUT_FILENO) {
    putbuf(buffer, size);
    bytes_written = (off_t) size;
    
  } else {
    struct file_elem *file_elem = get_file(t, fd);

    if (file_elem != NULL) {
      lock_acquire(&filesys_lock);
      bytes_written = file_write(file_elem->file, buffer, (off_t) size);
      lock_release(&filesys_lock);
    }
  }
  
  return_value_to_frame(f, (uint32_t) bytes_written);
}

static void syscall_seek(struct intr_frame *f) {
  int fd = GET_ARGUMENT_VALUE(f, int, 1);
  unsigned position = GET_ARGUMENT_VALUE(f, unsigned, 2);

  struct thread *t = thread_current();

  struct file_elem *file = get_file(t, fd);

  /* If a file is found, set its position to the position argument */
  if(file != NULL) {
    lock_acquire(&filesys_lock);
    file_seek(file->file, (off_t) position);
    lock_release(&filesys_lock);
  }
}

static void syscall_tell(struct intr_frame *f) {
  int fd = GET_ARGUMENT_VALUE(f, int, 1);
  unsigned position = -1;

  struct thread *t = thread_current();

  struct file_elem *file = get_file(t, fd);

  /* If a file is found, get next byte to be read */
  if(file != NULL) {
    lock_acquire(&filesys_lock);
    position = (unsigned) file_tell(file->file);
    lock_release(&filesys_lock);
  }
  
  return_value_to_frame(f, (uint32_t) position);
}

static void syscall_close(struct intr_frame *f) {
  int fd = GET_ARGUMENT_VALUE(f, int, 1);
  struct thread *t = thread_current();

  struct file_elem *file = get_file(t, fd);

  if(file != NULL) {
    lock_acquire(&filesys_lock);
    file_close(file->file);
    lock_release(&filesys_lock);
    
    /* Remove file_elem struct from list of files and
       free allocated memory */
    list_remove(&file->elem);
    free(file);

  } else {
    thread_exit();
  }
}

/* MEMORY ACCESS FUNCTION
   Release any acquired locks before calling syscall_access_memory.
   Checks validity of any user supplied pointer
   A valid pointer is one that is in user space and on an allocated page */
static void syscall_access_memory(const void *vaddr) {
  struct thread *t = thread_current();
  if(!(is_user_vaddr(vaddr) && pagedir_get_page(t->pagedir, vaddr))) {
    thread_exit();
  }
}

/* Checks validity and length of a filename */
static bool check_filename(const char *name) {
  const char *curr = name;
  int i = 0;
  
  do {
    syscall_access_memory(curr);
    i++;
  } while(*(curr++) != '\0' && i < NAME_MAX);

  /* File name is too long and will break the file system */
  if(i == NAME_MAX) {
    return false;
  }
  
  return true;
}

/* Checks validity of all char addresses in a string */
static void syscall_access_string(const char *str) {
  const char *curr = str;

  do {
    syscall_access_memory(curr);
  } while(*(curr++) != '\0');
}

/* HELPER FUNCTIONS */

static void *get_argument(void *esp, int arg_no) {
  syscall_access_memory(esp + arg_no * 4);
  return  esp + arg_no * 4;
}

static void return_value_to_frame(struct intr_frame *f, uint32_t val) {
  f->eax = val;
}

/* Takes in a thread and a file descriptor
   Returns the file with file descriptor equal to fd
   Returns NULL if no such file could be found */
static struct file_elem* get_file(struct thread *t, int fd) {
  if(fd <= STDOUT_FILENO) {
    return NULL;
  }

  /* fd cannot exist so we short circuit the list
     traversal and return null */
  if(fd >= t->next_available_fd) {
    return NULL;
  }

  struct file_elem *current;
  struct list_elem *e;

  /* Attempt to find a matching fd in the thread's files list */
  for(e = list_begin(&t->files); e != list_end(&t->files); e = list_next(e)) {
    current = list_entry(e, struct file_elem, elem);
    if(current->fd == fd) {
      return current;
    }
  }

  /* Nothing found */
  return NULL; 
}
