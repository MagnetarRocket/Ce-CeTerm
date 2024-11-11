/*.pad.c
/phx/bin/e pad.c;exit
*/
/*static char *sccsid = "%Z% %M% %I% - %G% %U% ";*/
#ifdef PAD
/***************************************************************
*  
*  ARPUS/Ce text editor and terminal emulator modeled after the
*  Apollo(r) Domain systems.
*  Copyright 1988 - 2002 Enabling Technologies Group
*  Copyright 2003 - 2005 Robert Styma Consulting
*  
*  This program is free software; you can redistribute it and/or
*  modify it under the terms of the GNU General Public License
*  as published by the Free Software Foundation; either version 2
*  of the License, or (at your option) any later version.
*  
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*  
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*  
*  Original Authors:  Robert Styma and Kevin Plyler
*  Email:  styma@swlink.net
*  
***************************************************************/

/***********************************************************
*
*  THIS IS THE NT VERSION OF PAD.C 
*
*
* E:\Cygnus\B19\cygnus.bat
* E:\Cygnus\B19\H-i386-cygwin32\bin\bash
* cmd
* U:\MFCTest\ftee\Debug\ftee
*
*   pad_init  -  start a shell up and set up async I/O and I/O signal
*
*      set up signal handlers, provide communications with the
*      shell, get rid of current control terminal, set up new control terminal
*
*   Note: on Apollo, all child must close fds[0] and only work with fds[1]
*                    parent closes fds[1] and only work with fds[0].
*         
*   Returns -1 on error, and child pid on success.
*
*   Parms:
*
* shell  -  The shell (including args/parms to be invoked
*
*   rfd  - The X socket passed in and the shell socket out
*
*  host  - host we display on (NULL implies not needed (No utmp_entry wanated))
*
*                                                                                
* External Interfaces:
*
*     pad_init              -  Allocate and initialize a pad window to a shell.
*     shell2pad             -  Reads shell output and puts it in the output pad
*     pad2shell             -  Reads pad commands and hands them to the shell
*     pad2shell_wait        -  Wait for the shell to be ready for more input.
*     kill_shell            -  Kill the child shell
*     vi_set_size           -  Register the screen size with the kernel
*     ce_getcwd             -  Get the current dir of the shell
*     close_shell           -  Force a close on the socket to the shell
*     tty_echo              -  Is echo mode on ~(su, passwd, telnet, vi ...)
*     do_open_file_dialog   - Use windows dialog to get edit filename
*     do_save_file_dialog   - Use windows dialog to get edit filename
*                      
* Internal Routines:     
*
*    dead_child             -  Routine called asynchronously for SIGCHLD
*    remove_backspaces      -  Remove backspaces from a line
*    tty_setmode            -  Set the tty characteristics                         
*    tty_raw                -  Put the tty in raw mode                         
*    pty_master             -  Connect to the tty master            
*    pty_slave              -  Connect to the tty slave            
*    insert_in_line         -  Insert a string into the begining of a string                         
*    get_pty                -  HP required pty routine
*    check_save             -  Check if slave is compatable with master
*    solaris_check_save     -  Check if solaris slave is compatable with master
*    pts_convert            -  Create a pts from a pty
*    build_utmp             -  Build a utmp entry for this process
*    init_utmp_ent          -  Initialize a utmp data structure.
*    dump_utmp              -  dump a utmp to stderr
*    ioctl_setup            -  set up the pty
*    wait_for_shell_termination - Thread to wait for shell termination signal from the OS
*    null_signal_handler    -  NOOP
*    my_local_socket_listen - Set up Listen (main thread) of a local socket
*
*   Optimizations:
* 
* 1  check if all #includes are needed
* 2  Buffer strm
* 3  stop flushing strm
* 4  combine pty_masters for SYS5 and regular
* 
****************************************************************************/

#define EOF_LINE "*** EOF ***"   /* is printed instead of '^D' */

#include <stdio.h>          /* usr/include/stdio.h      */ 
#include <string.h>         /* usr/include/strings.h    */ 
#include <errno.h>          /* usr/include/errno.h      */ 

#include <windows.h>
#include <io.h>
#include <stdlib.h>
#include <process.h>
#include <winsock.h>

#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef MAXPATHLEN
#define MAXPATHLEN	256
#endif

#ifndef FOPEN_MAX
#define FOPEN_MAX   64
#endif

#define _PAD_  1

#include "cc.h"                                          
#include "debug.h"                                          
#include "dmwin.h" 
#include "emalloc.h" 
#include "getevent.h"
#include "hexdump.h"
#include "pad.h"
#include "parms.h"
#include "redraw.h"
#include "str2argv.h"
#include "tab.h"
#include "txcursor.h"
#include "unixwin.h"  
#include "undo.h"
#include "unixpad.h"
#include "vt100.h"  
#include "windowdefs.h"  /* pad2shell needs dspl_descr */


DWORD WINAPI shell2pad(LPVOID pParam);
DWORD WINAPI wait_for_shell_termination(LPVOID pParam);

static void data_to_transcript_pad(DISPLAY_DESCR     *dspl_descr, char   *buf);

static void remove_backspaces(char *line);
static void insert_in_line(char *line, char *text);
static int  solaris_check_slave();

static int  online(char *str1, char *str2);

static int my_local_socket_listen(void);
static int my_local_socket_connect(void);


#if !defined(solaris) && !defined(linux)
char *getcwd(char *buf, int size);
#endif

typedef struct {
   HANDLE           process_handle;
   DISPLAY_DESCR   *dspl_descr;
} WAIT_FOR_SHELL_PARM;


int
isatty (int fd)
{
  struct stat buf;
  char    msg[256];

  if (fstat (fd, &buf) < 0)
  {
    _snprintf(msg, sizeof(msg), "fstat failure (%s)", strerror(errno));
    dm_error(msg, DM_ERROR_LOG);
    return 0;
  }
  if (_S_IFIFO & buf.st_mode)
     {
        _snprintf(msg, sizeof(msg), "is a pipe", strerror(errno));
     }
  if (_S_IFCHR & buf.st_mode)
     {
       return 1;
     }
  return 0;
}


/***************************************************************
*  
*  Global Data within pad.c
*  
***************************************************************/

int                   pipe2shell    = -1;     /* stdin to shell from this program */
int                   pipeFromShell = -1;     /* stdout and stderr from shell to this program */
DWORD                 shell_process_group_id; /* for generating ^c and ^d */
HANDLE               *shell2pad_thread_handle; /* handle for thread reading shell output */
HANDLE               *wait_for_shell_termination_thread_handle;
DWORD                 shell2pad_thread_id;    /* thread id */
DWORD                 wait_for_shell_termination_thread_id;    /* thread id */
PROCESS_INFORMATION   shell_p_info;
int                   tickle_socket   = -1;    /* used to wake up the main thread */

