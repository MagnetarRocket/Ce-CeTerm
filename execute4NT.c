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
*  module execute.c  - execute-nt.c
*  This is the Windows NT verison of execute.c.  It is a rewrite
*  from the original.  Instead of doing ifdef's, it is just a
*  separate module.
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
*     get_system_handles -  Get HANDLEs for stdin, stdout, and stderr.
*     bang_system        -  Version of the system command for use with dm_bang.
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h       */
#include <string.h>         /* /usr/include/string.h      */
#include <errno.h>          /* /usr/include/errno.h       */
#include <sys/types.h>      /* /usr/include/sys/types.h   */
#include <sys/stat.h>       /* /usr/include/sys/stat.h    */
#include <fcntl.h>          /* /usr/include/fcntl.h      NEED on WIN32 */
#ifndef MAXPATHLEN
#define MAXPATHLEN   256
#endif
#ifndef FOPEN_MAX
#ifdef WIN32
#define FOPEN_MAX   20
#else
#define FOPEN_MAX   64
#endif
#endif

#include <windows.h>
#include <io.h>
#include <stdlib.h>
#include <process.h>


#ifdef WIN32_XNT
#include "xnt.h"
#endif

#include "display.h"
#include "dmsyms.h"
#include "dmwin.h"
#include "emalloc.h"
#include "execute.h"
#include "getevent.h"
#include "help.h"
#include "init.h"
#include "kd.h"
#include "mark.h"
#include "memdata.h"
#include "mvcursor.h"
#include "normalize.h"
#ifdef PAD
#include "pad.h"
#else
#ifdef WIN32
#define SHELL "E:\\Cygnus\\B19\\H-i386-cygwin32\\bin\\bash"
/*#define SHELL "bash"*/
#else
#define SHELL "/bin/sh"
#endif
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
void   exit(int code);
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

static int   get_system_handles(char   *cmdname,
                                int     stdin_from_bang,
                                int     stdout_to_bang,
                                int     stderr_to_bang,
                                int     is_debug,
                                HANDLE *new_stdin,
                                HANDLE *new_stdout,
                                HANDLE *new_stderr);


static int bang_system(char    *s,
                       char    *shell_opts,
                       char    *stdin_path,
                       char    *stdout_path,
                       char    *stderr_path);

/************************************************************************

NAME:      dm_ce  DM ce, cv and cp(ceterm) commands

PURPOSE:    This routine invokes the ce or cv program on a file.

PARAMETERS:

   1.  dmc         - pointer to DMC (INPUT)
                     This is the command structure for the ce or cv command.

   2.  dspl_descr  - pointer to DISPLAY_DESCR (INPUT / OUTPUT in the child process)
                     This is the description for the display.  This routine needs to
                     get to all the pad descriptions.
                     It is reconfigured to the new file.


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
            DISPLAY_DESCR   *dspl_descr)             /* input / output  in child process */
{
int                   i;
int                   rc;
char                  msg[MAXPATHLEN];
char                  cmdline[MAX_LINE];
char                 *p;
char                 *type_parm;
char                 *exec_path;
char                  link[MAXPATHLEN];
DWORD                 dwCreationFlags;
STARTUPINFO           startup_info;
PROCESS_INFORMATION   p_info;
char                  sys_msg[256];
HANDLE                new_stdin = INVALID_HANDLE_VALUE;
HANDLE                new_stdout = INVALID_HANDLE_VALUE;
HANDLE                new_stderr = INVALID_HANDLE_VALUE;


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
      snprintf(link, sizeof(link), VERSION_TXT, VERSION);
      snprintf(msg, sizeof(msg), "%s  Compiled: %s %s", link, compile_date, compile_time);
      dm_error_dspl(msg, DM_ERROR_MSG, dspl_descr);
      return(-1);
   }

/***************************************************************
*  
*  Global variable cmd_path (parms.h) contains the path of the
*  program being executed.  If the name starts with drive: or
*  a / we will use it.  Otherwise see if we can find it with
*  get execution path.
*  
***************************************************************/
if ((cmd_path[1] == ':') || (cmd_path[0] == '\\') || (cmd_path[0] == '/'))
   exec_path = cmd_path;
else
   exec_path = get_execution_path(dspl_descr,  cmdname); /* never returns null */

if (dmc->ce.cmd == DM_ce)
   type_parm = " -edit";
