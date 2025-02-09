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
*  module execute.c
*
*  Routines:
*     dm_ce              - Execute the ce, cv, cp and ceterm commands
*     dm_cpo             - Execute the cpo and cps commands
*     dm_bang            - ! command
*     dm_bang_lite       - The ! command used from .Cekeys
*     spoon              - Fork the edit session to background
*     dm_env             - DM env command, do a putenv
*     reap_child_process - Clean up terminated child processes
*
*  INTERMAL
*     stderr_to_dmoutput -  Paste a file in the dmoutput window.
*     trim_pad           -  Trim blank lines off the bottom of a pad
*     reset_child_files  -  Repoint stdin, stdout, and stderr in a child process.
*     bang_system        -  Version of the system command for use with dm_bang.
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h       */
#include <string.h>         /* /usr/include/string.h      */
#include <errno.h>          /* /usr/include/errno.h       */
#include <sys/types.h>      /* /usr/include/sys/types.h   */
#include <sys/stat.h>       /* /usr/include/sys/stat.h    */
#include <fcntl.h>          /* /usr/include/fcntl.h       */
#include <signal.h>         /* /usr/include/signal.h      */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
#include "usleep.h"
#endif

#ifdef OMVS
/* needed cause GNU compiler supplies signum.h which is short these */
#ifndef SIGHUP
#define SIGHUP    1
#endif
#ifndef SIGCHLD
#define SIGCHLD  20
#endif
#ifndef SIGQUIT
#define SIGQUIT  24
#endif
#endif
#ifndef OMVS
#include <sys/param.h>      /* /usr/include/sys/param.h   */
#endif
#ifndef MAXPATHLEN
#define MAXPATHLEN   1024
#endif
#ifndef FOPEN_MAX
#define FOPEN_MAX   64
#endif
#ifndef OMVS
#define _NO_PROTO    /* DEC DEPRIVATION */
#endif
#include <sys/wait.h>       /* /usr/include/sys/wait.h    */
#ifndef OMVS
#undef  _NO_PROTO
#endif


#include <X11/Xlib.h>       /* /usr/include/X11/Xlib.h    */

#include "display.h"
#include "dmsyms.h"
#include "dmwin.h"
#include "emalloc.h"
#include "execute.h"
#include "getevent.h"
#ifndef HAVE_CONFIG_H
#include "help.h"
#endif
#include "init.h"
#include "kd.h"
#include "mark.h"
#include "memdata.h"
#include "mvcursor.h"
#include "normalize.h"
#ifdef PAD
#include "pad.h"
#else
#define SHELL "/bin/sh"
#endif
#include "parms.h"
#include "parsedm.h"
#include "pastebuf.h"
#include "pw.h"
#include "record.h"         /* needed to turn recording off in child after forks */
#include "serverdef.h"
#include "str2argv.h"
#include "txcursor.h"
#ifdef PAD
#include "unixpad.h"
#include "unixwin.h"
#endif
#include "wdf.h"
#include "window.h"
#include "windowdefs.h"
#include "xc.h"


/***************************************************************
*  
*  System routines which have no built in defines
*  
***************************************************************/

int    fork(void);
int    getppid(void);
int    getpid(void);
#if !defined(IBMRS6000) && !defined(linux)
int    setpgrp(int pid, int pgrp);  /* bsd version */
void   close(int fd);
#endif
#ifndef OMVS
void   exit(int code);
#endif
#if !defined(FREEBSD) && !defined(WIN32) && !defined(_OSF_SOURCE) && !defined(OMVS)
char  *getenv(char *name);
int    putenv(char *name);
#endif
#ifndef PAD
char  *getcwd(char *target, int maxlen);
#endif


#ifdef IBMRS6000
#define  setpgrp   tcsetpgrp
#endif


#define MAX_CV_ARGS 255


/* from generated routine link_date.c */
extern char   *compile_date;
extern char   *compile_time;



static void stderr_to_dmoutput(int                init_dmoutput_pos,
                               DISPLAY_DESCR     *dspl_descr);


static void trim_pad(PAD_DESCR         *dmoutput_pad,
                     int                init_dmoutput_pos);

static void  reset_child_files(char   *paste_dir,
                               int     is_server,
                               int     is_debug,
                               int     save_stdout);


static int bang_system(char    *s,
                       char    *shell_opts,
                       char    *stdin_path,
                       char    *stdout_path,
                       char    *stderr_path);

#ifdef OpenNT
extern int setpgrp(int i, int j){ return(0);}  /* KBP-NT 4/16/1998 */
#endif

/************************************************************************

NAME:      dm_ce  DM ce, cv and cp(ceterm) commands

PURPOSE:    This routine invokes the ce, cv, or ceterm program on a file.

PARAMETERS:

   1.  dmc         - pointer to DMC (INPUT)
                     This is the command structure for the ce or cv command.

   2.  dspl_descr  - pointer to DISPLAY_DESCR (INPUT / OUTPUT in the child process)
                     This is the description for the display.  This routine needs to
                     get to all the pad descriptions.
                     It is reconfigured to the new file.


FUNCTIONS :

   1.   Handle special cases such as ce -ver, readlock set (no fork's allowed),
        and null parms which slipped through.

   2.   Fork(), the parent just returns.
   
   3.   Find the current execution path.  If we cannot find it, bail.

   4.   Close out all the file descriptors, become a separate process group,

   5.   Set the environment variable so messages will come back to the paren
        window until the child is intialized.  The variable is looked at by
        the dm_error routine.

   6.   Exec ourselves with the options passed.


RETURNED VALUE:
   child  -  The returned value indicates whether this is the parent or the child
             0  -  child
            ~0  -  the parent (child pid is returned) or -1 on failure.
        

*************************************************************************/