/**********************************************************************/
/*************************** MAIN *************************************/
/**********************************************************************/
/************************************************************************

NAME:      pad_init  -   Allocate and initialize a pad window to a shell.


PURPOSE:    This routine initializes the pipes to the shell and starts the
            shell.

PARAMETERS:

   1.  shell          - pointer to char (INPUT)
                        This is a pointer to the name of the program to run.
                        If NULL or empty, the SHELL environment variable is
                        used.  The the environment variable is not set, drop
                        back 10 yards and punt..

   2.  rfd            - pointer to int (OUTPUT)
                        The pipe from this program to the shellis returned in
                        the int pointed to by this parm.

   3.  host           - pointer to char (INPUT)
                        This is the node name.  It is not currently used.

   4.  login_shell    - int (INPUT)
                        This flag specifies that we tell the shell to run the .profile
                        via the - parmaeter on the front of the name.

GLOBAL DATA:
   This data is global within pad.c but not visible outside.

   1.  pipe2shell    -  int
                        This is the pipe from this program to stdin on the shell
   
   2.  pipeFromShell - int 
                       This is the pipe from stdout and stderr in the shell to this
                       program.
   
FUNCTIONS :
   1.  Open a pair of pipes to use with the shell.

   2.  Convert one end of each pipe to a HANDLE for use by create process.

   3.  Create the SHELL process.

   4.  Start a thread to put shell output in the transcript pad.

RETURNED VALUE:
   rc    -   int
             0  -  Pad initialization failed
             -1 -  Pad initialization failed, message output.

*************************************************************************/

int pad_init(char *shell, int *rfd, char *host, int login_shell, DISPLAY_DESCR *dspl_descr)
{
int                   rc = -1;
int                   to_shell[2];
int                   from_shell[2];
//int                   to_main[2];
int                   inheritable_stdin;
int                   inheritable_stdout;
#define               PIPE_READ_INDEX     0
#define               PIPE_WRITE_INDEX    1
#define               PIPE_BUFFER_SIZE    8192
HANDLE                read_by_shell_handle;
HANDLE                write_by_shell_handle;
char                  sys_msg[256];
char                  msg[512];
DWORD                 dwCreationFlags;
STARTUPINFO           startup_info;
DWORD                 len;
static WAIT_FOR_SHELL_PARM wait_for_shell_parm;
int                   listen_socket;
int                   clilen;
struct sockaddr_in    cli_addr;


char         *p;

DEBUG16(fprintf(stderr, "\nPAD_INIT(%s,%d,%s,%d)\n", (shell ? shell : "<NULL>"), *rfd, (host ? host : "<NULL>"), login_shell);)

*rfd = -1; /* show initially bad */
ce_tty_char[TTY_INT] = 3; /* default value of ^c for int */
ce_tty_char[TTY_EOF] = 4; /* default value of ^d for eof */

/***************************************************************
*  Build 2 pipes.  One to write to the shell and one to read from
*  the shell.  _O_BINARY
***************************************************************/
if (_pipe(to_shell, PIPE_BUFFER_SIZE, _O_TEXT | _O_NOINHERIT) < 0)
   {
      _snprintf(msg, sizeof(msg), "Could not create first pipe (%s)", strerror(errno));
      dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
      return(-1);
   }

if (_pipe(from_shell, PIPE_BUFFER_SIZE, _O_TEXT | _O_NOINHERIT) < 0)
   {
      _snprintf(msg, sizeof(msg), "Could not create second pipe (%s)", strerror(errno));
      dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
      return(-1);
   }

#ifdef blah
/***************************************************************
*  Build a pipe to tickle the main thread and wake it up.
***************************************************************/
if (_pipe(to_main, PIPE_BUFFER_SIZE, _O_TEXT | _O_NOINHERIT) < 0)
   {
      _snprintf(msg, sizeof(msg), "Could not create tickle pipe (%s)", strerror(errno));
      dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
      return(-1);
   }
#endif

/***************************************************************
*  Get an inheritable copy of the end of the pipe we plan to give
*  to the shell per the example in the manual page for _pipe.
***************************************************************/
if ((inheritable_stdin = _dup(to_shell[PIPE_READ_INDEX])) < 0)
   {
      _snprintf(msg, sizeof(msg), "Could not dup pipe end for shell to read (%s)", strerror(errno));
      dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
      return(-1);
   }

if ((inheritable_stdout = _dup(from_shell[PIPE_WRITE_INDEX])) < 0)
   {
      _snprintf(msg, sizeof(msg), "Could not dup pipe end for shell to write (%s)", strerror(errno));
      dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
      return(-1);
   }

/* get rid of the non-inheritable copies of the pipe ends */
close(to_shell[PIPE_READ_INDEX]);
close(from_shell[PIPE_WRITE_INDEX]);

/* set the global variables, and returned value */
pipe2shell    = to_shell[PIPE_WRITE_INDEX];
pipeFromShell = from_shell[PIPE_READ_INDEX];
//*rfd          = -1; /* not used in NT verison */
//*rfd          = to_main[PIPE_READ_INDEX];
//tickle_pipe   = to_main[PIPE_WRITE_INDEX];
listen_socket   = my_local_socket_listen();


/* Convert to file handles for use by Create Process */
read_by_shell_handle  = (HANDLE)_get_osfhandle(inheritable_stdin);
write_by_shell_handle = (HANDLE)_get_osfhandle(inheritable_stdout);

if ((long)read_by_shell_handle < 0)
   {
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                    NULL,  /*  pointer to message source, ignored */
                    GetLastError(),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                    sys_msg,
                    sizeof(sys_msg),
                    NULL);
      _snprintf(msg, sizeof(msg), "Could not convert FD to HANDLE #1 (%s)", sys_msg);
      dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
      return(-1);
   }

if ((long)write_by_shell_handle < 0)
   {
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                    NULL,  /*  pointer to message source, ignored */
                    GetLastError(),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                    sys_msg,
                    sizeof(sys_msg),
                    NULL);
      _snprintf(msg, sizeof(msg), "Could not convert FD to HANDLE #2 (%s)", sys_msg);
      dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
      return(-1);
   }

/***************************************************************
*  Determine what to run.
***************************************************************/
if (!shell | !(*shell))
   {
      shell = getenv("SHELL");
      if ((!shell) || (!(*shell)))
         shell = SHELL; /* literal SHELL defined in pad.h */
   }


/***************************************************************
*  Fix up environment variables as needed.
***************************************************************/
if (getenv(DMERROR_MSG_WINDOW) != NULL)
   {
      _snprintf(msg, sizeof(msg), "%s=", DMERROR_MSG_WINDOW);
      p = malloc_copy(msg);
      if (p)
         putenv(p);
   }

_snprintf(msg, sizeof(msg), "CE_SH_PID=%d",  _getpid());
p = malloc_copy(msg);
if (p)
   putenv(p);


DEBUG16( fprintf(stderr, " Going to shell: '%s'\n", shell);)

/***************************************************************
*  Create the shell process
***************************************************************/
dwCreationFlags = CREATE_NEW_PROCESS_GROUP; /*| CREATE_NEW_CONSOLE;*/
memset((char *)&startup_info, 0, sizeof(startup_info));
startup_info.cb = sizeof(startup_info);
startup_info.dwFlags = STARTF_USESHOWWINDOW
                       | STARTF_USESTDHANDLES;
startup_info.wShowWindow = SW_HIDE; /* no console window */
startup_info.hStdInput  = read_by_shell_handle;
startup_info.hStdOutput = write_by_shell_handle;
startup_info.hStdError  = write_by_shell_handle;

