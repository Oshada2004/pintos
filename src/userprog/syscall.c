#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);
static void sys_exit (int status);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static bool is_valid_ptr(const void *uaddr){
  struct thread *cur=thread_current();
  return uaddr!=NULL && is_user_vaddr(uaddr) && pagedir_get_page(cur->pagedir,uaddr)!=NULL;
}

static void
syscall_handler (struct intr_frame *f UNUSED){

  if(!is_valid_ptr(f->esp) || !is_valid_ptr(f->esp+3)) sys_exit(-1);

  int syscall_num=*(int*) f->esp;

  switch(syscall_num){
    case SYS_HALT:
      shutdown_power_off();
      break;

    case SYS_EXIT:
      if(!is_valid_ptr(f->esp+4) || !is_valid_ptr(f->esp+7)) sys_exit(-1);
      sys_exit(*(int*) (f->esp+4));
      break;

    case SYS_WRITE:
      {
        if(!is_valid_ptr(f->esp+4) || !is_valid_ptr(f->esp+15)) sys_exit(-1);

        int fd=*(int*) (f->esp+4);
        const void *buffer=*(const void **) (f->esp+8);
        unsigned size=*(unsigned*) (f->esp+12);

        if(size>0 && (!is_valid_ptr(buffer) || !is_valid_ptr(buffer+size-1))) sys_exit(-1);

        if(fd==1){
          putbuf(buffer,size);
          f->eax=size;
        }
        else f->eax=0;

        break;
      }

    default:
      sys_exit(-1);
      break;
    }
}

static void sys_exit(int status)
{
  struct thread* cur=thread_current();
  cur->exit_status=status;
  thread_exit();
}