int   dm_ce(DMC             *dmc,                    /* input  */
            DISPLAY_DESCR   *dspl_descr)             /* input / output  in child process */
{
char                  cmd[MAX_LINE+10];
int                   i;
int                   pid;
char                  msg[MAXPATHLEN];
char                 *p;
char                 *exec_path;
char                  link[MAXPATHLEN];
char                 *old_display;

DEBUG4(
   fprintf(stderr, "cmd = %s\n",  dmsyms[dmc->ce.cmd].name);
   for (i = 0; i < dmc->cv.argc; i++)
      fprintf(stderr, "argv[%02d] = %s\n", i, dmc->cv.argv[i]);
)

/***************************************************************
*  
*  If readlock is set, do not allow ce, cp, etc.
*  
***************************************************************/

if (READLOCK)
   {
      dm_error_dspl("cv, ce, cp, and ceterm commands not available", DM_ERROR_BEEP, dspl_descr);
      return(-1);
   }

/***************************************************************
*  
*  Handle the case of the null parm, should have been weeded out by parser.
*  
***************************************************************/
if ((dmc->ce.cmd != DM_cp) && (dmc->ce.cmd != DM_ceterm))
   if (dmc->ce.argc <= 1 || (dmc->ce.argc == 2 && (dmc->ce.argv[1][0] == '\0')))
      return(-1);


/***************************************************************
*  
*  Handle ce -ver as a special case
*  
***************************************************************/

if (dmc->ce.argc == 2 && (strncmp(dmc->ce.argv[1], "-ve", 3) == 0))
   {
      snprintf(link, sizeof(link), PACKAGE_STRING, VERSION);
      snprintf(msg, sizeof(msg), "%s  Compiled: %s %s", link, compile_date, compile_time);
      dm_error_dspl(msg, DM_ERROR_MSG, dspl_descr);
      return(-1);
   }

/***************************************************************
*  
*  Fork,  If we are the parent, we are done.
*  If we are not the parent, we will get rid of the old file,
*  and start a new one.
*  
***************************************************************/

signal(SIGHUP,  SIG_IGN);                               

pid = fork();
if (pid < 0)
   {
      snprintf(cmd, sizeof(cmd), "(%s) Can't fork:  %s", dmsyms[dmc->ce.cmd].name, strerror(errno));
      dm_error_dspl(cmd, DM_ERROR_BEEP, dspl_descr);
      return(-1);
   }
if (pid != 0)
   {
      DEBUG4(fprintf(stderr, "%s Parent: child pid = %d, ppid = %d\n", dmsyms[dmc->ce.cmd].name, pid, getppid());)
   }
else
   {
      /***************************************************************
      *  
      *  The child process.
      *  First make us look like a cv or ce to get_execution_path to
      *  determine the correct name.
      *  
      ***************************************************************/

      WRITABLE(dspl_descr->main_pad->token) = (dmc->ce.cmd == DM_ce);
      dspl_descr->pad_mode = ((dmc->ce.cmd == DM_cp) || (dmc->ce.cmd == DM_ceterm));

      exec_path = get_execution_path(dspl_descr,  dmsyms[dmc->ce.cmd].name); /* never returns null */

      DEBUG4(fprintf(stderr, "Will execv \"%s\"\n", exec_path);)
      if (!exec_path || !(*exec_path))
         {
            snprintf(msg, sizeof(msg), "(%s) Cannot find ce/ceterm path to execute, pad mode = %d", dmsyms[dmc->ce.cmd].name, dspl_descr->pad_mode);
            dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
            exit(1);
         }

      /***************************************************************
      *  
      *  Make the current display the default.
      *  
      ***************************************************************/
      old_display = getenv("DISPLAY");
      if (!old_display)
         old_display = ""; /* avoid memory fault */
      if (strcmp(old_display, DisplayString(dspl_descr->display)) != 0)
         {
            snprintf(link, sizeof(link), "DISPLAY=%s", DisplayString(dspl_descr->display));
            DEBUG4(fprintf(stderr, "%s Child: Using %s\n", dmsyms[dmc->ce.cmd].name, link);)
            p = malloc_copy(link);
            if (p)
               putenv(p);
            else
               return(-1); /* out of memory, msg already output */
         }

      /***************************************************************
      *  
      *  Close all the file descriptors and show no more display
      *  
      ***************************************************************/
      for (i = 3; i < FOPEN_MAX; i++)
         close(i);
      dspl_descr->dmoutput_pad->x_window = None; /* disable dm_error, no more window */
      dspl_descr->display = NULL;

      /***************************************************************
      *  Become our own process group.
      ***************************************************************/
#if defined(PAD) && !defined(ultrix)
      isolate_process(getpid());
#endif
/* ifdef added 9/9/2004 for Fedora Core 2 */
#ifdef linux
      (void) setpgrp();
#else
      (void) setpgrp(0, getpid());
#endif
      
      DEBUG4(fprintf(stderr, "%s Child(1): pid = %d, ppid = %d\n", dmsyms[dmc->ce.cmd].name, getpid(), getppid());)

      /***************************************************************
      *  The parent is not waiting for us, detach from the parent.
      ***************************************************************/
      signal(SIGHUP,  SIG_DFL);

#ifdef SIGTSTP
      signal(SIGTSTP, SIG_IGN);
#endif
      /*mem_kill(dspl_descr->main_pad->token); RES, forces read in whole memdata */

      /***************************************************************
      *  
      *  Set the variable to let the messages come back to this
      *  window until the new window gets going.
      *  
      ***************************************************************/
       
      dmc->cv.argv[0] = exec_path;
      /* this tells the child process how to send messages back to this ce session during startup */
      snprintf(link, sizeof(link), "%s=%X", DMERROR_MSG_WINDOW, dspl_descr->main_pad->x_window);
      p = malloc_copy(link);
      if (p)
         putenv(p);

      /***************************************************************
      *  
      *  Process any $ variables in path names, if required.
      *  
      ***************************************************************/
      if (ENV_VARS)
         for (i=1; i<dmc->cv.argc; i++)
             if (strchr(dmc->cv.argv[i], '$'))
                {
                   strcpy(link, dmc->cv.argv[i]);
                   substitute_env_vars(link, sizeof(link), dspl_descr->escape_char);
                   if (strcmp(dmc->cv.argv[i], link) != 0)
                      {
                         p = malloc_copy(link);
                         if (p)
                            dmc->cv.argv[i] = p;
                      }
                }
#ifdef linux
      usleep(500000); /* prevents dead shells on Linux */
#endif
      /***************************************************************
      *  
      *  Exec the ce/cv command
      *  
      ***************************************************************/
      execv(exec_path, dmc->cv.argv);

      /* should never get here */

      snprintf(msg, sizeof(msg), "(%s) Call to execv failed: %s", dmsyms[dmc->ce.cmd].name, strerror(errno));
      dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);

      exit(23);
   } /* end of child process code */


return(pid);

} /* end of dm_ce  */


/************************************************************************

NAME:      dm_cpo  DM cpo and cps commands

PURPOSE:    This routine invokes the another executable.  cps has the child do
            a no-hup.

PARAMETERS:

   1.  dmc            - pointer to DMC (INPUT)
                        This is the command structure for the cpo or cps command.
                        It contains the wait flag and the program argc/argv.
                        {cpo | cps} [-w] [-d] [-s] "command arg1 arg2 ... "
                        -w  -  have Ce wait for the process to complete
                        -d  -  Debug mode, leave stderr and stdout alone
                        -s  -  Save stdout and stderr to BangOut and BangErr (implied by -w)

   2.  dspl_descr  - pointer to DISPLAY_DESCR (INPUT / OUTPUT in the child process)
                     This is the description for the display.  This routine needs to
                     get to all the pad descriptions.
                     It is reconfigured to the new file.


FUNCTIONS :

   1.   If the argv[0] passed does not start with slash, search $PATH
        for it.

   2.   Verify that the program to run exists and is executable.

   3.   Fork() and check to see the fork succeeded.
   
   4.   The parent checks the wait flag.  If -w was specified, the parent
        waits for the child and reports any abnormal termination in the child.

   5.   The child process checks to see if this is a cps.  If so, it closes out
        stdin, stdout, and stderr and requests the system to ignore SIGHUP's
        The child then does an execv to the program to be called.
 


*************************************************************************/