else
   if (dmc->ce.cmd == DM_cv)
      type_parm = " -browse";
   else
      type_parm = " -term";

DEBUG4(fprintf(stderr, "Will execv \"%s\"\n", exec_path);)
if (!exec_path || !(*exec_path))
  {
      snprintf(msg, sizeof(msg), "(%s) Cannot find ce/ceterm path to execute, pad mode = %d", dmsyms[dmc->ce.cmd].name, dspl_descr->pad_mode);
      dm_error_dspl(msg, DM_ERROR_LOG, dspl_descr);
      return(-1);
   }

/***************************************************************
*  
*  Collect the args into a single command line
*  
***************************************************************/

strcpy(cmdline, exec_path);
strcat(cmdline, type_parm);

for (i=1; i<dmc->cv.argc; i++)
{
   strcat(cmdline, " "); 
   if ((ENV_VARS) && (strchr(dmc->cv.argv[i], '$') != NULL))
      {
         strcpy(link, dmc->cv.argv[i]);
         substitute_env_vars(link, sizeof(link), dspl_descr->escape_char);
         strcat(cmdline, link);
      }
   else
      strcat(cmdline, dmc->cv.argv[i]);
}

/***************************************************************
*  
*  This tells the child process how to send messages back to
*  this ce session during startup
*  
***************************************************************/
snprintf(link, sizeof(link), "%s=%X", DMERROR_MSG_WINDOW, dspl_descr->main_pad->x_window);
p = malloc_copy(link);
if (p)
   putenv(p);


rc = get_system_handles(dmsyms[dmc->any.cmd].name,
                        FALSE,
                        FALSE, /* save stdout - no */
                        FALSE, /* save stderr - no */
                        FALSE, /* is_debug doesn't do anything right now */
                        &new_stdin,
                        &new_stdout,
                        &new_stderr);
if (rc != 0)
   return(-1);


dwCreationFlags = 0;
memset((char *)&startup_info, 0, sizeof(startup_info));
startup_info.cb = sizeof(startup_info);
startup_info.dwFlags = STARTF_USESHOWWINDOW
                       | STARTF_USESTDHANDLES;
startup_info.wShowWindow = SW_HIDE; /* no console window */
startup_info.hStdInput  = new_stdin;
startup_info.hStdOutput = new_stdout;
startup_info.hStdError  = new_stderr;

if (CreateProcess(NULL, /* pointer to name of executable module, take from commandline */
                  cmdline, /* pointer to command line string */
                  NULL,  /* pointer to process security attributes */
                  NULL,  /* pointer to thread security attributes */
                  TRUE,  /* handle inheritance flag */
                  dwCreationFlags, /* creation flags */
                  NULL, /* pointer to new environment block - Use current environment */
                  NULL, /* pointer to current directory name. Use currect dir  */
                  &startup_info, /* pointer to STARTUPINFO */
                  &p_info /* pointer to PROCESS_INFORMATION */
                  ))
   {
      CloseHandle(p_info.hProcess);
      CloseHandle(p_info.hThread);
      rc = 0;
   }
else
   {
      /* create process failed */
      rc = GetLastError();
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                    NULL,  /*  pointer to message source, ignored */
                    rc,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                    sys_msg,
                    sizeof(sys_msg),
                    NULL);
      snprintf(msg, sizeof(msg), "(cpo) CreateProcess failed (%s) - \"%s\"", dmsyms[dmc->any.cmd].name, sys_msg, cmdline);
      dm_error(msg, DM_ERROR_BEEP);
      rc = -1;
   }

if (new_stdin != INVALID_HANDLE_VALUE)
   CloseHandle(new_stdin);

if (new_stdout != INVALID_HANDLE_VALUE)
   CloseHandle(new_stdout);

if (new_stderr != INVALID_HANDLE_VALUE)
   CloseHandle(new_stderr);

return(rc);

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
char                  msg[MAXPATHLEN+256];
char                 *command_line;
int                   i;
int                   len = 0;
int                   rc;
int                   init_dmoutput_pos;
int                   save_line;
int                   save_col;
PAD_DESCR            *save_buffer;
char                 *p;
DWORD                 dwCreationFlags;
STARTUPINFO           startup_info;
PROCESS_INFORMATION   p_info;
char                  sys_msg[256];
HANDLE                new_stdin = INVALID_HANDLE_VALUE;
HANDLE                new_stdout = INVALID_HANDLE_VALUE;
HANDLE                new_stderr = INVALID_HANDLE_VALUE;


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