if (!CreateProcess(NULL, /* pointer to name of executable module, take from commandline */
                  shell, /* pointer to command line string */
                  NULL,  /* pointer to process security attributes */
                  NULL,  /* pointer to thread security attributes */
                  TRUE,  /* handle inheritance flag */
                  dwCreationFlags, /* creation flags */
                  NULL, /* pointer to new environment block - Use current environment */
                  NULL, /* pointer to current directory name. Use currect dir  */
                  &startup_info, /* pointer to STARTUPINFO */
                  &shell_p_info /* pointer to PROCESS_INFORMATION */
                  ))
   {
      /* create process failed */
      len = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                          NULL,  /*  pointer to message source, ignored */
                          GetLastError(),
                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                          sys_msg,
                          sizeof(sys_msg),
                          NULL);
      _snprintf(msg, sizeof(msg), "(pad_init) CreateProcess failed (%s) - \"%s\"", sys_msg, shell);
      dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
      return(-1);
   }


shell_process_group_id = shell_p_info.dwProcessId;
/*CloseHandle(shell_p_info.hProcess); /* release the child */
/*CloseHandle(shell_p_info.hThread);*/

if (_close(inheritable_stdin) != 0)
   {
      _snprintf(msg, sizeof(msg), "close 1 failed: (%s)", strerror(errno));
      dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
   }

if (_close(inheritable_stdout) != 0)
   {
      _snprintf(msg, sizeof(msg), "close 1 failed: (%s)", strerror(errno));
      dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
   }

shell2pad_thread_handle =  CreateThread(NULL, /* pointer to thread security attributes  */
                                        0,    /* initial thread stack size, in bytes  */
                                        &shell2pad, /* pointer to thread function  */
                                        dspl_descr, /* argument for new thread  */
                                        0, /* creation flags  */
                                        &shell2pad_thread_id); /* pointer to returned thread identifier  */

wait_for_shell_parm.dspl_descr     = dspl_descr;
wait_for_shell_parm.process_handle = shell_p_info.hProcess;

wait_for_shell_termination_thread_handle =  CreateThread(NULL, /* pointer to thread security attributes  */
                                        0,    /* initial thread stack size, in bytes  */
                                        &wait_for_shell_termination, /* pointer to thread function  */
                                        &wait_for_shell_parm, /* argument for new thread  */
                                        0, /* creation flags  */
                                        &wait_for_shell_termination_thread_id); /* pointer to returned thread identifier  */

/*********************************************************************
* Get the socket link to shell2pad, this will be used to wake up this process
* when stuff has been put in the transcript pad.
*********************************************************************/
clilen = sizeof(cli_addr);
if ((*rfd = accept(listen_socket, (struct sockaddr *) &cli_addr, &clilen)) == INVALID_SOCKET)
   {
      _snprintf(msg, sizeof(msg), "pad_init:  accept on tickle socket failed (WSAGetLastError = %d)\n", WSAGetLastError());
      dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
      *rfd = -1;
      kill_shell(0);
      shell_process_group_id = -1;
   }

closesocket(listen_socket);

return(shell_process_group_id);

} /* pad_init() */


/****************************************************************** 
*
*  kill_shell -  Routine terminates child
*
******************************************************************/

int kill_shell(int sig)
{ 
char    sys_msg[256];
char    msg[512];

DEBUG16( fprintf(stderr, "@kill_child: signal:%d\n", sig);)

#ifdef blah
if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, shell_process_group_id))
   {
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                    NULL,  /*  pointer to message source, ignored */
                    GetLastError(),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                    sys_msg,
                    sizeof(sys_msg),
                    NULL);
      _snprintf(msg, sizeof(msg), "GenerateConsoleCtrlEvent failed (%s)", sys_msg);
      dm_error(msg, DM_ERROR_BEEP);
      return(-1);
   }
#endif

if (pipe2shell >= 0)
   close(pipe2shell);

if (!TerminateProcess(shell_p_info.hProcess, sig)) // exit code for the process 
   {
      DEBUG16(
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                    NULL,  /*  pointer to message source, ignored */
                    GetLastError(),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                    sys_msg,
                    sizeof(sys_msg),
                    NULL);
      _snprintf(msg, sizeof(msg), "TerminateProcess failed (%s)", sys_msg);
      dm_error(msg, DM_ERROR_BEEP);
      )
      return(-1);
   }

return(0);

} /* kill_shell() */

/****************************************************************** 
*
*  send_control_c -  send a interupt to the shell
*
******************************************************************/

void send_control_c(void)
{ 
char    sys_msg[256];
char    msg[512];

DEBUG16( fprintf(stderr, "send_control_c:\n");)

SetLastError(0);

if (!GenerateConsoleCtrlEvent(CTRL_C_EVENT , shell_process_group_id))
/*if (GenerateConsoleCtrlEvent(CTRL_C_EVENT , 0) == 0)*/
   {
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                    NULL,  /*  pointer to message source, ignored */
                    GetLastError(),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                    sys_msg,
                    sizeof(sys_msg),
                    NULL);
      _snprintf(msg, sizeof(msg), "GenerateConsoleCtrlEvent failed (%s)", sys_msg);
      dm_error(msg, DM_ERROR_BEEP);
   }

} /* send_control_c() */

/****************************************************************** 
*
*  shell2pad - Reads shell output and puts it in the output pad
*              returns number of lines read from shell or -1
*              on i/o error from the shell socket.
*
******************************************************************/

DWORD WINAPI shell2pad(LPVOID pParam) 
{
DISPLAY_DESCR     *dspl_descr = (DISPLAY_DESCR *)pParam;

int                   rc;
int                   all_done = False;
char                  msg[256];

char buff[MAX_LINE+1];

/****************************************************************** 
*  Get the socket used to wake up the main thread after we put data
*  in the transcript pad.
******************************************************************/
tickle_socket = my_local_socket_connect();
if (tickle_socket == -1)
   {
      _snprintf(msg, sizeof(msg), "shell2pad cannot get local socket connection, aborting");
      dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
      return(-1);
   }

while (!all_done)
{

   /****************************************************************** 
   *  Read data.  If the read fails, bail out.  If zero bytes are
   *  returned, it means EOF.  Quit normally. 
   ******************************************************************/
   errno = 0;
   while(((rc = read(pipeFromShell, buff, MAX_LINE)) < 0) && (errno == EINTR))
   {
      DEBUG16(fprintf(stderr, "Read[1] interupted, retrying\n");) /* Retry reads when an interupt occurs */
   }      
   if (rc < 0){
       DEBUG16(fprintf(stderr, "Can't read from shell (%s)%d\n", strerror(errno), errno);)
       /*kill_unixcmd_window(1);*/
       kill_shell(0);
       break;
   }
   if (rc == 0)
      break;

   buff[rc]='\0';

   /****************************************************************** 
   *  Lock the display so we can add data.  
   ******************************************************************/
   lock_display_descr(dspl_descr, "shell2pad");

   data_to_transcript_pad(dspl_descr, buff);


   /****************************************************************** 
   *  Unlock the display so we can go back and read more.  
   ******************************************************************/
   unlock_display_descr(dspl_descr, "shell2pad");

} /* while not all done */

DEBUG16(fprintf(stderr, "Exiting Shell2pad Thread\n");)

kill_unixcmd_window(1);
kill_shell(0);

return(rc);

}  /* shell2pad() */

