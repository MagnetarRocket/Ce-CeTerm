/*static char *sccsid = "%Z% %M% %I% - %G% %U% ";*/
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

/***************************************************************
*
*  module cepopen.c  - execute-nt.c
*
* These routines take the same arguments as popen and pclose on
* UNIX and _popen and _pclose on NT, but unlike the NT versions
* These work.  They can be macro'ed to _popen and _pclose when
* NT is fixed.
*
*  Routines:
*     ce_popen           - Like _popen, but works under NT 4
*     ce_pclose          - Matches ce_popen
*
*  INTERNAL
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h       */
#include <string.h>         /* /usr/include/string.h      */
#include <errno.h>          /* /usr/include/errno.h       */
#include <sys/types.h>      /* /usr/include/sys/types.h   */
/*#include <sys/stat.h>       /* /usr/include/sys/stat.h    */
#include <fcntl.h>          /* /usr/include/fcntl.h      NEED on WIN32 */

#include <windows.h>
#include <io.h>
#include <stdlib.h>
#include <process.h>

#include "cepopen.h"


#if defined(ARPUS_CE) && !defined(PGM_XDMC)
//ifdef ARPUS_CE
#include "debug.h"
#include "dmwin.h" 
#include "emalloc.h"
#else
#ifndef DEBUG4
#define DEBUG4(a) {}
#endif
#define CE_MALLOC  malloc
#ifdef MESSAGE_BOX
#define dm_error(m,j) MessageBox(NULL, m,  "Popen Message", MB_OK)
#else
#define dm_error(m,j) fprintf(stderr, "%s\n", m)
#endif
#endif


#define sprintf _sprintf

/***************************************************************
*
*  Linked list of current popen files.  Gives pclose info it needs
*  to clean up the process.
*
***************************************************************/

struct POPEN_LIST  ;

struct POPEN_LIST {
        struct POPEN_LIST      *next;              /* next data in list                          */
        FILE                   *popen_stream;      /* paste buffer name                          */
        HANDLE                 *process_handle;    /* associated display                         */
        int                     the_pipe[2];
  };

typedef struct POPEN_LIST POPEN_LIST;

static HANDLE   popen_mutex = NULL;
static POPEN_LIST  *popen_head = NULL;
static POPEN_LIST  *popen_tail = NULL;

/************************************************************************

NAME:      ce_popen  -  Working popen for NT


PURPOSE:    This routine performs the function of the popen (_popen on NT)
            library call.  It works on NT Service Pack 3, which the native one
            does not.

PARAMETERS:
   1.  cmd            - pointer to char (INPUT)
                        This is a pointer to the command line to call

   2.  mode           - pointer to char (INPUT)
                        This is the open type.  The first character can be "r"
                        or "w" which indicates the caller of popen will read stdout
                        from the called process or write to stdin on the called
                        process respectively.  The second character is "t" for text
                        mode (default) and "b" for binary mode.


GLOBAL DATA:
   This data is global within execute.c but not visible outside.

   1.  pipe2shell    -  int
                        This is the pipe from this program to stdin on the shell
   
   2.  pipeFromShell - int 
                       This is the pipe from stdout and stderr in the shell to this
                       program.
   
FUNCTIONS :
   1.  Open a pipes to use with the program.

   2.  Convert one end of the pipe to a HANDLE for use by create process.

   3.  Create the SHELL process.

   4.  Save some data for ce_pclose in a linked list held statically in this
       module.

RETURNED VALUE:
   rc    -   FILE *
             NULL  -  Failure, Use GetLastError to generate the message.
             !NULL -  Stream to read or write.

*************************************************************************/

FILE *ce_popen(char *cmd, char *mode)
{
int                   rc = -1;
int                   inheritable_fd;
#define               PIPE_READ_INDEX     0
#define               PIPE_WRITE_INDEX    1
#define               PIPE_BUFFER_SIZE    8192
HANDLE                pipe_handle;
char                  sys_msg[256];
char                  msg[512];
DWORD                 dwCreationFlags;
STARTUPINFO           startup_info;
unsigned int          pipe_mode;
POPEN_LIST           *new_popen;
int                   remote_pipe_end;
int                   local_pipe_end;
DWORD                 dwRc;
PROCESS_INFORMATION   shell_p_info;

DEBUG4(fprintf(stderr, "\nce_popen(%s,%s)\n", cmd, mode);)

if (popen_mutex == NULL)
   {
      popen_mutex = CreateMutex(NULL, FALSE, NULL);
      if (popen_mutex == NULL)
         {
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                          NULL,  /*  pointer to message source, ignored */
                          GetLastError(),
                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                          sys_msg,
                          sizeof(sys_msg),
                          NULL);

            snprintf(msg, sizeof(msg), "ce_popen: CreateMutex failed (%s)", sys_msg);
            dm_error(msg, DM_ERROR_BEEP);
            return(NULL);
         }
   }

