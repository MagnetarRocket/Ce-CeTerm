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
*  module pw.c
*
*  Routines:
*         usage               - Print error message.
*         set_file_from_parm  - Get the file name from the parm string.
*         initial_file_open   - Open the file to be edited or browsed
*         initial_file_setup  - Test the file for read/write access
*         dm_ro               - Flip Flop the read only flag
*         ok_for_output       - Verify that a file can be opened for output.
*         ok_for_input        - Verify that a file can be opened for input.
*         dm_pn               - Pad Name / Rename command
*         dm_pw               - Pad Write (save) command
*         dm_cd               - Change Dir (for edit process)
*         dm_pwd              - print current DM working directory
*
*  Internal:
*         get_fake_file_stats - Make up a fake stat buff for new files.
*         bump_backup_files   - Rename .bak files to make room for a new .bak.
*         create_backup_dm    - Create .bak file DM style (use rename to make .bak file)
*         create_backup_vi    - Create .bak file vi style (use copy to make .bak file)
*         save_edit_dir       - Save the directory of the current file being edited
*         translate_tilde_dot - Process file names starting with ~./
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <signal.h>         /* /usr/include/signal.      */
#include <string.h>         /* /usr/include/string.h     */
#include <limits.h>         /* /usr/include/limits.h     */
#include <errno.h>          /* /usr/include/errno.h      */
#include <sys/types.h>      /* /usr/include/sys/types.h  */
#include <fcntl.h>          /* /usr/include/fcntl.h      */
#include <stdlib.h>
#ifdef WIN32
#include <io.h>
#include <direct.h>
#include <time.h>
#include <sys\utime.h>
#include <windows.h>
#include "usleep.h"
#else
#include <utime.h>          /* /usr/include/utime.h      */
#endif

#ifdef MVS_SERVICES
#include <requests.h>
#include <services.h>
#endif

/* kloodge to get around problem on apollo 
   NZERO is defined differently in limits.h and sys/param.h.
   I don't use it anyway.  Needed only when both limits.h and sys/param.h are included.
*/
#undef NZERO
#if !defined(OMVS) && !defined(WIN32)
#include <sys/param.h>      /* /usr/include/sys/param.h */
#endif
#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "cc.h"
#include "debug.h"
#include "emalloc.h"  /* needed for malloc copy */
#include "getevent.h"
#include "ind.h"
#include "dmsyms.h"
#include "dmwin.h"
#include "init.h"
#include "lock.h"
#ifdef PAD
#include "pad.h"
#endif
#include "pw.h"
#include "mark.h"
#include "normalize.h"
#include "parms.h"
#include "xerror.h"

#ifndef HAVE_STRLCPY
#include "strl.h"
#endif

#ifdef WIN32
#define getuid() -1
#define getgid() -1
#endif

/***************************************************************
*  
*  Declares of system routines whose definitions move around
*  to much to use the include.
*  
***************************************************************/

#ifndef PAD
char *getcwd(char *dirname, int maxsize);
#endif

#if !defined(WIN32) && !defined(IBMRS6000) && ! defined(linux)
int    access(char *path, int mode);
int    unlink(char *path);
int    read(int fd, char *buff, int maxsize);
int    write(int fd, char *buff, int count);
void   close(int fd);
int    chown(const char *file, int uid, int gid);
char   *getenv(const char *name);
#endif
  

static int   create_backup_dm(char       *edit_file);
static int   create_backup_vi(char         *edit_file,
                              struct stat  *file_stats);

static void  get_fake_file_stats(char          *edit_file,
                                 struct stat   *file_stats);

static void save_edit_dir(char  *dir);

static void translate_tilde_dot(char  *file);

/***************************************************************
*  
*  Usage - print a usage summary.
*  
***************************************************************/

static char *help [] = {
    "",
    "SYNOPSIS",
    "",
    "    ce [options] [<pathname>]",
    "    cv [options] [<pathname>]",
    "    ceterm [options] [<shell_pathname>]",
    "",
    "ARGUMENTS AND OPTIONS",
    "",
    "    OPTION                      DESCRIPTION                                                 X RESOURCE (CLASS)",
    "    ------                      -----------                                                 ------------------",
    "    -autoclose {y | n}          In ceterm, close window when eof is sent to the shell       Ce.autoclose: {y | n} ",
    "    -autocut {y | n}            Autocut highlighted region on typing, delete, or paste      Ce.autocut: {y | n} ",
    "    -autohold {y | n}           In ceterm, switch to hold mode when the screen fills        Ce.autochold: {y | n} ",
    "    -autosave <n>               Automatically write the edit file to disk every <n>         Ce.autosave: <n>",
    "                                keystrokes",
    "    -background <color>         Set background color to <color>                             Ce.background: <color>",
    "    -bell {y|n|visual|VISUAL}   Set set the action for a warning beep                       Ce.bell {y|n|visual|VISUAL}",
    "    -bgc <color>                Set background color to <color>, same as -background",
    "    -bkuptype {dm | vi | none}[<n>]  Use the specified backup style                         Ce.bkuptype: {dm | vi | none}[<n>]",
    "                                Code dm3 or vi3 to keep .bak, .bak1, .bak2, .bak3",
    "    -                           Path to site .Cekeys file, usually in app-defaults          Ce.cekeys : <path>",
    "    -CEHELPDIR <dir>            Directory to find .hlp files in                             Ce.CEHELPDIR: <dir>",
    "    -cmd \"<cmd>;<cmd>;...\"      Initial commands to be executed after bringup             Ce.cmd: <cmd>;<cmd>;...",
    "    -display <host>:0.0         Use the specified display, where <host> is a host name",
    "                                or a host internet address",
    "    -dotmode {0 | 1[string] | 2} ceterm detect password mode.                               ceterm.dotmode : {0 | 1[string] | 2}",
    "    -dpb <name>                 Specify default paste buffer name                           Ce.dfltPasteBuf : <name>",
    "    -envar {y | n}              Expand environment variables when clicking on file names    Ce.envar: {y | n}",
    "    -expr {a | u}               Use AEGIS (a) or UNIX (u) rules when parsing regular        Ce.expr: {Aegis | Unix}",
    "                                expressions",
    "    -foreground <color>         Set foreground (text) color to <color>                      Ce.foreground: <color>",
    "    -fgc <color>                Set foreground (text) color to <color>, same as -foreground",
    "    -findbrdr <num>             Num lines between top/bottom of window and found string     Ce.findbrdr: <num>",
    "    -font <name>                Use the X font <name>                                       Ce.font: <name>",
    "    -geometry <geometry>        Size and locate the window according to <geometry>,         Ce.geometry: <geometry>",
    "                                a string of the form '[c]<w>x<h>+<x>+<y>', where the",
    "                                leading 'c' is optional and if present instructs ce",
    "                                to treat <w> and <h> as columns and rows instead of",
    "                                pixels",
    "    -help                       show this help text and terminate",
    "    -ib                         Name of bitmap file for icon                                Ce.iconBitmap : <filepath>",
    "    -iconic                     Start ce or ceterm as an icon                               Ce.iconic : {y | n}",
    "    -isolatin {y | n}           Allow ISO Latin special characters                          Ce.isolatin : {y | n}",
    "    -kdp <name>                 Key Definition Propname, alternate set of key definitions   Ce.keydefProp : <name>",
    "    -lineno {y | n}             Show the line number column                                 Ce.lineno: {y | n}",
    "    -linemax <num>              Max lines to keep in a ceterm transcript pad                Ce.linemax: <num>",
    "    -load                       Load key definitions if not already loaded and terminate",
    "    -lockf {y | n}              Use SYS5 file locking                                       Ce.lockf: {y | n}",
    "    -ls                         Start the shell as a login shell, run the .profile          ceterm.loginShell : {y | n}",
    "    -man                        Throw away backspace characters - useful for browsing       ceterm.man : {y | n}",
    "                                'man' pages (Using resource outside ceterm is not advised.)",
    "    -mouse {on | off}           -mouse off divorces mouse cursor from text mouse            Ce.mouse: off  ",
    "    -name <name>                name for resources for the application in .Xdefaults",
    "    -nb                         do not create a backup file (same as '-bkuptype none')",
    "    -noreadahead                Do not read whole file into memory until needed             Ce.noreadahead: {y | n}",
    "    -offdspl {y | n}            Allow specification of geometries which are off screen      Ce.offdspl: {y | n}",
    "    -oplist                     Print the list of options and quit",
    "    -padding <n> DEPRECATED     Make vertical space between lines <n> percent of a line     Ce.padding: <n>  DEPRECATED",
    "    -pbd <dir>                  create paste buffers in <dir>                               Ce.pasteBuffDir: <dir>",
    "    -pdm {y | n}                Pull down menus, initially on or off                        Ce.pdm: {y | n}",
    "    -reload                     Load key definitions and terminate",
    "    -readlock                   Execute in read only mode",
    "    -sb {y | n | auto}          Scroll bars, on/off/on as needed                            Ce.scrollBar: {y | n | auto}",
    "    -sbcolors 'g hl sl sh'      Scroll bar colors: gutter highlight slider shadow, resp.    Ce.scrollBarColors: g hl sl sh",
    "    -sbwidth <num>              Default width in chars for horizontal scroll bar            Ce.scrollBarWidth: <num>",
    "    -sc {y | n}                 Make search operations case-sensitive                       Ce.caseSensitive: {y | n}",
    "    -scroll {y | n}             In ceterm, start with scroll on or off,                     Ce.scroll: {y | n}",
    "    -stdin                      Accept DM commands from stdin in ceterm process (used with popen)",
    "    -tabstops \"ts cmd\"          Tab positions for window,                                   Ce.tabstops : ts_cmd   (eg: ts 8 17 -r)",
    "    -tbf <name>                 Title bar font, use not <name> in the Ce title              Ce.titlebarfont : <name>",
    "    -title \"string\"             Title for the Motif title window,                           Ce.title : title string",
    "    -transpad                   Get data from a stdin via a slow device (used with popen)",
    "    -ut                         In ceterm, do not create utmp entries (see utmp(4))         Ce .utmpInhibit : {True | False}",
    "    -vcolors \"c1,c2,...c8\"      Colors for vt100 colorization, 8 comma separated values     Ce.vcolors: brown,red,#00aa00,yellow,blue,magenta,#00aaaa,gray",
    "    -version                    Show ce version information and terminate",
    "    -vt {on | off | auto}       Switch to vt100 mode for telent, passwords, etc.            Ce.vt: {on | off | auto}",
    "    -w                          Cause parent to wait for edit session to end - allows",
    "                                you to use ce as a mail editor",
    "    -wmadjx <num>               x offset added to all geo commands (<num> usually negative) Ce.wmAdjustX: <num>",
    "    -wmadjy <num>               y offset added to all geo commands (<num> usually negative) Ce.wmAdjustY: <num>",
    "    -xrm 'name:value'           extra X resources to set",
    "    <pathname>                  Edit the file at <pathname>",
    "    <shell_pathname>            Name of the shell to run, defaults to $SHELL",
    "",
    "CUSTOMER SERVICE",
    "",
    "    PHONE:          623-582-7323  or cell 602-478-0114",
    "    FAX:            602-993-5901",
    "    EMAIL:          stymar@lucent.com  or styma@swlink.net",
    "    HTML:           http://www.swlink.net/~styma/ce.shtml",
    "    ADDRESS:        Robert Styma Consulting, 3120 W. Anderson Dr., Phoenix, AZ   85053.",
    "",
    NULL
};