for (i = 0; i < dmc->cpo.argc; i++)
   len += strlen(dmc->cpo.argv[i]) + 1;

command_line = CE_MALLOC(len+1);
*command_line = '\0';

for (i = 0; i < dmc->cpo.argc; i++)
{
   if (i > 0)
      strcat(command_line, " ");
   strcat(command_line, dmc->cpo.argv[i]);
}


/***************************************************************
*  Dump the arguments for debugging
***************************************************************/
DEBUG4(fprintf(stderr, "cpo: \"%s\"\n", command_line);)

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


rc = get_system_handles(dmsyms[dmc->any.cmd].name,
                        FALSE,
                        dmc->cpo.dash_s && dmc->cpo.dash_w,  /* save stdout if told to and we are going to wait */
                        dmc->cpo.dash_w,  /* save stderr if we are going to wait */
                        FALSE, /* is_debug doen't do anything right now */
                        &new_stdin,
                        &new_stdout,
                        &new_stderr);
if (rc != 0)
   return;


dwCreationFlags = 0;
memset((char *)&startup_info, 0, sizeof(startup_info));
startup_info.cb = sizeof(startup_info);
startup_info.dwFlags = STARTF_USESHOWWINDOW
                       | STARTF_USESTDHANDLES;
startup_info.wShowWindow = SW_HIDE; /* no console window */
startup_info.hStdInput  = new_stdin;
startup_info.hStdOutput = new_stdout;
startup_info.hStdError  = new_stderr;

if (CreateProcess(NULL, /* pointer to name of executable module, take from commandline */
                  command_line, /* pointer to command line string */
                  NULL,  /* pointer to process security attributes */
                  NULL,  /* pointer to thread security attributes */
                  TRUE,  /* handle inheritance flag */
                  dwCreationFlags, /* creation flags */
                  NULL, /* pointer to new environment block - Use current environment */
                  NULL, /* pointer to current directory name. Use currect dir  */
                  &startup_info, /* pointer to STARTUPINFO */
                  &p_info /* pointer to PROCESS_INFORMATION */
                  ))
   {
      if (dmc->cpo.dash_w)
         {
            WaitForSingleObject(p_info.hProcess, INFINITE);
            if (!GetExitCodeProcess(p_info.hProcess, &rc))
               rc = GetLastError();

            mark_point_setup(NULL,                    /* input  */
                             dspl_descr->cursor_buff, /* input  */
                             &save_line,              /* output */
                             &save_col,               /* output */
                             &save_buffer);           /* output */

            dm_clean(True);  /* make sure dmoutput window starts out empty */
            init_dmoutput_pos = total_lines(dspl_descr->dmoutput_pad->token);

            stderr_to_dmoutput(init_dmoutput_pos, dspl_descr);

            dm_position(dspl_descr->cursor_buff, save_buffer, save_line, save_col);

            if (rc != 0)
               {
                  snprintf(msg, sizeof(msg), "(%s) Process exited, code %d", dmsyms[dmc->any.cmd].name, rc);
                  dm_error_dspl(msg, DM_ERROR_BEEP, dspl_descr);
               }

         }
      CloseHandle(p_info.hProcess);
      CloseHandle(p_info.hThread);
   }
else
   {
      /* create process failed */
      rc = GetLastError();
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                    NULL,  /*  pointer to message source, ignored */
                    rc,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                    sys_msg,
                    sizeof(sys_msg),
                    NULL);
      snprintf(msg, sizeof(msg), "(cpo) CreateProcess failed (%s) - \"%s\"", dmsyms[dmc->any.cmd].name, sys_msg, command_line);
      dm_error(msg, DM_ERROR_BEEP);
   }

free(command_line);
if (new_stdin != INVALID_HANDLE_VALUE)
   CloseHandle(new_stdin);

if (new_stdout != INVALID_HANDLE_VALUE)
   CloseHandle(new_stdout);

if (new_stderr != INVALID_HANDLE_VALUE)
   CloseHandle(new_stderr);

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

NAME:      get_system_handles -  Get HANDLEs for stdin, stdout, and stderr.

PURPOSE:    This routine closes and reopens files in a child process
            created by cps, cpo, or cp.