static void data_to_transcript_pad(DISPLAY_DESCR     *dspl_descr, char   *buff)
{
int                   buf_len = strlen(buff);
int                   prompt_changed;
int                   lines_displayed;
int                   lines_copied;
int                   lines = 0;
int                   redraw_needed;
char *ptr, *lptr;
char *bptr, *cptr;
static char *save_vt100;

char buff_out[(MAX_LINE*2)+1];

if (*buff) /* show wait_for_input that we read something even if backspaces kill if off */
   prompt_changed = True;
else
   prompt_changed = False;

/***************************************************************
*  Get data on what the screen looks like, we will need it
*  if output is written to the screen.
***************************************************************/
lines_displayed = total_lines(dspl_descr->main_pad->token) - dspl_descr->main_pad->first_line;
if (lines_displayed > dspl_descr->main_pad->window->lines_on_screen)
   lines_displayed = dspl_descr->main_pad->window->lines_on_screen;

if (dspl_descr->vt100_mode)
   {
      DEBUG16(hex_dump(stderr, "VT100 data", (char *)buff, buf_len);)
      if (!autovt_switch())
         {
            vt100_parse(buf_len, buff, dspl_descr);
            prompt_changed = True;
         }
      else
         {
            DEBUG16(fprintf(stderr, "Switching out of VT100 mode: len %d\n", buf_len);)
         } /* not vt100 mode */
   }

if (!dspl_descr->vt100_mode)
   {
      DEBUG16(hex_dump(stderr, "S2P-IO:", (char *)buff, buf_len);)

      if ((buff[0] == VT_ESC) || ((buf_len < 100) && strchr(buff, VT_ESC)))
         {
            if (autovt_switch())
               {
                  DEBUG16(fprintf(stderr, "Switching to VT100 mode: len %d\n", buf_len);)
                  if (save_vt100)
                    {
                       vt100_parse(strlen(save_vt100), save_vt100, dspl_descr);
                       free(save_vt100);
                       save_vt100 = NULL;
                    }
                  vt100_parse(buf_len, buff, dspl_descr);
                  prompt_changed = True;
                }
             else
                {
                   ptr = strchr(buff, VT_ESC);
                   if (!save_vt100)
                      save_vt100 = malloc_copy(ptr);
                    else
                       {
                          strcpy(buff_out, save_vt100);
                          strcat(buff_out, ptr);
                          free(save_vt100);
                          save_vt100 = malloc_copy(buff_out);
                       }
                }
         }
      else
         if (save_vt100)
            {
               free(save_vt100);
               save_vt100 = NULL;
            }

      if (MANFORMAT)
         {
            remove_backspaces(buff); 
            vt100_eat(buff, buff); /* does work in place */
         }
      else
         remove_backspaces(buff); 

      DEBUG16(fprintf(stderr, "I/O is: [%d] bytes:'%s'\n", buf_len, buff);)

      /* get the prompt and any pre-newline stuff and write it out */

      ptr = get_unix_prompt();
      strcpy(buff_out, ptr);
      if (*ptr)
         prompt_changed = True;

      ptr = buff;
      lptr = "";
      while (ptr) /* something is in the buffer to process */
      {
         bptr = ptr;
         while (bptr = strchr(bptr, 07))
         {  /* process bells */
             ce_XBell(dspl_descr, 0);
             cptr = bptr;
             while (*cptr)
             {  /* remove the bell */
                *cptr = *(cptr+1); 
                cptr++;
             }
             bptr++;
         }

         if (ptr[0] == 0x04) /* EOF on line */
            insert_in_line(ptr, EOF_LINE);

         if (ptr[0] == 0x03)  /* Interupt on line */
            insert_in_line(ptr, "^c");

         lptr = ptr;
         ptr = strchr(ptr, '\n');

         if (ptr)
            {     /* we got (mo)  data [multiple lines]*/
               *ptr = '\0';
               ptr++;
               if (buff_out[0])
                  {
                     strcat(buff_out, lptr);
                     lptr = buff_out;
                  }
                 
               if (!ceterm_prefix_cmds(dspl_descr, lptr))
                  {
                     put_line_by_num(dspl_descr->main_pad->token, total_lines(dspl_descr->main_pad->token) - 1, lptr, INSERT);
                     if (dspl_descr->linemax)
                        while (total_lines(dspl_descr->main_pad->token) > dspl_descr->linemax)
                           delete_line_by_num(dspl_descr->main_pad->token, 0, 1);
     
                     lines++;
                     buff_out[0] = '\0';
                  }
               else /* RES 8/28/96 added to allow prefix commands in middle of line */
                  if (*lptr)
                     strcpy(buff_out, lptr);
                  else
                     buff_out[0] = '\0';

           }  /* if (mo data ) - found newline char */

      }  /* while (data) */
        
      if (*lptr)
         {   /* do we need to set a prompt */
            strcat(buff_out, lptr);
            set_unix_prompt(dspl_descr, buff_out);
            prompt_changed = True;
#ifdef blah
            if (online(buff_out, "pas") && (tty_echo(0, DOTMODE),telnet_mode)){ /* Royal kloodge!!! */
 /*            DEBUG16(fprintf(stderr, "s2p: Forcing tty_echo_mode to False\n");)
              tty_echo_mode = 0;
              telnet_mode = 0;
            }else
              tty_echo_mode = -1; /* invalidate known echo mode */
#endif
         }
      else
         {
            set_unix_prompt(dspl_descr, NULL);
            tty_echo_mode = -1; /* invalidate known echo mode */
         }

   } /* Not auto-vt mode */

/****************************************************************** 
*  redraw the screen in vt100 mode or pad mode if filling the screen.  
******************************************************************/
if (dspl_descr->vt100_mode || dspl_descr->hold_mode)
   {
      if ((!dspl_descr->vt100_mode) &&
          (dspl_descr->main_pad->first_line+dspl_descr->main_pad->window->lines_on_screen >= total_lines(dspl_descr->main_pad->token)))
         {
            /*process_redraw(dspl_descr, (dspl_descr->main_pad->redraw_mask & FULL_REDRAW), False);*/
            /* We don't call process redraw from this thread, force a window redraw */
            if (!CMDS_FROM_ARG)
               {
                  store_cmds(dspl_descr,"rw", False); /* put commands where keypress will find them */
                  fake_keystroke(dspl_descr);
               }
         }
   }
else
   {
      lines_copied = lines + get_background_work(BACKGROUND_SCROLL);
      DEBUG19(fprintf(stderr, "shell2pad put %d lines in the pad, prompt was %schanged, cursor in %s window\n", lines, (prompt_changed ? "" : "not "), which_window_names[dspl_descr->cursor_buff->which_window]);)
      /* return of just a prompt counts for doing more reads from shell */
      DEBUG22(fprintf(stderr, "(1)unix pad first line = %d\n", dspl_descr->unix_pad->first_line);)
      if (prompt_changed && (dspl_descr->unix_pad->first_line == 0))   /* RES 3/3/94  - addedlast test to cut down on cursor jumping */
         {
            if (lines > 0) /* RES 10/22/97 Test added to avoid mouse dropping in main window when only cursor changed */
               clear_text_cursor(dspl_descr->cursor_buff->current_win_buff->x_window, dspl_descr);  /* if the cursor is in the main window, we blasted it */
            /*lines_read_from_shell = 1;*/
            redraw_needed = (dspl_descr->unix_pad->redraw_mask & FULL_REDRAW);
            set_window_col_from_file_col(dspl_descr->cursor_buff);
            prompt_changed = (dspl_descr->cursor_buff->which_window == UNIXCMD_WINDOW);  /* RES 3/15/94 only warp if we are in the unix window */
            /*process_redraw(dspl_descr, redraw_needed, prompt_changed);*/
#ifdef blah
            if (!CMDS_FROM_ARG)
               {
                  /* We don't call process redraw from this thread, force a window redraw */
                  store_cmds(dspl_descr,"rw", False); /* put commands where keypress will find them */
                  fake_keystroke(dspl_descr);
               }
#endif
         }

      change_background_work(dspl_descr, BACKGROUND_SCROLL, lines_copied);
      tickle_main_thread(dspl_descr);
      /*send(tickle_socket, "Tickle", 6, 0);*/
      /*XGetSelectionOwner(dspl_descr->display, XA_PRIMARY); /* tickle the X server to let the main thread know whats going on */
      /*scroll_some(&lines_copied,
                  dspl_descr,
                  lines_displayed); don't do X calls in this thread */
   }

} /* end of data_to_transcript_pad */


/****************************************************************** 
*
*  pad2shell - Write to the shell. If the shell can not take all,
*              buffer it until next call, if the shell did not take
*              any, return:
*
*                   1 (try again another day), 
*                  -1 error;
*                   0 successfull line sent
*
******************************************************************/

static char p2s_buf[MAX_LINE] = "";
int p2s_len = 0;

int pad2shell(char *line, int newline)
{

int rc, len;
char *ptr1;
char *ptr2;
char MEOF = EOF;
char buf[MAX_LINE];
char msg[256];


DEBUG16(
if (!p2s_len) p2s_buf[0] = '\0';
if (*line)
    fprintf(stderr, " @pad2shell([%d]%s)nl:%s[p2sbuf: '%s']\n", *line, line, (newline ? "True" : "False"), p2s_buf);
else
    fprintf(stderr, " @pad2shell(NULL[FLUSH buffers])\n");
)


/****************************************************************** 
*  Lock the display so we can add data to the pad.  
******************************************************************/
lock_display_descr(dspl_descr, "pad2shell");

/*
 * Have we got buffered stuff from last time ?
 */

if (line && 
    (line[0] == ce_tty_char[TTY_INT]) &&
    (line[1] == '\0'))
   {
      DEBUG16(fprintf(stderr, "INTERUPT!\n");)
      p2s_len = 0;
      p2s_buf[0] = '\0';
   }

errno = 0;
if (p2s_len)
   {   /* we have buffered stuff from last time */
      rc = write(pipe2shell, p2s_buf, p2s_len); 
      DEBUG16(if (errno) fprintf(stderr, "ERRNO: '%s'\n",  strerror(errno));)
      if (rc == -1)
        {
           DEBUG16(fprintf(stderr, "Can't write to shell (%s)[1]\n", strerror(errno));)
           kill_unixcmd_window(1);
           kill_shell(0);
           return(PAD_FAIL);
        }
       DEBUG16( fprintf(stderr, "Old data written %d of %d\n", rc, p2s_len);)
       p2s_len -= rc;
       if (!p2s_len)
          p2s_buf[0] = '\0';
       else
          {       /* socket still full */
             ptr1 = p2s_buf;        
             ptr2 = p2s_buf + rc;        
             while (*ptr2)         /* shift down any that did copy */
                *(ptr1++) = *(ptr2++); 
             *ptr1 = '\0';
             DEBUG16( fprintf(stderr, "Overflow of %d\n", p2s_len);)
             return(PAD_BLOCKED);
    }
}  /* old buffered data */

if (line && *line == MEOF)
   { /* EOF request */
      buf[0] = ce_tty_char[TTY_EOF]; buf[1] = '\0';
      DEBUG16( fprintf(stderr, "Writing'EOF':x%02X\n", buf[0]);)
      errno = 0;
      /* echo to the transcript pad */
      /*data_to_transcript_pad(dspl_descr, EOF_LINE);
      if (dspl_descr->linemax)
         while (total_lines(dspl_descr->main_pad->token) > dspl_descr->linemax)
            delete_line_by_num(dspl_descr->main_pad->token, 0, 1);*/

      rc =  write(pipe2shell, buf, 1);
      DEBUG16(if (errno) fprintf(stderr, "ERRNO: '%s'\n",  strerror(errno));)
      if (rc == -1)
         {
            _snprintf(msg, sizeof(msg), "Can't write to shell (%s)[2]", strerror(errno));
            dm_error(msg, DM_ERROR_LOG);  
            kill_unixcmd_window(1);
            kill_shell(0);
            return(PAD_FAIL);
         }
      return(PAD_SUCCESS); 
   }

/*
 * Lets copy as much as we can of the new line
 */

if (!line) return(PAD_SUCCESS);  /* flush request */

strcpy(buf, line);
if (newline){
    if (setprompt_mode())
       strcat(buf, "\n"); /* Fix setprompt mode in slim 11/18/97 */
    else
       strcat(buf, "\n"); /* Testing with BASH on Windows NT   6/9/1999 */
}
len = strlen(buf);

errno = 0;
rc = write(pipe2shell, buf, len);
DEBUG16(if (errno) fprintf(stderr, "ERRNO: '%s'\n",  strerror(errno));)
DEBUG16( fprintf(stderr, "Wrote %d char of %d \"%s\"\n", rc, len, buf);)
if (!dspl_descr->vt100_mode)
   data_to_transcript_pad(dspl_descr, buf);


if (rc == -1){
    _snprintf(msg, sizeof(msg), "Can't write to shell (%s)[3]", strerror(errno));
    dm_error(msg, DM_ERROR_LOG);  
    kill_unixcmd_window(1);
    kill_shell(0);
    return(PAD_FAIL);
}

p2s_len = len - rc;

if (!p2s_len) p2s_buf[0] = '\0';
if (p2s_len){         /* did not write everything */
    DEBUG16( fprintf(stderr, "Not every thing wrote %d of %d\n", rc, len);)
    /* if (!rc) return(PAD_BLOCKED); 3/2/94 p2s_len should be set to 0 */ /* failed, blocked socket */
    strcpy(p2s_buf, buf + rc);
    return(PAD_PARTIAL);
}

/****************************************************************** 
*  Unlock the display description.  
******************************************************************/
unlock_display_descr(dspl_descr, "pad2shell");

return(PAD_SUCCESS);

} /* pad2shell */

/****************************************************************** 
*
*  pad2shell_wait - Wait for the shell to be ready for more input.
*
*                   0 select timed out
*                  -1 error;
*                   1 shell is ready for more input
*
******************************************************************/

int pad2shell_wait(void)
{
fd_set wfds_bits;
struct timeval time_out;
int nfound;

FD_ZERO(&wfds_bits);
FD_SET(pipe2shell, &wfds_bits);
time_out.tv_sec = 2;
time_out.tv_usec = 0;

nfound = select(pipe2shell+1, NULL, HT &wfds_bits, NULL, &time_out);

return(nfound);

} /* pad2shell_wait */

/****************************************************************** 
*
*  remove_backspaces - removes backspaces and carage returns
*                      does not let a person backup over a \n or
*                      begining of buffer
*
******************************************************************/

static void remove_backspaces(char *line)
{

char *bptr, *tptr;
       static char special_string2[] = { 0x02, 0x02, 0 };

/* RES 9/12/95 special ce_isceterm check */
if ((line[0] == 0x02) && (line[1] == 0x02) &&
    (line[2] == 0x08) && (line[3] == 0x08)){
    DEBUG16(fprintf(stderr, "ce_isceterm_chk: special string matched(0)\n");)
    pad2shell(special_string2, False);
}

bptr = tptr = line;
while (*bptr){

   if (*bptr == '\b'){     /* back spaces */
       tptr--;
       if (*tptr == '\n') 
           tptr++;    /* can't back over newlines */
   }else{
       if (*tptr != *bptr)
          *tptr = *bptr;
       tptr++;
   }

   if (*bptr == '\r')    /* carriage returns */
       tptr--;

   /* RES 9/12/95 special ce_isceterm check */
   if (*bptr == '\n')
      if ((bptr[1] == 0x02) && (bptr[2] == 0x02) &&
          (bptr[3] == 0x08) && (bptr[4] == 0x08)){
          DEBUG16(fprintf(stderr, "ce_isceterm_chk: special string matched\n");)
          pad2shell(special_string2, False);
      }

   if (tptr < line)    /* incase we get more backspaces than characters! */
         tptr = line; 

   bptr++;
}

*tptr = '\0';

} /* remove_backspaces */

/****************************************************************** 
*
*  wait_for_shell_termination - This thread waits for the OS to
*    say the process ended and makes sure ceterm knows.
*
******************************************************************/

DWORD WINAPI wait_for_shell_termination(LPVOID pParam) 
{
WAIT_FOR_SHELL_PARM  *wait_parm = (WAIT_FOR_SHELL_PARM *)pParam;
HANDLE                process_handle;
DISPLAY_DESCR        *dspl_descr;
int                   rc = 0;
char                  sys_msg[256];
char                  msg[512];
DWORD                 exit_status;

process_handle = wait_parm->process_handle;
dspl_descr =     wait_parm->dspl_descr;

/***************************************************************
*  Wait for the shell process to terminate.  The handle is 
*  passed in.
***************************************************************/
if (WaitForSingleObject(process_handle, INFINITE) == WAIT_FAILED)
   {
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                    NULL,  /*  pointer to message source, ignored */
                    GetLastError(),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                    sys_msg,
                    sizeof(sys_msg),
                    NULL);
       _snprintf(msg, sizeof(msg), "WaitForSingleObject failed waiting for shell to terminate (%s)", sys_msg);
       DEBUG16(fprintf(stderr, "%s\n", msg);)
       dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
       rc = -1;
   }
else
   {
      if (!GetExitCodeProcess(process_handle, &exit_status))
         {
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                          NULL,  /*  pointer to message source, ignored */
                          GetLastError(),
                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                          sys_msg,
                          sizeof(sys_msg),
                          NULL);
             _snprintf(msg, sizeof(msg), "GetExitCodeProcess failed after shell termination (%s)", sys_msg);
             dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
             rc = -1;
         }
      else
         DEBUG(  /* switch to different debug level (like 0) after testing */
             _snprintf(msg, sizeof(msg), "shell termination code = %d", exit_status);
             dm_error_dspl(msg, DM_ERROR_MSG, dspl_descr);
         )
   }