/* ARGSUSED */
void usage(char  *cmdname)
{
int     i;

for(i = 0; help[i]; i++) fprintf(stdout, "%s\n", help[i]);

} /* end usage */




/************************************************************************

NAME:      set_file_from_parm

PURPOSE:    This routine will get the file name from the parm string.
            If stdin is being read (no positional args), the special
            stdin string will be used.

PARAMETERS:

   1.  argc       - int (INPUT)
                    This is what is left of argc after the X parameter routines are
                    done with it.  argc should be 1 for stdin or 2 for a passed path name.

   2.  argv       - pointer to array of pointer to char (INPUT)
                    This is what is left of argv after the X parameter routines are
                    done with it.  All that should be left is the path name
                    to edit, or nothing at all for stdin.

   3.  edit_file  - pointer to char (INPUT)
                    This is the file being edited.
                    for a read of stdin, a special value <stdin> is placed
                    in this string.


FUNCTIONS :

   1.   If there are too many arguments, flag the error.

   2.   If there are no arguments, copy in the special stdin string..

   3.   copy the parm to the file name.

RETURNED VALUE
   rc  - int
         0  -  file set successfully
        -1  -  file set failed

*************************************************************************/

int  set_file_from_parm(DISPLAY_DESCR *dspl_descr,  /* input  */
                        int            argc,        /* input  */
                        char          *argv[],      /* input  */
                        char          *edit_file)   /* output */
{

int                   rc = 0;
int                   i, j;
char                  msg[1024];
char                  work_ce_cmd[MAXPATHLEN+7]; /* 4 for ';ce ', two for quotes, one for null char */
int                   lcl_argc;
char                 *lcl_argv[256];

/***************************************************************
*  
*  If there are more than 2 args, we have a bad command line.
*  bail out.  The dash options have already been parsed out.
*  
***************************************************************/

if (argc > 2)
   {
      for (i = 1; i < argc; i++)
         if (argv[i][0] == '-')
            {
               rc = -1;
               snprintf(msg, sizeof(msg), "Invalid or non-unique option \"%s\"", argv[i]);
               dm_error(msg, DM_ERROR_BEEP);
               if (argv[i][1] == 'h')
                  usage(argv[0]);
            }
         else
            if ((argv[i][0] == '+') && (sscanf(&argv[i][1], "%d%9s", &j, msg) == 1))
               {
                  /* else added 10/23/95 by RES to allow +800 as an option like vi and emacs */
                  store_cmds(dspl_descr, &argv[i][1], False);
                  for (j = i; j < argc; j++)
                      argv[j] = argv[j+1];
                  i--;
                  argc--;
               }

      if (rc == 0)
         {
            /***************************************************************
            *  Get the arguments to apply to each saved ce/cv comand.
            ***************************************************************/
            build_cmd_args(dspl_descr, edit_file, cmdname, &lcl_argc, lcl_argv);
            /***************************************************************
            *  Make a local copy of the args so they cannot get trashed by store cmds (-cmd option).
            *  RES 9/27/95
            ***************************************************************/
            for (j = 1; j < lcl_argc-1; j++) /* skip cmd name and edit file name */
                lcl_argv[j] = (char *)malloc_copy(lcl_argv[j]); 

            for (i = 2; i < argc; i++)
            {
               if (((cmdname[0] == 'c') && (cmdname[1] == 'v')) || (strcmp(cmdname, "crpad") == 0))
                  strncpy(work_ce_cmd, "cv", 3);
               else
                  strncpy(work_ce_cmd, "ce", 3);

               for (j = 1; j < lcl_argc-1; j++) /* skip cmd name and edit file name */
               {
                  strcat(work_ce_cmd, " ");
                  if (lcl_argv[j][0] != '-')
                     strcat(work_ce_cmd, "'");
                  strcat(work_ce_cmd, lcl_argv[j]);
                  if (lcl_argv[j][0] != '-')
                     strcat(work_ce_cmd, "'");
               }
               strcat(work_ce_cmd, " '");
               strncat(work_ce_cmd, argv[i], (sizeof(work_ce_cmd)-strlen(work_ce_cmd)));
               strcat(work_ce_cmd, "'");
               store_cmds(dspl_descr, work_ce_cmd, True);
            }
            fake_keystroke(dspl_descr); /* trigger execution of the commands we just saved away */
            strlcpy(edit_file, argv[1], MAX_LINE);
            /***************************************************************
            *  Free the local copy of the args
            *  RES 9/27/95
            ***************************************************************/
            for (j = 1; j < lcl_argc-1; j++) /* skip cmd name and edit file name */
                free(lcl_argv[j]); 
         }
   }
else
   if (argc < 2) /* stdin */
      {
         /***************************************************************
         *  
         *  If there are no arguments, read stdin
         *  
         ***************************************************************/
         strcpy(edit_file, STDIN_FILE_STRING);
      }
   else   /* edit a single file */
      {
         /***************************************************************
         *  
         *  One argument, a file name.  
         *  
         ***************************************************************/

         if (argv[1][0] == '-')
            {
               rc = -1;
               snprintf(msg, sizeof(msg), "Invalid or non-unique option \"%s\"", argv[1]);
               dm_error(msg, DM_ERROR_BEEP);
               if (argv[1][1] == 'h')
                  usage(argv[0]);
            }
         else
            strlcpy(edit_file, argv[1], MAX_LINE);
      }  /* argc == 2 */

if (ENV_VARS)
   substitute_env_vars(edit_file, MAXPATHLEN, dspl_descr->escape_char);

return(rc);
   
}  /* end of set_file_from_parm */