PARAMETERS:

   1. cmdname          - pointer to char (INPUT)
                         This is the command name, it is used in error messages.

   2. stdin_from_bang  - int (INPUT)
                         If true, this flag indicates that stdin should be pointed to
                         the BANGIN paste buffer.  If false, it is pointed to a scratch
                         /dev/null file.

   3. stdout_to_bang   - int (INPUT)
                         If true, this flag indicates that stdin should be pointed to
                         the BANGOUT paste buffer.  If false, it is pointed to a scratch
                         /dev/null file.

   4. stderr_to_bang   - int (INPUT)
                         If true, this flag indicates that stdin should be pointed to
                         the ERR paste buffer.  If false, it is pointed to a scratch
                         /dev/null file.

   5. is_debug         - int (INPUT)
                         If true, this flag indicates that we are debugging the
                         command being executed.  stdout and stderr are left alone
                         which means they probably point to the terminal which invoked ce.

   6. new_stdin        - pointer to HANDLE (OUTPUT)
                         The file handle for stdin is placed in the HANDLE pointed to by
                         this handle.  On failure, it is set to the constant 
                         INVALID_HANDLE_VALUE

   7. new_stdout       - pointer to HANDLE (OUTPUT)
                         The file handle for stdin is placed in the HANDLE pointed to by
                         this handle.  On failure, it is set to the constant 
                         INVALID_HANDLE_VALUE

   8. new_stderr       - pointer to HANDLE (OUTPUT)
                         The file handle for stdin is placed in the HANDLE pointed to by
                         this handle.  On failure, it is set to the constant 
                         INVALID_HANDLE_VALUE


FUNCTIONS :

   1.   If the paste dir pointer is NULL, use /tmp.

   2.   Point stdin to /dev/null

   3.   If this is a server or we are not saving stdout, point stdout
        and stdin at /dev/null unless we are in debug mode.

   3.   If we are saving stdout and stderr, point them at the special
        paste buffers.


*************************************************************************/

static int   get_system_handles(char   *cmdname,
                                int     stdin_from_bang,
                                int     stdout_to_bang,
                                int     stderr_to_bang,
                                int     is_debug,
                                HANDLE *new_stdin,
                                HANDLE *new_stdout,
                                HANDLE *new_stderr)
{
SECURITY_ATTRIBUTES   sa;
char                  msg[512];
char                  sys_msg[256];
char                 *paste_dir;
char                 *temp_dir;
char                  temp_file[20];
char                  stdin_path[MAXPATHLEN];
char                  stdout_path[MAXPATHLEN];
char                  stderr_path[MAXPATHLEN];

*new_stdin  = INVALID_HANDLE_VALUE;
*new_stdout = INVALID_HANDLE_VALUE;
*new_stderr = INVALID_HANDLE_VALUE;

/***************************************************************
*  
*  Get the paste buffer directory if we will need it.
*  
***************************************************************/
if (stdin_from_bang || stdout_to_bang || stderr_to_bang)
   {
      paste_dir = setup_paste_buff_dir(cmdname);
      if (paste_dir == NULL)
         return(1);
   }

if (!stdin_from_bang || !stdout_to_bang || !stderr_to_bang)
   {
      temp_dir = getenv("TEMP");
      if (temp_dir == NULL)
         temp_dir = "C:\\TEMP";
      snprintf(temp_file, sizeof(temp_file), ".%d", timeGetTime());
   }

/***************************************************************
*  
*  Build the two paste buffer names and the command to pass to system.
*  
***************************************************************/

if (stdin_from_bang)
   snprintf(stdin_path, sizeof(stdin_path),   "%s\\%s",  paste_dir, BANGIN_PASTEBUF);
else
   snprintf(stdin_path, sizeof(stdin_path),   "%s\\ce_stdin%s.txt",  temp_dir, temp_file);

if (stdout_to_bang)
   snprintf(stdout_path, sizeof(stdout_path), "%s\\%s", paste_dir, BANGOUT_PASTEBUF);
else
   snprintf(stdout_path, sizeof(stdout_path),   "%s\\ce_stdout%s.txt",  temp_dir, temp_file);

if (stderr_to_bang)
   snprintf(stderr_path, sizeof(stderr_path), "%s\\%s", paste_dir, ERR_PASTEBUF);
else
   snprintf(stderr_path, sizeof(stderr_path),   "%s\\ce_stderr%s.txt",  temp_dir, temp_file);

/***************************************************************
*  If stdin is temporary, create it.
***************************************************************/
memset((char *)&sa, 0, sizeof(sa));
sa.nLength = sizeof(sa);
sa.bInheritHandle = TRUE;


if (!stdin_from_bang)
   {
      *new_stdin = CreateFile(stdin_path,      // pointer to name of the file 
                              GENERIC_WRITE,    // access (read-write) mode 
                              FILE_SHARE_READ, // share mode 
                              &sa,             // pointer to security attributes 
                              CREATE_ALWAYS, // how to create 
                              FILE_ATTRIBUTE_NORMAL, // file attributes 
                              NULL); // handle to file with attributes to copy  
      if (*new_stdin == INVALID_HANDLE_VALUE)
         {
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                          NULL,  /*  pointer to message source, ignored */
                          GetLastError(),
                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                          sys_msg,
                          sizeof(sys_msg),
                          NULL);

            snprintf(msg, sizeof(msg), "(%s): Could not create stdin file %s(%s)\n", cmdname, stdin_path, sys_msg);
            dm_error(msg, DM_ERROR_BEEP);
            return(101);
         }
      else
         CloseHandle(*new_stdin);  /* empty file */
   }