lock_display_descr(dspl_descr, "wait_for_shell_termination");

/*store_cmds(dspl_descr,"rw", False); /* put commands where keypress will find them */
/*fake_keystroke(dspl_descr);*/
tickle_main_thread(dspl_descr);
/*send(tickle_socket, "Tickle", 6, 0);*/
closesocket(tickle_socket);

/*TerminateThread(shell2pad_thread_handle, 0);*/

unlock_display_descr(dspl_descr, "wait_for_shell_termination");

return(rc);

}  /* wait_for_shell_termination() */



/****************************************************************** 
*
*  tty_echo - return whether we should display characters or not.
*             this routine checks if we are echoing characters and returns that
*             if called with (1), and factors in telnet_mode otherwise
*             this is incase we are telneted in.
*  RETURNED VALUE:           
*             True, echo characters is enabled.  Not dot mode.
*             False, use dot mode, echoing is disabled.
*
******************************************************************/

int tty_echo(int   echo_only,
             char *dotmode) /* Added 8/30/97 RES */
{
char  *prompt;

if (!echo_only){
   if (dotmode && (dotmode[0] == '0'))
      return(1);

   if (dotmode && (dotmode[0] == '1')){
      prompt = get_unix_prompt();
      if (dotmode[1]) /* if value was supplied for dotmode 1 */
         if (online(prompt, &dotmode[1]))
            return(0);
         else
            return(1);
      else
         if (online(prompt, "pas"))
            return(0);
         else
            return(1);

   }
}

return(1);
       

} /* tty_echo() */