void dm_cpo(DMC               *dmc,
            DISPLAY_DESCR     *dspl_descr)
{
char                  path_name[MAXPATHLEN];
char                  msg[MAXPATHLEN+256];
int                   i;
int                   pid;
int                   wait_pid;
int                   status_data;
char                  *paste_dir;
int                   init_dmoutput_pos;
int                   save_line;
int                   save_col;
PAD_DESCR            *save_buffer;
void                  (*cstat)();
#ifdef SIGCLD
#if (SIGCLD != SIGCHLD)
void                  (*c1stat)();
#endif
#endif
char                 *lcl_argv[MAX_CV_ARGS];
int                   lcl_argc;
char                **lcl_argvp;
DISPLAY_DESCR        *walk_dspl;
char                 *p;
#ifdef WIN32
char                  separator = ';';
#else
char                  separator = ':';
#endif

/***************************************************************
*  
*  If readlock is set, do not allow cpo or cps
*  
***************************************************************/

if (READLOCK)
   {
      dm_error_dspl("cpo and cps commands not available", DM_ERROR_BEEP, dspl_descr);
      return;
   }

/***************************************************************
*  
*  Check to see if the command was entered in cp format.
*  that is: the command name and parms in one quoted argument.
*  We check this by noting that there is only one argument
*  and that the argument contains blanks or tabs.  This means
*  that we cannot execute commands with tabs or blanks in the
*  name unless they are quoted two deep.
*  
***************************************************************/

if ((dmc->cpo.argc == 1) && ((strchr(dmc->cpo.argv[0], ' ') != NULL) || (strchr(dmc->cpo.argv[0], '\t') != NULL)))
   {
      i = char_to_argv(NULL,
                       dmc->cpo.argv[0],
                       dmsyms[dmc->any.cmd].name,
                       dmc->cpo.argv[0],
                       &lcl_argc,
                       &lcl_argvp,
                       dspl_descr->escape_char); /* in parsedm.c */
      if (i != 0)
         return; /* message already output */
   }
else
   {
      if (dmc->cpo.argc >= MAX_CV_ARGS-1)
         {
            snprintf(msg, sizeof(msg), "(%s) Too many args %d", dmsyms[dmc->any.cmd].name, dmc->cpo.argc);
            dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
            lcl_argc = MAX_CV_ARGS-1;
         }
      else
         lcl_argc = dmc->cpo.argc;

      for (i = 0; i < lcl_argc; i++)
         lcl_argv[i] = dmc->cpo.argv[i];
      lcl_argv[lcl_argc] = NULL;
      lcl_argvp = lcl_argv;
   }


/***************************************************************
*  
*  First see if lcl_argvp[0] is a command or a path name.
*  If command, search the path.  If a path name, make sure
*  it is executable.
*  
***************************************************************/

search_path_n(lcl_argvp[0], sizeof(path_name), path_name, "PATH", separator); /* in normalize.h */
if (path_name[0] == '\0')
   {
      snprintf(msg, sizeof(msg), "(%s) Cannot execute %s (%s)", dmsyms[dmc->any.cmd].name, lcl_argvp[0], strerror(errno));
      dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
      if (lcl_argvp != lcl_argv)
         {
            for (i = 0; i < lcl_argc; i++)
               free(lcl_argvp[i]);
            free((char *)lcl_argvp);
         }
       return;
   }

/***************************************************************
*  
*  Fork a new process.
*  
***************************************************************/

if (dmc->cpo.dash_w)
   {
      cstat = signal(SIGCHLD, SIG_DFL);                               
#ifdef SIGCLD
#if (SIGCLD != SIGCHLD)
      c1stat = signal(SIGCLD, SIG_DFL);                               
#endif
#endif
   }


pid = fork();
if (pid == -1)
   {
      snprintf(msg, sizeof(msg), "(%s) Can't fork:  %s", dmsyms[dmc->xc.cmd].name, strerror(errno));
      dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
      return;
   }

if (pid != 0)
   {
      /***************************************************************
      *  
      *  The parent process.
      *
      *  First free the malloc'ed argv if necessary.  We do not need it.
      *
      *  If we are to do a wait, do the wait.
      *  Otherwise, just return.
      *  
      ***************************************************************/

      if (lcl_argvp != lcl_argv)
         {
            for (i = 0; i < lcl_argc; i++)
               free(lcl_argvp[i]);
            free((char *)lcl_argvp);
         }

      DEBUG4(fprintf(stderr, "%s Parent: child pid = %d, ppid = %d\n", dmsyms[dmc->ce.cmd].name, pid, getppid());)
      if (dmc->cpo.dash_w)
         {
            wait_pid = wait(&status_data);
            while(wait_pid > 0 && wait_pid != pid)
               wait_pid = wait(&status_data);

#ifdef SIGCLD
#if (SIGCLD != SIGCHLD)
            signal(SIGCLD, c1stat);                               
#endif
#endif
            signal(SIGCHLD, cstat);  /* order is important, filo */                             
            mark_point_setup(NULL,                    /* input  */
                             dspl_descr->cursor_buff, /* input  */
                             &save_line,              /* output */
                             &save_col,               /* output */
                             &save_buffer);           /* output */

            dm_clean(True);  /* make sure dmoutput window starts out empty */
            init_dmoutput_pos = total_lines(dspl_descr->dmoutput_pad->token);

            stderr_to_dmoutput(init_dmoutput_pos, dspl_descr);

            dm_position(dspl_descr->cursor_buff, save_buffer, save_line, save_col);


            if (wait_pid < 0)
               {
                  snprintf(msg, sizeof(msg), "Wait terminated by signal (%s)", strerror(errno));
                  dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
               }
            else
               if (status_data == 0)
                  return;
               else
                  if ((status_data & 0x000000ff) == 0)
                     {
                        snprintf(msg, sizeof(msg), "Process exited, code %d", status_data >> 8);
                        dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
                     }
                  else
                     if ((status_data & 0x0000ff00) == 0)
                        {
                           snprintf(msg, sizeof(msg), "Process aborted, signal code %d", status_data);
                           dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
                        }
                     else
                        {
                           snprintf(msg, sizeof(msg), "Process killed, signal code %d", status_data >> 8);
                           dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
                        }
            
         }
      else
         return;
   } /* end of is parent */
else
   {
      /***************************************************************
      *  
      *  The child process.
      *
      *  if this is a CPS, break off from the parent.
      *  cps with a -w is a cpo really.
      *  
      ***************************************************************/

      /***************************************************************
      *  If command recording was on in the parent, turn it off.
      *  Also, disconnect from the X server.
      ***************************************************************/
      dm_rec(NULL, dspl_descr);
      close(ConnectionNumber(dspl_descr->display));
      /* RES 11/15/95 make sure no dm_error calls try to use the X server */ 
      dspl_descr->dmoutput_pad->x_window = None; /* disable dm_error, no more window */
      dspl_descr->display = NULL;
      for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
      {
         dm_rec(NULL, walk_dspl);
         close(ConnectionNumber(walk_dspl->display));
         walk_dspl->dmoutput_pad->x_window = None; /* disable dm_error, no more window */
         walk_dspl->display = NULL;
      }

      signal(SIGCHLD, SIG_DFL);                               
#ifdef SIGCLD
#if (SIGCLD != SIGCHLD)
      signal(SIGCLD, SIG_DFL);                               
#endif
#endif

      DEBUG4(fprintf(stderr, "%s Child(1): pid = %d, ppid = %d\n", dmsyms[dmc->ce.cmd].name, getpid(), getppid());)
      paste_dir = setup_paste_buff_dir(dmsyms[dmc->any.cmd].name);
      reset_child_files(paste_dir,
                        (dmc->cpo.cmd == DM_cps) && (!dmc->cpo.dash_w),
                        dmc->cpo.dash_d,
                        dmc->cpo.dash_s);
#if defined(PAD) && !defined(ultrix)
      isolate_process(getpid());
#endif
      if (dmc->cpo.cmd == DM_cps)
         {
            if (!dmc->cpo.dash_w)
               {
/* ifdef added 9/9/2004 for Fedora Core 2 */
#ifdef linux
                  (void) setpgrp();
#else
                  (void) setpgrp(0, getpid());
#endif
               }
            signal(SIGHUP,  SIG_IGN);                               
         }
      else
         signal(SIGHUP,  SIG_DFL);                               

#ifdef SIGTSTP
      signal(SIGTSTP, SIG_IGN);
#endif
            

      DEBUG4(
         fprintf(stderr, "path     = %s\n", path_name);
         for (i = 0; i < lcl_argc; i++)
            fprintf(stderr, "argv[%02d] = %s\n", i, lcl_argvp[i]);
      )

      /***************************************************************
      *  Save the main window id in the environment for the ce api.
      ***************************************************************/
      snprintf(msg, sizeof(msg), "CE_WINDOW=%08X", dspl_descr->main_pad->x_window);
      p = malloc_copy(msg);
      if (p)
         putenv(p);

      /***************************************************************
      *  Save the expression mode in an environment variable.
      ***************************************************************/
      snprintf(msg, sizeof(msg), "CE_EXPR=%c", (UNIXEXPR ? 'U' : 'A'));
      p = malloc_copy(msg);
      if (p)
         putenv(p);

      /***************************************************************
      *  Make sure we do not have a saved uid of root.
      ***************************************************************/
      if (getuid())
         setuid(getuid());

      execv(path_name, lcl_argvp);

      /* should never get here */

      snprintf(msg, sizeof(msg), "(%s) Call to execv failed: %s", dmsyms[dmc->ce.cmd].name, strerror(errno));
      dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);

      exit(22);

   } /* end of else the child */


} /* end of dm_cpo */


