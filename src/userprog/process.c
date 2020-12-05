#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hash.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/timer.h"
#include "vm/frame.h"
#include "vm/page.h"
//#include "vm/swap.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);
static void push_word_to_stack(struct intr_frame *if_, int32_t val);
static void clean_arguments(struct list *arg_list, void *arg_page);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;
  
  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if(fn_copy == NULL)
    return TID_ERROR;
  strlcpy(fn_copy, file_name, PGSIZE);
  
  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create(file_name, PRI_DEFAULT, start_process, fn_copy);
 
  if(tid == TID_ERROR) {
    palloc_free_page(fn_copy);
  }

  /* Find the newly created child in the child_tid_list and sema_down
     the child_semaphore waiting for it to load it's executable */
  struct tid_elem *curr;
  struct list_elem *curr_elem = list_begin(&thread_current()->child_tid_list);
  bool match = false;
  while(curr_elem != list_end(&thread_current()->child_tid_list) && !match) {
    curr = list_entry(curr_elem, struct tid_elem, elem);

    if(curr->tid == tid) {
      match = true;
    }
    curr_elem = list_next(curr_elem);
  }

  if(match) { 
    sema_down(&curr->child_semaphore);
  
    /* If the child process has failed to execute returns TID_ERROR */
    if(curr->has_faulted) {
      tid = TID_ERROR;
    }
  }
  return tid;
}

/* Used in start_process to keep track of the parsed arguments */
struct argument {
  char *arg;                 /* Tokenised argument from the command line */
  struct list_elem arg_elem; /* Places argument in global list of arguments */
};

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;

  char *save_ptr;
  char *arg_page, *next_arg_location; 
  struct list arg_list;
  
  list_init(&arg_list);

  /* Create a page to keep track of the tokenised arguments. */
  arg_page = palloc_get_page (0);
  if(arg_page == NULL) {
    thread_current()->tid_elem->has_faulted = true;
    
    /* Sema up child_semaphore to let parent continue at failure */
    sema_up(&thread_current()->tid_elem->child_semaphore);
    thread_exit();
  }

  char *token;
  next_arg_location = arg_page;

  struct argument *current_arg;
  
  for(token = strtok_r (file_name, " ", &save_ptr); token != NULL;
      token = strtok_r (NULL, " ", &save_ptr)) {
    current_arg = malloc(sizeof(struct argument));

    if(current_arg == NULL) {
      thread_current()->tid_elem->has_faulted = true;

      /* Sema up child_semaphore to let parent continue at failure */
      sema_up(&thread_current()->tid_elem->child_semaphore);
      
      clean_arguments(&arg_list, arg_page);
      thread_exit();
    }
        
    current_arg->arg = next_arg_location;    
    strlcpy(current_arg->arg, token, PGSIZE);
    
    list_push_back(&arg_list, &current_arg->arg_elem);
    
    next_arg_location += strlen(token) + 1;
  }

  char *arg1 = list_entry(list_begin(&arg_list), struct argument,
				      arg_elem)->arg;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (arg1, &if_.eip, &if_.esp);
  
  /* If load failed, quit. */
  palloc_free_page (file_name);
  
  if(!success) {
    thread_current()->tid_elem->has_faulted = true;
    
    /* Sema up to let parent continue at failure */
    sema_up(&thread_current()->tid_elem->child_semaphore);
    
    clean_arguments(&arg_list, arg_page);
    thread_exit ();
  }

  struct argument *argument;
  struct list_elem *e;
  int32_t argc = 0;
  
  /* Push arguments onto stack.
     Store location of arguments on stack in the arg_list.
     Calculates number of arguments */
  for(e = list_rbegin(&arg_list);
      e != list_rend(&arg_list); e = list_prev(e)) {
    argument = list_entry(e, struct argument, arg_elem);
    if_.esp -= strlen(argument->arg) + 1;
    strlcpy(if_.esp, argument->arg, PGSIZE);
    
    argument->arg = (char*) if_.esp;
    argc++;
  }

  /* Word align to keep stack aligned on size of char* */
  if_.esp -= ((int) if_.esp % sizeof(char*));


  /* Push a null pointer sentinel onto stack */
  push_word_to_stack(&if_, 0);

  /* Push pointers to each argument onto stack */
  for(e = list_rbegin(&arg_list);
      e != list_rend(&arg_list); e = list_prev(e)) {
    argument = list_entry(e, struct argument, arg_elem);
    
    push_word_to_stack(&if_, (int32_t) argument->arg);
  }

  /* Push argv, argc and false return address onto stack */
  push_word_to_stack(&if_, (int32_t) if_.esp);
  push_word_to_stack(&if_, argc);
  push_word_to_stack(&if_, (int32_t) 0);

  clean_arguments(&arg_list, arg_page);

  /* If load is complete, sema_up child_semaphore to allow the parent
     to continue */
  sema_up(&thread_current()->tid_elem->child_semaphore);

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Used for argument parsing
   Frees all malloc-ed argument structs and the arg_page */