/************************************************************************

NAME:      initial_file_open

PURPOSE:    This routine will return the file descriptor to read the
            file to be edited or browsed.

PARAMETERS:


   1.  edit_file  -  pointer to char (INPUT/OUTPUT)
                     This is the name of the file to open.  For a read of
                     stdin, the special value defined in STDIN_FILE_STRING
                     is passed.  The passed filename/path, is updated to
                     be a fully qualified path name.

   2.  need_write -  pointer to int (INPUT/OUTPUT)
                     This flag specifies whether we are doing an edit or browse and
                     intend up front to modify this file.  If the request is for writable
                     and the file exists and is readable, the parm is changed to read only.

   3.  dspl_descr  - pointer to DISPLAY_DESCR (INPUT)
                     This is the current display description.  It is passed through to
                     other routines and used to access parameters set from arguments
                     and xresources to ce.


FUNCTIONS :

   1.   If we are reading stdin, return stdin.

   2.   If we are just reading, make sure the file exists and is readable.
        If so, open it otherwise return NULL.

   3.   If we are opening for write, make sure the file is writeable or
        that the file does not exist and the directory is writeable.
        If the file does not exist, /dev/null is opened.

RETURNED VALUE:

   instream - pointer to FILE.  
              The pointer to file is returned for the opened file edit file.
              On failure, NULL is returned.


*************************************************************************/

FILE  *initial_file_open(char          *edit_file,
                         int           *need_write,
                         DISPLAY_DESCR *dspl_descr)
{

char                  msg[MAXPATHLEN+1];
char                  new_cwd[MAXPATHLEN+1];
FILE                 *instream;
int                   ok;
int                   rc;
int                   file_exists;
struct stat           file_stats;
char                 *p;
#ifdef WIN32
char                 *temp_dir;
char                  stdin_path[MAXPATHLEN+1];
#endif

if (strcmp(edit_file, STDIN_FILE_STRING) == 0)
   {
      /***************************************************************
      *  
      *  If we are reading stdin, we are all set.
      *  If we are in transscript pad mode, lines will be read in one
      *  at a time, so we just process /dev/null.
      *  
      ***************************************************************/
      if (TRANSPAD)  /* command line option */
         {
#ifdef WIN32
            temp_dir = getenv("TEMP");
            if (temp_dir == NULL)
               temp_dir = "C:\\TEMP";
            snprintf(stdin_path, sizeof(stdin_path), "%s\\ce_dummy%d.txt",  temp_dir, timeGetTime());
            instream = fopen(stdin_path, "w");
            if (instream == NULL)
               {
                  snprintf(msg, sizeof(msg), "Can't open %s for output (%s)", stdin_path, strerror(errno));
                  dm_error(msg, DM_ERROR_LOG);
               }
            else
               {
                  fclose(instream);
                  instream = fopen(stdin_path, "w");
                  if (instream == NULL)
                     {
                        snprintf(msg, sizeof(msg), "Can't open %s for input (%s)", stdin_path, strerror(errno));
                        dm_error(msg, DM_ERROR_LOG);
                     }
               }
#else
            instream = fopen("/dev/null", "r");

            if (instream == NULL)
               {
                  snprintf(msg, sizeof(msg), "Can't open /dev/null (%s)", strerror(errno));
                  dm_error(msg, DM_ERROR_LOG);
               }
#endif
         }
      else
         instream = stdin;
   }
else
   {
      /***************************************************************
      *  
      *  -transpad is not valid if a file name is supplied
      *  
      ***************************************************************/
      if (TRANSPAD)  /* command line option */
         {
            dm_error("-transpad option ignored when file name supplied", DM_ERROR_BEEP); 
            free(OPTION_VALUES[TRANSPAD_IDX]);
            OPTION_VALUES[TRANSPAD_IDX] = strdup("no");
         }

      /***************************************************************
      *  RES 2/05/97
      *  Add code so that in ce/cv the initial current working 
      *  directory is the directory containing the file being edited.
      *  First we figure out what the new cwd will be.  We wait till
      *  the open is done to do the actual cd.
      ***************************************************************/
      translate_tilde_dot(edit_file);
      strlcpy(new_cwd, edit_file, sizeof(new_cwd));
      get_full_file_name(new_cwd, False); /* in normalize.c, False = Don't follow soft links */
      
      p = strrchr(new_cwd, '/');
      if (p) /* should always be true */
         *p = '\0';
      
      if (new_cwd[0] == '\0') /* was root directory */
         strcpy(new_cwd, "/");

      /***************************************************************
      *  
      *  Make sure that the file looks openable.  Change to read only
      *  mode if necessary.  Adjust the file name to get an absolute
      *  directory.
      *  
      ***************************************************************/

      ok = initial_file_setup(edit_file, need_write, &file_exists, &file_stats, dspl_descr);
      save_file_stats(&file_stats);/* in lock.c */

      /***************************************************************
      *  
      *  Finally, check that this is not a directory, and then open the file.
      *  
      ***************************************************************/

      if (ok)
         {
            /***************************************************************
            *  
            *  Check if there are other windows open on this file.
            *  cc_check_other_windows does not return if some other window
            *  has the file open and the routine successfully sends it a cc request.
            *  A zero return means all is ok.  A negative rc means X trouble.
            *  
            ***************************************************************/

            rc = cc_check_other_windows(&file_stats, dspl_descr, *need_write);
            if (rc == 0)
               {
                  if (file_exists)
                     {
                        if (LSF)
                           instream = filter_in(&(dspl_descr->ind_data), dspl_descr->hsearch_data, edit_file, "r");
                        else
                           instream = fopen(edit_file, "r");
                     }
                  else
                     instream = fopen("/dev/null", "r");

                  if (instream == NULL)
                     {
                        snprintf(msg, sizeof(msg), "Can't open %s (%s)", edit_file, strerror(errno));
                        dm_error(msg, DM_ERROR_BEEP);
                     }
                  /* RES 9/5/95 add file locking */
                  else
                     if (file_exists && LOCKF && (!LSF || /* LOCKF and LSF are cmdline/resource parms */
                         !is_virtual(&(dspl_descr->ind_data), dspl_descr->hsearch_data, edit_file)))
                        {
                           lock_init(&file_stats);/* in lock.c */
                           if (*need_write)
                              {
                                 if (lock_file(edit_file) == False) /* in lock.c */
                                    *need_write = False;
                              }
                        }
               }
            else
               instream = NULL;
         }
      else
         instream = NULL;

      /***************************************************************
      *  Cd to the make the current working directory the directory
      *  containing the file being editied.
      ***************************************************************/
      if (instream)
         {
            DEBUG1(fprintf(stderr, "initial_file_open: saving edit file dir %s\n", new_cwd);)
            save_edit_dir(new_cwd);
         }

   }  /* not stdin */

return(instream);

}  /* end of initial_file_open */


/************************************************************************

NAME:      initial_file_setup

PURPOSE:    This routine will return the file descriptor to read the
            file to be edited or browsed.

PARAMETERS:


   1.  edit_file  -  pointer to char (INPUT/OUTPUT)
                     This is the name of the file to open.  For a read of
                     stdin, the special value defined in STDIN_FILE_STRING
                     is passed.  The passed filename/path, is updated to
                     be a fully qualified path name.

   2.  need_write  - pointer to int (INPUT/OUTPUT)
                     This flag specifies whether we are doing an edit or browse and
                     intend up front to modify this file.  If the request is for writable
                     and the file exists and is readable, the parm is changed to read only.

   3.  file_exists - pointer to int (OUTPUT)
                     This flag is set to indicate whether the file already exists.
                     Values:  True  -  File exists
                              False -  File does not currently exist.

   4.  file_stats  - pointer to struct stat (OUTPUT)
                     If non-null, this parameter points to a stat structure into
                     which the stats for the file are placed if the file exists.
                     If NULL is passed, a local structure is used.

   5.  dspl_descr  - pointer to DISPLAY_DESCR (INPUT)
                     This is the current display description.  It is passed through to
                     other routines and used to access parameters set from arguments
                     and xresources to ce.

FUNCTIONS :

   1.   If file_stats is null, point it at a local stat buffer.

   2.   For non-vi backups translate the path name to a full name.

   3.   If we are opening for write, make sure the file is writeable or
        that the file does not exist and the directory is writeable.

   4.   Make sure the passed path is not a directory.

RETURNED VALUE:

   ok   -  int
           This flag indicates whether the file has been shown to be
           acceptable.
           True  -  File is ok, continue processing
           False -  File not ok, message already sent to DM error.


*************************************************************************/