new_popen = CE_MALLOC(sizeof(POPEN_LIST));
if (new_popen)
   memset((char *)new_popen, 0, sizeof(POPEN_LIST));
else
   return(NULL); /* message already output */

/***************************************************************
*  Make sure there is something to run
***************************************************************/
if (!cmd | !(*cmd))
   {
      snprintf(msg, sizeof(msg), "ce_popen:  NULL command line");
      dm_error(msg, DM_ERROR_LOG);
      return(NULL);
   }


/***************************************************************
*  Build the pipe which will go to or from the shell.
*  In NT, element 0 is read and element 1 is write.
***************************************************************/
if (mode[1])
   if (mode[1] == 'b')
      pipe_mode = _O_BINARY;
   else
      pipe_mode = _O_TEXT;
else
   pipe_mode = _O_TEXT;

if (_pipe(new_popen->the_pipe, PIPE_BUFFER_SIZE, pipe_mode | _O_NOINHERIT) < 0)
   {
      snprintf(msg, sizeof(msg), "ce_popen:  Could not create pipe (%s)", strerror(errno));
      dm_error(msg, DM_ERROR_LOG);
      return(NULL);
   }

/***************************************************************
*  Parse the direction (pipe to shell or pipe from shell) from
*  the type parameter.
***************************************************************/
if ((*mode == 'w') || (*mode == 'W'))
   {
      remote_pipe_end = PIPE_READ_INDEX;
      local_pipe_end  = PIPE_WRITE_INDEX;
   }
else
   if ((*mode == 'r') || (*mode == 'R'))
   {
      remote_pipe_end = PIPE_WRITE_INDEX;
      local_pipe_end  = PIPE_READ_INDEX;
   }
   else
      {
         snprintf(msg, sizeof(msg), "First character of mode (2nd parm) must be 'r' or 'w' - was '%c'", *mode);
         dm_error(msg, DM_ERROR_LOG);
         close(new_popen->the_pipe[0]);
         close(new_popen->the_pipe[1]);
         free(new_popen);
         return(NULL);
      }


/***************************************************************
*  Get and inheritable copy of the end of the pipe we plan to give
*  to the shell per the example in the manual page for _pipe.
***************************************************************/
if ((inheritable_fd = _dup(new_popen->the_pipe[remote_pipe_end])) < 0)
   {
      snprintf(msg, sizeof(msg), "Could not dup pipe end for shell (%s)", strerror(errno));
      dm_error(msg, DM_ERROR_LOG);
      close(new_popen->the_pipe[0]);
      close(new_popen->the_pipe[1]);
      free(new_popen);
      return(NULL);
   }

/* get rid of the non-inheritable copies of the pipe ends */
close(new_popen->the_pipe[remote_pipe_end]);

/* Convert to file handles for use by Create Process */
pipe_handle  = (HANDLE)_get_osfhandle(inheritable_fd);

if ((long)pipe_handle < 0)
   {
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                    NULL,  /*  pointer to message source, ignored */
                    GetLastError(),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                    sys_msg,
                    sizeof(sys_msg),
                    NULL);
      snprintf(msg, sizeof(msg), "ce_popen:  Could not convert FD to HANDLE (%s)", sys_msg);
      dm_error(msg, DM_ERROR_LOG);
      close(new_popen->the_pipe[local_pipe_end]);
      close(inheritable_fd);
      free(new_popen);
      return(NULL);
   }


DEBUG4( fprintf(stderr, "ce_popen: Calling '%s'\n", cmd);)

/***************************************************************
*  Setup parms to CreateProcess
***************************************************************/
dwCreationFlags = CREATE_NEW_PROCESS_GROUP | CREATE_NEW_CONSOLE;
memset((char *)&startup_info, 0, sizeof(startup_info));
startup_info.cb = sizeof(startup_info);
startup_info.dwFlags = STARTF_USESHOWWINDOW
                       | STARTF_USESTDHANDLES;