static void clean_arguments(struct list *arg_list, void *arg_page) {
  struct list_elem *e = list_begin(arg_list);
  while (e != list_end(arg_list)) {
    struct argument *arg = list_entry(e, struct argument, arg_elem);
    e = list_next(e);
    free(arg);
  }
  palloc_free_page(arg_page);
}

/* Used for argument parsing 
   Adds a four byte item (word) onto the stack and updates the stack */
static void push_word_to_stack(struct intr_frame *if_, int32_t val) {
  if_->esp -= sizeof(char*);
  int32_t *stack_ptr = if_->esp;
  *stack_ptr = val;
}


/* Waits for thread TID to die and returns its exit status. 
 * If it was terminated by the kernel (i.e. killed due to an exception), 
 * returns -1.  
 * If TID is invalid or if it was not a child of the calling process, or if 
 * process_wait() has already been successfully called for the given TID, 
 * returns -1 immediately, without waiting.
 * 
 * This function will be implemented in task 2.
 * For now, it does nothing. */
int
process_wait (tid_t child_tid) 
{
  struct thread *t = thread_current();

  struct list_elem *curr = list_begin(&t->child_tid_list);
  struct tid_elem *curr_elem;
  bool match = false;
  while(curr != list_end(&t->child_tid_list) && !match) {
    curr_elem = list_entry(curr, struct tid_elem, elem);

    if(curr_elem->tid == child_tid) {
      match = true;
   }

    curr = list_next(curr);
  }

  /* If the child process is alive sema down child_semaphore and wait
     for it to finish */
  if(match) {
    sema_down(&curr_elem->child_semaphore);
    lock_acquire(&curr_elem->tid_elem_lock);

    /* If after sema_down the child process is still not dead then there
       occured an error */
    if(!curr_elem->process_dead) {
      return -1;
    }

    /* At this point the child process is dead and the parent can free its
       tid_elem and remove it from their child_tid_list */
    int exit_status = curr_elem->exit_status;
    list_remove(&curr_elem->elem);
    free(curr_elem);
    return exit_status;
    
  } else {
    return -1;
  }
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *t = thread_current ();
  uint32_t *pd;

  /* Frees all memory associated with open files */
  struct list_elem *current;
  struct file_elem *current_file;
  
  while(!list_empty(&t->files)) {
    current = list_pop_front(&t->files);
    current_file = list_entry(current, struct file_elem, elem);
    file_close(current_file->file);
    free(current_file);
  }

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = t->pagedir;
  if (pd != NULL) 
    {
      /* Printing termination messages */
      char *name = t->name;
      char* token = strtok_r(name, " ", &name);
      printf("%s: exit(%d)\n", token, t->tid_elem->exit_status);

      /* Free children's tid_elem if the child has terminated */
      struct list_elem *child_elem = list_begin(&t->child_tid_list);
      struct tid_elem *child_tid_elem;
      
      while(child_elem != list_end(&t->child_tid_list)) {
	child_tid_elem = list_entry(child_elem, struct tid_elem, elem);	
	lock_acquire(&child_tid_elem->tid_elem_lock);
	
	if(child_tid_elem->process_dead) {
	  lock_release(&child_tid_elem->tid_elem_lock);
	  child_elem = list_next(child_elem);
	  free(child_tid_elem);
	  
	} else {
   	  child_tid_elem->process_dead = true;
	  child_elem = list_next(child_elem);
	  lock_release(&child_tid_elem->tid_elem_lock);
	}
      }
      
      /* If parent process is dead then free shared tid_elem
         otherwise sema_up for if the parent process is waiting */
      lock_acquire(&t->tid_elem->tid_elem_lock);
      if(t->tid_elem->process_dead) {
        lock_release(&t->tid_elem->tid_elem_lock);
        free(&t->tid_elem);
	
      } else {
        t->tid_elem->process_dead = true;
	sema_up(&t->tid_elem->child_semaphore);
	lock_release(&t->tid_elem->tid_elem_lock);
      }

      /* Close processe's executable (will allow write) */
      file_close(t->executable);

      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      t->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
     }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  
  process_activate ();

  /* Open executable file. */
  filesys_lock_acquire();
  file = filesys_open (file_name);
  filesys_lock_release();
    
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  /* Save processe's executable file to executable and deny write to it */
  t->executable = file;
  filesys_lock_acquire();
  file_deny_write(file);
  filesys_lock_release();

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */ 
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;
      
      /* Check if virtual page already allocated */
      struct thread *t = thread_current ();
      uint8_t *kpage = pagedir_get_page (t->pagedir, upage);
      
      if (kpage == NULL){
        
        /* Get a new page of memory. */
        // put file into spt	
        create_file_page(upage, file, ofs, writable);      
      	kpage = allocate_user_page(upage, 0, writable);

	if (kpage == NULL) {
	  return false;
	}
      }

      /* Load data into the page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  void *upage = PHYS_BASE - PGSIZE;
  create_stack_page(upage);
  uint8_t *kpage = allocate_user_page(((uint8_t *) PHYS_BASE) - PGSIZE,
				      PAL_ZERO, true);

  if (kpage == NULL) {
    return false;
  }

  *esp = PHYS_BASE;
  
  return true;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();
  
  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

/* Allocates a user page and installs it into the frame table 
   takes a user address to allocate space for, extra palloc flags and whether
   the file is writable or not 
   Returns the address of the frame allocated or null if out of pages */
void *allocate_user_page (void* uaddr, enum palloc_flags flags, bool writable) {
  void *kpage = palloc_get_page(PAL_USER | flags);
  struct thread *t = thread_current();

  if(kpage != NULL) {
    bool success = install_page(uaddr, kpage, writable);
    
    //TODO: add eviction in null case
    if(!success) {
      palloc_free_page(kpage);
      PANIC("OUT OF FRAMES");
    }
    
    struct frame_table_entry *ft = malloc(sizeof(struct frame_table_entry));
    if(ft == NULL) {
      palloc_free_page(kpage);
      thread_exit(); //may have to be handled in a diff way!!!
    }
       
    ft->frame = kpage;
    ft->uaddr = pg_round_down(uaddr);
    ft->owner = t;
    ft->timestamp = timer_ticks();
    ft->reference_bit = 0;
    ft->modified = 0;
    ft->writable = writable;

    struct sup_table_entry *spt = spt_find_entry(t, uaddr);

    /* If something goes horribly wrong */
    if(spt == NULL) {
      thread_exit();
    }
    //copy_to_frame(ft, spt);

    // not sure if this needs to be acquired earlier?
    ft_lock_acquire();
    ft_insert_entry(&ft->elem);
    ft_lock_release();
  }

  return kpage;
}