int  initial_file_setup(char          *edit_file,
                        int           *need_write,
                        int           *file_exists,
                        struct stat   *file_stats,
                        DISPLAY_DESCR *dspl_descr)
{

char                  link[MAXPATHLEN+1];
char                  homedir[MAXPATHLEN+1];
char                  orig_edit_file[MAXPATHLEN+1];
char                 *p;
int                   ok;
int                   rc;
int                   hold_errno;
struct stat           lcl_stats;

*file_exists = True;  /* start out assuming the file exists */

if (file_stats == NULL)
   file_stats = &lcl_stats;

strlcpy(orig_edit_file, edit_file, sizeof(orig_edit_file));
#ifdef blah
RES 8/30/95, do_dollar_vars is always nailed to False 
if (do_dollar_vars)
   substitute_env_vars(edit_file, MAXPATHLEN, dspl_descr->escape_char);
#endif

/***************************************************************
*  
*  If the backup type is not vi and not none, then it is dm.
*  Track down any soft links since they mess up the DM backup
*  scheme.  The "or" with 0x20, turns an upper case V into a lower case v.
*  
*  BACKUP_TYPE is a macro in parms.h
*
***************************************************************/

if (!(BACKUP_TYPE) || ((*(BACKUP_TYPE) | 0x20) != 'v' && (*(BACKUP_TYPE) | 0x20) != 'n'))
   get_full_file_name(edit_file, True); /* in normalize.c */
else
   translate_tilde(edit_file);

/***************************************************************
*  
*  Check if the file exists. 
*  If so, make sure we can read/write it as required.
*  If it does not exist and we are planning a write, make
*  sure we can write in the directory.
*  
***************************************************************/

if (*need_write)
   {
      ok =  ok_for_output(edit_file, file_exists, dspl_descr);
      if (ok && !(*file_exists))
         get_fake_file_stats(edit_file, file_stats);
      if (!ok && *file_exists)
         {
            ok =  ok_for_input(edit_file, dspl_descr);
            if (ok)
               *need_write = False;
         }
   }
else
   ok =  ok_for_input(edit_file, dspl_descr);

/***************************************************************
*  
*  If we cannot get to the file, check for $variables and the
*  special Apollo ` in the file name.  If found, see if we
*  can access the file name normally.  If so, switch to vi
*  backup mode and use edit file.
*  
***************************************************************/

if (!ok && strpbrk(edit_file, "$`") && (strcmp(orig_edit_file, edit_file) != 0))
   {
      hold_errno = errno;
      if (*need_write)
         {
            p = translate_tilde(orig_edit_file); /* in normalize.c */
            if (p == NULL)
               return(ok);
            else
               strlcpy(link, orig_edit_file, sizeof(link));

            ok =  ok_for_output(orig_edit_file, file_exists, dspl_descr);
            if (!ok && *file_exists)
               {
                  ok =  ok_for_input(orig_edit_file, dspl_descr);
                  if (ok)
                     *need_write = False;
               }
         }
      else
         ok =  ok_for_input(orig_edit_file, dspl_descr);

      if (ok)
         {
            dm_error("File is a special Apollo soft link, switching to copy backups", DM_ERROR_BEEP);
            strlcpy(edit_file, orig_edit_file, MAXPATHLEN+1);
            *(BACKUP_TYPE) = 'v';
         }
      else
         errno = hold_errno;
   }


/***************************************************************
*  
*  Finally, check that this is not a directory.
*  
***************************************************************/

if (ok && *file_exists)
   {

      if (LSF)
         rc = ind_stat(&(dspl_descr->ind_data), dspl_descr->hsearch_data, edit_file, file_stats);
      else
         rc = stat(edit_file, file_stats);
#ifdef S_ISDIR
      if ((rc == 0) && (S_ISDIR(file_stats->st_mode)))
#else
      if ((rc == 0) && ((file_stats->st_mode & S_IFDIR) == S_IFDIR ))
#endif
         {
            snprintf(homedir, sizeof(homedir), "%s : is a directory", edit_file);
            dm_error(homedir, DM_ERROR_BEEP);
            ok = False;
         }
   }
else
   if (!ok)
      { 
         snprintf(homedir, sizeof(homedir), "%s : %s%s", edit_file, strerror(errno), 
                          ((!(*file_exists) && *need_write) ? " - directory is not writable" : ""));
         dm_error(homedir, DM_ERROR_BEEP);
      }

/***************************************************************
*  Check for crash files left around.  Added 9/8/95 RES
*  link and orig_edit_file are used as scrap variables.
***************************************************************/

strlcpy(link, edit_file, sizeof(link));
strlcat(link, ".CRA", sizeof(link));
if (access(link, 00) == 0)
   {
      snprintf(orig_edit_file, sizeof(orig_edit_file), "Crash file found: %s", link);
      dm_error(orig_edit_file, DM_ERROR_BEEP);
   }

return(ok);

}  /* end of initial_file_setup */


/************************************************************************

NAME:      get_fake_file_stats - Make up a fake stat buff for new files.

PURPOSE:    This routine will make up fake file stats for non existant
            files, the stats are reproducable based on name, but are
            not likely to be real.

PARAMETERS:


   1.  edit_file  -  pointer to char (INPUT)
                     This is the name of the file which does not exist.  This may
                     be a full or partial name.

   2.  file_stats  - pointer to struct stat (OUTPUT)
                     If non-null, this parameter points to a stat structure into
                     which the stats for the file are placed if the file exists.
                     If NULL is passed, a local structure is used.

FUNCTIONS :

   1.   Fill in the fixed fake values with mostly zeros.

   2.   Make up a unique device and inode number.

*************************************************************************/

static void  get_fake_file_stats(char          *edit_file,
                                 struct stat   *file_stats)
{
char                  work[MAXPATHLEN];
char                 *basename;
int                   i;
struct stat           dir_stats;

strlcpy(work, edit_file, sizeof(work));


/***************************************************************
*  
*  Do the fixed stuff first.
*  
***************************************************************/
memset((char *)file_stats, 0, sizeof(struct stat));
file_stats->st_mode = 0777;
/*file_stats->st_rdev = 0; */
/*file_stats->st_nlink = 0;*/
file_stats->st_uid = getuid();
file_stats->st_gid = getgid();
/*file_stats->st_size = 0;   */
/*file_stats->st_blksize = 0;*/
/*file_stats->st_blocks = 0; */
file_stats->st_mtime =
file_stats->st_atime = time(0);
/*file_stats->st_ctime = ce_gethostid();*/

/***************************************************************
*  
*  Build a fake device from the directory and a fake inode from 
*  the last part of the name.
*  
***************************************************************/

get_full_file_name(work, True); /* in normalize.c */
#ifdef WIN32
basename = strrchr(work, '\\'); /* always works, get_full_file_name guarentees this */
if (!basename) basename = strrchr(work, '/');
#else
basename = strrchr(work, '/'); /* always works, get_full_file_name guarentees this */
#endif

*basename++ = '\0';

for (i = 0; work[i]; i++)
   file_stats->st_dev ^= work[i] << (i % 24);

for (i = 0; basename[i]; i++)
   file_stats->st_ino ^= basename[i] << (i % 24);

/***************************************************************
*  
*  RES 1/20/1999
*  Use the directory inode as the create time.  This prevents
*  cc window detection when you do ce /tmp/z and ce $HOME/z
*  and neither file exists.
*  
***************************************************************/

memset((char *)&dir_stats, 0, sizeof(dir_stats));
stat(work, &dir_stats);
file_stats->st_ctime = dir_stats.st_dev;

} /* end of get_fake_file_stats */


/************************************************************************

NAME:      dm_ro  -  DM ro Read only toggle command

PURPOSE:    This routine will change the read only status of a main window
            if it is valid to do so.

PARAMETERS:

   1.  dspl_descr  - pointer to DISPLAY_DESCR (INPUT)
                     This is the display description for the main window.
                     It is used to get to the main window token and the
                     pad mode flag.

   2.  mode        - int (INPUT)
                     This is the mode from the ro command.
                     DM_TOGGLE, DM_ON, or DM_OFF

   3.  read_locked - int (INPUT)
                     This flag is true if the -readlock option was specified.

   4.  edit_file   - pointer to char (INPUT)
                     This is the file being edited.  It is
                     checked to see if it is writeable.


FUNCTIONS :

   1.   If we are not in an edit window, do nothing.

   2.   If this is a stdin file, issue an error and do nothing.

   3.   If we are changing to writable mode, verify that the file is
        writeable.

   4.   If we are changeing to read only mode, verify that the file is
        not "dirty".  That is, the file has not been changed since the
        last save.

NOTES:
   The NOT int the toggle switch call is used because ro -on means 
   read only (not writable).


*************************************************************************/

