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
*     dm_ce              - Invoke ce in initial write mode
*     dm_cp              - execute the cp command
*     dm_cpo             - execute the cpo and cps commands
*     dm_bang            - ! command
*     spoon              - Fork the edit session to background
*     dm_env             - DM env command, do a putenv
*
*  INTERMAL
*     stderr_to_dmoutput -  paste a file in the dmoutput window.
*     trim_pad           -  Trim blank lines off the bottom of a pad
*     reset_child_files  -  Repoint stdin, stdout, and stderr in a child process.
*     bang_system        -  Version of the system command for use with dm_bang.
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h         */
#include <string.h>         /* /bsd4.3/usr/include/string.h */
#include <errno.h>          /* /usr/include/errno.h         */
#include <sys/types.h>      /* /usr/include/sys/types.h   */
#include <sys/stat.h>       /* /usr/include/sys/stat.h    */
#include <fcntl.h>          /* "/usr/include/fcntl.h"      */
#include <signal.h>         /* "/usr/include/signal.h"      */
#include <sys/param.h>      /* /bsd4.3/usr/include/sys/param.h */
#ifndef MAXPATHLEN
#define MAXPATHLEN   1024
#endif


#include <X11/Xlib.h>       /* /usr/include/X11/Xlib.h   */

#include "cc.h"  /* needed for variable cc_ce */
#include "display.h"
#include "dmsyms.h"
#include "dmwin.h"
#include "emalloc.h"
#include "execute.h"
#include "getevent.h"
#include "getxopts.h"
#include "help.h"
#include "init.h"
#include "kd.h"
#include "mark.h"
#include "memdata.h"
#include "mvcursor.h"
#include "newdspl.h"
#include "normalize.h"
#ifdef PAD
#include "pad.h"
#endif
#include "parms.h"
#include "pastebuf.h"
#include "pw.h"
#include "record.h"         /* needed to turn recording off in child after forks */
#include "serverdef.h"
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
int    setpgrp(int pid, int pgrp);  /* bsd version */
void   close(int fd);
void   exit(int code);
char  *getenv(char *name);
#ifndef PAD
char  *getcwd(char *target, int maxlen);
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
                       char    *stdin_path,
                       char    *stdout_path,
                       char    *stderr_path);



/************************************************************************

NAME:      dm_ce  DM ce and cv commands

PURPOSE:    This routine invokes the ce or cv program on a file.

PARAMETERS:

   1.  dmc         - pointer to DMC (INPUT)
                     This is the command structure for the ce or cv command.

   2.  dspl_descr  - pointer to DISPLAY_DESCR (INPUT / OUTPUT in the child process)
                     This is the description for the display.  This routine needs to
                     get to all the pad descriptions.
                     It is reconfigured to the new file.

   3.  edit_file   - pointer to char (INPUT / OUTPUT in the child process)
                     This is the name if the file being edited.

   4.  shell_fd    - int (INPUT)
                     This is the file descriptor for the shell
                     if we are in pad mode.  The child process
                     needs to close this file.
                     Values:  -1    Not in pad mode
                              >=0   The file descriptor to the shell

   5.  writeable   - pointer to int (OUTPUT)
                     This flag shows whether the file will be edit or browse
                     Values:  True  -  editable 
                              False -  Read Only

FUNCTIONS :

   1.   If this is a ce, make sure the file is writeable.

   2.   If this is a cv, make sure the file exists and is readable.

   3.   Build the command.

   4.   Fork(), the parent just returns.
   
   5.   The child will close out the current edit file and initialize a new one.
        It will open the new edit file and read in the first block of data.

   6.   The child positions to top of file and returns.


RETURNED VALUE:
   child  -  The returned value indicates whether this is the parent or the child
             0  -  child
            ~0  -  the parent (child pid is returned) or -1 on failure.
        

*************************************************************************/