/***************************************************************
*  Open the input paste buffer
***************************************************************/
*new_stdin = CreateFile(stdin_path,      // pointer to name of the file 
                        GENERIC_READ,    // access (read-write) mode 
                        FILE_SHARE_READ, // share mode 
                        &sa,             // pointer to security attributes 
                        OPEN_EXISTING , // how to create 
                        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN, // file attributes 
                        NULL); // handle to file with attributes to copy  
if (*new_stdin == INVALID_HANDLE_VALUE)
   {
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                    NULL,  /*  pointer to message source, ignored */
                    GetLastError(),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                    sys_msg,
                    sizeof(sys_msg),
                    NULL);

       snprintf(msg, sizeof(msg), "(%s): Could not open stdin file %s(%s)\n", cmdname, stdin_path, sys_msg);
       dm_error(msg, DM_ERROR_BEEP);
       return(100);
   }


/***************************************************************
*  Open the output target files.
***************************************************************/
*new_stdout = CreateFile(stdout_path,      // pointer to name of the file 
                         GENERIC_WRITE,    // access (read-write) mode 
                         FILE_SHARE_READ, // share mode 
                         &sa,             // pointer to security attributes 
                         CREATE_ALWAYS, // how to create 
                         FILE_ATTRIBUTE_NORMAL, // file attributes 
                         NULL); // handle to file with attributes to copy  
if (*new_stdout == INVALID_HANDLE_VALUE)
   {
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                    NULL,  /*  pointer to message source, ignored */
                    GetLastError(),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                    sys_msg,
                    sizeof(sys_msg),
                    NULL);

      snprintf(msg, sizeof(msg), "(%s): Could not open stdout file %s(%s)\n", cmdname, stdout_path, sys_msg);
      dm_error(msg, DM_ERROR_BEEP);
      CloseHandle(*new_stdin);
      *new_stdin = INVALID_HANDLE_VALUE;
      return(101);
   }

*new_stderr = CreateFile(stderr_path,      // pointer to name of the file 
                         GENERIC_WRITE|FILE_FLAG_NO_BUFFERING,    // access (read-write) mode 
                         FILE_SHARE_READ, // share mode 
                         &sa,             // pointer to security attributes 
                         CREATE_ALWAYS, // how to create 
                         FILE_ATTRIBUTE_NORMAL, // file attributes 
                         NULL); // handle to file with attributes to copy  
if (*new_stderr == INVALID_HANDLE_VALUE)
   {
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                    NULL,  /*  pointer to message source, ignored */
                    GetLastError(),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                    sys_msg,
                    sizeof(sys_msg),
                    NULL);

      snprintf(msg, sizeof(msg), "(%s): Could not open stderr file %s(%s)\n", cmdname, stderr_path, sys_msg);
      dm_error(msg, DM_ERROR_BEEP);
      CloseHandle(*new_stdin);
      *new_stdin = INVALID_HANDLE_VALUE;
      CloseHandle(*new_stdout);
      *new_stdout = INVALID_HANDLE_VALUE;
      return(102);
   }
      
return(0);

} /* end of get_system_handles */



