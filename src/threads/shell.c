#include "threads/shell.h"

#include <stdio.h>
#include <string.h>

#include "devices/input.h"
#include "devices/rtc.h"
#include "devices/shutdown.h"
#include "threads/loader.h"
#include "threads/pte.h"
#include "threads/thread.h"

void shell_run(void)
{
  char buf[128];
  int idx;

  printf ("\n=========================================\n");
  printf ("     CS2042 Pintos Interactive Shell     \n");
  printf ("=========================================\n\n");

  while (1)
    {
      printf ("CS2042> ");
      idx=0;

      while (1)
        {
          char c=input_getc();

          if(c=='\r' || c=='\n')
            {
              printf ("\n");
              buf[idx]='\0';
              break;
            }
          else if(c=='\b' || c==127)
            {
              if(idx > 0)
                {
                  idx--;
                  printf ("\b \b");
                }
            }
          else if(idx < 127)
            {
              buf[idx++]=c;
              printf ("%c",c);
            }
        }

      if(strlen (buf)==0)
        continue;

      if(!strcmp(buf,"whoami"))
        {
          printf ("Name: Oshada\nIndex: 240279C\n");
        }
      else if(!strcmp(buf,"shutdown"))
        {
          printf ("Shutting down Pintos...\n");
          shutdown_power_off();
        }
      else if(!strcmp(buf,"time"))
        {
          unsigned total_seconds=rtc_get_time();
          printf ("Seconds since Unix epoch: %u s\n",total_seconds);
          printf ("Current Time: %u : %u : %u (UTC)\n",(total_seconds%86400)/3600,(total_seconds%3600)/60,total_seconds%60);
        }
      else if(!strcmp(buf,"ram"))
        {
          uint32_t total_ram_bytes=init_ram_pages*PGSIZE;
          printf ("RAM Pages: %u (%u KB / %u MB)\n",
                  init_ram_pages,
                  total_ram_bytes/1024,
                  total_ram_bytes/(1024*1024));
        }
      else if(!strcmp(buf,"thread"))
        {
          thread_print_stats();
        }
      else if(!strcmp(buf,"priority"))
        {
          printf ("Current thread priority: %d\n",thread_get_priority());
        }
      else if(!strcmp(buf,"exit"))
        {
          printf ("Exiting shell...\n");
          break;
        }
      else
        {
          printf ("Unknown command: %s\n",buf);
        }
    }
}