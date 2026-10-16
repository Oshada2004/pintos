#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);
static bool is_valid_user_ptr(const void *ptr, size_t size);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static bool
is_valid_user_ptr(const void *ptr, size_t size)
{
  struct thread *cur=thread_current();
  if (ptr==NULL || !is_user_vaddr (ptr) || !is_user_vaddr ((const uint8_t *) ptr+size-1))
    return false;
  if (pagedir_get_page (cur->pagedir,ptr)==NULL ||
      pagedir_get_page (cur->pagedir, (const uint8_t *) ptr+size-1)==NULL)
    return false;
  return true;
}

static void
syscall_handler (struct intr_frame *f) 
{
  struct thread *cur=thread_current();
  int *esp=(int *) f->esp;

  if (is_valid_user_ptr (esp, sizeof(int)))
    {
      int syscall_num=*esp;
      if (syscall_num==SYS_HALT)
        {
          shutdown_power_off();
        }
      else if (syscall_num==SYS_EXIT)
        {
          int *status_ptr=esp+1;
          if (is_valid_user_ptr(status_ptr, sizeof (int)))
            cur->exit_status=*status_ptr;
          else
            cur->exit_status=-1;
          thread_exit();
        }
    }
  else
    {
      cur->exit_status=-1;
      thread_exit();
    }

  printf ("system call!\n");
  thread_exit ();
}