void  dm_ro(DISPLAY_DESCR     *dspl_descr,            /* input   */
            int                mode,                  /* input   */
            int                read_locked,           /* input   */
            char              *edit_file)             /* input   */
{

int                   temp_fd;
int                   changed;
int                   file_exists;
char                  msg[512];
DMC                   pn_dmc;

DEBUG4(fprintf(stderr, "(ro) checking file \"%s\"\n", edit_file);)

if (read_locked)
   {
      dm_error("(ro) Write mode locked out by -readlock option", DM_ERROR_BEEP);
      return;
   } 

if (strcmp(edit_file, STDIN_FILE_STRING) == 0)
   {
#ifdef WIN32
      if (do_save_file_dialog(msg, 256) == 0) /* in NT version of pad.c */
         {
            memset((char *)&pn_dmc, 0, sizeof(DMC));
            pn_dmc.pn.cmd = DM_pn;
            pn_dmc.pn.dash_c = 'c';
            pn_dmc.pn.path = msg;
            dm_pn(&pn_dmc, edit_file, dspl_descr);
         }
#else
      /***************************************************************
      *  RES 12/15/95, allow rw of stdin file, give the file a name.
      *  If this is stdin, make up a name and do a pn -c
      *  I don't know how mktmp can fail, but if it does, we are prepared.
      ***************************************************************/
      strcpy(msg, "/tmp/stdin.XXXXXX");
      temp_fd = mkstemp(msg); /* RES 9/9/2004 changed from mktemp */
      if (temp_fd >= 0)
         {
            close(temp_fd); /* RES 9/9/2004 added */
            memset((char *)&pn_dmc, 0, sizeof(DMC));
            pn_dmc.pn.cmd = DM_pn;
            pn_dmc.pn.dash_c = 'c';
            pn_dmc.pn.dash_r = 'r';     /* RES 9/9/2004 added */

            pn_dmc.pn.path = msg;
            dm_pn(&pn_dmc, edit_file, dspl_descr);
         }
#endif
   }

if ((strcmp(edit_file, STDIN_FILE_STRING) == 0) || (find_padmode_dspl(dspl_descr)))
   {
      dm_error("(ro) Not writable", DM_ERROR_BEEP);
      return;
   }

WRITABLE(dspl_descr->main_pad->token) = !toggle_switch(mode, 
                                                       !(WRITABLE(dspl_descr->main_pad->token)),
                                                       &changed);
if (changed)
   {
      if (WRITABLE(dspl_descr->main_pad->token))
         {
            /* the value was changed to writeable */
            if (!ok_for_output(edit_file, &file_exists, dspl_descr)) /* i not used */
               {
                  /* on error, errno is set */
                  snprintf(msg, sizeof(msg), "(ro) %s", strerror(errno));
                  dm_error(msg, DM_ERROR_BEEP);
                  WRITABLE(dspl_descr->main_pad->token) = False;
               }
            else  /* RES 9/5/95 adde file locking */
               if (file_exists && LOCKF && (!LSF || /* LOCKF and LSF are cmdline/resource parms */
                   !is_virtual(&(dspl_descr->ind_data), dspl_descr->hsearch_data, edit_file)))
                  if (lock_file(edit_file) == False) /* in lock.c */
                     WRITABLE(dspl_descr->main_pad->token) = False;
         }
      else
         if (dirty_bit(dspl_descr->main_pad->token))
            {
               dm_error("(RO) File already modified", DM_ERROR_BEEP);
               WRITABLE(dspl_descr->main_pad->token) = True;
            }
         else /* RES 9/5/95 adde file locking */
            unlock_file(); /* in lock.c */
  
      if (cc_ce)  /* if we are in cc mode, redraw the other window's title bars */
          cc_titlebar();

   }
} /* end of dm_ro */


/************************************************************************

NAME:      ok_for_output

PURPOSE:    This routine checks if the passed file can be opened for output.

PARAMETERS:

   1.  edit_file - pointer to char (INPUT)
                   This is the path name to be tested.  It may be
                   a relative or absolute path.

   2.  file_exists - pointer to int (OUTPUT)
                   This flag is set to true if the passed file exists.
                   It is possible for the file to be writeable and the file
                   not exist.


FUNCTIONS :

   1.   See if the file exists.

   2.   If the file exists, make sure it is writeable.

   3.   If the file does not exist, make sure the containing directory is writeable.

OUTPUTS:
   writeable - Returned value
               0  -  Not writeable
               1  -  Writeable

*************************************************************************/

int   ok_for_output(char          *edit_file,      /* input  */
                    int           *file_exists,    /* output */
                    DISPLAY_DESCR *dspl_descr)     /* input  */
{

char                  work[MAXPATHLEN+1];
char                 *p;
int                   i;

*file_exists = False;
errno = 0;

if (strcmp(edit_file, STDIN_FILE_STRING) == 0)
   return(False);

if (LSF)
   i = lsf_access(&(dspl_descr->ind_data), dspl_descr->hsearch_data, edit_file, 00);  /* is it there? */
else
   i = access(edit_file, 00);  /* is it there? */
if (i == 0)
   {
      i = access(edit_file, 02); /* is it writeable? */
      *file_exists = True;
   }
else
   {
      strlcpy(work, edit_file, sizeof(work));
      p = strrchr(work, '/');
      if (p == NULL)
         strcpy(work, ".");
      else
         *p = '\0';
      if (LSF)
         i = lsf_access(&(dspl_descr->ind_data), dspl_descr->hsearch_data, work, 02);  /* is it there? */
      else
         i = access(work, 02);  /* is the containing directory writable? */
   }
   
if (i == 0)
   return(True);
else
   return(False);

}  /* end of ok_for_output */


/************************************************************************

NAME:      ok_for_input

PURPOSE:    This routine checks if the passed file can be opened for input.

PARAMETERS:

   1.  edit_file - pointer to char (INPUT)
                   This is the path name to be tested.  It may be
                   a relative or absolute path.


FUNCTIONS :

   1.   See if the file exists.

   2.   If the file exists, make sure it is readable.

   3.   If the file does not exist, it cannot be read.

OUTPUTS:
   readeable - Returned value
               0  -  Not readable
               1  -  Readable

*************************************************************************/

int   ok_for_input(char          *edit_file,       /* input  */
                   DISPLAY_DESCR *dspl_descr)      /* input  */
{

int                   i;

if (LSF)
   i = lsf_access(&(dspl_descr->ind_data), dspl_descr->hsearch_data, edit_file, 04);  /* is it there? */
else
   i = access(edit_file, 04);  /* is it there and readable? */
   
if (i == 0)
   return(True);
else
   return(False);

}  /* end of ok_for_input */



/************************************************************************

NAME:      dm_pn

PURPOSE:    This routine will do a pad name or rename a file.

PARAMETERS:

   1.  dmc       - pointer to DMC (INPUT)
                   This is the pn command buffer.  It contains flags
                   and the target path name.

   2.  edit_file - pointer to char (OUTPUT)
                   This is the current path name being edited.  It can be
                   a relative or absolute path.

   3.  main_window_cur_descr - pointer to PAD_DESCR (OUTPUT)
                   This is the main window pad description,
                   the dirty bit is set to force a write on
                   exit if the -c option is used.


FUNCTIONS :

   1.   If the current file is stdin or the -c option was specified, 
        just change the name of the file in the title.

   2.   If the file exists, rename it.  If not, just change the
        name of the file in the title.

   3.   If there is a .bak file, rename it too.



*************************************************************************/