/****************************************************************** 
*
*  vi_set_size - Register the size of the screen with the kernel
*                so programs like vi can be used
*
******************************************************************/

int vi_set_size(int fd, int lines, int columns, int hpix, int vpix)
{

#ifdef blah
struct winsize winsiz;

winsiz.ws_row =  lines;
winsiz.ws_col =  columns;
winsiz.ws_xpixel =  hpix;
winsiz.ws_ypixel =  vpix;

DEBUG16(fprintf(stderr, "chid=%d in window size\n", chid);)

if ((ioctl(fd, TIOCSWINSZ, &winsiz) < 0) && chid){
    _snprintf(msg, sizeof(msg), "Could not reset Kernel window size(%s))", strerror(errno));   
    DEBUG16(fprintf(stderr, "%s\n", msg);)
    return(-1);
}

DEBUG16(fprintf(stderr, "Window size changed to char:(%d,%d) pix:(%d,%d)\n", lines, columns, hpix, vpix);)
#endif

return(0);

} /* vi_set_size() */

/****************************************************************** 
*
*  ce_getcwd - Get the working directory for the subshell
*
******************************************************************/
static char sun_getcwd_storage[MAXPATHLEN+5] = "PWD=";
static char *sun_getcwd_bug = sun_getcwd_storage +4;

char *ce_getcwd(char *buf, int size)
{

if (buf && (size > (int)strlen(sun_getcwd_bug))) strcpy(buf, sun_getcwd_bug);

#ifdef GETCWD_DEBUG
fprintf(stderr, "@ce_getcwd(%s,%d)strlen(sgb)=[%d], = %s\n", buf, size, (int)strlen(sun_getcwd_bug), sun_getcwd_bug);
#endif


return(sun_getcwd_bug);

}  /* ce_getcwd() */

/****************************************************************** 
*
*  ce_setcwd - Set the working directory for the subshell
*              Try the ksh $PWD and see if its ok if so
*              preserve it since the logical path is better
*
******************************************************************/