/************************************************************************

NAME:      bang_system -  Reset stdin, stdout, and stderr and call a program

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
HANDLE                new_stdin = INVALID_HANDLE_VALUE;
HANDLE                new_stdout = INVALID_HANDLE_VALUE;
HANDLE                new_stderr = INVALID_HANDLE_VALUE;
SECURITY_ATTRIBUTES   sa;
int                   rc;
int                   i;
char                  msg[512];
char                 *p;
char                 *shell;
char                 *command_line;
DWORD                 dwCreationFlags;
STARTUPINFO           startup_info;
PROCESS_INFORMATION   p_info;
char                  sys_msg[256];


/***************************************************************
*  Open the input paste buffer and the output target files.
***************************************************************/
if (stdin_path && *stdin_path)
   {
      memset((char *)&sa, 0, sizeof(sa));
      sa.nLength = sizeof(sa);
      sa.bInheritHandle = TRUE;

      new_stdin = CreateFile(stdin_path,      // pointer to name of the file 
                             GENERIC_READ,    // access (read-write) mode 
                             FILE_SHARE_READ, // share mode 
                             &sa,             // pointer to security attributes 
                             OPEN_EXISTING , // how to create 
                             FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN, // file attributes 
                             NULL); // handle to file with attributes to copy  
      if (new_stdin == INVALID_HANDLE_VALUE)
         {
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                          NULL,  /*  pointer to message source, ignored */
                          GetLastError(),
                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                          sys_msg,
                          sizeof(sys_msg),
                          NULL);

            snprintf(msg, sizeof(msg), "ce_system: Could not open stdin file %s(%s)\n", stdin_path, sys_msg);
            dm_error(msg, DM_ERROR_BEEP);
            return(100);
         }
   }
else
   {
      dm_error("Internal Error:  Null stdin file name passed to bang_system", DM_ERROR_LOG);
      return(100);
   }

if (stdout_path && *stdout_path)
   {
      memset((char *)&sa, 0, sizeof(sa));
      sa.nLength = sizeof(sa);
      sa.bInheritHandle = TRUE;

      new_stdout = CreateFile(stdout_path,      // pointer to name of the file 
                              GENERIC_WRITE,    // access (read-write) mode 
                              FILE_SHARE_READ, // share mode 
                              &sa,             // pointer to security attributes 
                              CREATE_ALWAYS, // how to create 
                              FILE_ATTRIBUTE_NORMAL, // file attributes 
                              NULL); // handle to file with attributes to copy  
      if (new_stdout == INVALID_HANDLE_VALUE)
         {
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                          NULL,  /*  pointer to message source, ignored */
                          GetLastError(),
                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                          sys_msg,
                          sizeof(sys_msg),
                          NULL);

            snprintf(msg, sizeof(msg), "ce_system: Could not open stdout file %s(%s)\n", stdout_path, sys_msg);
            dm_error(msg, DM_ERROR_BEEP);
            CloseHandle(new_stdin);
            return(101);
         }
   }
else
   {
      dm_error("Internal Error:  Null stdout file name passed to bang_system", DM_ERROR_LOG);
      CloseHandle(new_stdin);
      return(101);
   }

if (stderr_path && *stderr_path)
   {
      memset((char *)&sa, 0, sizeof(sa));
      sa.nLength = sizeof(sa);
      sa.bInheritHandle = TRUE;

      new_stderr = CreateFile(stderr_path,      // pointer to name of the file 
                              GENERIC_WRITE|FILE_FLAG_NO_BUFFERING,    // access (read-write) mode 
                              FILE_SHARE_READ, // share mode 
                              &sa,             // pointer to security attributes 
                              CREATE_ALWAYS, // how to create 
                              FILE_ATTRIBUTE_NORMAL, // file attributes 
                              NULL); // handle to file with attributes to copy  
      if (new_stderr == INVALID_HANDLE_VALUE)
         {
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                          NULL,  /*  pointer to message source, ignored */
                          GetLastError(),
                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                          sys_msg,
                          sizeof(sys_msg),
                          NULL);

            snprintf(msg, sizeof(msg), "ce_system: Could not open stderr file %s(%s)\n", stderr_path, sys_msg);
            dm_error(msg, DM_ERROR_BEEP);
            CloseHandle(new_stdin);
            CloseHandle(new_stdout);
            return(102);
         }
   }
else
   {
      dm_error("Internal Error:  Null stderr file name passed to bang_system", DM_ERROR_LOG);
      CloseHandle(new_stdin);
      CloseHandle(new_stdout);
      return(102);
   }
      
