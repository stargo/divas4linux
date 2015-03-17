/* $Id$
 *
 * DIVA-ISDN driver for Linux. (Control-Utility)
 *
 * Copyright 2001-2009 Cytronics & Melware (info@melware.de)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Log$
 *
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <linux/types.h>
#include <poll.h>

#ifdef HAVE_NCURSES_H
#       include <ncurses.h>
#endif
#ifdef HAVE_CURSES_H
#       include <curses.h>
#endif
#ifdef HAVE_CURSES_NCURSES_H
#       include <curses/ncurses.h>
#endif
#ifdef HAVE_NCURSES_CURSES_H
#       include <ncurses/curses.h>
#endif

#include <platform.h>
#include <di_defs.h>
#include <cardtype.h>
#include <divasync.h>
#undef OK
#include <pc.h>
#include <di.h>


static int diva_cfg_lib_init (const char* device_name,
                              const char* configuration_file_name,
                              int* pdevice_fd,
                              int* presponse_length);
static int diva_cfg_lib_update (const char* device_name, const char* configuration_file_name);


extern int divaload_main (int, char**);
extern int mantool_main (int, char**);
extern int mlog_main (int, char**);
extern int dchannel_main (int, char**);
extern int ditrace_main (int, char**);

extern word xlog(FILE *stream, void * buffer);

void no_printf (char * x ,...)
{
  /* dummy debug function */
}
DIVA_DI_PRINTF diva_dprintf = no_printf;

char *cmd;

void divactrl_usage(void)
{
  fprintf(stderr,"divactrl utility version 3.1.6-109.75-1   (c) 2001-2009 Cytronics & Melware\n");
  fprintf(stderr,"usage: %s load     [arg1 arg2 argn]  (load and start card)\n", cmd);
  fprintf(stderr,"       %s ctrl     [arg1 arg2 argn]  (card controlling functions) \n", cmd);
  fprintf(stderr,"       %s mantool  [arg1 arg2 argn]  (B-Channel state trace utility)\n", cmd);
  fprintf(stderr,"       %s mlog     [arg1 arg2 argn]  (trace/log utility)\n", cmd);
  fprintf(stderr,"       %s dchannel [arg1 arg2 argn]  (D-Channel log)\n", cmd);
  fprintf(stderr,"       %s ditrace  [arg1 arg2 argn]  (Driver log)\n", cmd);
  fprintf(stderr,"       %s cfglib   init,update device file\n", cmd);
  fprintf(stderr,"\n");
  fprintf(stderr,"Type %s [load|ctrl|mantool|mlog|ditrace] -h for function related help.\n", cmd);

  exit(-1);
}

/*
** The main !
*/
int main(int argc, char **argv)
{
  char *newargv[100];
  int newarg;
  int x;
  int ret = 0;

  cmd = argv[0];

  if(argc < 2)
    divactrl_usage();
  if (!strcmp(argv[1], "-h"))
    divactrl_usage();
  if (!strcmp(argv[1], "--help"))
    divactrl_usage();

  /*
    build new arg(v)
  */
  newarg = 1;
  newargv[0] = malloc(80);
  for (x = 2; x < argc; x++)
  {
    newargv[newarg] = malloc(strlen(argv[x]) + 1);
    strcpy(newargv[newarg++],argv[x]);
  }
  newargv[newarg] = NULL;




  /*
    divaload
  */
  if(!strcmp(argv[1], "load")) {
    strcpy(newargv[0], "divactrl load");
    ret = divaload_main(newarg, newargv);
    if(ret){
      printf("If an error occurs, you can also try the option -Debug.\n");
      printf("Type 'divactrl load -h' for help.\n");
	}
    return(ret);
  }
  if(!strcmp(argv[1], "ctrl")) {
    strcpy(newargv[0], "divactrl ctrl");
    ret = divaload_main(newarg, newargv);
    if(ret){
      printf("If an error occurs, you can also try the option -Debug.\n");
      printf("Type 'divactrl ctrl -h' for help.\n");
	}
    return(ret);
  }

  /*
    mantool
  */
  if(!strcmp(argv[1], "mantool")) {
    strcpy(newargv[0], "divactrl mantool");
    ret = mantool_main(newarg, newargv);
    if(ret)
      printf("Type 'divactrl mantool -h' for help.\n");
    return(ret);
  }

  /*
    ditrace
  */
  if(!strcmp(argv[1], "ditrace")) {
    strcpy(newargv[0], "divactrl ditrace");
    ret = ditrace_main(newarg, newargv);
    if(ret)
      printf("Type 'divactrl ditrace -h' for help.\n");
    return(ret);
  }

  /*
    mlog
  */
  if(!strcmp(argv[1], "mlog")) {
    strcpy(newargv[0], "divactrl mlog");
    ret = mlog_main(newarg, newargv);
    if(ret)
      printf("Type 'divactrl mlog -h' for help.\n");
    return(ret);
  }

  /*
    dchannel
  */
  if(!strcmp(argv[1], "dchannel")) {
    strcpy(newargv[0], "divactrl dchannel");
    ret = dchannel_main(newarg, newargv);
    if(ret)
      printf("Type 'divactrl dchannel -h' for help.\n");
    return(ret);
  }

  /*
    CfgLib helper
    */
  if (!strcmp(argv[1], "cfglib")) {
    if (argc == 5) {
      if (!strcmp(argv[2], "init")) {
        return (diva_cfg_lib_init (argv[3], argv[4], 0, 0));
      }
      if (!strcmp(argv[2], "update")) {
        return (diva_cfg_lib_update (argv[3], argv[4]));
      }
    }
  }

  divactrl_usage();
  return(0);
}





