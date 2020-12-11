#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"

#define MAX_PUSH_SIZE (32)
#define MAX_STACK_PAGES (2048)

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* HELPER FUNCTIONS */
static void exception_exit(struct intr_frame *);
static bool file_to_frame(struct sup_table_entry *, void *);
static void swap_to_frame(struct sup_table_entry *, void *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill, "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill, "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill, "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      thread_exit (); 

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  
         Shouldn't happen.  Panic the kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      PANIC ("Kernel bug - this shouldn't be possible!");
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to task 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */

  struct thread *t = thread_current();   /* The current thread */
   
  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;
  
   //printf("page_fault: %p, %lld \n", fault_addr, page_fault_cnt);		      
  /* Checks for if the page fault happened in a valid case */
  if(!not_present || !fault_addr
     || pagedir_get_page(t->pagedir, pg_round_down(fault_addr))) {
    exception_exit(f);
  }

  if(!load_frame(fault_addr, f->esp, FAULT_ACCESS, user, write)) {
    exception_exit(f);
  }
  
  /* To implement virtual memory, delete the rest of the function
     body, and replace it with code that brings in the page to
     which fault_addr refers. */
  /*
    printf ("Page fault at %p: %s error %s page in %s context.\n",
          fault_addr,
          not_present ? "not present" : "rights violation",
          write ? "writing" : "reading",
          user ? "user" : "kernel");
    kill (f);
  */
}

/* HELPER FUNCTIONS */

/* Exits the thread after preserving the current result
   and returning an error code of -1 */
static void exception_exit(struct intr_frame *f) {
  if(filesys_lock_held_by_current_thread()) {
    filesys_lock_release();
  }
  
  f->eip = (void *) f->eax;
  f->eax = 0xffffffff;
  thread_exit();
}

/* Writes a page of data from a file into a frame 
   Takes the supplemental page entry related to the file 
   and the frame table entry related to the frame 
   Returns false if more data is found than expected */
static bool file_to_frame(struct sup_table_entry *spt, void *frame) {
  
  bool lock_held = filesys_lock_held_by_current_thread();
  
  run_if_false(filesys_lock_acquire(), lock_held);
  file_seek(spt->file, spt->offset);
  size_t bytes_read = file_read(spt->file, frame, spt->read_bytes);
  run_if_false(filesys_lock_release(), lock_held);

  if(bytes_read > spt->read_bytes) {
    return false;
  }
  
  memset(frame + bytes_read, 0, PGSIZE - bytes_read);
  return true;
}

/* Writes a page of data from the swap system to a frame 
   Takes the supplemental page table entry corresponding to the swapped data
   and the frame table entry corresponding to the frame */
static void swap_to_frame(struct sup_table_entry *spt, void *frame) {
  bool lock_held = swap_lock_held_by_current_thread();

  if(spt->type == IN_SWAP_FILE) {
    spt->type = FILE_PAGE;
  }

  run_if_false(swap_lock_acquire(), lock_held);
  ft_pin(spt->upage, PGSIZE);
  swap_read_frame(frame, spt->block_number);
  ft_unpin(spt->upage, PGSIZE);
  remove_swap_space(spt->block_number, 1);
  run_if_false(swap_lock_release(), lock_held);
}