void dm_pn(DMC           *dmc,                    /* input  */
           char          *edit_file,              /* input / output */
           DISPLAY_DESCR *dspl_descr)             /* input / output */
{

int                   rc;
int                   file_exists;
struct stat           file_stats;
char                  msg[512];
char                  new_file[MAXPATHLEN+4];
char                  bak_file[MAXPATHLEN+4];
char                  bak_file_to[MAXPATHLEN+4];
FILE                 *stream;
char                 *p;

/***************************************************************
*  
*  Make sure there is something to rename to.
*  
***************************************************************/

if (dmc->pn.path == NULL || dmc->pn.path[0] == '\0')
   {
      dm_error(edit_file, DM_ERROR_MSG);
      return;
   }
else
   if (strcmp(dmc->pn.path, edit_file) == 0)
      return;


/***************************************************************
*  
*  Make the file name a real file name.
*  
***************************************************************/

strlcpy(new_file, dmc->pn.path, sizeof(new_file));
substitute_env_vars(new_file, MAXPATHLEN, dspl_descr->escape_char);
get_full_file_name(new_file, True); /* in normalize.c */


if (strcmp(new_file, edit_file) == 0)
   return;

/***************************************************************
*  
*  Make sure the new file does not exist and is writeable.
*  
***************************************************************/

if ((!dmc->pn.dash_r) && (access(new_file, 00) == 0))
   {
      dm_error("(pn) File already exists", DM_ERROR_BEEP);
      return;
   }

if (!ok_for_output(new_file, &file_exists, dspl_descr))
   {
      if (file_exists)
         {
           rc = unlink(new_file);
           if (rc == -1)
              {
                 snprintf(msg, sizeof(msg), "(pn) Could not delete or write file (%s)", strerror(errno));
                 dm_error(msg, DM_ERROR_BEEP);
                 return;
              }
         }
      else
         snprintf(msg, sizeof(msg), "(pn) Directory not writable (%s)", strerror(errno));
      dm_error(msg, DM_ERROR_BEEP);
      return;
   }


/***************************************************************
*  
*  Easy cases, the file was piped in from stdin or -c (copy)
*  was specified.  All we need do is set the file name in the 
*  titlebar.
*  
***************************************************************/

if (strcmp(edit_file, STDIN_FILE_STRING) == 0 || dmc->pn.dash_c)
   {
      stream = fopen(new_file, "w");
      if (stream != NULL)
         fclose(stream);
      else
         {
            snprintf(msg, sizeof(msg), "(pn) Cannot open %s for output (%s)", new_file, strerror(errno));
            dm_error(msg, DM_ERROR_BEEP);
            return;
         }

      strlcpy(edit_file, new_file, MAXPATHLEN+1);
      dirty_bit(dspl_descr->main_pad->token) = True;
      /***************************************************************
      *  RES 4/4/2003
      *  A pad name occured, flag this.  This causes wc
      *  to make sure the file is saved.
      ***************************************************************/
      set_edit_file_changed(dspl_descr, True);

      cc_padname(dspl_descr, edit_file);
      if (LOCKF && (!LSF || /* LOCKF and LSF are cmdline/resource parms */
          !is_virtual(&(dspl_descr->ind_data), dspl_descr->hsearch_data, edit_file)))
         {
            lock_rename(edit_file);
            lock_file(edit_file);
         }
      return;
   }

/***************************************************************
*  
*  You cannot rename a virtual file.
*  
***************************************************************/
if (LSF && is_virtual(&(dspl_descr->ind_data), dspl_descr->hsearch_data, edit_file))
   {
      snprintf(msg, sizeof(msg), "(pn) Cannot rename a virtual file (use 'pn -c')");
      dm_error(msg, DM_ERROR_BEEP);
      return;
   }


/***************************************************************
*  
*  Try to do a stat on the current edit file.  If it does not exist.
*  Just do the rename in the titlebar.  Otherwise rename the
*  file.  If the rename fails, we fail.
*  
***************************************************************/

rc = stat(edit_file, &file_stats);
if (rc == 0)
   {
      if (!ok_for_output(edit_file, &file_exists, dspl_descr))
         {
            snprintf(msg, sizeof(msg), "(pn) Cannot rename (%s)", strerror(errno));
            dm_error(msg, DM_ERROR_BEEP);
            return;
         }
#ifdef WIN32
      /* WIN32 rename does not replace existing file */
      unlink(new_file);
#endif
      if (rename(edit_file, new_file) != 0)
         {
            snprintf(msg, sizeof(msg), "(pn) Can't rename, (%s)", strerror(errno));
            dm_error(msg, DM_ERROR_BEEP);
         }
      else
         {
            /***************************************************************
            *  RES 4/4/2003
            *  A pad name occured, flag this.  This causes wc
            *  to make sure the file is saved.
            ***************************************************************/
            set_edit_file_changed(dspl_descr, True);

            /***************************************************************
            *  Try to do a stat on the.bak file.  If it exists, try to rename
            *  it, if the rename fails, flag it, but we are still ok.
            ***************************************************************/
            strlcpy(bak_file, edit_file, sizeof(bak_file));
            strlcat(bak_file, ".bak",    sizeof(bak_file));
            strlcpy(bak_file_to, new_file, sizeof(bak_file_to));
            strlcat(bak_file_to, ".bak",    sizeof(bak_file_to));
            strlcpy(edit_file, new_file, MAXPATHLEN+1);
            cc_padname(dspl_descr, edit_file);

            rc = stat(bak_file, &file_stats);
            if (rc == 0)
              {
#ifdef WIN32
                 /* WIN32 rename does not replace existing file */
                unlink(bak_file_to);
#endif
                 if (rename(bak_file, bak_file_to) != 0)
                    {
                       snprintf(msg, sizeof(msg), "(pn) Rename of .bak file failed, (%s)", strerror(errno));
                       dm_error(msg, DM_ERROR_BEEP);
                    }
              }
            p = strrchr(new_file, '/');
            if (p) /* should always be true */
               *p = '\0';
            if (new_file[0] == '\0') /* was root directory */
               strcpy(new_file, "/");
            save_edit_dir(new_file);

            if (LOCKF) /* RES 2/10/97 added to this location */
               {
                  lock_rename(edit_file);
                  lock_file(edit_file);
               }
         }
   }
else
   {
      stream = fopen(new_file, "w");
      if (stream != NULL)
          fclose(stream);
      else
         {
            snprintf(msg, sizeof(msg), "(pn) Cannot create %s (%s)", new_file, strerror(errno));
            dm_error(msg, DM_ERROR_BEEP);
            return;
         }

      strcpy(edit_file, new_file);
      dirty_bit(dspl_descr->main_pad->token) = True;
      /***************************************************************
      *  RES 4/4/2003
      *  A pad name occured, flag this.  This causes wc
      *  to make sure the file is saved.
       ***************************************************************/
            set_edit_file_changed(dspl_descr, True);

      cc_padname(dspl_descr, edit_file);
      if (LOCKF) /* RES 9/5/95 added file locking */
         {
            lock_rename(edit_file);
            lock_file(edit_file);
         }

      p = strrchr(new_file, '/');
      if (p) /* should always be true */
         *p = '\0';
      if (new_file[0] == '\0') /* was root directory */
         strcpy(new_file, "/");
      save_edit_dir(new_file);

   }

}  /* end of dm_pn */


/************************************************************************

NAME:      bump_backup_files - Rename .bak files to make room for a new .bak.

PURPOSE:    This routine supports keeping multiple .bak files named
            .bak, .bak1, .bak2, and so forth.

PARAMETERS:

   1.  edit_file       - pointer to char (INPUT)
                         This is the path name being edited.  It can be
                         a relative or absolute path.


   2.  max_keep_number - int (INPUT)
                         This is the number of the maximum backup to keep.
                         Thus if we are keeping .bak, .bak1, and .bak2,
                         the number value would be 2.

FUNCTIONS :
   1.   Delete the last backup file off the list

   2.   Loop backwards through the backup numbers renaming each file
        to the next higher number.


*************************************************************************/

static void bump_backup_files(char *edit_file, unsigned int max_keep_number)
{
char           work_backup_file[MAXPATHLEN+1];
char           work_backup_file2[MAXPATHLEN+1];
unsigned int   backup_num;
char            lcl_msg[512];

for (backup_num = max_keep_number; backup_num > 0; backup_num--)
{
   snprintf(work_backup_file2, sizeof(work_backup_file2), "%s.bak%d", edit_file, backup_num);

   if (backup_num == max_keep_number) /* first time through */
      { /* delete the last backup file off the end */
         if (access(work_backup_file2, 0) == 0)
            if (unlink(work_backup_file2) != 0)
               {
                  snprintf(lcl_msg, sizeof(lcl_msg), "?   Unable to unlink %s (%s)\n", work_backup_file2, strerror(errno));
                  dm_error(lcl_msg, DM_ERROR_BEEP);
               }
      }

   if (backup_num == 1) /* last time through */
      snprintf(work_backup_file, sizeof(work_backup_file), "%s.bak", edit_file, backup_num-1);
   else
      snprintf(work_backup_file, sizeof(work_backup_file), "%s.bak%d", edit_file, backup_num-1);
   /* rename file.bak -> file.bak1, file.bak1 -> file.bak2 */
   if (access(work_backup_file, 0) == 0)
      if (rename(work_backup_file, work_backup_file2) != 0)
         {
            snprintf(lcl_msg, sizeof(lcl_msg), "?   Unable to rename %s to %s (%s)\n", work_backup_file, work_backup_file2, strerror(errno));
            dm_error(lcl_msg, DM_ERROR_BEEP);
         }
} /* end of for looping through backup names */

} /* end of bump_backup_files */


/************************************************************************

NAME:      create_backup_dm

PURPOSE:    This routine will create or replace a backup file according to dm rules.

PARAMETERS:

   1.  edit_file - pointer to char (INPUT)
                   This is the path name being edited.  It can be
                   a relative or absolute path.


FUNCTIONS :

   1.   Rename the file being edited to the backup file.


OUTPUTS:
   rc - Returned value
        0  -  Backup created
       -1  -  Backup failed, dm_error call already issued.

*************************************************************************/

static int   create_backup_dm(char       *edit_file)
{

char                  msg[MAXPATHLEN];
char                  bak_file[MAXPATHLEN+4];

if (strcmp(edit_file, STDIN_FILE_STRING) == 0)
   return(-1);

strlcpy(bak_file, edit_file, sizeof(bak_file));
strlcat(bak_file, ".bak",    sizeof(bak_file));


/***************************************************************
*  
*  Rename the original file to the .bak file.
*  
***************************************************************/

#ifdef WIN32
/* WIN32 rename does not replace existing file */
unlink(bak_file);
#endif
if (rename(edit_file, bak_file) != 0)
   {
      DEBUG(perror(bak_file);)
      snprintf(msg, sizeof(msg), "Can't rename to create .bak file, (%s)", strerror(errno));
      dm_error(msg, DM_ERROR_BEEP);
      return(-1);
   }
else
   return(0);


}  /* end of create_backup_dm */