startup_info.wShowWindow = SW_HIDE; /* no console window */

if (remote_pipe_end == PIPE_WRITE_INDEX) /* we will read, child will write */
   {
      startup_info.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
      if (startup_info.hStdInput == INVALID_HANDLE_VALUE)
         {
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                          NULL,  /*  pointer to message source, ignored */
                          GetLastError(),
                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                          sys_msg,
                          sizeof(sys_msg),
                          NULL);
            snprintf(msg, sizeof(msg), "ce_popen:  Could not setup stdin handle (%s)", sys_msg);
            dm_error(msg, DM_ERROR_LOG);
            close(new_popen->the_pipe[local_pipe_end]);
            close(inheritable_fd);
            free(new_popen);
            return(NULL);
         }

      startup_info.hStdOutput = pipe_handle;
      startup_info.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
   }
else
   {
      startup_info.hStdInput  = pipe_handle;
      startup_info.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
      if (startup_info.hStdOutput == INVALID_HANDLE_VALUE)
         {
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                          NULL,  /*  pointer to message source, ignored */
                          GetLastError(),
                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                          sys_msg,
                          sizeof(sys_msg),
                          NULL);
            snprintf(msg, sizeof(msg), "ce_popen:  Could not setup stdout handle (%s)", sys_msg);
            dm_error(msg, DM_ERROR_LOG);
            close(new_popen->the_pipe[local_pipe_end]);
            close(inheritable_fd);
            free(new_popen);
            return(NULL);
         }

      startup_info.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
   }

if (startup_info.hStdError == INVALID_HANDLE_VALUE)
   {
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                    NULL,  /*  pointer to message source, ignored */
                    GetLastError(),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                    sys_msg,
                    sizeof(sys_msg),
                    NULL);
      snprintf(msg, sizeof(msg), "ce_popen:  Could not setup stderr handle (%s)", sys_msg);
      dm_error(msg, DM_ERROR_LOG);
      close(new_popen->the_pipe[local_pipe_end]);
      close(inheritable_fd);
      free(new_popen);
      return(NULL);
   }


/***************************************************************
*  Create the shell process
***************************************************************/
if (!CreateProcess(NULL, /* pointer to name of executable module, take from commandline */
                  cmd, /* pointer to command line string */
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
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                       NULL,  /*  pointer to message source, ignored */
                       GetLastError(),
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                       sys_msg,
                       sizeof(sys_msg),
                       NULL);
      snprintf(msg, sizeof(msg), "ce_popen: CreateProcess failed (%s) - \"%s\"", sys_msg, cmd);
      dm_error(msg, DM_ERROR_LOG);
      close(new_popen->the_pipe[local_pipe_end]);
      close(inheritable_fd);
      free(new_popen);
      return(NULL);
   }

new_popen->process_handle = shell_p_info.hProcess;
CloseHandle(shell_p_info.hThread);

if (_close(inheritable_fd) != 0)
   {
      snprintf(msg, sizeof(msg), "ce_popen:  close 1 failed: (%s)", strerror(errno));
      dm_error(msg, DM_ERROR_LOG);
   }

/***************************************************************
*  Convert the pipe end to a pointer to stream (FILE *)
***************************************************************/
if ((new_popen->popen_stream = _fdopen(new_popen->the_pipe[local_pipe_end], mode)) == NULL)
   {
      snprintf(msg, sizeof(msg), "ce_popen:  fdopen failed: (%s)", strerror(errno));
      dm_error(msg, DM_ERROR_LOG);
   }

dwRc = WaitForSingleObject(popen_mutex, INFINITE);
if (dwRc == WAIT_FAILED)
   {
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                    NULL,  /*  pointer to message source, ignored */
                    GetLastError(),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                    sys_msg,
                    sizeof(sys_msg),
                    NULL);
       snprintf(msg, sizeof(msg), "ce_popen:  WaitForSingleObject failed (%s)", sys_msg);
       dm_error(msg, DM_ERROR_LOG);
   }

if (popen_head == NULL)
   popen_head = new_popen;
else
   popen_tail->next = new_popen;

popen_tail = new_popen;

if (!ReleaseMutex(popen_mutex))
   {
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                    NULL,  /*  pointer to message source, ignored */
                    GetLastError(),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                    sys_msg,
                    sizeof(sys_msg),
                    NULL);
      snprintf(msg, sizeof(msg), "ce_popen: ReleaseMutex failed (%s)", sys_msg);
      dm_error(msg, DM_ERROR_LOG);
   }