/************************************************************************

NAME:      dm_bang -  the ! command

PURPOSE:    This routine cuts a region of text into a paste buffer,
            executes a command using the paste buffer as stdin, and
            pastes any stdout from the command into the region cut.

PARAMETERS:
   1.  dmc           -  pointer to DMC (INPUT)
                        This is the command structure for the ce or cv command.
                        The dash options are currently ignored.

   2.  dspl_descr    -  pointer to DISPLAY_DESCR (INPUT)
                        This is the current display description.  It is used to
                        do the cut/copy/pasting as well as access various
                        parameters.

FUNCTIONS :
   1.   Validate that if a write is to be done, the pad is writable.

   2.   Set up the range to be cut or copied and do the cut or copy

   3.   Build the special 'bang' paste buffer names and call the system
        command passing them as stdin, stdout, and stderr.

   4.   Paste the results back in the window if required.

   5.   Execute stdout from the command as a cmdf file if required.
   
   6.   Copy the output from stdout to the message window if required.

   7.   Copy stderr to the output message window.

RETURNED VALUE:
   new_dmc -  Pointer to DMC
              If the bang command generates DMC commands(-e), they are returned. 
              NULL is returned on error or no DMC's

*************************************************************************/

DMC   *dm_bang(DMC               *dmc,                /* input   */
              DISPLAY_DESCR     *dspl_descr)         /* input   */
{
DMC                  *new_dmc = NULL;
DMC                   built_cmd;
char                 *paste_dir;
char                  cut_name[MAXPATHLEN];
char                  paste_name[MAXPATHLEN];
char                  error_name[MAXPATHLEN];
int                   rc;
int                   top_line;
int                   top_col;
int                   bottom_line;
int                   bottom_col;
PAD_DESCR            *buffer;
int                   save_line;
int                   save_col;
PAD_DESCR            *save_buffer;
struct stat           file_stats;
int                   init_dmoutput_pos;
int                   save_echo_mode;
int                   save_mark_set;

/***************************************************************
*  
*  If readlock is set, do not allow bang (!)
*  
***************************************************************/

if (READLOCK)
   {
      dm_error_dspl("bang (!) command not available", DM_ERROR_BEEP, dspl_descr);
      return(NULL);
   }

/***************************************************************
*  
*  Validate that if a write is to be done, the pad is writable.
*  
***************************************************************/

if (!WRITABLE(dspl_descr->cursor_buff->current_win_buff->token) && !dmc->bang.dash_c)
   {
      dm_error_dspl("(!) Text is read-only", DM_ERROR_BEEP, dspl_descr);
      stop_text_highlight(dspl_descr);
      dspl_descr->echo_mode = NO_HIGHLIGHT;
      dspl_descr->mark1.mark_set = False;
      return(new_dmc);
   }
else
   {
      save_line = check_and_add_lines(dspl_descr->cursor_buff);
      if (!WRITABLE(dspl_descr->cursor_buff->current_win_buff->token))
         dspl_descr->cursor_buff->current_win_buff->file_line_no -= save_line;
   }

dm_clean(True);  /* make sure dmoutput window starts out empty */
init_dmoutput_pos = total_lines(dspl_descr->dmoutput_pad->token);
save_echo_mode    = dspl_descr->echo_mode;
save_mark_set     = dspl_descr->mark1.mark_set;

/***************************************************************
*  Make sure the range is in the same window.
***************************************************************/
rc = range_setup(&dspl_descr->mark1,      /* input   */
                 dspl_descr->cursor_buff, /* input   */
                 &top_line,               /* output  */
                 &top_col,                /* output  */
                 &bottom_line,            /* output  */
                 &bottom_col,             /* output  */
                 &buffer);                /* output  */

if (rc != 0)
   {
      dm_error_dspl("(!) Invalid Text Range", DM_ERROR_BEEP, dspl_descr);
      stop_text_highlight(dspl_descr);
      dspl_descr->echo_mode = NO_HIGHLIGHT;
      dspl_descr->mark1.mark_set = False;
      return(new_dmc);
   }

/***************************************************************
*  
*  First build the name of the paste buffer to use.
*  
***************************************************************/

paste_dir = setup_paste_buff_dir(dmsyms[dmc->any.cmd].name);
if (paste_dir == NULL)
   {
      stop_text_highlight(dspl_descr);
      dspl_descr->echo_mode = NO_HIGHLIGHT;
      dspl_descr->mark1.mark_set = False;
      return(new_dmc);
   }

/***************************************************************
*  
*  Save the spot to return to.
*  
***************************************************************/

mark_point_setup(&dspl_descr->mark1,      /* input  */
                 dspl_descr->cursor_buff, /* input  */
                 &save_line,              /* output */
                 &save_col,               /* output */
                 &save_buffer);           /* output */


/***************************************************************
*  
*  Build the two paste buffer names and the command to pass to system.
*  
***************************************************************/

snprintf(cut_name, sizeof(cut_name),   "%s/%s",  paste_dir, BANGIN_PASTEBUF);
snprintf(paste_name, sizeof(paste_name), "%s/%s", paste_dir, BANGOUT_PASTEBUF);
snprintf(error_name, sizeof(error_name), "%s/%s", paste_dir, ERR_PASTEBUF);

/***************************************************************
*  
*  Cut the marked region into the first paste buffer.
*  
***************************************************************/

memset(&built_cmd, 0, sizeof(built_cmd));
built_cmd.next      = NULL;
if ((dmc->bang.dash_c) || (dmc->bang.dash_e))
   built_cmd.xc.cmd    = DM_xc;
else
   built_cmd.xc.cmd    = DM_xd;
built_cmd.xc.temp   = False;
built_cmd.xc.dash_f = True;
built_cmd.xc.dash_a = False;
built_cmd.xc.dash_r = False;
built_cmd.xc.path   = cut_name;

dm_xc(&built_cmd,
      &dspl_descr->mark1,
      dspl_descr->cursor_buff,
      dspl_descr->display,
      dspl_descr->main_pad->x_window,
      dspl_descr->echo_mode,
      None,
      dspl_descr->escape_char);

/***************************************************************
*  We need to keep the position marked.
***************************************************************/
dspl_descr->echo_mode      = save_echo_mode;
dspl_descr->mark1.mark_set = save_mark_set;

/***************************************************************
*  
*  Call system to execute the passed in command.
*  If the rc is bad, flag the fact, but keep going.
*  
***************************************************************/

DEBUG4(fprintf(stderr, "!: %s\n", dmc->bang.cmdline);)
bang_system(dmc->bang.cmdline, dmc->bang.dash_s, cut_name, paste_name, error_name);

/***************************************************************
*  
*  If a paste buffer was created, paste its contents into the
*  marked area.  If -c was specified, we do not do any paste
*  unless the -m (message) option was specified.  -m always
*  tells us to write the output in the DM output window.
*  
***************************************************************/

if ((!dmc->bang.dash_c && !dmc->bang.dash_e) || dmc->bang.dash_m)
   {
      rc = stat(paste_name, &file_stats);

      /***************************************************************
      *  
      *  If there is a BangOut file and it contains data,
      *  do the paste.
      *  
      ***************************************************************/

      if (rc == 0 && file_stats.st_size > 0)
         {
            if (!dmc->bang.dash_m)
               {
                   rc = range_setup(&dspl_descr->mark1,      /* input   */
                                    dspl_descr->cursor_buff, /* input   */
                                    &top_line,               /* output  */
                                    &top_col,                /* output  */
                                    &bottom_line,            /* output  */
                                    &bottom_col,             /* output  */
                                    &buffer);                /* output  */ 
                   if (rc != 0)
                      {
                         snprintf(paste_name, sizeof(paste_name), "(%s) Invalid Text Range", dmsyms[dmc->xc.cmd].name);
                         dm_error_dspl(paste_name, DM_ERROR_BEEP, dspl_descr);
                         return(0);
                      }

                   if ((dspl_descr->echo_mode == RECTANGULAR_HIGHLIGHT) && (top_col > bottom_col))
                      top_col = bottom_col;

                   (void) dm_position(dspl_descr->cursor_buff, buffer, top_line, top_col);
               }
            else
               {
                  dm_error_dspl(" ", DM_ERROR_MSG, dspl_descr);
                  (void) dm_position(dspl_descr->cursor_buff, dspl_descr->dmoutput_pad, total_lines(dspl_descr->dmoutput_pad->token)-1, 0);
               }
            /***************************************************************
            *  Set up for the paste.  Set up the line into the work buffer.
            ***************************************************************/
            strcpy(dspl_descr->cursor_buff->current_win_buff->work_buff_ptr, dspl_descr->cursor_buff->current_win_buff->buff_ptr);
            dspl_descr->cursor_buff->current_win_buff->buff_ptr = dspl_descr->cursor_buff->current_win_buff->work_buff_ptr;

            /***************************************************************
            *  Paste the BangOut buffer in either the input or output
            *  windows.
            ***************************************************************/
            memset(&built_cmd, 0, sizeof(built_cmd));
            built_cmd.xp.cmd    = DM_xp;
            built_cmd.xp.dash_f = False;
            built_cmd.xp.dash_a = False;
            if (dspl_descr->echo_mode == RECTANGULAR_HIGHLIGHT)
               built_cmd.xp.dash_r = True;
            else
               built_cmd.xp.dash_r = False;
            built_cmd.xp.path   = BANGOUT_PASTEBUF;
            (void) dm_xp(&built_cmd, dspl_descr->cursor_buff, dspl_descr->display, dspl_descr->main_pad->x_window, dspl_descr->escape_char);

            /***************************************************************
            *  If the output is going to the dm output window, trim off
            *  trailing blank or null lines.
            ***************************************************************/
            if (dmc->bang.dash_m)
               trim_pad(dspl_descr->dmoutput_pad, init_dmoutput_pos);

         }
   }

if (dmc->bang.dash_e)
   {
      built_cmd.next        = NULL;
      built_cmd.cmdf.cmd    = DM_cmdf;
      built_cmd.cmdf.temp   = False;
      built_cmd.cmdf.dash_p = True;
      built_cmd.cmdf.path   = BANGOUT_PASTEBUF;
      new_dmc = dm_cmdf(&built_cmd,
                        dspl_descr->display,
                        dspl_descr->main_pad->x_window,
                        dspl_descr->escape_char,
                        dspl_descr->hsearch_data,
                        True /* prompts allowed */);
   }

stderr_to_dmoutput(init_dmoutput_pos, dspl_descr);

dspl_descr->echo_mode = NO_HIGHLIGHT;
dspl_descr->mark1.mark_set = False;
return(new_dmc);

} /* end of dm_bang */


