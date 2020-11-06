#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/thread.h"
#include "filesys/off_t.h"
#include "filesys/file.h"

static void syscall_handler (struct intr_frame *);

/* SYSTEM CALL FUNCTIONS */
static void halt(struct intr_frame *f) {}
static void exit(struct intr_frame *f);
static void exec(struct intr_frame *f) {}
static void wait(struct intr_frame *f) {}
static void create(struct intr_frame *f) {}
static void remove(struct intr_frame *f) {}
static void open(struct intr_frame *f) {}
static void filesize(struct intr_frame *f){}
static void read(struct intr_frame *f) {}
static void write(struct intr_frame *f);
static void seek(struct intr_frame *f) {}
static void tell(struct intr_frame *f) {}
static void close(struct intr_frame *f) {}

/* MEMORY ACCESS FUNCTION */
static void syscall_access_memory(const void *f) {}

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
  return  esp + arg_no * 4;
}

static void
syscall_handler (struct intr_frame *f) 
{
  //check user mem access
  int32_t call_no = *((int32_t *) f->esp);
  syscalls[call_no](f);
}

static void exit(struct intr_frame *f) {
  int status = (int) get_argument(f->esp, 1);
  f->eax = (uint32_t) status;
  thread_exit();
}

struct file_elem {
  struct list_elem files_elem;
  struct file *file;
  int fd;
};

static void write(struct intr_frame *f){
  int fd = (int) get_argument(f->esp, 1);
  const void *buffer = get_argument(f->esp, 2);
  unsigned size = (unsigned) get_argument(f->esp, 3);
  off_t bytes_written;
  if(fd == 1) {
    putbuf(buffer, size);
    bytes_written = (off_t) size;
  } else {
    struct list_elem *e = list_begin(&thread_current()->files);	  
    struct file_elem *file_elem = list_entry(e, struct file_elem, files_elem);
    while(file_elem->fd != fd) {
      e = list_next(e);
      file_elem = list_entry(e, struct file_elem, files_elem);
    }
    syscall_access_memory(file_elem->file); 
    bytes_written = file_write(file_elem->file, buffer, (off_t) size);
  }
  f->eax = bytes_written;
}