/************************************************************************

NAME:      create_backup_vi

PURPOSE:    This routine will create or replace a backup file according to vi rules.

PARAMETERS:

   1.  edit_file - pointer to char (INPUT)
                   This is the path name being edited.  It can be
                   a relative or absolute path.


FUNCTIONS :

   1.   If the .bak file exists and is writeable. Open it for write and
        copy the file into it.

   2.   If the .bak file is not writeable, try to delete it.  If not fail.

   3.   Open the edit file for input.

   4.   Copy the data from the input file to the edit file.


OUTPUTS:
   rc - Returned value
        0  -  Backup created
       -1  -  Backup failed, dm_error call already issued.

*************************************************************************/

static int   create_backup_vi(char         *edit_file,
                              struct stat  *file_stats)
{

int                   out_fd;
int                   in_fd;
char                  buff[8192];
char                  bak_file[MAXPATHLEN+4];
int                   count;
struct utimbuf        times;


strlcpy(bak_file, edit_file, sizeof(bak_file));
strlcat(bak_file, ".bak",    sizeof(bak_file));


/***************************************************************
*  
*  Open the backup file for output.
*  If we cannot flat out open it, try deleting and recreating.
*  
***************************************************************/

if ((out_fd = open(bak_file, (O_WRONLY | O_CREAT), 0666)) < 0)
   {
      snprintf(buff, sizeof(buff), "Unable to create .bak file, (%s)", strerror(errno));
      if (unlink(bak_file) != 0)
         {
            DEBUG(perror("unlink .bak file");)
            dm_error(buff, DM_ERROR_BEEP);
            return(-1);
         }
      else
         if ((out_fd = open(bak_file, O_WRONLY | O_CREAT), 0666) < 0)
            {
               DEBUG(perror("Second open of .bak file");)
               dm_error(buff, DM_ERROR_BEEP);
               return(-1);
            }
   }


/***************************************************************
*  
*  Open the original file for input
*  
***************************************************************/

if ((in_fd = open(edit_file, O_RDONLY)) < 0)
   {
      snprintf(buff, sizeof(buff), "Unable to read file to create .bak file, (%s)", strerror(errno));
      dm_error(buff, DM_ERROR_BEEP);
      return(-1);
   }



/***************************************************************
*  
*  copy the file
*  
***************************************************************/

while ((count = read(in_fd, buff, sizeof(buff))) > 0)
   write(out_fd, buff, count);


/***************************************************************
*  
*  close the file.
*  
***************************************************************/

close(out_fd);
close(in_fd);

/***************************************************************
*  
*  Copy the date/time stamps from the original file to the
*  .bak file.
*  
***************************************************************/

times.actime  =  file_stats->st_atime;
times.modtime =  file_stats->st_mtime;
utime(bak_file, &times);

return(0);

}  /* end of create_backup_vi */


/************************************************************************

NAME:      dm_pw

PURPOSE:    This routine will do a pad write of an editable file.

PARAMETERS:

   1.  dspl_descr      - pointer to DISPLAY_DESCR (INPUT)
                         This is the display description for the current display.
                         It is needed for the memdata token for the file being
                         edited and also is passed on to cc_padname.  cc_padname
                         is needed because a dm type backup renames the device
                         and inode of the file and we need to change the cc data.

   2.  edit_file       - pointer to char (INPUT)
                         This is the path name being edited.  It can be
                         a relative or absolute path.

   3.  from_crash_file - int (INPUT)
                         This flag is set to True in the call from create_crash_file.
                         Other callers have this flat set to False.  It prevents
                         this routine from calling create_crash_file and recursing.

FUNCTIONS :

   1.   If the file already exists and a backup has not been created,
        create a backup.

   2.   Open the file for write and copy it out from the memory copy.

   3.   Close the file and adjust the stats and ownership.



OUTPUTS:
   rc - Returned value
        0  -  Save succeeded
       -1  -  Save failed, dm_error call already issued.

*************************************************************************/

int   dm_pw(DISPLAY_DESCR *dspl_descr,         /* input  */
            char          *edit_file,          /* input  */
            int            from_crash_file)    /* input  */
{

int                   rc;
FILE                 *stream;
struct stat           file_stats;
struct stat           new_file_stats;
int                   file_recreated = False;
char                  msg[512];
int                   new_file = False;
int                   backup_count = 0;

static int            backup_created = False;


if (strcmp(edit_file, STDIN_FILE_STRING) == 0)
   return(-1);

load_enough_data(INT_MAX);

/***************************************************************
*  
*  Try to do a stat on the edit file.  If it does not exist.
*  Forget about unlinking the backup.
*
*  The backup_created flag is always set if a backup is attempted.
*  This way, if the user does a pw, and the backup fails, he gets
*  a message.  If he tries it again, it tries the save without the
*  backup.
*  
***************************************************************/



rc = stat(edit_file, &file_stats);
if ((rc == 0) && (file_stats.st_size != 0))
   {
      if (!backup_created && ((*(BACKUP_TYPE) | 0x20) != 'n') &&
          (!LSF || !is_virtual(&(dspl_descr->ind_data), dspl_descr->hsearch_data, edit_file)))
         {
            /* if the backup type dm or vi followed by a number, we do keep that many extra .bak files */
            if (sscanf(BACKUP_TYPE+2, "%d%9s", &backup_count, msg /* msg is junk here */) == 1)
               bump_backup_files(edit_file, backup_count);

            DEBUG1(fprintf(stderr, "dm_pw: backup_count = %d\n", backup_count);)


            backup_created = True;
            if ((*(BACKUP_TYPE) | 0x20) != 'v')
               if (create_backup_dm(edit_file) == 0)
                  file_recreated = True;
               else
                  return(-1);
            else
              if (create_backup_vi(edit_file, &file_stats) != 0)
                 return(-1);
         }
      
   }
else
   {
      backup_created = False;  /* If file does not exist, this may be the result of a pn, or edit of new file */
      new_file = True; /* RES 9/27/95 */
   }


if (LSF)
   stream = filter_out(&(dspl_descr->ind_data), dspl_descr->hsearch_data, edit_file, "w");
else
   stream = fopen(edit_file, "w");

if (stream == NULL)
   {
      snprintf(msg, sizeof(msg), "Can't save file, (%s) -> Try 1,$xc -f <some_file> to save elsewhere.", strerror(errno));
      dm_error(msg, DM_ERROR_BEEP);
      return(-1);
   }


save_file(dspl_descr->main_pad->token, stream); /* in memdata.c */

#ifdef MVS_SERVICES
         mvs_file(edit_file) ? mvs_close((MVS_FILE *)stream, 0) : 
#endif
if (fclose(stream) != 0) /* RES 2/9/1999 added test for failure */
   {
      snprintf(msg, sizeof(msg), "fclose of %s failed, (%s) file may be corrupted!, CREATING CRASH FILE", edit_file, strerror(errno));
      dm_error(msg, DM_ERROR_LOG);
      if (!from_crash_file)
         create_crash_file();
      return(-1);
   }
file_has_changed(edit_file);

/***************************************************************
*  
*  NFS and AFS caching can sometimes mess up the stat information.
*  We have created a new file, due to an pad name rename.  The
*  inode and create time had better have changed.
*
***************************************************************/

if (file_recreated)
   {
      rc = stat(edit_file, &new_file_stats);
      if ((file_stats.st_ino   == new_file_stats.st_ino) ||
          (file_stats.st_ctime == new_file_stats.st_ctime))
         {
            DEBUG(fprintf(stderr, "pw:  File inode and create time stayed the same in %s after DM type backup, trying tickle\n", edit_file);) 
            stream = fopen(edit_file, "r");  /* access the file again */
            if (stream)
               fclose(stream);
            else
               DEBUG(fprintf(stderr, "pw: tickle: Could not open %s for input (%s)\n", edit_file, strerror(errno));)
#ifdef WIN32
            usleep(2000000);
#else
            sleep(2);
#endif
            rc = stat(edit_file, &new_file_stats);
            if ((file_stats.st_ino   == new_file_stats.st_ino) ||
                (file_stats.st_ctime == new_file_stats.st_ctime))
               {
                  snprintf(msg, sizeof(msg), "pw:  File inode and create time stayed the same in %s after rename to %s.bak and recreate of file, automatic CC detection frustrated", edit_file, edit_file);
                  dm_error(msg, DM_ERROR_LOG);
               }
         }
   }

/***************************************************************
*  
*  Get the permissions straightened out if we recreated the file.
*  Also let the CC list know the new device, inode, and size.
*  
***************************************************************/

if (file_recreated || new_file)
   {
      if (!new_file) /* RES 9/27/95 new_file processing added */
         {
            (void) chmod(edit_file, file_stats.st_mode);
#ifndef WIN32
            (void) chown(edit_file, file_stats.st_uid, file_stats.st_gid);
#endif
         }
      if (LOCKF && /* RES 9/5/95 added file locking */
          (!LSF || !is_virtual(&(dspl_descr->ind_data), dspl_descr->hsearch_data, edit_file)))
         {
            lock_rename(edit_file);
            lock_file(edit_file);
         }
      /* Bad idea, 9/23/97 RES -  need cc_name in all cases in clase file changes size.  cc_padname(dspl_descr, edit_file); * moved inside if group from just after it 9/5/95 RES */
   }
else
   {
      if (LOCKF &&
          (!LSF || !is_virtual(&(dspl_descr->ind_data), dspl_descr->hsearch_data, edit_file)))
         update_lock_data(edit_file); /* in lock.c */
      /*cc_titlebar(); Removed, called from cc_padname, */
   }

cc_padname(dspl_descr, edit_file); /* cc_padname is needed to update file size in cc data  9/23/97 RES */


return(0);

}  /* end of dm_pw */