void ce_setcwd(char *buf)
{
char  *ptr;
char path[MAXPATHLEN+1];
static int wasSet = 1;
struct stat sbuf1, sbuf2;

#ifdef GETCWD_DEBUG
fprintf(stderr, "@ce_setcwd(\"%s\")\n", buf ? buf : "");
#endif

if (!buf){
    buf = (char *)getenv("PWD");
    ptr = getcwd(path, sizeof(path));
    if (!ptr)
        ptr = "/"; /* handles case of cd into a directory, remove the directory, fire up ce */
#ifdef GETCWD_DEBUG
    fprintf(stderr, "getenv(%s)\n", buf ? buf : "");
    fprintf(stderr, "getcwd(%s)\n", ptr ? ptr : "");
#endif
    if (!buf){
        buf = ptr;
        wasSet = 0;
    }else{
       if (stat(buf, &sbuf1))
           buf = ptr;     /* PWD failed */
       else{
          if (!stat(ptr, &sbuf2)) /* getcwd failed */
              if (!((sbuf1.st_dev == sbuf2.st_dev) && 
                    (sbuf1.st_ino == sbuf2.st_ino)))
                       buf = ptr;
       }
    }
}

strcpy(sun_getcwd_bug, buf);
if (wasSet){
#ifdef GETCWD_DEBUG
   fprintf(stderr, "putenv(%s)\n", sun_getcwd_storage);
#endif
   putenv(sun_getcwd_storage);
} 

} /* ce_setcwd() */

/****************************************************************** 
*
*  close_shell - Force a close on the socket to the shell
*
******************************************************************/

void close_shell(void)
{
if (pipe2shell >= 0)
   close(pipe2shell);

if (pipeFromShell >= 0)
   close(pipeFromShell);

pipe2shell    = -1;     /* stdin to shell from this program */
pipeFromShell = -1;     /* stdout and stderr from shell to this program */
}


/****************************************************************** 
*
*  insert_in_line - replace an EOF
*
******************************************************************/
static void insert_in_line(char *line, char *text)
{
char eof_buf[MAX_LINE];

DEBUG16(fprintf(stderr, " @insert_in_line(%s,%s)\n", line, text);)
DEBUG16(hex_dump(stderr, "  LINE:", (char *)line, strlen(line));)

strcpy(eof_buf, line+1);
strcpy(line, text);
strcat(line, "\n");
strcat(line, eof_buf);

}  /*  insert_in_line() */


/**************************************************************
*
*   Routine online
*
*   Taken from liba.a, which is missing on this system.
*
*    This routine finds the first occurance of the second
*    string in the first and returns the offset of the
*    second string.
*
**************************************************************/

static int online(char *str1, char *str2)
{

char   c   = *str2;
char   *s1 = str1;
char   *s2 = str2;

int    len1 = strlen(s1);
int    len2 = strlen(s2);

while (len2 <= len1--){
   if (c == (*s1 | 040)){  /* case is independant */
      s2 = str2;
      while ((*s1 | 040) == *s2){s1++; s2++;}
      if (!*s2) return(1);/*  return(s1 - str1); */
   }
   s1++;
}

return(0);   /* (-1) */

} /* online() */


#endif  /* PAD */    

#ifdef WIN32
void lock_display_descr(DISPLAY_DESCR *dspl_descr, char  *title)
{
char                  sys_msg[256];
char                  msg[512];
DWORD                 dwRc;

/***************************************************************
*  When debugging is on, see if we will have to wait.
***************************************************************/
DEBUG19(
   dwRc = WaitForSingleObject((HANDLE)dspl_descr->term_mainpad_mutex, 0);
   if (dwRc == WAIT_TIMEOUT)
      {
          fprintf(stderr, "lock_display_descr:  Will have to wait for (%s:%d)\n", title, GetCurrentThreadId());
      }
   else
      {
         fprintf(stderr, "lock_display_descr:  Locked Display Immed (%s:%d)\n", title, GetCurrentThreadId());
         return;
      }
)

/***************************************************************
*  In Windows, lock the display description while we process
*  commands to sync with the shell2pad thread or transpad input
*  or -STDIN input and normal command processing.
***************************************************************/

//dwRc = WaitForSingleObject((HANDLE)dspl_descr->term_mainpad_mutex, 10000);
dwRc = WaitForSingleObject((HANDLE)dspl_descr->term_mainpad_mutex, INFINITE);
if (dwRc == WAIT_FAILED)
   {
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                    NULL,  /*  pointer to message source, ignored */
                    GetLastError(),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                    sys_msg,
                    sizeof(sys_msg),
                    NULL);
       _snprintf(msg, sizeof(msg), "%s:%d:  WaitForSingleObject failed (%s)", title, GetCurrentThreadId(), sys_msg);
       dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
   }
else
   if (dwRc == WAIT_TIMEOUT)
      {
          _snprintf(msg, sizeof(msg), "lock_display_descr:  WaitForSingleObject timed out (%s:%d)", title, GetCurrentThreadId());
          DEBUG19(fprintf(stderr, "%s\n", msg);)
          dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
      }
   else
      {
         DEBUG19(fprintf(stderr, "lock_display_descr:  Locked Display (%s:%d)\n", title, GetCurrentThreadId());)
      }
} /* end of lock_display_descr */


void unlock_display_descr(DISPLAY_DESCR *dspl_descr, char  *title)
{
char                  sys_msg[256];
char                  msg[512];

/***************************************************************
*  In Windows, release the mutex on the display description
***************************************************************/
if (!ReleaseMutex((HANDLE)dspl_descr->term_mainpad_mutex))
   {
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                    NULL,  /*  pointer to message source, ignored */
                    GetLastError(),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                    sys_msg,
                    sizeof(sys_msg),
                    NULL);
      _snprintf(msg, sizeof(msg), "unlock_display_descr:  %s:%d ReleaseMutex failed (%s)", title, GetCurrentThreadId(), sys_msg);
      DEBUG19(fprintf(stderr, "%s\n", msg);)
      dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
   }
else
   {
      DEBUG19(fprintf(stderr, "unlock_display_descr:  Unlocked Display (%s:%d)\n", title, GetCurrentThreadId());)
   }

} /* end of unlock_display_descr */


/**************************************************************
*
*   do_open_file_dialog - Use windows dialog to get edit filename
*
*   Taken from liba.a, which is missing on this system.
*
*    This routine finds the first occurance of the second
*    string in the first and returns the offset of the
*    second string.
*
**************************************************************/
/*#include <CDERR.H> */
int do_open_file_dialog(char   *edit_file_ret,
                        int     size_of_edit_file_ret,
                        int     new_file_ok)
{
OPENFILENAME      ofn;
int               rc;
DWORD             err_rc;

memset((char *)&ofn, 0, sizeof(ofn));
ofn.lStructSize = sizeof(ofn);
*edit_file_ret = '\0';
ofn.lpstrFile = edit_file_ret;
ofn.nMaxFile  = size_of_edit_file_ret;
if (new_file_ok)
   ofn.Flags = OFN_CREATEPROMPT;
else
   ofn.Flags = OFN_FILEMUSTEXIST;

ofn.Flags |= OFN_EXPLORER;

if (GetOpenFileName(&ofn) == FALSE)
   {
      rc = -1;
      err_rc = CommDlgExtendedError();
   }
else
   rc = 0;

return(rc);

} /* end of do_open_file_dialog */


