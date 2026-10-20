#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"

#include "threads/malloc.h"
#include <stdlib.h>
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/process.h"

static void syscall_handler (struct intr_frame *);
static void sys_exit (int status);

void
syscall_init (void) 
{
  lock_init (&filesys_lock);
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


static struct file_descriptor *
get_file_descriptor(int fd){
  struct thread *cur=thread_current();
  struct list_elem *e;

  for(e=list_begin (&cur->fd_list); e!=list_end(&cur->fd_list); e=list_next(e)){
    struct file_descriptor *desc=list_entry(e,struct file_descriptor,elem);
    if(desc->fd==fd) return desc;
  }
  return NULL;
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

    case SYS_EXEC:
      {
        validate_buffer (f->esp+4, sizeof(char*));
        const char *cmd_line= *(const char**)(f->esp+4);

        validate_string(cmd_line);

        lock_acquire(&filesys_lock);
        f->eax=process_execute(cmd_line);
        lock_release(&filesys_lock);
        break;
      }

    case SYS_WAIT:
      {
        validate_buffer(f->esp+4,sizeof(int));
        tid_t pid= *(tid_t*)(f->esp+4);

        f->eax=process_wait(pid);
        break;
      }

    case SYS_CREATE:
      {
        validate_buffer (f->esp+4,2*sizeof(int));
        const char *file=*(const char**) (f->esp+4);
        unsigned initial_size=*(unsigned*) (f->esp+8);

        validate_string(file);

        lock_acquire(&filesys_lock);
        f->eax=filesys_create(file,initial_size);
        lock_release(&filesys_lock);
        break;
      }

    case SYS_REMOVE:
      {
        validate_buffer(f->esp+4,sizeof(int));
        const char* file=*(const char**) (f->esp+4);

        validate_string(file);

        lock_acquire(&filesys_lock);
        f->eax=filesys_remove(file);
        lock_release(&filesys_lock);
        break;
      }

    case SYS_OPEN:
      {
        validate_buffer(f->esp+4,sizeof(int));
        const char *file_name=*(const char**) (f->esp+4);

        validate_string(file_name);

        lock_acquire(&filesys_lock);
        struct file* f_ptr=filesys_open(file_name);
        lock_release(&filesys_lock);

        if(f_ptr==NULL) f->eax=-1;
        else{
          struct thread *cur=thread_current();
          struct file_descriptor* desc=malloc(sizeof(struct file_descriptor));
          if(desc==NULL){
            lock_acquire(&filesys_lock);
            file_close(f_ptr);
            lock_release(&filesys_lock);
            f->eax=-1;
          }else{
            desc->fd=cur->next_fd++;
            desc->file=f_ptr;
            list_push_back(&cur->fd_list,&desc->elem);
            f->eax=desc->fd;
          }
        }
        break;
      }

    case SYS_FILESIZE:
      {
        validate_buffer(f->esp+4, sizeof(int));
        int fd=*(int*) (f->esp+4);

        struct file_descriptor *desc=get_file_descriptor (fd);
        if(desc==NULL) f->eax=-1;
        else{
          lock_acquire(&filesys_lock);
          f->eax=file_length(desc->file);
          lock_release(&filesys_lock);
        }
        break;
      }

    case SYS_READ:
      {
        validate_buffer(f->esp+4, 3*sizeof(int));
        int fd=*(int*)(f->esp+4);
        void *buffer=*(void**)(f->esp+8);
        unsigned size=*(unsigned*)(f->esp+12);

        validate_buffer(buffer,size);

        if(fd==0){
          uint8_t *byte_buf=(uint8_t*) buffer;
          for(unsigned i=0; i<size; i++){
            byte_buf[i]=input_getc();
          }
          f->eax=size;

        }else if(fd==1) f->eax=-1;
        else{
          struct file_descriptor* desc=get_file_descriptor(fd);
          if(desc==NULL) f->eax=-1;
          else{
            lock_acquire(&filesys_lock);
            f->eax=file_read(desc->file,buffer,size);
            lock_release(&filesys_lock);
          }
        }
        break;
      }

    case SYS_WRITE:
      {
        validate_buffer(f->esp+4,3*sizeof(int));

        int fd=*(int*) (f->esp+4);
        const void *buffer=*(const void **) (f->esp+8);
        unsigned size=*(unsigned*)(f->esp+12);

        validate_buffer(buffer,size);

        if(fd==1){
          putbuf(buffer,size);
          f->eax=size;
        }
        else if(fd==0) f->eax=-1;
        else{
          struct file_descriptor* desc=get_file_descriptor(fd);
          if(desc==NULL) f->eax=-1;
          else{
            lock_acquire(&filesys_lock);
            f->eax=file_write(desc->file,buffer,size);
            lock_release(&filesys_lock);
          }
        }
        break;
      }

    case SYS_SEEK:
      {
        validate_buffer(f->esp+4,2*sizeof(int));
        int fd=*(int*)(f->esp+4);
        unsigned position=*(unsigned*)(f->esp+8);

        struct file_descriptor *desc=get_file_descriptor(fd);
        if(desc!=NULL){
          lock_acquire(&filesys_lock);
          file_seek(desc->file,position);
          lock_release(&filesys_lock);
        }
        break;
      }

    case SYS_TELL:
      {
        validate_buffer (f->esp+4,sizeof(int));
        int fd=*(int*) (f->esp+4);

        struct file_descriptor *desc=get_file_descriptor(fd);
        if(desc==NULL) f->eax=-1;
        else{
          lock_acquire(&filesys_lock);
          f->eax=file_tell(desc->file);
          lock_release(&filesys_lock);
        }
        break;
      }

    case SYS_CLOSE:
      {
        validate_buffer(f->esp+4,sizeof(int));
        int fd=*(int*)(f->esp+4);

        struct file_descriptor* desc=get_file_descriptor(fd);
        if(desc!=NULL)
        {
          lock_acquire(&filesys_lock);
          file_close(desc->file);
          lock_release(&filesys_lock);

          list_remove(&desc->elem);
          free(desc);
        }
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