/************************************************************************

NAME:      dm_bang_lite -  The ! command used from .Cekeys

PURPOSE:    This routine provides some of the functionality of the 
            bang (!) command when called from the .Cekeys file.
            It does not do any copy/cut or paste since the windows do not
            exist yet.  It assume -e.

PARAMETERS:
   1.  dmc           -  pointer to DMC (INPUT)
                        This is the command structure for the ce or cv command.
                        The dash options are currently ignored.

   2.  dspl_descr    -  pointer to DISPLAY_DESCR (INPUT)
                        This is the current display description.  It is used to
                        pick up some environment characteristics which are known
                        during .Cekeys processing.

FUNCTIONS :
   1.   Call bang_system on the command line.

   2.   If anything is returned is stdout, run cmdf on it and return the results.

RETURNED VALUE:
   new_dmc -  Pointer to DMC
              If the bang command generates DMC commands, they are returned. 
              NULL is returned on error or no DMC's
        

*************************************************************************/

DMC *dm_bang_lite(DMC               *dmc,                /* input   */
                  DISPLAY_DESCR     *dspl_descr)         /* input   */
{
DMC                   built_cmd;
char                 *paste_dir;
char                  msg[MAXPATHLEN];
char                  paste_name[MAXPATHLEN];
char                  error_name[MAXPATHLEN];
DMC                  *new_dmc = NULL;
FILE                 *err_file;
int                   rc;
struct stat           file_stats;

/***************************************************************
*  If readlock is set, do not allow bang (!)
***************************************************************/
if (READLOCK)
   {
      dm_error_dspl("bang (!) command not available", DM_ERROR_BEEP, dspl_descr);
      return(NULL);
   }

/***************************************************************
*  First build the name of the paste buffer to use.
***************************************************************/

paste_dir = setup_paste_buff_dir(dmsyms[dmc->any.cmd].name);
if (paste_dir == NULL)
   return(NULL);

/***************************************************************
*  Build the target stdout and stderr for the called command and
*  issue the call.
***************************************************************/

snprintf(paste_name, sizeof(paste_name), "%s/%s", paste_dir, BANGOUT_PASTEBUF);
snprintf(error_name, sizeof(error_name), "%s/%s", paste_dir, ERR_PASTEBUF);

DEBUG4(fprintf(stderr, "!: %s\n", dmc->bang.cmdline);)

bang_system(dmc->bang.cmdline, dmc->bang.dash_s, "/dev/null", paste_name, error_name);

/***************************************************************
*  Print any error messages and then run the command file.
***************************************************************/

rc = stat(error_name, &file_stats);
if (rc == 0 && file_stats.st_size > 0)
   if ((err_file = fopen(error_name, "r")) == NULL)
      {
         snprintf(msg, sizeof(msg), "Cannot open %s for input (%s)", error_name, strerror(errno));   
         dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
      }
   else
      {
         while(fgets(msg, sizeof(msg), err_file) != NULL)
            dm_error(msg, DM_ERROR_BEEP);
         fclose(err_file);
      }

rc = stat(paste_name, &file_stats);
if (rc == 0 && file_stats.st_size > 0)
   {
      built_cmd.next        = NULL;
      built_cmd.cmdf.cmd    = DM_cmdf;
      built_cmd.cmdf.temp   = False;
      built_cmd.cmdf.dash_p = False;
      built_cmd.cmdf.path   = paste_name;
      new_dmc = dm_cmdf(&built_cmd,
                        NULL,
                        None,
                        dspl_descr->escape_char,
                        dspl_descr->hsearch_data,
                        True /* prompts allowed */);
   }

return(new_dmc);

} /* end of dm_bang_lite */