/**************************************************************
*
*   do_save_file_dialog - Use windows dialog to get edit filename
*
*   Taken from liba.a, which is missing on this system.
*
*    This routine finds the first occurance of the second
*    string in the first and returns the offset of the
*    second string.
*
**************************************************************/

int do_save_file_dialog(char   *edit_file_ret,
                        int     size_of_edit_file_ret)
{
OPENFILENAME      ofn;
int               rc;
DWORD             err_rc;

memset((char *)&ofn, 0, sizeof(ofn));
ofn.lStructSize = sizeof(ofn);
*edit_file_ret = '\0';
ofn.lpstrFile = edit_file_ret;
ofn.nMaxFile  = size_of_edit_file_ret;

ofn.Flags = OFN_OVERWRITEPROMPT
          | OFN_EXPLORER
          | OFN_NOREADONLYRETURN;

if (GetSaveFileName(&ofn) == FALSE)
   {
      rc = -1;
      err_rc = CommDlgExtendedError();
   }
else
   rc = 0;

return(rc);

} /* end of do_save_file_dialog */


/************************************************************************

NAME:      my_local_socket_listen - Set up Listen (main thread) of a local socket

PURPOSE:    This routine is used to start setting up a socket connection between
            the shell2pad thread and the main thread which processes X events.
            Windows requires that select can only be used on sockets and they
            have to be the same kind of socket (tcp/ip or udp/ip).

            No interesting data is sent across this socket.  Just the word Tickle.
            This causes the select in the main thread to return and process the X
            event which was saved in the Ce X event queue.

PARAMETERS:
   None

GLOBAL DATA:
   1.  my_local_port -  int (OUTPUT)
                     This is the port number used for the listen.  This routine starts
                     with the process number + 10000 as the port number and tries ports
                     till one is found which can be listened on.  This value is set here
                     and later used by my_local_socket_connect.

FUNCTIONS:
   1.  Get a Socket.

   2.  Starting with the port PID + 10000, do listens until one works,  Increment the
       port number till one is found upon which we can do a listen.

   3.  Save the port we are listening on in the global variable and return the socket
       file descriptor.

RETURNED VALUE:
   fd    - int
           Upon success the socket file descriptor we are listening on is
           returned.  On failure, -1 is returned.

NOTES:
   1.  No race condition exists because this routine is called before the
       shell2pad thread is started.  The shell2pad thread completes the
       connection  while the main thread waits using the accept call.

*************************************************************************/

static int  my_local_port = -1;

static int my_local_socket_listen(void)
{
struct sockaddr_in     tcp_srv_addr;   /* servers' Internet socket addr */

int                    fd;
char                   lcl_msg[256];
#define                NUM_PORT_TRIES 100
int                    start_port;
int                    end_port;

memset((char *) &tcp_srv_addr, 0, sizeof(tcp_srv_addr));
tcp_srv_addr.sin_family       = AF_INET;
tcp_srv_addr.sin_addr.s_addr  = htonl(INADDR_ANY);

 
if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
   {
      _snprintf(lcl_msg, sizeof(lcl_msg), "my_local_socket_listen:  Cannot get a socket file descriptor (WSAGetLastError = %d)\n",
                        WSAGetLastError());
      dm_error(lcl_msg, DM_ERROR_LOG);
      return(-1);
   }

start_port = _getpid();
if (start_port < 0)
   start_port = -start_port;
if (start_port > 32767)
   start_port = start_port % 32767;
if (start_port < 10000)
   start_port += 10000;
end_port = start_port + NUM_PORT_TRIES;

while((start_port < end_port) && (my_local_port == -1))
{
   tcp_srv_addr.sin_port = htons((short)start_port);
   if (bind(fd, (struct sockaddr *) &tcp_srv_addr, sizeof(tcp_srv_addr)) == 0)
      my_local_port = start_port;
   else
      start_port++;
}

if (my_local_port != -1)
   {
      listen(fd, 1);
   }
else
   {
      _snprintf(lcl_msg, sizeof(lcl_msg), "my_local_socket_listen:  Cannot access port in range %d to %d (WSAGetLastError = %d)\n",
              end_port-NUM_PORT_TRIES, end_port-1, WSAGetLastError());
      closesocket(fd);
      dm_error(lcl_msg, DM_ERROR_LOG);
      fd = -1;
   }

return(fd);

} /* end of my_local_socket_listen */


/************************************************************************

NAME:      my_local_socket_connect - Set up Connect (shell2pad thread) of a local socket

PURPOSE:    This routine is used to complete setting up a socket connection between
            the shell2pad thread and the main thread which processes X events.
            Windows requires that select can only be used on sockets and they
            have to be the same kind of socket (tcp/ip or udp/ip).

PARAMETERS:
   None

GLOBAL DATA:
   1.  my_local_port -  int (INPUT)
                     This is the port number used for the connect.  It was set by
                     routine my_local_socket_listen.

FUNCTIONS:
   1.  Get a Socket.

   2.  Connect to the loopback (127.0.0.1) address and the port specified by my_local_port.

   3.  Return the socket file descriptor.

RETURNED VALUE:
   fd    - int
           Upon success the socket file descriptor we are listening on is
           returned.  On failure, -1 is returned.

*************************************************************************/

static int my_local_socket_connect(void)
{
struct sockaddr_in     tcp_srv_addr;   /* servers' Internet socket addr */

int                    fd;
unsigned long          inaddr;
char                   lcl_msg[256];
char                  *host = "127.0.0.1";

if (my_local_port < 0)
   {
      _snprintf(lcl_msg, sizeof(lcl_msg), "my_local_socket_connect:  No local socket defined\n");
      dm_error(lcl_msg, DM_ERROR_LOG);
      return(-1);
   }

memset((char *) &tcp_srv_addr, 0, sizeof(tcp_srv_addr));
tcp_srv_addr.sin_family = AF_INET;
tcp_srv_addr.sin_port = htons((short)my_local_port);
inaddr = inet_addr(host);
memcpy((char *)&tcp_srv_addr.sin_addr, (char *)&inaddr, sizeof(inaddr));

if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
   {
      _snprintf(lcl_msg, sizeof(lcl_msg), "my_local_socket_connect:  Cannot get a socket file descriptor (WSAGetLastError = %d)\n",
               WSAGetLastError());
      dm_error(lcl_msg, DM_ERROR_LOG);
      return(-1);
   }

if (connect(fd, (struct sockaddr *)&tcp_srv_addr, sizeof(tcp_srv_addr)) < 0)
   {
      _snprintf(lcl_msg, sizeof(lcl_msg), "?    tcp_open:  Cannot connect to port %d on host %s (WSAGetLastError = %d)\n", my_local_port, host, WSAGetLastError());
      closesocket(fd);
      dm_error(lcl_msg, DM_ERROR_LOG);
      return(-1);
   }
 
return(fd);

} /* end of my_local_socket_connect */

void tickle_main_thread(DISPLAY_DESCR *dspl_descr)
{
if (!CMDS_FROM_ARG)
   {
      /* We don't call process redraw from this thread, force a window redraw */
      store_cmds(dspl_descr,"rw", False); /* put commands where keypress will find them */
      fake_keystroke(dspl_descr);
   }
send(tickle_socket, "Tickle", 6, 0);
} /* end of tickle_main_thread */
#endif