return(new_popen->popen_stream);

} /* ce_popen() */


/************************************************************************

NAME:      ce_pclose  -  Working pclose for NT


PURPOSE:    This routine performs the function of the pclose (_pclose on NT)
            library call.  It works on NT Service Pack 3, which the native one
            does not.

PARAMETERS:
   1.  cmd            - pointer to char (INPUT)
                        This is a pointer to the command line to call

   2.  mode           - pointer to char (INPUT)
                        This is the open type.  The first character can be "r"
                        or "w" which indicates the caller of popen will read stdout
                        from the called process or write to stdin on the called
                        process respectively.  The second character is "t" for text
                        mode (default) and "b" for binary mode.


GLOBAL DATA:
   This data is global within execute.c but not visible outside.

   1.  pipe2shell    -  int
                        This is the pipe from this program to stdin on the shell
   
   2.  pipeFromShell - int 
                       This is the pipe from stdout and stderr in the shell to this
                       program.
   
FUNCTIONS :
   1.  Open a pipes to use with the program.

   2.  Convert one end of the pipe to a HANDLE for use by create process.

   3.  Create the SHELL process.

   4.  Save some data for ce_pclose in a linked list held statically in this
       module.

RETURNED VALUE:
   rc    -   FILE *
             NULL  -  Failure, Use GetLastError to generate the message.
             !NULL -  Stream to read or write.

*************************************************************************/

int ce_pclose(FILE *stream)
{
int                   rc = 0;
char                  sys_msg[256];
char                  msg[512];
POPEN_LIST           *new_popen;
POPEN_LIST           *last_popen = NULL;
DWORD                 dwRc;

DEBUG4(fprintf(stderr, "\nce_pclose(%s,%s)\n");)

dwRc = WaitForSingleObject(popen_mutex, INFINITE);
if (dwRc == WAIT_FAILED)
   {
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                    NULL,  /*  pointer to message source, ignored */
                    GetLastError(),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                    sys_msg,
                    sizeof(sys_msg),
                    NULL);
       snprintf(msg, sizeof(msg), "ce_pclose:  WaitForSingleObject failed (%s)", sys_msg);
       dm_error(msg, DM_ERROR_LOG);
   }

for (new_popen = popen_head; new_popen; new_popen = new_popen->next)
   if (new_popen->popen_stream = stream)
      break;
   else
      last_popen = new_popen;

if (new_popen)
   {
      if (last_popen)
         last_popen->next = new_popen->next;
      else
         popen_head = new_popen->next;
   }

if (!ReleaseMutex(popen_mutex))
   {
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                    NULL,  /*  pointer to message source, ignored */
                    GetLastError(),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                    sys_msg,
                    sizeof(sys_msg),
                    NULL);
      snprintf(msg, sizeof(msg), "ce_pclose: ReleaseMutex failed (%s)", sys_msg);
      dm_error(msg, DM_ERROR_LOG);
   }

if (new_popen)
   {

      fclose(new_popen->popen_stream);

      dwRc = WaitForSingleObject(new_popen->process_handle, 10000);
      if (dwRc == WAIT_FAILED)
         {
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                          NULL,  /*  pointer to message source, ignored */
                          GetLastError(),
                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                          sys_msg,
                          sizeof(sys_msg),
                          NULL);
             snprintf(msg, sizeof(msg), "ce_pclose:  WaitForSingleObject failed on process wait (%s)", sys_msg);
             dm_error(msg, DM_ERROR_LOG);
         }

      if (dwRc == WAIT_TIMEOUT)
         {
            if (!TerminateProcess(new_popen->process_handle, 1)) // exit code for the process 
               {
                  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                                NULL,  /*  pointer to message source, ignored */
                                GetLastError(),
                                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                                sys_msg,
                                sizeof(sys_msg),
                                NULL);
                  snprintf(msg, sizeof(msg), "ce_pclose:  TerminateProcess failed (%s)", sys_msg);
                  dm_error(msg, DM_ERROR_BEEP);
                  return(-1);
               }
         }

      CloseHandle(new_popen->process_handle);
   }
else
   {
      snprintf(msg, sizeof(msg), "ce_pclose: passed file was not opened by ce_popen");
      dm_error(msg, DM_ERROR_LOG);
      rc = -1;
      errno = ENOENT;
   }

return(rc);

} /* ce_pclose() */