/************************************************************************

NAME:      stderr_to_dmoutput - Paste a file in the dmoutput window.

PURPOSE:    This routine pastes the special BangErr paste buffer into
            the DM output window.  It trims any blank lines up to the
            passed starting position.

PARAMETERS:
   1.  init_dmoutput_pos -  int (INPUT)
                            This is position to trim blank lines to.
                            It is a file line number.

   2.  dspl_descr         - pointer to DISPLAY_DESCR (INPUT)
                            This is the description for the display.

FUNCTIONS :
   1.   Use stat(2) to see if there is anthing in the buffer to paste.  If
        not just return.

   2.   Save the current cursor position and then position to the bottom of the
        DM output window.

   3.   Generate a paste command to paste the BangErr paste buffer into the
        DM output window and call paste to do the work.

   4.   Trim any trailing blank lines and reposition the cursor buff.

*************************************************************************/

static void stderr_to_dmoutput(int                init_dmoutput_pos,
                               DISPLAY_DESCR     *dspl_descr)
{
char                 *paste_dir;
char                  stderr_name[MAXPATHLEN];
DMC                   built_cmd;
int                   rc;
struct stat           file_stats;
BUFF_DESCR            saved_cursor_pos;

paste_dir = setup_paste_buff_dir("stderr_to_dmoutput");
if (paste_dir == NULL)
   paste_dir = "/tmp";
snprintf(stderr_name, sizeof(stderr_name), "%s/%s", paste_dir, ERR_PASTEBUF);


rc = stat(stderr_name, &file_stats);

/***************************************************************
*  
*  If there is a BangOut file and it has something in it,
*  do the paste.
*  
***************************************************************/

if (rc == 0 && file_stats.st_size > 0)
   {
      /***************************************************************
      *  Save the current cursor state and position to the DM output
      *  windo to do the paste.
      ***************************************************************/
      saved_cursor_pos =  *(dspl_descr->cursor_buff);
      dm_error_dspl(" ", DM_ERROR_MSG, dspl_descr);
      (void) dm_position(dspl_descr->cursor_buff, dspl_descr->dmoutput_pad, total_lines(dspl_descr->dmoutput_pad->token)-1, 0);


      /***************************************************************
      *  Paste the data into the dmoutput window.
      ***************************************************************/
      memset(&built_cmd, 0, sizeof(built_cmd));
      built_cmd.xp.cmd    = DM_xp;
      built_cmd.xp.dash_f = False;
      built_cmd.xp.dash_a = False;
      built_cmd.xp.dash_r = False;
      built_cmd.xp.path   = ERR_PASTEBUF;
      dm_xp(&built_cmd, dspl_descr->cursor_buff, dspl_descr->display, dspl_descr->main_pad->x_window, dspl_descr->escape_char);


      /***************************************************************
      *  Trim any trailing blank lines and restore the cursor buff.
      ***************************************************************/
      trim_pad(dspl_descr->dmoutput_pad, init_dmoutput_pos);
      *(dspl_descr->cursor_buff) = saved_cursor_pos;
   }

} /* end of stderr_to_dmoutput */


/************************************************************************

NAME:      trim_pad -  Trim blank lines off the bottom of a pad

PURPOSE:    This routine trims blank lines off the bottom of a pad.
            command name exists.

PARAMETERS:

   1.  pad           - pointer to PAD_DESCR (INPUT)
                       This is the pad description for the window to be trimmed.
                       It is described in buffer.h.

   2. init_pos          - int (INPUT)
                          This is the total number of lines in the pad not to be
                          deleted past.  Normally the caller saves away total_lines
                          from the file in a work variable.  Then passes this value
                          in after all pasting is done.  The extra checking done with
                          this parm prevents backing up to the previous message even
                          when nothing was pasted.


FUNCTIONS :

   1.   Read the last line of the pad.

   2.   If the last line of the pad contains only blanks and tabs, delete it.
        then, read the new last line and repeat.


*************************************************************************/

static void trim_pad(PAD_DESCR         *pad,
                     int                init_pos)
{

char                 *line;

/***************************************************************
*  
*  Walk backwards on the pad getting rid of blank lines.
*  
***************************************************************/

line = get_line_by_num(pad->token, total_lines(pad->token)-1);
while((line) && (total_lines(pad->token) > init_pos))
{
   while(*line == ' ' || *line == '\t') line++;
   if (*line == '\0')
      {
         delete_line_by_num(pad->token, total_lines(pad->token)-1, 1);
         line = get_line_by_num(pad->token, total_lines(pad->token)-1);
      }
   else
      line = NULL;
}
} /* end of trim_pad */


/************************************************************************

NAME:      reset_child_files -  Close and reopen files in a child process.

PURPOSE:    This routine closes and reopens files in a child process
            created by cps, cpo, or cp.

PARAMETERS:

   1.  paste_dir     - pointer to char (INPUT)
                       This is the paste buffer directory name.  If we are
                       saving paste buffers, we need this value.

   2.  is_server     - int (INPUT)
                       If true, this flag indicates that this is a server
                       process and we will not be saving stdout, or stderr.
                       They are routed to /dev/null

   3.  is_debug      - int (INPUT)
                       If true, this flag indicates that we are debugging the
                       command being executed.  stdout and stderr are left alone
                       which means they probably point to the terminal which invoked ce.

   4.  save_stdout   - int (INPUT)
                       If true, this flag indicates that we want to put stdout and
                       stderr into paste buffers.


FUNCTIONS :

   1.   If the paste dir pointer is NULL, use /tmp.

   2.   Point stdin to /dev/null

   3.   If this is a server or we are not saving stdout, point stdout
        and stdin at /dev/null unless we are in debug mode.

   3.   If we are saving stdout and stderr, point them at the special
        paste buffers.


*************************************************************************/

static void  reset_child_files(char   *paste_dir,
                               int     is_server,
                               int     is_debug,
                               int     save_stdout)
{
char                  paste_name[MAXPATHLEN];
char                  stderr_name[MAXPATHLEN];
char                  msg[MAXPATHLEN*2];
int                   i;
FILE                 *stream;

if (!paste_dir)
   paste_dir = "/tmp";

snprintf(stderr_name, sizeof(stderr_name), "%s/%s", paste_dir, ERR_PASTEBUF);
unlink(stderr_name);

fclose(stdin);
stream = fopen("/dev/null", "r");
if (stream != stdin)
   dm_error("Could not reopen stdin child process", DM_ERROR_BEEP);
   

if (is_server || !save_stdout)
   {
      if (!is_debug && !debug)
         {
            strcpy(paste_name, "/dev/null");

            fclose(stdout);
            stream = fopen(paste_name, "w");
            if (stream != stdout)
               {
                  if (stream)
                     snprintf(msg, sizeof(msg), "Wrong file descriptor opened in pointing stdout to /dev/null in child process");
                  else
                     snprintf(msg, sizeof(msg), "Could not reopen stdout to /dev/null in child process (%s)", strerror(errno));
                  dm_error(msg, DM_ERROR_BEEP);
               }

            close(2);
            dup(1);
         }
      else
         {
            stream = fopen(stderr_name, "w");
            fclose(stream);
         }
   }
else
   {
      snprintf(paste_name, sizeof(paste_name), "%s/%s", paste_dir, BANGOUT_PASTEBUF);
      unlink(paste_name);

      fclose(stdout);
      stream = fopen(paste_name, "w");
      if (stream != stdout)
         {
            if (stream)
               snprintf(msg, sizeof(msg), "Wrong file descriptor opened in pointing stdout to %s in child process", paste_name);
            else
               snprintf(msg, sizeof(msg), "Could not reopen stdout to %s in child process (%s)", paste_name, strerror(errno));
            dm_error(msg, DM_ERROR_BEEP);
         }
      else
         fflush(stream);

      fclose(stderr);
      stream = fopen(stderr_name, "w");
      if (stream != stderr)
         {
            if (stream)
               snprintf(msg, sizeof(msg), "Wrong file descriptor opened in pointing stderr to %s in child process", stderr_name);
            else
               snprintf(msg, sizeof(msg), "Could not reopen stderr to %s in child process (%s)", stderr_name, strerror(errno));
            dm_error(msg, DM_ERROR_BEEP);
         }
      else
         fflush(stream);
   }

/***************************************************************
*  Close all the open file descriptors.  This disconnects us
*  from the old Xserver connection without wiping out the
*  stuff still in use by the parent process.
*  We start with 3 to skip the file descriptors for stdin, stdout, and stderr.
***************************************************************/
for (i = 3; i < FOPEN_MAX; i++)
    close(i);

} /* end of reset_child_files */



