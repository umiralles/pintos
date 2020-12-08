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
#include "vm/page.h"
#include "vm/frame.h"

#define MAX_PUSH_SIZE (32)
#define MAX_STACK_PAGES (2048)

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* HELPER FUNCTIONS */
static void exception_exit(struct intr_frame *);
static bool file_to_frame(const struct sup_table_entry *, void *);

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

  struct sup_table_entry *sup_entry;     /* The entry of this address in
				           the suplemental page table */
  struct frame_table_entry *frame_entry; /* The entry of this address in
				           the frame table */
  void *frame;                           /* The frame of physical
					    memory the fault_addr
					    accesses */
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

  /* Checks for if the page fault happened in a valid case */
  if(!not_present || !is_user_vaddr(fault_addr) || !user) {
    exception_exit(f);
  }
  
  /* Check if page fault occurred because of full stack*/
  if((f->esp - MAX_PUSH_SIZE) <= fault_addr) {	
    if(t->stack_page_cnt++ >= MAX_STACK_PAGES){
      exception_exit(f);
    }
    create_stack_page(pg_round_down(fault_addr));
  }
  
  /* See if an entry into the frame table has already been made */
  ft_lock_acquire();
  frame_entry = ft_find_entry(fault_addr);
  ft_lock_release();

  /* See if the access is supposed to exist in virtual memory */
  sup_entry = spt_find_entry(t, fault_addr);

  if(sup_entry == NULL) {
    exception_exit(f);
  }

  /* Check whether a frame_entry has already been allocated */
  if(frame_entry == NULL) {

    /* Allocate physical memory to map to the fault_addr */
    switch(sup_entry->type) {
      case ZERO_PAGE:
      case STACK_PAGE:
	frame = allocate_user_page(fault_addr, PAL_ZERO,
				   sup_entry->writable);
	break;
      case MMAPED_PAGE:  		
      case FILE_PAGE:
	frame = allocate_user_page(fault_addr, PAL_USER,
				   sup_entry->writable);
 	if(!file_to_frame(sup_entry, frame)) {
 	  exception_exit(f);
 	}
      	
	break;
      default:
	//swap to file
	exception_exit(f);
    }
  } else {
    // only happens in swap
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
  f->eip = (void *) f->eax;
  f->eax = 0xffffffff;
  thread_exit();
}

static bool file_to_frame(const struct sup_table_entry *sup_entry, void *frame) {
  
  //check if file is null/ mapid is -1
  //if both true - return false
  //if mapid - not -1 - call helper func
  
  filesys_lock_acquire();
  file_seek(sup_entry->file, sup_entry->offset);
  size_t bytes_read = file_read(sup_entry->file, frame, sup_entry->read_bytes);
  filesys_lock_release();

  if(bytes_read > sup_entry->read_bytes) {
    return false;
  }
  
  memset(frame + bytes_read, 0, PGSIZE - bytes_read);
  return true;
}