/************************************************************************

NAME:      dm_cd

PURPOSE:    This routine will change the working directory of the 
            editor process.

PARAMETERS:

   1.  dmc       - pointer to DMC (INPUT)
                   This is the cd command buffer.
                   It contains target path name.


FUNCTIONS :

   1.   Call the system chdir to change the directory.  On error
        produced a message.


*************************************************************************/

void dm_cd(DMC        *dmc,                    /* input  */
           char       *edit_file)              /* input  */
{
int                   rc;
char                  msg[512];
char                  work_dir[MAXPATHLEN+1];
#ifndef WIN32
char                  final_dir[MAXPATHLEN+1];
char                  the_dir[MAXPATHLEN+1];
char                 *p;
#endif

/***************************************************************
*  
*  Figure out what we are going to cd to.
*  A null parm means cd to $HOME, the normal thing.
*  cd to ~. is special, it means cd to the dir where the current
*  edit file is. translate_tilde_dot does this if needed.
*  
***************************************************************/

if (dmc->cd.parm == NULL || dmc->cd.parm[0] == '\0')
   get_home_dir(work_dir, NULL);
else
   strlcpy(work_dir, dmc->cd.parm, sizeof(work_dir));

#ifdef WIN32
get_full_file_name(work_dir, False);

/***************************************************************
*  
*  Try the chdir
*  
***************************************************************/

rc = chdir(work_dir);
if (rc != 0)
   {

      snprintf(msg, sizeof(msg), "(cd) Can't change directory to %s (%s)", work_dir, strerror(errno));
      dm_error(msg, DM_ERROR_BEEP);
   }

#ifdef PAD
ce_setcwd(work_dir);
#endif



#else
translate_tilde_dot(work_dir);
strlcpy(the_dir, work_dir, sizeof(the_dir));


/***************************************************************
*  
*  If the file does not begin with a /, get the current working
*  directory and put that on the front.
*  
***************************************************************/

if (the_dir[0] != '/')
   {
      if (the_dir[0] == '~')
         {
            strlcpy(work_dir, the_dir, sizeof(work_dir));
            p = translate_tilde(work_dir);
            if (p == NULL)
               strcpy(work_dir, "/");
         }
      else
#ifdef PAD
         if (ce_getcwd(work_dir, sizeof(work_dir)) == NULL)
#else
         if (getcwd(work_dir, sizeof(work_dir)) == NULL)
#endif
            {
               snprintf(work_dir, sizeof(work_dir), "Can't get current working dir (%s)", strerror(errno));
               dm_error(work_dir, DM_ERROR_BEEP);
               strcpy(work_dir, "/");
            }
         else
            {
               if (work_dir[strlen(work_dir)-1] != '/')  /* make sure we are not in the root directory */
                  strcat(work_dir, "/");
               strcat(work_dir, the_dir);
            }
   }
else
   strlcpy(work_dir, the_dir, sizeof(work_dir));

/***************************************************************
*  
*  Get rid of any . or .. constructs in the file name.
*  
***************************************************************/

normalize_file_name(work_dir, (short)strlen(work_dir), final_dir);


/***************************************************************
*  
*  Try the chdir
*  
***************************************************************/

rc = chdir(final_dir);
if (rc != 0)
   {
      snprintf(msg, sizeof(msg), "(cd) Can't change directory to %s (%s)", final_dir, strerror(errno));
      dm_error(msg, DM_ERROR_BEEP);
   }

#ifdef PAD
ce_setcwd(final_dir);
#endif

#endif /* not WIN32 */

}  /* end of dm_cd */


/************************************************************************

NAME:     dm_pwd   - print current DM working directory


PURPOSE:    This routine will change the working directory of the 
            editor process.

FUNCTIONS :

   1.  Get the current working directory and put in in the DM output window.


*************************************************************************/

void dm_pwd(void)
{
char                  work_dir[MAXPATHLEN];


/***************************************************************
*  
*  If the file does not begin with a /, get the current working
*  directory and put that on the front.
*  
***************************************************************/

#ifdef PAD
if (ce_getcwd(work_dir, sizeof(work_dir)) == NULL)
#else
if (getcwd(work_dir, sizeof(work_dir)) == NULL)
#endif
   {
      snprintf(work_dir, sizeof(work_dir), "Can't get current working dir (%s)", strerror(errno));
      dm_error(work_dir, DM_ERROR_BEEP);
      strcpy(work_dir, "/");
   }
else
   dm_error(work_dir, DM_ERROR_MSG);

}  /* end of dm_pwd */



/************************************************************************

NAME:      save_edit_dir       - Save the directory of the current file being edited

PURPOSE:    This routine saves the name of the directory containing
            the file being edited in a environment variable.  This is
            later used, possibly by invoked copies of ce, to resolve
            file names beginning with ~./

PARAMETERS:

   1.  dir       - pointer to char (INPUT)
                   This is the name of the directory to be saved.

FUNCTIONS :
   1.   Create an environment variable to save the passed directory
        name in.


*************************************************************************/

static void save_edit_dir(char  *dir)
{
char                  edit_file_dir[MAXPATHLEN+12];
char                 *p;

snprintf(edit_file_dir, sizeof(edit_file_dir), "%s=%s", CE_EDIT_DIR, dir);
p = strdup(edit_file_dir);
if (p)
   putenv(p);

} /* end of save_edit_dir */

/************************************************************************

NAME:      translate_tilde_dot - Process file names starting with ~./

PURPOSE:    This routine will translate file names beginning with
            ~./ to the correct name.  This is an extension of normal
            Unix naming conventions.

PARAMETERS:
   1.  file      - pointer to char (INPUT/OUTPUT)
                   This is the file name to be translated.  

FUNCTIONS :
   1.   Get the environment variable used to translate '~./'.  This
        is the directory of the file which is either currently being
        editited, or the directory of the file being edited when this
        ce session was initiated via the dmc command ce or cv.

   2.   If the passed file name starts with '~./', replace the ~. with
        the correct directory name.

NOTE:

   The environment variable CE_EDIT_DIR is effectively used as a global
   variable between ce sessions.  It is normally not seen in ceterm
   environments except if the ceterm was started with a cp or ceterm
   dmc command.


*************************************************************************/

static void translate_tilde_dot(char  *file)
{
char                  workfile[MAXPATHLEN];
char                 *edit_file_dir;

edit_file_dir = getenv(CE_EDIT_DIR);
if (!edit_file_dir)
   {
      DEBUG(fprintf(stderr, "translate_tilde_dot : setting edit file dir from current working directory!\n");)
#ifdef PAD
      edit_file_dir = ce_getcwd(NULL, 0);
#else
      edit_file_dir = getcwd(NULL, 0);
#endif
   }

if ((file[0] == '~') && (file[1] == '.') && ((file[2] == '/') || (file[2] == '\0')))
   {
      /***************************************************************
      *  Special extension.  ~. is the directory of the current ce
      *  file.
      *  RES 2/10/97
      *  Add code so that in ce/cv the initial current working 
      *  directory is the directory containing the file being edited.
      *  First we figure out what the new cwd will be.  We wait till
      *  the open is done to do the actual cd.
      ***************************************************************/
      snprintf(workfile, sizeof(workfile), "%s%s", edit_file_dir, &file[2]);
      strcpy(file, workfile);
   }

} /* end of translate_tilde_dot */

