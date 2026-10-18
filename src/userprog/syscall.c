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

static void validate_buffer(const void *uaddr,size_t size){
  if(size==0) return;

  if(uaddr==NULL) sys_exit(-1);

  const uint8_t* byte_ptr=(const uint8_t*)uaddr;
  for(size_t i=0; i<size; i++){
    if(!is_valid_ptr(byte_ptr+i)) sys_exit(-1);
  }
}


static void validate_string(const char* str){
  if(str==NULL) sys_exit(-1);

  while(true){
    if(!is_valid_ptr(str)) sys_exit(-1);
    if(*str=='\0') break;
    str++;
  }
}

static void
syscall_handler (struct intr_frame *f UNUSED){
  validate_buffer(f->esp,sizeof(int));

  int syscall_num=*(int*) f->esp;

  switch(syscall_num){
    case SYS_HALT:
      shutdown_power_off();
      break;

    case SYS_EXIT:
      validate_buffer(f->esp+4,sizeof(int));
      sys_exit(*(int*) (f->esp+4));
      break;

    case SYS_WRITE:
      {
        validate_buffer(f->esp+4,3*sizeof(int));

        int fd=*(int*) (f->esp+4);
        const void *buffer=*(const void **) (f->esp+8);
        unsigned size=*(unsigned*) (f->esp+12);

        validate_buffer(buffer,size);

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
  struct thread*cur=thread_current();
  cur->exit_status=status;
  thread_exit();
}