/************************************************************************

NAME:      bang_system -  Search $PATH

PURPOSE:    This routine checks the pieces of $PATH to see if a passed
            command name exists.

PARAMETERS:

   1.  cmd           -  pointer to char (INPUT)
                        This is the command name to look for.
                        It may be a full path, a relative path, or 
                        a command name.

   2.  fullpath      -  pointer to MARK_REGION (OUTPUT)
                        The full path name of the command is placed in
                        this variable.  If the command is not found or
                        is not executeable, the string is set to a zero
                        length string.

FUNCTIONS :

   1.   If the passed cmd is a fully qualified path, make sure it is executable
        and not a directory.

   2.   If we do not already have the $PATH variable, get it from the environment.

   3.   Use Malloc to make a copy of this variable for hacking on.

   4.   Chop the Path variable into its pieces and use the pieces to check the
        existence and executablity of the command.

   5.   If the path is found, return it.  Otherwise set the output string to zero length.


*************************************************************************/

static int bang_system(char    *s,
                       char    *shell_opts,
                       char    *stdin_path,
                       char    *stdout_path,
                       char    *stderr_path)
{
int                   status_data;
int                   pid;
int                   i;
int                   code;
int                   wait_pid;
char                  msg[256];
char                 *p;
char                 *shell;
char                 *shell2;
int                   lcl_argc;
char                **lcl_argvp;
char                **lcl_argvp2;

void                  (*istat)(), (*qstat)();
void                  (*cstat)();
#ifdef SIGCLD
#if (SIGCLD != SIGCHLD)
void                  (*c1stat)();
#endif
#endif

cstat = signal(SIGCHLD, SIG_DFL);                               
#ifdef SIGCLD
#if (SIGCLD != SIGCHLD)
c1stat = signal(SIGCLD, SIG_DFL);                               
#endif
#endif

if ((pid = fork()) == 0)
   {
      /***************************************************************
      *  If command recording was on in the parent, turn it off.
      ***************************************************************/
      dm_rec(NULL, dspl_descr);

      /***************************************************************
      *  
      *  The child, reset stdin, stdout, and stderr to the correct places.
      *  and call the shell.
      *  
      ***************************************************************/
      close(0);
      if (open(stdin_path, O_RDONLY) != 0)
         {
            DEBUG(fprintf(stderr, "ce_system: Could not reopen stdin (%s)\n", strerror(errno));)
            exit(100);
         }
      close(1);
      if (open(stdout_path, O_WRONLY | O_CREAT | O_TRUNC, 0666) != 1)
         {
            DEBUG(fprintf(stderr, "ce_system: Could not reopen stdout (%s)\n", strerror(errno));)
            exit(101);
         }
      close(2);
      if (open(stderr_path, O_WRONLY | O_CREAT | O_TRUNC, 0666) != 2)
         exit(102);

      /***************************************************************
      *  Make sure we do not have a saved uid of root.
      ***************************************************************/
      if (getuid())
         setuid(getuid());
      
      /***************************************************************
      *  Save the main window id in the environment for the ce api.
      ***************************************************************/
      snprintf(msg, sizeof(msg), "CE_WINDOW=%08X", dspl_descr->main_pad->x_window);
      p = malloc_copy(msg);
      if (p)
         putenv(p);

      /***************************************************************
      *  Save the expression mode in an environment variable.
      ***************************************************************/
      snprintf(msg, sizeof(msg), "CE_EXPR=%c", (UNIXEXPR ? 'U' : 'A'));
      p = malloc_copy(msg);
      if (p)
         putenv(p);

      /***************************************************************
      *  Parse any extra shell arguments from the -s option.
      ***************************************************************/
      if (shell_opts && *shell_opts)
         {
            if (char_to_argv(NULL, shell_opts, "bang", shell_opts, &lcl_argc, &lcl_argvp2, '\\' /* unix always uses backslash as an esacpe char */))
               lcl_argc = 0;
         }
      else
         {
            lcl_argc = 0;
            lcl_argvp2 = NULL;
         }

      shell = getenv("SHELL");
      if ((!shell) || (!(*shell)))
         shell = SHELL; /* literal SHELL defined in pad.h */
      shell2 = strrchr(shell,'/');
      if (shell2)
         shell2++;
      else
         shell2 = shell;


      /***************************************************************
      *  Create the new argvp
      ***************************************************************/
      /*lcl_argvp = (char **)CE_MALLOC(sizeof(p)*(lcl_argc+4));*/
      i = sizeof(p); /* done this way to get around a gcc compiler bug on OMVS */
      i *= (lcl_argc+4);
      lcl_argvp = (char **)CE_MALLOC(i);
      if (lcl_argvp)
         {
            lcl_argvp[0] = shell2;
            for (i = 0; i < lcl_argc; i++)
               lcl_argvp[i+1] = lcl_argvp2[i];
            i++;
            lcl_argvp[i++] = "-c";
            lcl_argvp[i++] = s;
            lcl_argvp[i] = NULL;
            free((char *)lcl_argvp2);
         }
      else
#ifdef linux
         exit(103);
#else
         _exit(103);
#endif

      /***************************************************************
      *  Go to the shell.
      ***************************************************************/
      DEBUG4(
      fprintf(stderr, "invoking: %s\n", shell);
      for (i = 0; i < lcl_argc+3; i++)
         fprintf(stderr, "argv[%d] = \"%s\"\n", i, lcl_argvp[i]);
      )
      execvp(shell, lcl_argvp); 
      fprintf(stderr, "(!) execvp failed (%s)", strerror(errno));
#ifdef linux
         exit(127);
#else
         _exit(127);
#endif
   }

if (pid == -1)
   {
      snprintf(msg, sizeof(msg), "(!) Fork failed (%s)", strerror(errno));
      dm_error(msg, DM_ERROR_LOG);
      return(-1);
   }

/***************************************************************
*  
*  Parent process.
*  
***************************************************************/

istat = signal(SIGINT, SIG_IGN);
qstat = signal(SIGQUIT, SIG_IGN);
while((wait_pid = wait(&status_data)) != pid && wait_pid != -1)
   ;
(void) signal(SIGINT, istat);
(void) signal(SIGQUIT, qstat);

#ifdef SIGCLD
#if (SIGCLD != SIGCHLD)
signal(SIGCLD, c1stat);                               
#endif
#endif
signal(SIGCHLD, cstat);                               

if (wait_pid < 0)
   {
      snprintf(msg, sizeof(msg), "Wait terminated by signal (%s)", strerror(errno));
      dm_error(msg, DM_ERROR_LOG);
   }
else
   if (status_data == 0)
      return(0);
   else
      {
         if (WIFEXITED(status_data))
            {
               code = WEXITSTATUS(status_data);
               switch(code)
               {
               case 100:
                  snprintf(msg, sizeof(msg), "Child process could not open stdin file %s", stdin_path);
                  break;
               case 101:
                  snprintf(msg, sizeof(msg), "Child process could not open stdout file %s", stdout_path);
                  break;
               case 102:
                  snprintf(msg, sizeof(msg), "Child process could not open stderr file %s", stderr_path);
                  break;
               case 103:
                  snprintf(msg, sizeof(msg), "Out of memory");
                  break;
               case 127:
                  snprintf(msg, sizeof(msg), "System call execvp failed");
                  break;
               default:
                  snprintf(msg, sizeof(msg), "Command exited, code %d", code);
                  break;
               }
            }
         else
            if (WIFSIGNALED(status_data))
               {
                  snprintf(msg, sizeof(msg), "Process received signal %d", WTERMSIG(status_data));
               }
            else
               if (WIFSTOPPED(status_data))
                  snprintf(msg, sizeof(msg), "Process stopped, signal code %d", WSTOPSIG(status_data));
               else
#ifdef WIFCONTINUED
                  if (WIFCONTINUED(status_data))
                     snprintf(msg, sizeof(msg), "Process continued, status = %d", status_data);
                  else
#endif
                     snprintf(msg, sizeof(msg), "Unknown cause of termination status = %d", status_data);

#ifdef WCOREDUMP
         if (WCOREDUMP(status_data))
            strcat(msg, "  (Core was dumped)");
#endif

         dm_error(msg, DM_ERROR_BEEP);
      }

return((wait_pid == -1) ? wait_pid: status_data);

} /* end of bang_system */