/*
**  This section does contain all functions that are necessary to
**  resolve all externals unused in the user mode configurator
*/
void
diva_os_prepare_pri_functions (PISDN_ADAPTER IoAdapter)
{
}

byte
scom_test_int(ADAPTER * a)
{
        printf ("test int ?\n");
        exit (10);
        return (0);
}

byte
pr_dpc(ADAPTER * a)
{
        printf ("pr_dpc ?\n");
        exit (10);
        return (0);
}

void
pr_out(ADAPTER * a)
{
        printf ("pr_out ?\n");
        exit (10);
}

void
diva_os_prepare_pri2_functions (PISDN_ADAPTER IoAdapter)
{
}

int
diva_os_schedule_soft_isr (diva_os_soft_isr_t* psoft_isr)
{
        return (0);
}

void
scom_clear_int(ADAPTER * a)
{
        printf ("scom_clear_int ?\n");
        exit (10);
}

void
diva_os_wait (dword wait_mS)
{
}

void
diva_os_sleep (dword wait_mS)
{
}

long
diva_os_atomic_increment (diva_os_atomic_t* p)
{
  printf ("atomic increment ?\n");
  exit (10);
  return (0);
}

long
diva_os_atomic_decrement (diva_os_atomic_t *p)
{
  printf ("atomic decrement ?\n");
  exit (10);
  return (0);
}

void
inppw_buffer (void* addr, void* p, int lengrh)
{
}

void
diva_os_enter_spin_lock (diva_os_spin_lock_t* pspin,
                         diva_os_spin_lock_magic_t* pmagic,
                         void* debug)
{
}

void
diva_os_leave_spin_lock (diva_os_spin_lock_t* pspin,
                         const diva_os_spin_lock_magic_t* pmagic,
                         void* debug)
{
}

/*
  Initial write of configuration file to CfgLib device
  */
static int diva_cfg_lib_init (const char* device_name,
                              const char* configuration_file_name,
                              int* pdevice_fd,
                              int* presponse_length) {
  int device_fd, cfg_file_fd;
  struct stat stat;
  char* data;

  if ((device_fd = open ("/dev/DivasDIDD", O_RDWR | O_NONBLOCK)) < 0) {
    device_fd = open ("/proc/net/eicon/divadidd", O_RDWR | O_NONBLOCK);
  }
  if (device_fd < 0) {
    perror ("cfglib write");
    return (1);
  }

  if ((cfg_file_fd = open (configuration_file_name, 0)) < 0) {
    perror ("cfglib write");
    close (device_fd);
    return (2);
  }
  if (fstat (cfg_file_fd, &stat) < 0) {
    perror ("cfglib write");
    close (cfg_file_fd);
    close (device_fd);
    return (2);
  }
  if (stat.st_size < 4) {
    perror ("cfglib write");
    close (cfg_file_fd);
    close (device_fd);
    return (2);
  }
  if ((data = (char*)malloc (stat.st_size+1)) == 0) {
    perror ("cfglib write");
    close (cfg_file_fd);
    close (device_fd);
    return (3);
  }
  if (read (cfg_file_fd, data, stat.st_size) != stat.st_size) {
    perror ("cfglib write");
    free (data);
    close (cfg_file_fd);
    close (device_fd);
    return (4);
  }
  data[stat.st_size] = 0;

  if (write (device_fd, data, stat.st_size+1) != stat.st_size+1) {
    perror ("cfglib write");
    free (data);
    close (cfg_file_fd);
    close (device_fd);
    return (10);
  }

  free (data);
  close (cfg_file_fd);
  if (pdevice_fd) {
    *pdevice_fd = device_fd;
    if (presponse_length) {
      *presponse_length = (int)(stat.st_size * 3);
    }
  } else {
    close (device_fd);
  }

  return (0);
}

/*
  Update stored in CfgLib configuration using configuration file
  */
static int diva_cfg_lib_update (const char* device_name, const char* configuration_file_name) {
  int device_fd, response_length;
  int ret = diva_cfg_lib_init (device_name, configuration_file_name, &device_fd, &response_length);

  if (ret == 0) {
    struct pollfd fds;
    char data[response_length + 1];

    fds.fd      = device_fd;
    fds.events  = POLLIN | POLLERR | POLLHUP | POLLNVAL;
    fds.revents = 0;
    if ((poll(&fds, 1, 1000) <= 0) ||
        (fds.revents & (POLLERR | POLLHUP | POLLNVAL)) ||
        (!(fds.revents & POLLIN))) {
      close (device_fd);
      return (11);
    }

    if ((response_length = read (device_fd, data, response_length)) <= 0) {
      close (device_fd);
      return (12);
    }
    data[response_length] = 0;

    fprintf (stdout, "%s", data);
    fflush (stdout);

    close (device_fd);
  }

  return (ret);
}