/***************************************************************
*  Save the main window id in the environment for the ce api.
***************************************************************/
snprintf(msg, sizeof(msg), "CE_WINDOW=%08X", dspl_descr->main_pad->x_window);
p = malloc_copy(msg);
if (p)
   _putenv(p);

/***************************************************************
*  Save the expression mode in an environment variable.
***************************************************************/
snprintf(msg, sizeof(msg), "CE_EXPR=%c", (UNIXEXPR ? 'U' : 'A'));
p = malloc_copy(msg);
if (p)
   _putenv(p);


i = 0;
if (shell_opts && *shell_opts)
   i += strlen(shell_opts);
else
   shell_opts = "";

if (s && *s)
   i += strlen(s);
else
   s = "";

shell = getenv("SHELL");
if ((!shell) || (!(*shell)))
   shell = SHELL; /* literal SHELL defined in pad.h */

i += strlen(shell);
i += 16; /* newline, spaces, quotes, and -c */

command_line = CE_MALLOC(i);
if (!command_line)  /* message already output */
   return(105);
snprintf(command_line, i, "%s %s -c \"%s\"", shell, shell_opts, s);

DEBUG4(
fprintf(stderr, "bang_system: \"%s\"\n", command_line);
)

fflush(stderr);


dwCreationFlags = 0;
memset((char *)&startup_info, 0, sizeof(startup_info));
startup_info.cb = sizeof(startup_info);
startup_info.dwFlags = STARTF_USESHOWWINDOW
                       | STARTF_USESTDHANDLES;
startup_info.wShowWindow = SW_HIDE; /* no console window */
startup_info.hStdInput  = new_stdin;
startup_info.hStdOutput = new_stdout;
startup_info.hStdError  = new_stderr;

if (CreateProcess(NULL, /* pointer to name of executable module, take from commandline */
                  command_line, /* pointer to command line string */
                  NULL,  /* pointer to process security attributes */
                  NULL,  /* pointer to thread security attributes */
                  TRUE,  /* handle inheritance flag */
                  dwCreationFlags, /* creation flags */
                  NULL, /* pointer to new environment block - Use current environment */
                  NULL, /* pointer to current directory name. Use currect dir  */
                  &startup_info, /* pointer to STARTUPINFO */
                  &p_info /* pointer to PROCESS_INFORMATION */
                  ))
   {

      WaitForSingleObject(p_info.hProcess, INFINITE);
      if (!GetExitCodeProcess(p_info.hProcess, &rc))
         rc = GetLastError();
      CloseHandle(p_info.hProcess);
      CloseHandle(p_info.hThread);
   }
else
   {
      /* create process failed */
      rc = GetLastError();
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM ,
                    NULL,  /*  pointer to message source, ignored */
                    rc,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                    sys_msg,
                    sizeof(sys_msg),
                    NULL);
      snprintf(msg, sizeof(msg), "(!) CreateProcess failed (%s) - \"%s\"", sys_msg, command_line);
      dm_error(msg, DM_ERROR_BEEP);
   }

free(command_line);

/***************************************************************
*  Restore stdin, stdout, and stderr.
***************************************************************/
if (new_stdin != INVALID_HANDLE_VALUE)
   CloseHandle(new_stdin);

if (new_stdout != INVALID_HANDLE_VALUE)
   CloseHandle(new_stdout);

if (new_stderr != INVALID_HANDLE_VALUE)
   CloseHandle(new_stderr);

return(rc);

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
         N  -  pid of child
         0  -  This is the child
        -1  -  fork failed

*************************************************************************/

int  spoon(void)
{
return(0);

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


/************************************************************************

NAME:      is_executable      - Test if a path is executable

PURPOSE:    This routine builds the fully qualified path name and 
            calls the system routine to see if it is executable.
         
PARAMETERS:

   1.  path       - pointer to char (INPUT)
                    This is the name of the file to check.  It does not
                    have to be fully qualified.

FUNCTIONS :
   1.   Use our routine to get the full path namel

   2.   Use the system routine to get whether it is executable

RETURNED VALUE:
   True   -   The file is executable
   False  -   The file is not executable

*************************************************************************/

int is_executable(char   *path)
{

char      work[MAXPATHLEN];
DWORD     junk;

strcpy(work, path);
get_full_file_name(work, FALSE);

return(GetBinaryType(work, &junk));

} /* end of is_executable */