int   dm_ce(DMC             *dmc,                    /* input  */
            DISPLAY_DESCR   *dspl_descr,             /* input / output  in child process */
            char            *edit_file,              /* input / output  in child process */
            int              shell_fd,               /* input  */
            int             *writeable)              /* output */
{
char                  cmd[MAX_LINE+10];
int                   i;
int                   rc;
int                   pid;
char                 *new_path;
char                  work_path[MAXPATHLEN];
char                 *p;
char                 *exec_path;
char                  link[MAXPATHLEN];
int                   file_exists;
char                **lcl_option_values;
char                 *lcl_option_from_parm;
char                 *lcl_argv[MAX_CV_ARGS];
int                   lcl_argc;
int                   ok;
char                  lcl_display_name[256];
DISPLAY_DESCR        *walk_dspl;
Display              *new_display;
PROPERTIES           *new_properties;
void                 *new_hash_table;

DEBUG4(
   fprintf(stderr, "cmd = %s\n",  dmsyms[dmc->ce.cmd].name);
   for (i = 0; i < dmc->cv.argc; i++)
      fprintf(stderr, "argv[%02d] = %s\n", i, dmc->cv.argv[i]);
)

/***************************************************************
*  
*  Handle the case of the null parm, should have been weeded out by parser.
*  
***************************************************************/

if (dmc->ce.argc <= 1 || (dmc->ce.argc == 2 && (dmc->ce.argv[1][0] == '\0')))
   return(-1);


/***************************************************************
*  
*  Process the ce options.  This should leave only the file name.
*  
***************************************************************/
lcl_argc = dmc->ce.argc;
if (lcl_argc > MAX_CV_ARGS)
   lcl_argc = MAX_CV_ARGS;

for (i = 0; i < lcl_argc; i++)
    lcl_argv[i] = dmc->ce.argv[i];

rc = open_new_dspl(&lcl_argc,             /* input/output in newdspl.c  */
                   lcl_argv,             /* input/output  */
                   dspl_descr->display,  /* input  */
                   &new_display,         /* output */
                   &lcl_option_values,   /* output */
                   &lcl_option_from_parm,/* output */
                   &new_properties,      /* output */
                   &new_hash_table,      /* output */
                   dmsyms[dmc->ce.cmd].name); /* input  */
if (rc != 0)
   return(-1);

 rc = set_file_from_parm(dspl_descr, lcl_argc, lcl_argv, work_path);
 if (rc != 0)
    {
      sprintf(link, "(%s) Invalid option or too many path names", dmsyms[dmc->ce.cmd].name);
      dm_error(link, DM_ERROR_BEEP);
      return(-1);
    }
else
   if (strcmp(work_path, STDIN_FILE_STRING) == 0)
      {
         sprintf(link, "(%s) Null file name", dmsyms[dmc->ce.cmd].name);
         dm_error(link, DM_ERROR_BEEP);
         return(-1);
      }
   else
      new_path = work_path;


/**************************************************************
*  
*  See whether we want the file for input or output and then
*  do the basic checking on the file.
*  
***************************************************************/

*writeable = (dmc->ce.cmd == DM_ce);

ok = initial_file_setup(new_path, writeable, &file_exists, NULL, dspl_descr->escape_char, ENV_VARS);
if (!ok)
   return(-1); /* message already output */


/***************************************************************
*  
*  Fork,  If we are the parent, we are done.
*  If we are not the parent, we will get rid of the old file,
*  and start a new one.
*  
***************************************************************/

signal(SIGHUP,  SIG_IGN);                               

pid = fork();
if (pid == -1)
   {
      if (lcl_option_values)
         FREE_NEW_OPTIONS(lcl_option_values, OPTION_COUNT);
      if (lcl_option_from_parm)
         free((char *)lcl_option_from_parm);
      if (new_hash_table)
         free((char *)new_hash_table);
      if (new_properties)
         free((char *)new_properties);
      if (new_display)
         XCloseDisplay(new_display);
      sprintf(cmd, "(%s) Can't fork:  %s", dmsyms[dmc->ce.cmd].name, strerror(errno));
      dm_error(cmd, DM_ERROR_BEEP);
      return(-1);
   }
if (pid != 0)
   {
      DEBUG4(fprintf(stderr, "%s Parent: child pid = %d, ppid = %d\n", dmsyms[dmc->ce.cmd].name, pid, getppid());)
      if (lcl_option_values)
         FREE_NEW_OPTIONS(lcl_option_values, OPTION_COUNT);
      if (lcl_option_from_parm)
         free((char *)lcl_option_from_parm);
      if (new_hash_table)
         free((char *)new_hash_table);
      if (new_properties)
         free((char *)new_properties);
      if (new_display)
         close(ConnectionNumber(new_display));
   }
else
   {
      /***************************************************************
      *  
      *  The child process.
      *  close all the file descriptors and point stderr at the console
      *  
      ***************************************************************/

      exec_path = get_execution_path(new_display,  dmsyms[dmc->ce.cmd].name); /* never returns null */
      DEBUG4(fprintf(stderr, "Will execv %s\n", exec_path);
      XCloseDisplay(new_display);
      for (i = 3; i < FOPEN_MAX; i++)
         close(i);

      /***************************************************************
      *  
      *  The child process.
      *  Exec the ce/cv command
      *  
      ***************************************************************/
       
      execv(exec_path, dmc->cv.argv);

      /* should never get here */

      sprintf(msg, "(%s) Call to execv failed: %s", dmsyms[dmc->ce.cmd].name, strerror(errno));
      dm_error(msg, DM_ERROR_LOG);

      exit(23);


#ifdef ce_noexec
      /***************************************************************
      *  
      *  The child process.
      *
      *  If command recording was on in the parent, turn it off.
      ***************************************************************/
      dm_rec(NULL, dspl_descr);
      for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
         dm_rec(NULL, walk_dspl);

      /***************************************************************
      *  Get rid of all the display discriptions except 1
      ***************************************************************/
      while(dspl_descr->next != dspl_descr)
      {
         close(ConnectionNumber(dspl_descr->next->display));
         del_display(dspl_descr->next);
      }

      /***************************************************************
      *  At this point we are not in cc mode.
      ***************************************************************/
      cc_ce = False;

      /***************************************************************
      *  Close the open file descriptors associated with the display(s)
      *  we have open.  Also close the file descriptor to the Shell
      *  if it exists.  Get the new display pointer and new hash_table
      *  and properties if needed.
      ***************************************************************/
      if (shell_fd >= 0)
         close(shell_fd);
      close(ConnectionNumber(dspl_descr->display));
      dspl_descr->display = new_display;
      if (new_hash_table)
         {
            hdestroy(dspl_descr->hsearch_data);
            dspl_descr->hsearch_data = new_hash_table;
         }
      if (new_properties)
         {
            free((char *)dspl_descr->properties);
            dspl_descr->properties = new_properties;
         }

      /***************************************************************
      *  Become our own process group.
      ***************************************************************/
#if defined(PAD) && !defined(ultrix)
      isolate_process(getpid());
#endif
      (void) setpgrp(0, getpid());
      
      DEBUG4(fprintf(stderr, "%s Child(1): pid = %d, ppid = %d\n", dmsyms[dmc->ce.cmd].name, getpid(), getppid());)

      /***************************************************************
      *  If the parent is not waiting for us, detach from the parent.
      ***************************************************************/

      signal(SIGHUP,  SIG_DFL);

#ifdef SIGTSTP
      signal(SIGTSTP, SIG_IGN);
#endif

      /***************************************************************
      *  Get rid of the old memdata description of the file the parent
      *  was working at.  Later new ones will be built for the new edit
      *  session.
      ***************************************************************/
      dspl_descr->main_pad->buff_modified = False;
      dspl_descr->main_pad->work_buff_ptr[0] = '\0';
      dspl_descr->main_pad->buff_ptr = dspl_descr->main_pad->work_buff_ptr;
      dspl_descr->main_pad->window->drawable = None;
      dspl_descr->main_pad->sub_window->drawable = None;
      dspl_descr->dminput_pad->buff_modified = False;
      dspl_descr->dminput_pad->work_buff_ptr[0] = '\0';
      dspl_descr->dminput_pad->buff_ptr = dspl_descr->dminput_pad->work_buff_ptr;
      dspl_descr->dminput_pad->window->drawable = None;
      dspl_descr->dminput_pad->sub_window->drawable = None;
      dspl_descr->dmoutput_pad->window->drawable = None;
      dspl_descr->dmoutput_pad->sub_window->drawable = None;

      /***************************************************************
      *  clear out the record of the text cursor whereever it may be. 
      ***************************************************************/
      clear_text_cursor(None, dspl_descr);

      release_x_pastebufs();
      mem_kill(dspl_descr->main_pad->token);
      dspl_descr->main_pad->token = NULL;
      mem_kill(dspl_descr->dminput_pad->token);
      dspl_descr->dminput_pad->token = NULL;
      mem_kill(dspl_descr->dmoutput_pad->token);
      dspl_descr->dmoutput_pad->token = NULL;
      strcpy(edit_file, new_path);
      if (dspl_descr->unix_pad)
         {
            if (dspl_descr->unix_pad->token)
               {
                  mem_kill(dspl_descr->unix_pad->token);
                  dspl_descr->unix_pad->token = NULL;
               }
            dspl_descr->unix_pad->buff_modified = False;
            dspl_descr->unix_pad->work_buff_ptr[0] = '\0';
            dspl_descr->unix_pad->buff_ptr = dspl_descr->unix_pad->work_buff_ptr;
         }

      /***************************************************************
      *  If options were supplied, copy them to the real option values
      *  holder.
      ***************************************************************/
      if (lcl_option_values)
         {
            FREE_NEW_OPTIONS(OPTION_VALUES, OPTION_COUNT);
            OPTION_VALUES = lcl_option_values;
         }
      if (lcl_option_from_parm)
         {
            free((char *)OPTION_FROM_PARM);
            OPTION_FROM_PARM = lcl_option_from_parm;
         }

      /***************************************************************
      *  Open the file to be edited.
      ***************************************************************/
      change_background_work(dspl_descr, MAIN_WINDOW_EOF, 0);
      instream = initial_file_open(edit_file, writeable, dspl_descr);
      if (instream == NULL)
         exit(3);

      /***************************************************************
      *  Move the window so it is not right over the last window
      *  and position us at the top of the dataset.  Use the geo parm if
      *  it was supplied.
      ***************************************************************/
      if (lcl_argc < 0 || OPTION_VALUES[GEOMETRY_IDX] == NULL)
         {
            OPTION_VALUES[GEOMETRY_IDX] = wdf_get_geometry(dspl_descr->display, dspl_descr->properties);
            if (OPTION_VALUES[GEOMETRY_IDX] == NULL)
               {
                  sprintf(link, "%dx%d+%d+%d", dspl_descr->main_pad->window->width, dspl_descr->main_pad->window->height, dspl_descr->main_pad->window->x+30, dspl_descr->main_pad->window->y+30);
                  OPTION_VALUES[GEOMETRY_IDX] = CE_MALLOC(strlen(link)+1);
                  if (OPTION_VALUES[GEOMETRY_IDX])
                     strcpy(OPTION_VALUES[GEOMETRY_IDX], link);
               }
         }

      dspl_descr->main_pad->file_col_no = 0;
      dspl_descr->main_pad->file_line_no = -1;
      dspl_descr->main_pad->first_line = 0;
      dspl_descr->main_pad->first_char = 0;
      dspl_descr->hold_mode = False;
      dspl_descr->main_pad->buff_modified = False;
      dspl_descr->main_pad->buff_ptr = NULL;

      dspl_descr->cursor_buff->x            = dspl_descr->main_pad->sub_window->x;
      dspl_descr->cursor_buff->y            = dspl_descr->main_pad->sub_window->y;
      dspl_descr->cursor_buff->win_col_no   = 0;
      dspl_descr->cursor_buff->win_line_no  = 0;
      dspl_descr->cursor_buff->up_to_snuff  = 0;

      *dspl_descr->main_pad->echo_mode = NO_HIGHLIGHT;
      dspl_descr->mark1.mark_set = False;
      dspl_descr->pad_mode = False;
      dspl_descr->vt100_mode = False;
#endif

   } /* end of child process code */


return(pid);

} /* end of dm_ce  */


#ifdef PAD
/************************************************************************

NAME:      dm_cp  - Execute the cp command

PURPOSE:    This routine starts a ceterm running on the passed command name

PARAMETERS:

   1.  dmc         - pointer to DMC (INPUT)
                     This is the command structure for the cp command.

   2.  dspl_descr  - pointer to DISPLAY_DESCR (INPUT / OUTPUT in the child process)
                     This is the description for the display.  This routine needs to
                     get to all the pad descriptions.
                     It is reconfigured to the new file.

   3.  edit_file   - pointer to char (INPUT / OUTPUT in the child process)
                     This is the name if the file being edited.

   4.  shell_fd    - int (INPUT)
                     This is the file descriptor for the shell
                     if we are in pad mode.  The child process
                     needs to close this file.
                     Values:  -1    Not in pad mode
                              >=0   The file descriptor to the shell

FUNCTIONS :

   1.   If there were any ceterm options specified, process them out of the arg list.
        Use a local copy to avoid trashing out key definitions.

   2.   Verify that there is no more than one parm left.  This is the command to execute
        along with any args. The command and args were enclosed in quotes.

   3.   If there were zero parms left, assume the $SHELL variable.  If that is null, 
        assume /bin/sh.

   4.   Verify that the command is findable and executable.

   5.   Fork(), the parent just returns.
   
   6.   The child closes out the current pad file and initializes a new one.
        It puts the new commmand and parms into the edit file name.

   7.   The child positions to top of file and returns.


RETURNED VALUE:
   child  -  The returned value indicates whether this is the parent or the child
             0  -  child
            ~0  -  the parent (child pid is returned) or -1 on failure.
        

EXAMPLE:
    cp -display desoto:0 -fgc blue '/bin/sh abc def -fgc red'

*************************************************************************/

int   dm_cp(DMC             *dmc,                    /* input  */
            DISPLAY_DESCR   *dspl_descr,             /* input / output  in child process */
            char            *edit_file,              /* input / output  in child process */
            int              shell_fd)               /* input  */
{
char                  cmd[MAX_LINE*2];
int                   i;
int                   rc;
int                   pid;
char                 *cmd_name;
char                 *args_start;
char                  work_path[MAXPATHLEN];
char                 *p;
int                   cumm_len;
int                   file_exists;
char                **lcl_option_values;
char                 *lcl_option_from_parm;
char                 *lcl_argv[MAX_CV_ARGS];
int                   lcl_argc;
int                   ok;
char                  lcl_display_name[256];
DISPLAY_DESCR        *walk_dspl;
Display              *new_display;
PROPERTIES           *new_properties;
void                 *new_hash_table;

DEBUG4(
   fprintf(stderr, "cmd = %s\n",  dmsyms[dmc->cp.cmd].name);
   for (i = 0; i < dmc->cp.argc; i++)
      fprintf(stderr, "argv[%02d] = %s\n", i, dmc->cp.argv[i]);
)


/***************************************************************
*  
*  If there is at least one argument, see if it is starts with
*  a dash.  If so, there are cp (ceterm) parms present and we
*  will use the Xparser to get them out.  The command name and
*  any arguments are in the a quoted parm.
*  If not, the first parm is the name of the thing to
*  execute.  The zero'th arg is cp (this command name)
*  
***************************************************************/

/***************************************************************
*  
*  Reprocess the option list to get any ce parms out of it.
*  The command to execute string was specified in quotes .
*  
***************************************************************/
lcl_argc = dmc->cp.argc;
if (lcl_argc > MAX_CV_ARGS)
   lcl_argc = MAX_CV_ARGS;

for (i = 0; i < lcl_argc; i++)
    lcl_argv[i] = dmc->cp.argv[i];

rc = open_new_dspl(&lcl_argc,             /* input/output in newdspl.c  */
                   lcl_argv,             /* input/output  */
                   dspl_descr->display,  /* input  */
                   &new_display,         /* output */
                   &lcl_option_values,   /* output */
                   &lcl_option_from_parm,/* output */
                   &new_properties,      /* output */
                   &new_hash_table,      /* output */
                   dmsyms[dmc->ce.cmd].name); /* input  */
if (rc != 0)
   return(-1);

/***************************************************************
*  
*  There better be zero or one thing left.  This is the
*  command to execute and its's parms.  Any parms were included
*  in quotes with the command name. eg: '/bin/cat /etc/hosts'
*  
***************************************************************/

if (lcl_argc > 2)
   {
      if (lcl_argv[1][0] == '-')
         {
            i = 1;
            strcpy(cmd, "Invalid or non-unique ceterm options: ");
         }
      else
         {
            i = 2;
            strcpy(cmd, "Cmd parms not enclosed in quotes: ");
         }

      cumm_len = strlen(cmd);

      for (; i < lcl_argc; i++)
      {
         cumm_len += strlen(lcl_argv[i]);
         if (cumm_len > MAX_LINE)
            {
               dm_error("(cp) Parm list is too long for message generation, truncated", DM_ERROR_BEEP);
               break;
            }
         else
            {
               strcat(cmd, " ");
               strcat(cmd, lcl_argv[i]);
            }
      }

      dm_error(cmd, DM_ERROR_BEEP);
      return(-1);

   }

/***************************************************************
*  
*  Get the command name,  It is either the first non-ceterm
*  parm, or $SHELL, if nothing was passed.
*  
***************************************************************/

if (lcl_argc < 2) 
   {
      /***************************************************************
      *  
      *  Get the $SHELL variable,
      *  
      ***************************************************************/
      cmd_name = getenv("SHELL");
      if (!cmd_name) 
         cmd_name = SHELL;
   }
else
   cmd_name = lcl_argv[1];


/***************************************************************
*  
*  Strip the command name away from the args.
*  
***************************************************************/

if ((args_start = strpbrk(cmd_name, " \t")) != NULL)
   {
      strncpy(cmd, cmd_name, args_start-cmd_name);
      cmd[args_start-cmd_name] = '\0';
   }
else
   strcpy(cmd, cmd_name);

/***************************************************************
*  
*  First see if argv[0] is a command or a path name.
*  If command, search the path.  If a path name, make sure
*  it is executable.
*  After this sequence, work_path contains the executable path.
*  
***************************************************************/

search_path(cmd, work_path);  /* in normalize.h */
if (work_path[0] == '\0')
   {
      sprintf(work_path, "(cp) Cannot execute %s (%s)", cmd, strerror(errno));
      dm_error(work_path, DM_ERROR_BEEP);
      return(-1);
   }
strcpy(cmd, work_path);

if (args_start)
   {
      if ((strlen(args_start) + strlen(cmd)) > sizeof(cmd))
         {
            dm_error("(cp) arg list too long", DM_ERROR_BEEP);
            return(-1);
         }
      strcat(cmd, args_start);
   }



/***************************************************************
*  
*  Fork,  If we are the parent, we are done.
*  If we are not the parent, we will get rid of the transcript
*  pad and start a new one.
*  
***************************************************************/

signal(SIGHUP,  SIG_IGN);                               

pid = fork();
if (pid == -1)
   {
      if (lcl_option_values)
         FREE_NEW_OPTIONS(lcl_option_values, OPTION_COUNT);
      if (lcl_option_from_parm)
         free((char *)lcl_option_from_parm);
      if (new_hash_table)
         free((char *)new_hash_table);
      if (new_properties)
         free((char *)new_properties);
      if (new_display)
         XCloseDisplay(new_display);
      sprintf(cmd, "(cp) Can't fork:  %s", strerror(errno));
      dm_error(cmd, DM_ERROR_BEEP);
      return(-1);
   }
if (pid != 0)
   {
      DEBUG4(fprintf(stderr, "%s Parent: child pid = %d, ppid = %d\n", dmsyms[dmc->ce.cmd].name, pid, getppid());)
      if (lcl_option_values)
         FREE_NEW_OPTIONS(lcl_option_values, OPTION_COUNT);
      if (lcl_option_from_parm)
         free((char *)lcl_option_from_parm);
      if (new_hash_table)
         free((char *)new_hash_table);
      if (new_properties)
         free((char *)new_properties);
      if (new_display)
         close(ConnectionNumber(new_display));
   }
else
   {
      /***************************************************************
      *
      *  The child process.
      *
      *  If command recording was on in the parent, turn it off.
      ***************************************************************/
      dm_rec(NULL, dspl_descr);
      for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
         dm_rec(NULL, walk_dspl);

      /***************************************************************
      *  Get rid of all the display discriptions except 1
      ***************************************************************/
      while(dspl_descr->next != dspl_descr)
      {
         close(ConnectionNumber(dspl_descr->next->display));
         del_display(dspl_descr->next);
      }


      /***************************************************************
      *  At this point we are not in cc mode.
      ***************************************************************/
      cc_ce = False;

      /***************************************************************
      *  Close the open file descriptors associated with the display(s)
      *  we have open.  Also close the file descriptor to the Shell
      *  if it exists.  Get the new display pointer and new hash_table
      *  and properties if needed.
      ***************************************************************/
      if (shell_fd >= 0)
         close(shell_fd);
      close(ConnectionNumber(dspl_descr->display));
      dspl_descr->display = new_display;
      if (new_hash_table)
         {
            hdestroy(dspl_descr->hsearch_data);
            dspl_descr->hsearch_data = new_hash_table;
         }
      if (new_properties)
         {
            free((char *)dspl_descr->properties);
            dspl_descr->properties = new_properties;
         }

      /***************************************************************
      *  Become our own process group leader.
      ***************************************************************/
#if defined(PAD) && !defined(ultrix)
      isolate_process(getpid());
#endif
      (void) setpgrp(0, getpid());
      
      DEBUG4(fprintf(stderr, "cp Child(1): pid = %d, ppid = %d\n", getpid(), getppid());)
      signal(SIGHUP,  SIG_DFL);                               
#ifdef SIGTSTP
      signal(SIGTSTP, SIG_IGN);
#endif
      DEBUG4(fprintf(stderr, "cp Child(2): pid = %d, ppid = %d\n", getpid(), getppid());)

      /***************************************************************
      *  Open a new connection to the X server.
      ***************************************************************/
      if (!(dspl_descr->display = XOpenDisplay(OPTION_VALUES[DISPLAY_IDX])))
         {
            fprintf(stderr, "(%s) Can't open display '%s' (%s)\n",
                             dmsyms[dmc->ce.cmd].name, XDisplayName(OPTION_VALUES[DISPLAY_IDX]), strerror(errno));
            exit(4);
         }


      /***************************************************************
      *  Get rid of the old memdata description of the pad the parent
      *  was working at.  Later new ones will be built for the new
      *  session.
      ***************************************************************/
      dspl_descr->main_pad->buff_modified = False;
      dspl_descr->main_pad->work_buff_ptr[0] = '\0';
      dspl_descr->main_pad->buff_ptr = dspl_descr->main_pad->work_buff_ptr;
      dspl_descr->main_pad->window_wrap_col = 0;
      dspl_descr->dminput_pad->buff_modified = False;
      dspl_descr->dminput_pad->work_buff_ptr[0] = '\0';
      dspl_descr->dminput_pad->buff_ptr = dspl_descr->dminput_pad->work_buff_ptr;

      /***************************************************************
      *  clear out the record of the text cursor whereever it may be. 
      ***************************************************************/
      clear_text_cursor(None, dspl_descr);

      release_x_pastebufs();
      mem_kill(dspl_descr->main_pad->token);
      dspl_descr->main_pad->token = NULL;
      mem_kill(dspl_descr->dminput_pad->token);
      dspl_descr->dminput_pad->token = NULL;
      mem_kill(dspl_descr->dmoutput_pad->token);
      dspl_descr->dmoutput_pad->token = NULL;
      if (dspl_descr->unix_pad)
         {
            if (dspl_descr->unix_pad->token)
               {
                  mem_kill(dspl_descr->unix_pad->token);
                  dspl_descr->unix_pad->token = NULL;
               }
            dspl_descr->unix_pad->buff_modified = False;
            dspl_descr->unix_pad->work_buff_ptr[0] = '\0';
            dspl_descr->unix_pad->buff_ptr = dspl_descr->unix_pad->work_buff_ptr;
         }
      set_unix_prompt(dspl_descr, "");

      strcpy(edit_file, cmd);

      /***************************************************************
      *  If options were supplied, copy them to the real option values
      *  holder.
      ***************************************************************/
      if (lcl_option_values)
         {
            FREE_NEW_OPTIONS(OPTION_VALUES, OPTION_COUNT);
            OPTION_VALUES = lcl_option_values;
         }
      if (lcl_option_from_parm)
         {
            free((char *)OPTION_FROM_PARM);
            OPTION_FROM_PARM = lcl_option_from_parm;
         }

      /***************************************************************
      *  Move the window so it is not right over the last window
      *  and position us at the top of the dataset.  Use the geo parm if
      *  it was supplied.
      ***************************************************************/
      if (lcl_argc < 0 || OPTION_VALUES[GEOMETRY_IDX] == NULL)
         {
            OPTION_VALUES[GEOMETRY_IDX] = wdf_get_geometry(dspl_descr->display, dspl_descr->properties);
            if (OPTION_VALUES[GEOMETRY_IDX] == NULL)
               {
                  sprintf(work_path, "%dx%d+%d+%d", dspl_descr->main_pad->window->width, dspl_descr->main_pad->window->height, dspl_descr->main_pad->window->x+30, dspl_descr->main_pad->window->y+30);
                  OPTION_VALUES[GEOMETRY_IDX] = CE_MALLOC(strlen(work_path)+1);
                  if (OPTION_VALUES[GEOMETRY_IDX])
                     strcpy(OPTION_VALUES[GEOMETRY_IDX], work_path);
               }
         }

      dspl_descr->main_pad->file_col_no = 0;
      dspl_descr->main_pad->file_line_no = -1;
      dspl_descr->main_pad->first_line = 0;
      dspl_descr->main_pad->first_char = 0;
      dspl_descr->hold_mode = False;
      dspl_descr->main_pad->buff_modified = False;
      dspl_descr->main_pad->buff_ptr = NULL;

      dspl_descr->cursor_buff->x            = dspl_descr->main_pad->sub_window->x;
      dspl_descr->cursor_buff->y            = dspl_descr->main_pad->sub_window->y;
      dspl_descr->cursor_buff->win_col_no   = 0;
      dspl_descr->cursor_buff->win_line_no  = 0;
      dspl_descr->cursor_buff->up_to_snuff  = 0;

      *dspl_descr->main_pad->echo_mode = NO_HIGHLIGHT;
      dspl_descr->mark1.mark_set = False;
      dspl_descr->pad_mode = True;
      dspl_descr->vt100_mode = False;

      /***************************************************************
      *  For reinitialization of the tty stuff and the utmp, switch
      *  back to being root if we can.  If we cannot, that is ok.
      ***************************************************************/
#ifndef ultrix
      if (getuid())
#endif
#ifndef _INCLUDE_HPUX_SOURCE
#ifdef ultrix
         setreuid(geteuid(), 0);
#else
         seteuid(0);
#endif
#else
         setresuid(-1, 0, -1);
#endif

      /***************************************************************
      *  Reset the scroll position to the new empty window.
      ***************************************************************/
      scroll_some(NULL, dspl_descr, 0);

   }

return(pid);

} /* end of dm_cp  */
#endif


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
void                  (*cstat)(), (*c1stat)();
char                 *lcl_argv[MAX_CV_ARGS];
int                   lcl_argc;
char                **lcl_argvp;
DISPLAY_DESCR        *walk_dspl;

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
            sprintf(msg, "(%s) Too many args %d", dmsyms[dmc->any.cmd].name, dmc->cpo.argc);
            dm_error(msg, DM_ERROR_BEEP);
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

search_path(lcl_argvp[0], path_name); /* in normalize.h */
if (path_name[0] == '\0')
   {
      sprintf(msg, "(%s) Cannot execute %s (%s)", dmsyms[dmc->any.cmd].name, lcl_argvp[0], strerror(errno));
      dm_error(msg, DM_ERROR_BEEP);
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
      c1stat = signal(SIGCLD, SIG_DFL);                               
#endif
   }


pid = fork();
if (pid == -1)
   {
      sprintf(msg, "(%s) Can't fork:  %s", dmsyms[dmc->xc.cmd].name, strerror(errno));
      dm_error(msg, DM_ERROR_BEEP);
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

            signal(SIGCHLD, cstat);                               
#ifdef SIGCLD
            signal(SIGCLD, c1stat);                               
#endif
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
                  sprintf(msg, "Wait terminated by signal (%s)", strerror(errno));
                  dm_error(msg, DM_ERROR_LOG);
               }
            else
               if (status_data == 0)
                  return;
               else
                  if ((status_data & 0x000000ff) == 0)
                     {
                        sprintf(msg, "Process exited, code %d", status_data >> 8);
                        dm_error(msg, DM_ERROR_BEEP);
                     }
                  else
                     if ((status_data & 0x0000ff00) == 0)
                        {
                           sprintf(msg, "Process aborted, signal code %d", status_data);
                           dm_error(msg, DM_ERROR_BEEP);
                        }
                     else
                        {
                           sprintf(msg, "Process killed, signal code %d", status_data >> 8);
                           dm_error(msg, DM_ERROR_BEEP);
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
      for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
      {
         dm_rec(NULL, walk_dspl);
         close(ConnectionNumber(walk_dspl->display));
      }

      signal(SIGCHLD, SIG_DFL);                               
#ifdef SIGCLD
      signal(SIGCLD, SIG_DFL);                               
#endif

      DEBUG4(fprintf(stderr, "%s Child(1): pid = %d, ppid = %d\n", dmsyms[dmc->ce.cmd].name, getpid(), getppid());)
      paste_dir = setup_paste_buff_dir(dspl_descr->main_pad->window, dmsyms[dmc->any.cmd].name);
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
                  (void) setpgrp(0, getpid());
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
         for (i = 0; i < dmc->cpo.argc; i++)
            fprintf(stderr, "argv[%02d] = %s\n", i, lcl_argvp[i]);
      )

      /***************************************************************
      *  Save the main window id in the environment for the ce api.
      ***************************************************************/
      sprintf(msg, "CE_WINDOW=%08X", dspl_descr->main_pad->window->drawable);
      putenv(msg);

      /***************************************************************
      *  Make sure we do not have a saved uid of root.
      ***************************************************************/
      if (getuid())
         setuid(getuid());

      execv(path_name, lcl_argvp);

      /* should never get here */

      sprintf(msg, "(%s) Call to execv failed: %s", dmsyms[dmc->ce.cmd].name, strerror(errno));
      dm_error(msg, DM_ERROR_LOG);

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

   2.  mark1         -  pointer to MARK_REGION (INPUT)
                        If set, this is the most current mark.  It is matched
                        with tdm_mark or cursor_buff to get the line range.

   3.  cursor_buff   - pointer to BUFF_DESCR (INPUT)
                       This is the buffer description which shows the current
                       location of the cursor.

   4.  main_window   - pointer to DRAWABLE_DESCR (INPUT)
                       This is the drawable description for the main window.  It is
                       needed by open_paste_buffer and is passed through.

   5.  dmoutput_cur_descr  - pointer to PAD_DESCR (INPUT)
                       This is the pad description for the dm_output window.  It is
                       needed if the command output is to be pasted into the dm_output window.

   6.  echo_mode     - int (INPUT)
                       This is the current echo mode.  An xc or xd acts like an
                       xc -r (or xd -r) if the echo mode is RECTANGULAR_HIGHLIGHT




FUNCTIONS :

   1.   Get the name of the paste buffer directory and build the two paste buffer names.

   2.   Take the passed command and put < input buffer and > output buffer on the end.

   3.   Cut the marked data into the input paste buffer.  Use -f to force a file to be created
        with no leading line number count.  If ! -c was specified, use copy instead of cut.

   4.   Delete the target paste buffer.

   5.   Use the system command to call the passed routine.
   
   6.   If the target paste buffer contains data, paste it into the area marked by the cut.
        Only do this is the data was cut and not copied.


RETURNED VALUE:
   redraw -  The window mask anded with the type of redraw needed is returned.
        

*************************************************************************/

int   dm_bang(DMC               *dmc,                /* input   */
              DISPLAY_DESCR     *dspl_descr)         /* input   */
{
static char          *system_cmd_pat = "(%s) ";

DMC                   built_cmd;
char                 *paste_dir;
char                  cut_name[MAXPATHLEN];
char                  paste_name[MAXPATHLEN];
char                  error_name[MAXPATHLEN];
int                   redraw_needed = (dspl_descr->cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);
int                   cmd_len;
int                   rc;
char                 *system_cmd;
int                   top_line;
int                   top_col;
int                   bottom_line;
int                   bottom_col;
PAD_DESCR            *buffer;
int                   save_line;
int                   save_col;
PAD_DESCR            *save_buffer;
struct stat           file_stats;
FILE                 *stream;
int                   init_dmoutput_pos;
int                   save_echo_mode;
int                   save_mark_set;

/***************************************************************
*  
*  Validate that if a write is to be done, the pad is writable.
*  
***************************************************************/

if (!WRITABLE(dspl_descr->cursor_buff->current_win_buff->token) && !dmc->bang.dash_c)
   {
      dm_error("(!) Text is read-only", DM_ERROR_BEEP);
      stop_text_highlight(dspl_descr);
      dspl_descr->echo_mode = NO_HIGHLIGHT;
      dspl_descr->mark1.mark_set = False;
      return(redraw_needed);
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
      dm_error("(!) Invalid Text Range", DM_ERROR_BEEP);
      stop_text_highlight(dspl_descr);
      dspl_descr->echo_mode = NO_HIGHLIGHT;
      dspl_descr->mark1.mark_set = False;
      return(redraw_needed);
   }

/***************************************************************
*  
*  First build the name of the paste buffer to use.
*  
***************************************************************/

paste_dir = setup_paste_buff_dir(dspl_descr->main_pad->window, dmsyms[dmc->any.cmd].name);
if (paste_dir == NULL)
   {
      stop_text_highlight(dspl_descr);
      dspl_descr->echo_mode = NO_HIGHLIGHT;
      dspl_descr->mark1.mark_set = False;
      return(redraw_needed);
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

sprintf(cut_name,   "%s/%s",  paste_dir, BANGIN_PASTEBUF);
sprintf(paste_name, "%s/%s", paste_dir, BANGOUT_PASTEBUF);
sprintf(error_name, "%s/%s", paste_dir, ERR_PASTEBUF);

cmd_len = strlen(dmc->bang.cmdline) + strlen(system_cmd_pat) + 5;


system_cmd = CE_MALLOC(cmd_len);
if (!system_cmd)
   {
      stop_text_highlight(dspl_descr);
      dspl_descr->echo_mode = NO_HIGHLIGHT;
      dspl_descr->mark1.mark_set = False;
      return(redraw_needed);
   }

DEBUG4(fprintf(stderr, "!: %s\n", dmc->bang.cmdline);)


/***************************************************************
*  
*  Cut the marked region into the first paste buffer.
*  
***************************************************************/

built_cmd.next      = NULL;
if (dmc->bang.dash_c)
   built_cmd.xc.cmd    = DM_xc;
else
   built_cmd.xc.cmd    = DM_xd;
built_cmd.xc.temp   = False;
built_cmd.xc.dash_f = True;
built_cmd.xc.dash_a = False;
built_cmd.xc.dash_r = False;
built_cmd.xc.path   = cut_name;

redraw_needed = dm_xc(&built_cmd,
                      &dspl_descr->mark1,
                      dspl_descr->cursor_buff,
                      dspl_descr->main_pad->window,
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


bang_system(dmc->bang.cmdline, cut_name, paste_name, error_name);

/***************************************************************
*  
*  If a paste buffer was created, paste its contents into the
*  marked area.  If -c was specified, we do not do any paste
*  unless the -m (message) option was specified.  -m always
*  tells us to write the output in the DM output window.
*  
***************************************************************/

if (!dmc->bang.dash_c || dmc->bang.dash_m)
   {
      rc = stat(paste_name, &file_stats);

      /***************************************************************
      *  
      *  If there is a BangOut file and it is bigger than the 2
      *  chars we loaded it with, do the paste.
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
                         sprintf(paste_name, "(%s) Invalid Text Range", dmsyms[dmc->xc.cmd].name);
                         dm_error(paste_name, DM_ERROR_BEEP);
                         return(0);
                      }

                   if ((dspl_descr->echo_mode == RECTANGULAR_HIGHLIGHT) && (top_col > bottom_col))
                      top_col = bottom_col;

                   redraw_needed |= dm_position(dspl_descr->cursor_buff, buffer, top_line, top_col);
               }
            else
               {
                  dm_error(" ", DM_ERROR_MSG);
                  (void) dm_position(dspl_descr->cursor_buff, dspl_descr->dmoutput_pad, total_lines(dspl_descr->dmoutput_pad->token)-1, 0);
                  redraw_needed |= DMOUTPUT_MASK & FULL_REDRAW;
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
            built_cmd.xp.cmd    = DM_xp;
            built_cmd.xp.dash_f = False;
            built_cmd.xp.dash_a = False;
            if (dspl_descr->echo_mode == RECTANGULAR_HIGHLIGHT)
               built_cmd.xp.dash_r = True;
            else
               built_cmd.xp.dash_r = False;
            built_cmd.xp.path   = BANGOUT_PASTEBUF;
            redraw_needed |= dm_xp(&built_cmd, dspl_descr->cursor_buff, dspl_descr->main_pad->window, dspl_descr->escape_char);

            /***************************************************************
            *  If the output is going to the dm output window, trim off
            *  trailing blank or null lines.
            ***************************************************************/
            if (dmc->bang.dash_m)
               trim_pad(dspl_descr->dmoutput_pad, init_dmoutput_pos);

         }
   }

stderr_to_dmoutput(init_dmoutput_pos, dspl_descr);
redraw_needed |= dm_position(dspl_descr->cursor_buff, save_buffer, save_line, save_col);

dspl_descr->echo_mode = NO_HIGHLIGHT;
dspl_descr->mark1.mark_set = False;
return(redraw_needed);

} /* end of dm_bang */


static void stderr_to_dmoutput(int                init_dmoutput_pos,
                               DISPLAY_DESCR     *dspl_descr)
{
FILE                 *stream;
char                 *paste_dir;
char                  stderr_name[MAXPATHLEN];
DMC                   built_cmd;
int                   rc;
struct stat           file_stats;

paste_dir = setup_paste_buff_dir(dspl_descr->main_pad->window, "stderr_to_dmoutput");
if (paste_dir == NULL)
   paste_dir = "/tmp";
sprintf(stderr_name, "%s/%s", paste_dir, ERR_PASTEBUF);


rc = stat(stderr_name, &file_stats);

/***************************************************************
*  
*  If there is a BangOut file and it is bigger than the 2
*  chars we loaded it with, do the paste.
*  
***************************************************************/

if (rc == 0 && file_stats.st_size > 0)
   {
      dm_error(" ", DM_ERROR_MSG);
      (void) dm_position(dspl_descr->cursor_buff, dspl_descr->dmoutput_pad, total_lines(dspl_descr->dmoutput_pad->token)-1, 0);

      /***************************************************************
      *  
      *  Paste the buffer in the dmoutput window.
      *  
      ***************************************************************/

      built_cmd.xp.cmd    = DM_xp;
      built_cmd.xp.dash_f = False;
      built_cmd.xp.dash_a = False;
      built_cmd.xp.dash_r = False;
      built_cmd.xp.path   = ERR_PASTEBUF;
      dm_xp(&built_cmd, dspl_descr->cursor_buff, dspl_descr->main_pad->window, dspl_descr->escape_char);

      /***************************************************************
      *  
      *  If the output is going to the dm output window, trim off
      *  trailing blank or null lines.
      *  
      ***************************************************************/

      trim_pad(dspl_descr->dmoutput_pad, init_dmoutput_pos);
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

sprintf(stderr_name, "%s/%s", paste_dir, ERR_PASTEBUF);
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
                     sprintf(msg, "Wrong file descriptor opened in pointing stdout to /dev/null in child process");
                  else
                     sprintf(msg, "Could not reopen stdout to /dev/null in child process (%s)", strerror(errno));
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
      sprintf(paste_name, "%s/%s", paste_dir, BANGOUT_PASTEBUF);
      unlink(paste_name);

      fclose(stdout);
      stream = fopen(paste_name, "w");
      if (stream != stdout)
         {
            if (stream)
               sprintf(msg, "Wrong file descriptor opened in pointing stdout to %s in child process", paste_name);
            else
               sprintf(msg, "Could not reopen stdout to %s in child process (%s)", paste_name, strerror(errno));
            dm_error(msg, DM_ERROR_BEEP);
         }
      else
         fflush(stream);

      fclose(stderr);
      stream = fopen(stderr_name, "w");
      if (stream != stderr)
         {
            if (stream)
               sprintf(msg, "Wrong file descriptor opened in pointing stderr to %s in child process", stderr_name);
            else
               sprintf(msg, "Could not reopen stderr to %s in child process (%s)", stderr_name, strerror(errno));
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
                       char    *stdin_path,
                       char    *stdout_path,
                       char    *stderr_path)
{
int                   status_data;
int                   pid;
int                   wait_pid;
int                   fd;
char                  msg[256];

void                  (*istat)(), (*qstat)();
void                  (*cstat)(), (*c1stat)();

cstat = signal(SIGCHLD, SIG_DFL);                               
#ifdef SIGCLD
c1stat = signal(SIGCLD, SIG_DFL);                               
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
      if ((fd = open(stdin_path, O_RDONLY)) != 0)
         {
            DEBUG(fprintf(stderr, "ce_system: Could not reopen stdin (%s)\n", strerror(errno));)
            exit(100);
         }
      close(1);
      if ((fd = open(stdout_path, O_WRONLY | O_CREAT | O_TRUNC, 0777)) != 1)
         {
            DEBUG(fprintf(stderr, "ce_system: Could not reopen stdout (%s)\n", strerror(errno));)
            exit(101);
         }
      close(2);
      if ((fd = open(stderr_path, O_WRONLY | O_CREAT | O_TRUNC, 0777)) != 2)
         exit(102);

      /***************************************************************
      *  Make sure we do not have a saved uid of root.
      ***************************************************************/
      if (getuid())
         setuid(getuid());
      
      /***************************************************************
      *  Save the main window id in the environment for the ce api.
      ***************************************************************/
      sprintf(msg, "CE_WINDOW=%08X", dspl_descr->main_pad->window->drawable);
      putenv(msg);


#ifdef PAD
      (void) execl(SHELL, "sh", "-c", s, 0);
#else
      (void) execl("/bin/sh", "sh", "-c", s, 0);
#endif
      _exit(127);
   }

if (pid == -1)
   {
      sprintf(msg, "(!) Fork failed (%s)", strerror(errno));
      dm_error(msg, DM_ERROR_LOG);
      return;
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
signal(SIGCHLD, cstat);                               
#ifdef SIGCLD
signal(SIGCLD, c1stat);                               
#endif

if (wait_pid < 0)
   {
      sprintf(msg, "Wait terminated by signal (%s)", strerror(errno));
      dm_error(msg, DM_ERROR_LOG);
   }
else
   if (status_data == 0)
      return;
   else
      if ((status_data & 0x000000ff) == 0)
         {
            switch(status_data >> 8)
            {
            case 100:
               sprintf(msg, "Child process could not open stdin file %s", stdin_path);
               break;
            case 101:
               sprintf(msg, "Child process could not open stdout file %s", stdout_path);
               break;
            case 102:
               sprintf(msg, "Child process could not open stderr file %s", stderr_path);
               break;
            default:
               sprintf(msg, "Command exited, code %d", status_data >> 8);
               break;
            }
            dm_error(msg, DM_ERROR_BEEP);
         }
      else
         if ((status_data & 0x0000ff00) == 0)
            {
               sprintf(msg, "Command aborted, signal code %d", status_data);
               dm_error(msg, DM_ERROR_BEEP);
            }
         else
            {
               sprintf(msg, "Command killed, signal code %d", status_data >> 8);
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
      signal(SIGCLD, SIG_IGN);                               
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
               (void) setpgrp(0, getpid());
   
               DEBUG4(fprintf(stderr, "Main: Child(1) pid = %d, ppid = %d\n", getpid(), getppid());)
               signal(SIGHUP,  SIG_DFL);                               
#ifdef SIGTSTP
               signal(SIGTSTP, SIG_IGN);
#endif
               DEBUG4(fprintf(stderr, "Main: Child(2) pid = %d, ppid = %d\n", getpid(), getppid());)
            }

return(pid);

} /* end of spoon */


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
char                 *msg;
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
            sprintf(buffer, "(env) %s does not exist", assignment);
            dm_error(buffer, DM_ERROR_BEEP);
         }
      else
         {
            sprintf(buffer, "%s=%s", assignment, value);
            dm_error(buffer, DM_ERROR_MSG);
         }
   }
else
   {
      value = CE_MALLOC(strlen(assignment) + 1);
      if (value)
         {
            strcpy(value, assignment);
            if (putenv(value) != 0)
               dm_error("(env) Unable to expand the environment", DM_ERROR_LOG);
         }

   }

} /* end of dm_env */