/* Load a frame for a faulting/unloaded address */
bool load_frame(void *fault_addr, void *esp, bool fault, bool user, bool write) {
  struct sup_table_entry *spt;           /* The entry of this address in the
					    suplemental page table */
  struct frame_table_entry *ft;          /* The entry of this address in the
                                            frame table */
  void *frame;                           /* The frame of physical memory the
					    fault_addr accesses */
  struct thread *t = thread_current();   /* The current thread */
  
  /* Validity checks */
  if(user && !is_user_vaddr(fault_addr)) {
    return false;
  }
  
  /* If in user access, update the curr_esp in the thread (to esp from
     intr_frame) */
  if(user) {
    t->curr_esp = esp;
  }

  /* See if the access is supposed to exist in virtual memory */
  spt = spt_find_entry(t, fault_addr);

  /* Grow stack if needed */
  spt = grow_stack(fault_addr, spt);

  if(spt == NULL) {
    return false;
  }

  if(fault && (write && !spt->writable)) {
    return false;
  }

  pagedir_set_accessed(spt->owner->pagedir, fault_addr, spt->accessed);
  pagedir_set_dirty(spt->owner->pagedir, fault_addr, spt->modified);
  ft = spt->ft;
  
  /* Check whether a frame_entry has already been allocated */
  if(ft == NULL) {
    /* Allocate physical memory to map to the fault_addr */
    switch(spt->type) {
      /* Allocate a zero page */
      case NEW_STACK_PAGE:
	frame = allocate_user_page(fault_addr, PAL_ZERO, spt->writable);
	break;

      /* Allocate a zero page to be put the swap space on eviction */
      case IN_SWAP_FILE:	
      case STACK_PAGE:
	frame = allocate_user_page(fault_addr, PAL_ZERO, spt->writable);
	swap_to_frame(spt, frame);
	break;

	/* Allocate a user accessable page which, if modified, will be put in the
	   swap space on eviction */
      case MMAPPED_PAGE:
	frame = allocate_user_page(fault_addr, PAL_USER, spt->writable);
	if(spt->modified) {
	  swap_to_frame(spt, frame);
	} else {
	  if(!file_to_frame(spt, frame)) {
	    return false;
	  }
	}
	break;

      /* Allocate a user accessable file or zero page which will be put in the swap 
	 space on eviction if writable */
      case ZERO_PAGE:
      case FILE_PAGE:
	if(spt->writable) {
	  frame = allocate_user_page(fault_addr, PAL_USER, spt->writable);
	  if(!file_to_frame(spt, frame)) {
	    return false;
	  }
	  break;
	}

	bool lock_held = st_lock_held_by_current_thread();
	
	run_if_false(st_lock_acquire(), lock_held);
	struct shared_table_entry *st = st_find_entry(spt->file, spt->offset);
	run_if_false(st_lock_release(), lock_held);
	
	if (st != NULL) {
	  install_shared_page(st, spt);
	} else {
	  frame = allocate_user_page(fault_addr, PAL_USER, spt->writable);
	  if(!file_to_frame(spt, frame)) {
	    return false;
	  }
	}
	break;

      default:
	return false;
    }
    
  }

  lock_acquire(&spt->ft->owners_lock);
  if(list_empty(&spt->ft->owners)) {
    lock_release(&spt->ft->owners_lock);
    return false;
  }
    
  struct sup_table_entry *spt_entry;
  struct list_elem *spt_elem = list_front(&spt->ft->owners);
  while(spt_elem != list_end(&spt->ft->owners)) {
    spt_entry = list_entry(spt_elem, struct sup_table_entry, frame_elem);
    spt_entry->accessed = true;
      
    spt_elem = list_next(spt_elem);
  }

  lock_release(&spt->ft->owners_lock);

  return true;
}

struct sup_table_entry *grow_stack(void *fault_addr,
 				   struct sup_table_entry *spt_entry) {
  struct thread *t = thread_current();
  struct sup_table_entry *spt = spt_entry;
  
  /* Check if page fault occurred because of full stack */
  if(spt == NULL && (t->curr_esp - MAX_PUSH_SIZE) <= fault_addr) {
    if(t->stack_page_cnt++ >= MAX_STACK_PAGES){
      return NULL;
    }
    
    create_stack_page(pg_round_down(fault_addr));
    spt = spt_find_entry(t, fault_addr);
  }

  return spt;
}

struct sup_table_entry *grow_stack_if_needed(struct thread *t, const void *uaddr) {
  struct sup_table_entry *spt;
  
  /* See if the access is supposed to exist in virtual memory */
  spt = spt_find_entry(t, uaddr);
  
  /* Check if page fault occurred because of full stack*/
  if(spt == NULL && (t->curr_esp - MAX_PUSH_SIZE) <= uaddr) {
    //printf("%d\n", page_fault_cnt);
    if(t->stack_page_cnt++ >= MAX_STACK_PAGES){
      return NULL;
    }
    
    create_stack_page(pg_round_down(uaddr));
    spt = spt_find_entry(t, uaddr);
  }

  return spt;
}