/************************************************************************

NAME:      spoon - Fork the edit session to background

PURPOSE:    This routine is used in the main to fork the child edit session and
            by set_file_from_parm for multiple edit sessions.
         
PARAMETERS:
   None

FUNCTIONS :
   1.  Turn off hup and child signals.

   2.  Fork

   3.  In the parent, just return the childs pid.

   4.  In the child,  isolate ourselves from the parent
       and return 0.
   


RETURNED VALUE
   rc  - int
         N  -  pid of child (0 if child)
        -1  -  fork failed

*************************************************************************/

int  spoon(void)
{

int pid;

      /***************************************************************
      *  
      *  Fork,  If we are the parent, we are done.
      *  If we are not the parent, we will get rid of the old file,
      *  and start a new one.
      *  If the fork fails, assume -w.  
      *  
      ***************************************************************/

      signal(SIGHUP,  SIG_IGN);                               
      signal(SIGCHLD, SIG_IGN);                               
#ifdef SIGCLD
#if (SIGCLD != SIGCHLD)
      signal(SIGCLD, SIG_IGN);                               
#endif
#endif

      pid = fork();
      if (pid == -1)
         perror("fork failed");
      else
         if (pid != 0)
               DEBUG4(fprintf(stderr, "Main: Parent child pid = %d, ppid = %d\n", pid, getppid());)
         else
            {
               /***************************************************************
               *  The child process.
               ***************************************************************/
/* ifdef added 9/9/2004 for Fedora Core 2 */
#ifdef linux
               (void) setpgrp();
#else
               (void) setpgrp(0, getpid());
#endif
   
               DEBUG4(fprintf(stderr, "Main: Child(1) pid = %d, ppid = %d\n", getpid(), getppid());)
               signal(SIGHUP,  SIG_DFL);                               
#ifdef SIGTSTP
               signal(SIGTSTP, SIG_IGN);
#endif
               DEBUG4(fprintf(stderr, "Main: Child(2) pid = %d, ppid = %d\n", getpid(), getppid());)
            }

return(pid);

} /* end of spoon */


/****************************************************************** 
*
*  reap_child_process -  Routine called asynchronously in response to
*                        SIGCHLD when we are not in pad mode.
*
******************************************************************/

void reap_child_process(int sig)
{ 
int                   wpid;
int                   status_data;

 /*
  *  On the ibm, the signal catcher has to be reset .
  */

DEBUG16( fprintf(stderr, "reap_child_process signal=%d, current pid = %d\n", sig, getpid());)

#ifdef SIGTTOU
if (sig == 22) signal(SIGTTOU, SIG_IGN); /* DEC_ULTRIX goes infinite otherwise! */                             
#endif 

#ifdef DebuG
wpid = wait(&status_data);
#else
(void) wait(&status_data);
#endif

#ifdef  DebuG
if (debug & D_BIT4 )
   {
      fprintf(stderr, "Process [%d] died!, signal was: %d, status:0x%X\n", wpid, sig, status_data);
      if (wpid < 0)
         fprintf(stderr, "Wait terminated by signal (%s)\n", strerror(errno));
      else
         if (WIFEXITED(status_data))
            fprintf(stderr, "Process exited, code %d\n", WEXITSTATUS(status_data));
         else
            if (WIFSIGNALED(status_data))
               fprintf(stderr, "Process signaled, signal code %d\n", WTERMSIG(status_data));
            else
               if (WIFSTOPPED(status_data))
                  fprintf(stderr, "Process stopped, signal code %d\n", WSTOPSIG(status_data));
               else
#ifdef WIFCONTINUED
                  if (WIFCONTINUED(status_data))
                     fprintf(stderr, "Process continued, status = %d\n", status_data);
                  else
#endif
                     fprintf(stderr, "Unknown cause of termination status = %d\n", status_data);

#ifdef WCOREDUMP
        if (WCOREDUMP(status_data))
           fprintf(stderr, "Core was dumped\n");
#endif
   }
#endif

#ifdef blah
DEBUG16(
   fprintf(stderr, "Process [%d] died!, signal was:%d\n", wpid, sig);
   if (wpid < 0)
      fprintf(stderr, "Wait terminated by signal (%s)\n", strerror(errno));
   else
      if ((status_data & 0x000000ff) == 0)
         fprintf(stderr, "Process exited, code %d\n", status_data >> 8);
      else
         if ((status_data & 0x0000ff00) == 0)
            fprintf(stderr, "Process aborted, signal code %d\n", status_data);
         else
            fprintf(stderr, "Process killed, signal code %d\n", status_data >> 8);

)
#endif

/***************************************************************
*  Reset the signal handler.
***************************************************************/
signal(SIGCHLD, reap_child_process);                               
#ifdef SIGCLD
#if (SIGCLD != SIGCHLD)
signal(SIGCLD, reap_child_process);                               
#endif
#endif
}  /* end of reap_child_process */


/************************************************************************

NAME:      dm_env             - DM env command, do a putenv

PURPOSE:    This routine performs the env command which will list or 
            set an environment variable.  If just given a name, the
            environment variable is listed.  If given name=name
            the environment variable is set.
         
PARAMETERS:

   1.  assignment - pointer to char (INPUT)
                    This routine accepts strings of the form:
                    varname
                      and
                    varname=value
                    In the first case, varname is printed in the
                    dm_output window.  In the second case, varname is
                    is assigned to value in the environment.


FUNCTIONS :
   1.   See if there is an equal sign in the name.  If not, get the
        value from the environment and print it.

   2.   If there is an equal sign, malloc storage for the variable
        and do a putenv of it.


*************************************************************************/

void  dm_env(char  *assignment)
{
char                 *value;
char                  buffer[MAX_LINE*2];

if (!assignment)
   return;

if (strchr(assignment, '=') == NULL)
   {
      /***************************************************************
      *  Normal case, get an environment variable
      ***************************************************************/
      value = getenv(assignment);
      if (value == NULL)
         {
            snprintf(buffer, sizeof(buffer), "(env) %s does not exist", assignment);
            dm_error(buffer, DM_ERROR_BEEP);
         }
      else
         {
            snprintf(buffer, sizeof(buffer), "%s=%s", assignment, value);
            dm_error(buffer, DM_ERROR_MSG);
         }
   }
else
   {
      value = malloc_copy(assignment);
      if (value)
         {
            if (putenv(value) != 0)
               dm_error("(env) Unable to expand the environment", DM_ERROR_LOG);
         }

   }

} /* end of dm_env */



