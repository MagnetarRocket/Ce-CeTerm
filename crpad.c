/*static char *sccsid = "%Z% %M% %I% - %G% %U%";*/
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

/**************************************************************
*
*  MODULE crpad.c  main program for crpad
*
*  ARGUMENTS:
*      See summary in pw.c.  This is where the usage prompt is.
*
*
*  OVERVIEW
*      The ce program is a programmers editor modeled after
*      the Apollo DM editor.  The program executable is named ce
*      and is aliased to names cv and crpad.  argv[0] is looked at
*      to determine which name was used.  cv and crpad bring the
*      editor up in read only mode.  The file, if specified, must
*      exist and be readable.  ce brings the editor up in read/write
*      mode.  The file either must exist and be writeable or not
*      exist and the directory is writable.
*
*      The windows get set up, some data read in, and the
*      window is put up.  When there is nothing to do, the
*      rest of the data is read in.  This allows the user to
*      interact with the window sooner.
*
*      Actual text drawing is done into a pixmap.  It is then
*      copied to the displayable window via XCopyArea.  This
*      makes expose events easy to process.  Since a corner
*      of the window may need to be redrawn.  There is no
*      pixmap backing for the DM windows.  They are completely
*      redrawn when the screen is exposed.
*
*      There is a structure called DRAWABLE_DESCR.  It has all the
*      interesting information about a window.  There are actually
*      two for each window.  One describes the window.  The other
*      describes the usable portion of the window.  I leave a margin
*      around the window.  There is a pair for the main window, a
*      pair for the dm input window, and a pair for the dm_output
*      window.
*
*      Configure_Notify is for when the screen changes size and shape.
*      The pixmap must be rebuilt, the screen redrawn, and the DM
*      windows remap.  The DM subwindows, are unmapped on resize and
*      therefore need to be sized and then mapped.  A window only
*      appears on the display when it and all its ancestors (parent
*      windows) are mapped.
*
*      Keypress and mouse events are translated into DM commands and the
*      commands executed. Note that there is some code left over from
*      before I had the DM stuff started.
*
*      BUFF_DESCR stuctures describe buffer information in a window.
*      the newbuffer is set up from cursor motion.  There is a current
*      buffer structure for each window which is referenced from the
*      newbuffer structure.  I am not yet sure I have all the correct
*      information in this structure.  The idea is that if you type a bit
*      and then do a tdm command, some data about the target buffer needs
*      to be kept around.  This stuff is in preparation for the editor.
*
*  NAMING
*      There are a number of similar names in use.  The convention they
*      follow is demonstrated by the following example for the DM kd command.
*
*      DM_kd          -   Defined constant in dmsyms.h.  Numerical id number for this
*                         command.
*      DMC_kd         -   Typedef of the command structure for the internal parsed form
*                         of the command.  
*      dmc->kd.cmdid  -   DMC is a union of all the DMC command structures.  Done just
*                         like the XEvent structure.  kd is the element in the structure.
*      dm_kd          -   The routine to execute a kd command if it is not done in line.
*
*  TRANSIENT CODE INFO:
*      When code is going to be gotten rid of, it is often bracketed with conditional
*      compilatin code until we are sure we really want to get rid of this code and
*      not see it again.  The bracketing code is:
*      #ifdef blah
*      #endif
*
*  DEBUGGING INFO:
*
*      The following is a list of the debugging bits which affect this program.
*      To activate the various types of debugging info, use the environment variable
*      DEBUG.
*      For example:
*
*      DEBUG=@1-3-4-5
*      export DEBUG
*
*      This will turn on DEBUG, DEBUG1, DEBUG3, DEBUG4, and DEBUG5.
*      The program has to be compiled with -DDebuG  for the debug to have
*      any effect.
*
*      DEBUG (any)  -  Basic setup info, font names, gc contents, event mask, error messages
*
*      DEBUG0       -  Memdata test for valid token
*      DEBUG1       -  Window setup, parms, intialization data,  crpad.c dmwin.c
*      DEBUG2       -  mark/copy/paste/find  mark.c dmfind.c
*      DEBUG3       -  Memdata debugging and Line / text insertion /replace
*      DEBUG4       -  DM cmd parsing, assignment, and execution  crpad.c kd.c parsedm.c
*      DEBUG5       -  cursor movement, window repositioning mvcursor.c
*      DEBUG6       -  undo/redo 
*      DEBUG7       -  typing es, ed, ee, en typing.c
*      DEBUG8       -  Text Cursor Motion (txcursor and highlight cursor motion)
*      DEBUG9       -  Set _Xdebug on (sync events in X)
*      DEBUG10      -  Warping cursor data,  as opposed to incoming handled with DEBUG8
*      DEBUG11      -  Keysym translation kd.c, initial keydef loading in serverdef.c
*      DEBUG12      -  Screen drawing and copying from pixmap (split out from DEBUG10)
*      DEBUG13      -  Search
*      DEBUG14      -  event dump for cursor motion events plus cursor pixel dumping
*      DEBUG15      -  Event reception for non-motion events
*      DEBUG16      -  Pad support (shell window) pad.c
*      DEBUG17      -  Set copy / paste incr size to 128, forces X INCR processing also dm_rec tracing
*      DEBUG18      -  cc (carbon copy and multi window edit)
*      DEBUG19      -  unix pad routines in unixpad.c
*      DEBUG20      -  wdf  and wdc and color(ca) processing
*      DEBUG21      -  Local Malloc routine debugging (macro CE_MALLOC)
*      DEBUG22      -  I/O signals coming from Shell socket and X socket
*      DEBUG23      -  vt100 mode emulation in vt100.c and scroll bar in sbwin.c
*      DEBUG24      -  dump atoms on property notify events, in crpad.c
*      DEBUG25      -  Keep track of signals in pad.c also low level text highlighting
*      DEBUG26      -  Hash table lookup stats in hsearch.c
*      DEBUG27      -  File locking in lock.c
*      DEBUG28      -  Indent processing ind.c
*      DEBUG29      -  Pulldown creation and processing in pd.c
*      DEBUG30      -  dde testing bit for ceterm, turns off the usleeps to avoid SIGALARM signals
*      DEBUG31   
*      DEBUG32      -  dynamic debugging (for debugging on the fly)
*
* MISC NOTES:
*
*   1.  In this application the term keyboard events is used to
*       refer to Key press and release events and Mouse button
*       press and release events.
*
* CONDITION CODE:
*
*   The following -D flags are also tested for withing Ce:
*
*   -DDebuG -  The DEBUG macro uses -DDebuG to include the code necessary
*              for debugging.
*
*   -DPAD   -  Used to conditionally compile in terminal emulation
*              support.  This is all of pad.c, unixpad.c, unixwin.c,
*              and pieces of many other modules.
*
*   -DARPUS_CE  - Used in modules such as normalize.c which are
*                 part of other programs.  This flag allows
*                 calls to dm_error and CE_MALLOC to be 
*                 conditionally compiled in.
*
*   -DNeedFunctionPrototypes -  Tested for in the system X11 includes
*
*   -DCE_LITTLE_ENDIAN - Used on Little Endian (reverse byte order)
*                        machines such as the dec, and intel machines.
*
*   -DUSE_BSD_SIGNALS - Used in module usleep.c which is 
*                       used on platforms which do not support
*                       the usleep function.
*
*   -DSYS5  -          Used to specify SYS5 as opposed to BSD
*                      code in pad.c.  Set automatically for
*                      _INCLUDE_HPUX_SOURCE and solaris and Linux.
*
*   -D_INCLUDE_HPUX_SOURCE  -  Used on HP/UX boxes for system calls which are different on
*                              HP/UX from other boxes.  HP/UX requires a bunch of other includes
*                              to compile correctly, but we do not test them explicetly.
*
*
*
*   -DTRACER -  In pad.c this includes code involed in calls to
*               ptrace, an experiment to access the working 
*               directory of the invoked shell.  Not currently used.
*
*   -DIBMRS6000  -  Set on the IBM RS6000, used in pad.c
*
*   -Dsolaris - Set on the Sun Solaris machine.
*
*
*  The following are major ifdefs which are not set via -D
*  These are all used only in pad.c
*
*    ultrix - Defined by the system on any Dec Ultrix machine
*
*    __alpha - Defined by the system on any Dec Alpha machine
*
*   DECBOX - defined in pad.c if either ultrix or __alpha are defined.
*
*   sun - Defined by the system on any Sun machine (either solaris or sunos).
*
*   _BSD  -  turned on when neither _INCLUDE_HPUX_SOURCE nor solaris
*            are specified.  Gets BSD stuff included on Apollo
*
*   blah - Used to get rid of code prior to actually deleting it.
*
*   There are some other tests for constants in include files
*   which are platform dependent but not under our control.
*
***************************************************************/



#include <stdio.h>          /* /usr/include/stdio.h      */
#include <errno.h>          /* /usr/include/errno.h      */
#include <string.h>         /* /usr/include/string.h   */
#include <limits.h>         /* /usr/include/limits.h   */
#ifndef WIN32
#include <signal.h>         /* "/usr/include/signal.h"    /usr/include/sys/signal.h    */
#endif
#include <sys/types.h>      /* "/usr/include/sys/types.h"     */
#include <time.h>           /* "/usr/include/time.h"   */
#include <stdlib.h>         /* needed on linux for exit */

#ifdef WIN32
#include <windows.h>
#ifdef WIN32_XNT
#include "xnt.h"
#else
#include "X11/Xlib.h"       /* /usr/include/X11/Xlib.h   */
#include "X11/keysym.h"     /* /usr/include/X11/keysym.h  /usr/include/X11/keysymdef.h */
#include "X11/cursorfont.h" /* /usr/include/X11/cursorfont.h */
#include "X11/Xatom.h"      /* /usr/include/X11/Xatom.h  */
#endif
#else
#include <X11/Xlib.h>       /* /usr/include/X11/Xlib.h   */
#include <X11/keysym.h>     /* /usr/include/X11/keysym.h  /usr/include/X11/keysymdef.h */
#include <X11/cursorfont.h> /* /usr/include/X11/cursorfont.h */
#include <X11/Xatom.h>      /* /usr/include/X11/Xatom.h  */
#endif

#ifdef WIN32
#include <io.h>
#endif

#define MAX_KEY_NAME_LEN    16
#define MAX_WINDOW_NAME_LEN 80

#define _MAIN_ 1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "cc.h"
#include "cswitch.h"
#include "debug.h"
#include "display.h"
#include "dmc.h"
#include "dmwin.h"
#include "dumpxevent.h"
#include "emalloc.h"
#include "execute.h"
#include "expose.h"
#include "pw.h"
#include "getevent.h"
#include "getxopts.h"
#ifndef HAVE_CONFIG_H
#include "help.h"
#endif
#include "hexdump.h"
#include "init.h"
#include "kd.h"
#include "keypress.h"
#include "lineno.h"
#include "mark.h"
#include "memdata.h"
#include "mvcursor.h"
#include "netlist.h"
#include "normalize.h"
#ifdef PAD
#include "pad.h"
#endif
#ifdef WIN32
#include "X11/Xw32defs.h"
#endif
#include "parms.h"
#include "pastebuf.h"
#include "pd.h"
#include "prompt.h"
#include "record.h"
#include "redraw.h"
#include "sbwin.h"
#include "sendevnt.h"
#include "serverdef.h"
#include "tab.h"
#include "titlebar.h"
#include "txcursor.h"
#include "typing.h"
#include "undo.h"
#ifdef PAD
#include "unixpad.h"
#include "unixwin.h"
#include "vt100.h"
#endif
#include "wc.h"
#include "window.h"
#include "windowdefs.h"
#include "winsetup.h"
#include "xerror.h"
#include "xerrorpos.h"
#include "xutil.h"


/***************************************************************
*  
*  System routines which have no built in defines
*  
***************************************************************/

char   *getenv(const char *name);
int    getppid(void);
int    getpid(void);
int    setpgrp(int pid, int pgrp);  /* bsd version */
int    isatty(int fd);
#ifndef WIN32
void   gethostname(char  *target, int size);
#endif
/*char *getcwd(char *buf, int size); creates error on solaris*/


/***************************************************************
*  
*  Local Protypes
*  
***************************************************************/

static int realloc_pixmap(DRAWABLE_DESCR   *pix,
                          Display          *display,
#ifdef WIN32
                          Drawable         *x_pixmap,
#else
                          Pixmap           *x_pixmap,
#endif
                          Window            main_window);

static void reset_geo_parm(DRAWABLE_DESCR   *window,
                           char            **option_value);


#ifdef CE_LITTLE_ENDIAN
int  magic_numbers[] = {1700949842, 1394635890, 1634564468, 1260398112, 1852405349, 2037141536, 544367980,  0};  /* time differentials */
#else
int  magic_numbers[] = {1383031397, 1920213075, 1954114913, 539369547,  1702259054, 542141561,  1818587680, 0};  /* time differentials */
#endif
static void check_differentials(int *m);

#ifdef DebuG
static void dump_atom(XEvent  *event);
#endif

static void process_dash_e_parm(int  *argc, char *argv[]);

#ifdef WIN32
static int dash_term_present(int  argc, char *argv[]);
#endif

#ifndef WIN32
static void setup_signal_handlers(void);
#endif


extern char   *compile_date;
extern char   *compile_time;


int main(int argc, char *argv[])
{
XEvent                event_union;      /* The event structure filled in by X as events occur            */

int                   redraw_needed;    /* bitmask flag to tell when window redraw is needed and also what kind  */                
int                   warp_needed;      /* Boolean flag to show that an XWarpCursor call is needed               */
int                  shell_fd = -1;     /* in pad mode, this is the file descriptor for the 2 way socket to the shell */
int                  cmd_fd;            /* When -input is specified, this is the file descriptor for the alterate source of DM commands */

int                  read_locked;       /* Set from -readlock option, locks windows and child windows into read only. */

int                   padmode_shell_pid;                 /* in pad mode, pid of the shell we are connected to */

int                   pid;              /* process id of forked processes when forking away from the terminal - localized use only */
PAD_DESCR            *from_buffer;      /* Buffer matched to an X event                                  */

int                   cursor_row;       /* work variable used when processing cursor motion events - localized use */
int                   cursor_col;       /*   "                   "                      "               "          */
int                   window_echo;      /* Boolean work variable used when processing cursor motion events - shows the current window has text highlighting */
int                   open_for_write;   /* Boolean work variable used to hold data about opening file and main window for r/w before memdata token is set up */

static int            conf_event   = ConfigureNotify; /* constant used to eat extra configure notify events */
static int            motion_event = MotionNotify;    /* constant used to eat extra motion events */

char                 *p,*q;             /* localized use scrap variables                                           */
int                   i;
#ifndef WIN32
unsigned int          ui;
#endif
int                   old_lines_on_screen;
int                   window_resized;
char                  msg[MAX_LINE];

/***************************************************************
*
*  Set the command name
*
***************************************************************/
#ifdef WIN32
cmd_path = argv[0];
p = strrchr(argv[0], '\\');
if (!p) p = strrchr(argv[0], '/');
#else
p = strrchr(argv[0], '/');
#endif

if (p == NULL)
   cmdname = argv[0];
else
   cmdname = p+1;

#ifdef WIN32
p = strstr(cmdname, ".exe");
if (!p) p = strstr(cmdname, ".EXE");
if (p)
   {
      q = strdup(cmdname);
      q[p-cmdname] = '\0';
      cmdname = q;
   }
#endif

#ifndef WIN32
/***************************************************************
*  
*  If this is not pad mode, drop the setuid right away.
*  This block of code added 10/10/95 RES
*
***************************************************************/
if (strncmp(cmdname, "ceterm", 6) != 0)
   if (getuid() && !geteuid())
      {
         setuid(getuid());
      }
#endif


/***************************************************************
*  
*  Set up our malloc protection scheme.  This supports the
*  CE_MALLOC macro defined in "malloc.h" (local).
*  2048 is the revserve storage used in case of failure.
*  That storage will be freed on failure to allow enough space
*  to try to do a save.
*  
***************************************************************/

malloc_init(2048);
set_global_dspl(init_display());   /* set_global_dspl in getevent.c  init_display in display.c */
dspl_descr->next = dspl_descr;     /* circular linked list with one element */
#ifdef WIN32
dspl_descr->term_mainpad_mutex = (unsigned long)CreateMutex(NULL, False, NULL);
dspl_descr->main_thread_id     = GetCurrentThreadId();
#endif

/***************************************************************
*  
*  Set the current working directory variables used by ce_getcwd
*  
***************************************************************/

#ifdef PAD
ce_setcwd(NULL);  /* NULL pointer tells ce_setcwd to do initialization in pad.c */
#endif

/***************************************************************
*
*  Get debugging flag from the environment.  If it is high
*  enough turn on X syncronization with the server.
*
***************************************************************/

#ifdef  DebuG
DEBUG0(Debug(DEBUG_ENV_NAME);)   /* in debug.c */
#endif


/***************************************************************
*  
*  If this is ceterm or ntterm, see if the special -e option
*  exists,  If so, we will modify the parm list so everything after
*  the -e is placed in one big parm.
*  
***************************************************************/

#ifdef WIN32
if ((strncmp(cmdname, "ceterm", 6) == 0)  || (strncmp(cmdname, "ntterm", 6) == 0) || (dash_term_present(argc, argv)))
#else
if (strncmp(cmdname, "ceterm", 6) == 0)
#endif
   process_dash_e_parm(&argc, argv);

/***************************************************************
*  
*  Get the command line options and merge them with stuff from
*  the .Xdefaults file and such.  getxopts in getxopts.c
*  
***************************************************************/

getxopts(cmd_options, OPTION_COUNT, defaults, &argc, argv, OPTION_VALUES, OPTION_FROM_PARM, &(dspl_descr->display), DISPLAY_IDX, NAME_IDX, resource_class);

#ifndef WIN32
if (DELAY_PARM)
   {
      float  secs;
      sscanf(DELAY_PARM, "%f", &secs);
      ui = (ABSVALUE(secs) * 1e6);
      usleep(ui);
   }
#endif /* WIN32 */

/***************************************************************
*  -version just prints the version and quits.
***************************************************************/
if (VERSION_PARM)
   {
      fprintf(stdout, PACKAGE_STRING, VERSION);
      fprintf(stdout, ",  Compiled: %s %s\n", compile_date, compile_time);
      return(0);   /* return from main */
   }

/***************************************************************
*  -oplist prints the options supplied and quits.
***************************************************************/
if (OPLIST)
   {
      for (i = 0; i < OPTION_COUNT; i++)
         if (OPTION_VALUES[i])
            fprintf(stdout, "%s%s : %s\n", cmdname, cmd_options[i].specifier, OPTION_VALUES[i]);
      return(0);   /* return from main */
   }

/***************************************************************
*  -help prints the list of available options and resources
***************************************************************/
if (HELP_PARM)
   {
      usage(argv[0]);
      return(0);  /* return from main */
   }

/***************************************************************
*  You can specify the directory for the ce .hlp files in the
*  app-defaults/Ce file (ce_install), or in the .Xdefaults, or
*  in the .profile.
***************************************************************/
if (CEHELPDIR)
   {
      p = getenv("CEHELPDIR");
      if (!p || !(*p))
         {
            snprintf(msg, sizeof(msg), "CEHELPDIR=%s", CEHELPDIR);
            p = malloc_copy(msg);
            if (p)
               putenv(p);
         }
   }

/***************************************************************
*  If the environment variable CE_KDP is set to a non-blank
*  value and -kdp was not specfied on the command line,
*  then set the -kdb property from the environment variable.
***************************************************************/
p = getenv("CE_KDP");
if (p && *p && !OPTION_FROM_PARM[KDP_IDX])
   {
      if (KEYDEF_PROP)
         free(KEYDEF_PROP);
      KEYDEF_PROP = malloc_copy(p);
   }

#ifdef DebuG
if (DebuG_PARM) /* Macro defined in parms.h */
   {
      snprintf(msg, sizeof(msg), "%s=%s", DEBUG_ENV_NAME, DebuG_PARM);
      p = malloc_copy(msg);
      if (p)
         putenv(p);
      Debug(DEBUG_ENV_NAME);
   }
#else
if (DebuG_PARM && *DebuG_PARM)
   dm_error("Ce not compiled with Debug enabled", DM_ERROR_BEEP);
#endif

/****************************************************************
*
*   getxopts opens the display, make sure it is indeed open.
*
****************************************************************/

 if (dspl_descr->display == NULL)
    {
       snprintf(msg, sizeof(msg), "%s: cannot connect to X server %s (%s)\n",
               argv[0], XDisplayName(DISPLAY_NAME), strerror(errno));
       dm_error(msg, DM_ERROR_LOG);
       return(1);  /* return from main */
    }

/***************************************************************
*  
*  If the display name was set in the option values, make it
*  the default.
*  
***************************************************************/

if (OPTION_FROM_PARM[DISPLAY_IDX])
   {
      snprintf(msg, sizeof(msg), "DISPLAY=%s", DISPLAY_NAME);
      p = malloc_copy(msg);
      if (p)
         putenv(p);
   }

#ifdef WIN32
#define _XFlush XFlush
#endif
DEBUG1(socketfd2hostinet(ConnectionNumber(dspl_descr->display), DISPLAY_PORT_LOCAL);socketfd2hostinet(ConnectionNumber(dspl_descr->display), DISPLAY_PORT_REMOTE);)
DEBUG32(_XFlush(dspl_descr->display);)
DEBUG9(XERRORPOS)
if (XSYNCHRONIZE)
   (void)XSynchronize(dspl_descr->display, True); /* RES 5/1/1998,  temp fix for X terminals X server */
else
   DEBUG9( (void)XSynchronize(dspl_descr->display, True);)

/***************************************************************
*  
*  Get the expression mode set real early on the first pass.
*  Processing $ chars in the name depend upon it.
*  
***************************************************************/

if (UNIXEXPR_ERR)
   {
      fprintf(stderr, "Invalid value for Ce.expr (%s), Aegis or Unix must be capitalized\n", OPTION_VALUES[EXPR_IDX]);
      fprintf(stderr, "The cpp pass over the .Xdefaults file may convert \"unix\" or \"aegis\" to \"1\"\n");
   }
if (UNIXEXPR)
   dspl_descr->escape_char = '\\';
else
   dspl_descr->escape_char = '@';
/*escape_char = dspl_descr->escape_char;*/

/***************************************************************
*  
*  If the program is being called as ce_merge, do the merge
*  operation and quit.
*  
***************************************************************/

if (strcmp(cmdname, "ce_merge") == 0)
   {
      i = ce_merge(dspl_descr);
      return(i); /* return from main */
   }

/***************************************************************
*  
*  If the -load option was specified, just load the key defintions
*  into the server, if needed, and quit.
*  
***************************************************************/

if ((LOAD_PARM) || (RELOAD_PARM))
   {
      if (RELOAD_PARM)
         dm_delsd(dspl_descr->display, KEYDEF_PROP, dspl_descr->properties); /* in serverdef.c */
      init_dm_keys(APP_KEYDEF, TABSTOPS, dspl_descr); /* in kd.c */
      while(process_some_keydefs(dspl_descr));        /* in kd.c */
      XFlush(dspl_descr->display);
      DEBUG9(XERRORPOS)
      XCloseDisplay(dspl_descr->display);
      return(0);  /* return from main */
   }

check_differentials(magic_numbers);

/***************************************************************
*  
*  There should be one or zero arguments left,  this is the
*  file name to open.  If zero, we will read stdin.
*  validate file either works or bails out.
*  
***************************************************************/

if (set_file_from_parm(dspl_descr, argc, argv, edit_file) != 0) /* in pw.c */
   return(1); /* return from main */

/***************************************************************
*  
*  Decide whether we plan on writing on this file.  The default
*  for ce is write, cv is read, crpad is read.
*  
***************************************************************/

if (((cmdname[0] == 'c') && (cmdname[1] == 'v')) || (strcmp(cmdname, "crpad") == 0) || (strcmp(edit_file, STDIN_FILE_STRING) == 0))
   open_for_write = False;
else
   open_for_write = True;

#ifdef PAD
/***************************************************************
*  
*  If the cmd_name is ce_pad, turn on cmd mode.
*  
***************************************************************/
#ifdef WIN32
if ((strncmp(cmdname, "ceterm", 6) == 0)  || (strncmp(cmdname, "ntterm", 6) == 0))
#else
if (strncmp(cmdname, "ceterm", 6) == 0)
#endif
   {
      dspl_descr->pad_mode = True;
      open_for_write = False;  /* pad is not user writable */
      if (TRANSPAD)
         {
            dm_error("-transpad option ignored in ceterm", DM_ERROR_BEEP); 
            free(OPTION_VALUES[TRANSPAD_IDX]);
            OPTION_VALUES[TRANSPAD_IDX] = (char *)strdup("no");
         }
   }
else
   dspl_descr->pad_mode = False;
#endif
#ifdef WIN32
/***************************************************************
*  
*  For WIN32, process the -term, -edit, and -browse options.
*  If -term was not specified and pad_mode is on anyway, the
*  program name is ntterm or ceterm.  If this is the case and
*  a file was specifed which is not executable, then someone
*  dragged a file onto the ntterm icon.  Come up in browse.
*  
***************************************************************/
if (TERM_MODE)
   {
      dspl_descr->pad_mode = True;
      open_for_write = False;  /* pad is not user writable */
   }

if (BROWSE_MODE)
   open_for_write = False;

if ((EDIT_MODE) && (strcmp(edit_file, STDIN_FILE_STRING) != 0))
   open_for_write = True;

#endif

/***************************************************************
*  
*  If the input is stdin, make sure it is not pointing to the
*  terminal and waiting for the poor fool to type a bunch of
*  lines.  Unless the transpad option was specified.
*  
***************************************************************/
/* 6/24/1999, don't need ifdef pad, just test pad mode */
if (!dspl_descr->pad_mode && !TRANSPAD && (strcmp(edit_file, STDIN_FILE_STRING) == 0) && (isatty(0)))
   {
#ifdef WIN32
      if (do_open_file_dialog(edit_file, 256, True) != 0) /* in pad.c */
         return(1); /* return from main */
#else
      fprintf(stderr, "%s: Input from stdin requested and stdin is a terminal, exiting\n\n", argv[0]);
      usage(argv[0]);
      fprintf(stderr, "\n%s: Input from stdin requested and stdin is a terminal, exiting\n", argv[0]);
      return(1);  /* return from main */
#endif
   }

/***************************************************************
*  
*  Set up the error handlers for problems with X.
*  The first one is for protocol problems, run out of server memory
*  and so forth.
*
*  The second is for things like a server crash.
*  
*  This set of calls is done twice, once here for intial_file_open
*  and once later to allow reinitialization for reopening.
*
***************************************************************/

DEBUG9(XERRORPOS)
XSetErrorHandler(ce_xerror);
DEBUG9(XERRORPOS)
XSetIOErrorHandler(ce_xioerror);

#ifdef PAD
if (dspl_descr->pad_mode)
   {
      change_background_work(dspl_descr, MAIN_WINDOW_EOF, True);
      /* Get rid of the CE_EDIT_DIR env var if it exists.  see pw.h */
      if (getenv(CE_EDIT_DIR) != NULL)
         {
            snprintf(msg, sizeof(msg), "%s=", CE_EDIT_DIR);
            p = malloc_copy(msg);
            if (p)
               putenv(p);
         }
   }
else
#endif
   {
      /***************************************************************
      *  If we are not in pad mode, Open the input file, which may be stdin.
      *  The open_for_write flag may be change if the file exists and
      *  is not writeable.
      *
      *  Put the edit file name in an environment variable.  indent
      *  processing may need this.
      ***************************************************************/
      snprintf(msg, sizeof(msg), "CE_FILENAME=%s", edit_file);
      p = malloc_copy(msg);
      if (p)
         putenv(p);

      /***************************************************************
      *  Make sure the keydefs are loaded.  LSF processing requires
      *  the hash table.
      ***************************************************************/
      if (LSF)
         {
            init_dm_keys(APP_KEYDEF, TABSTOPS, dspl_descr); /* in kd.c */
            while(process_some_keydefs(dspl_descr));
         }

      instream = initial_file_open(edit_file, &open_for_write, dspl_descr); /* in pw.c */
      if (instream == NULL)
         return(3); /* return from main */
   }

#ifndef WIN32
/***************************************************************
*  
*  Check the -stdin parm to see if there is an alternate
*  source of DM commands.  WIN32 does this later
*  
***************************************************************/

if (TRANSPAD || (STDIN_CMDS && (dspl_descr->pad_mode || (strcmp(edit_file, STDIN_FILE_STRING) != 0))))
   cmd_fd = 0;
else
   cmd_fd = -1;
#else
cmd_fd = -1;
#endif


/***************************************************************
*  
*  If the -w parm was not specified, go background.
*  
***************************************************************/

#ifndef CYGNUS
if (!WAIT_PARM)
   {
     pid = spoon(); 
#ifdef PAD
     if (((pid == -1) && dspl_descr->pad_mode) || (pid > 0))
        return(0); /* return from main */
#else
     if (pid != 0)
        return(0); /* return from main */
#endif
   }
#endif

/***************************************************************
*  
*  Set up signal handlers for attention type interupts so we
*  can try to create a crash file.
*  catch_quit is in xerror.c
*  
***************************************************************/

#ifndef WIN32 /* no signal handlers in WIN32 */
setup_signal_handlers(); /* local to this module */
#endif  /* not WIN32 */

/**************************************************************
*  
*  Start loading the key definitions we will be using.
*  This is continued in background mode.
*  
***************************************************************/

if (!LSF)
   init_dm_keys(APP_KEYDEF, TABSTOPS, dspl_descr); /* in kd.c */

/**************************************************************
*  
*  If -readlock was specified, start in read only mode and
*  disable the ro command.
*  
***************************************************************/

read_locked = READLOCK;
if (read_locked)
   open_for_write = False;

/**************************************************************
*  
*  Make sure the paste buffer directory is set.  Variable
*  paste_buff_dir in in pastebuf.h and is used by pastebuf.c
*  
***************************************************************/

if (PASTE_DIR != NULL)
   strncpy(paste_buff_dir, PASTE_DIR, sizeof(paste_buff_dir));
else
   strcpy(paste_buff_dir, "~/.CePaste");
translate_tilde(paste_buff_dir);  /* process names starting with ~ if the name starts that way */

/***************************************************************
*  
*  Build the windows
*  
***************************************************************/
win_setup(dspl_descr, edit_file, resource_class, open_for_write, argv[0]); /* in winsetup.c */

#ifdef WIN32
/***************************************************************
*  
*  Check the -stdin parm to see if there is an alternate
*  source of DM commands.  Non-WIN32 does this earlier in setup.
*  
***************************************************************/
if (TRANSPAD || (STDIN_CMDS && (dspl_descr->pad_mode || (strcmp(edit_file, STDIN_FILE_STRING) != 0))))
   transpad_input(dspl_descr); /* in unixpad.c */
#endif



#ifdef PAD
/***************************************************************
*  
*  If we are in pad mode, call the routine to start the shell.
*  Strip the command name down to the parm.
*  
***************************************************************/

if (dspl_descr->pad_mode)
   {
      /***************************************************************
      *  Save the window id for the error routine.  Sometimes because
      *  of timing, window events come in for the unixcmd window
      *  after we have issued a close for the window.  A error is
      *  generated when we try to move the cursor.  This is hard to
      *  prevent, so we will just suppress the message.
      *  ce_error_unixcmd_window is global data in xerror.h.
      ***************************************************************/
      ce_error_unixcmd_window = dspl_descr->unix_pad->x_window;

      /***************************************************************
      *  Put the edit file name in an environment variable.  indent
      *  processing may need this.
      ***************************************************************/
      snprintf(msg, sizeof(msg), "CE_FILENAME=");
      p = malloc_copy(msg);
      if (p)
         putenv(p);

      shell_fd = ConnectionNumber(dspl_descr->display); /* pass in the fd for X, get out the fd for the shell */
#ifdef WIN32
      padmode_shell_pid = pad_init(edit_file, &shell_fd, (MAKE_UTMP ? dspl_descr->display_host_name : NULL), LOGINSHELL, dspl_descr);
#else
      padmode_shell_pid = pad_init(edit_file, &shell_fd, (MAKE_UTMP ? dspl_descr->display_host_name : NULL), LOGINSHELL);
#endif
      if (padmode_shell_pid < 0)
         {
            dspl_descr->autoclose_mode = False;
            kill_unixcmd_window(True);
         }

      q = strpbrk(edit_file, " \t");  /* strip off any parms for the titlebar */
      if (q)
         *q = '\0';

      p = &edit_file[strlen(edit_file)];
      snprintf(p, (sizeof(edit_file)-(p-edit_file)), "(%d)", padmode_shell_pid);
      snprintf(msg, sizeof(msg), "CESHPID=%d", padmode_shell_pid);
      p = malloc_copy(msg);
      if (p)
         putenv(p);
      if (VTMODE) /* RES 10/4/94 make -vt on parameter work */
         {
            static DMC_vt100  vt100_dmc = {NULL, DM_vt, False, DM_TOGGLE};
            vt100_dmc.mode = DM_ON;
            dm_vt100((DMC *)&vt100_dmc, dspl_descr);
         }
   }
else
   {
#ifndef WIN32
      signal(SIGCHLD, reap_child_process);                               
#ifdef SIGCLD
#if (SIGCLD != SIGCHLD)
      signal(SIGCLD, reap_child_process);                               
#endif
#endif
#endif
      shell_fd = -1;
      padmode_shell_pid = 0;
   }
#else
#ifdef SIGCHLD
signal(SIGCHLD, reap_child_process);                               
#endif
#ifdef SIGCLD
#if (SIGCLD != SIGCHLD)
signal(SIGCLD, reap_child_process);                               
#endif
#endif

padmode_shell_pid = 0;

#endif

#ifdef PAD
/***************************************************************
*  
*  If we are not in wait mode.  Isolate ourselves from the caller.
*  
***************************************************************/

#if !defined(ultrix) && !defined(WIN32)
if (!WAIT_PARM)
   isolate_process(getpid());
#endif
#endif

/***************************************************************
*  
*  If we are not really root, stop being root.
*  
***************************************************************/

#ifndef WIN32
#ifdef ultrix
if (getuid() && !geteuid() && !(dspl_descr->pad_mode))
#else
if (getuid() && !geteuid())
#endif
   {
      DEBUG16(fprintf(stderr, "Setting uid to %d in main process\n", getuid());)
#ifndef _INCLUDE_HPUX_SOURCE
#ifdef ultrix
      setreuid(0, getuid());
#else
      seteuid(getuid());
#endif
#else
      setresuid(-1, getuid(), -1);
#endif
    }
#endif /* WIN32 */

/***************************************************************
*  
*  Save the top of the event loop for later use.
*  If we are running a cc window, potentially on another node,
*  and the network goes down, the XIO erro handler gets called.
*  If there are displays left which we can continue to process,
*  it longjmp's to here.
*  
***************************************************************/
(void) setjmp(setjmp_env);

/***************************************************************
****************************************************************
**
**  Main event loop.  We stay in here forever.
**  Or at least till a wc command exits the program.
**  <find marker>
**
****************************************************************
***************************************************************/

/* CONSTCOND */
while(1)
{
   get_next_X_event(&event_union, &shell_fd, &cmd_fd);
   DEBUG14(fprintf(stderr, "MAIN:  Cursor in %s, buff for %s\n", which_window_names[dspl_descr->cursor_buff->which_window],which_window_names[dspl_descr->cursor_buff->current_win_buff->which_window]);)

   /***************************************************************
   *  
   *  Switch on the type of event.  This separates screen things
   *  such as expose (pop), configure (resize and move), from
   *  user input such as keypress and mouse press.
   *  
   ***************************************************************/

   switch (event_union.type)
   {
   /***************************************************************
   *  In the case of an expose event, we need to copy the pixmap
   *  to the screen.  Copy the portion needed.
   ***************************************************************/
   case Expose:
      exposed_window(dspl_descr, &event_union); /* in expose.c */
      break;


   /***************************************************************
   *
   *  MotionNotify covers the case of pointer (MOUSE) motion
   *  The scroll bars are handled as a special case.
   *
   ***************************************************************/
   case MotionNotify:
   case EnterNotify:
   case LeaveNotify:
      /***************************************************************
      *  Process the events for pulldowns if needed.
      ***************************************************************/
      if (event_union.xany.window ==  dspl_descr->pd_data->menubar_x_window)
         {
            if (event_union.type == MotionNotify)
               pd_button_motion(&event_union, dspl_descr);

            break;
         }

      if (dspl_descr->pd_data->pulldown_active)
         {
            if (event_union.type == MotionNotify)
               {
                  if (pd_window_to_idx(dspl_descr->pd_data, event_union.xany.window) >= 0)
                     pd_button_motion(&event_union, dspl_descr);
               }
            else 
               if (event_union.type == EnterNotify)
                  if (pd_window_to_idx(dspl_descr->pd_data, event_union.xany.window) >= 0)
                     force_out_window(dspl_descr); /* kill any pending warps */
            break;
         }

      /***************************************************************
      *  Process the events for the scroll bar windows
      ***************************************************************/
      if ((event_union.xany.window == dspl_descr->sb_data->vert_window) ||
          (event_union.xany.window == dspl_descr->sb_data->horiz_window))
         {
            if (event_union.type == MotionNotify)
               {
                  if (event_union.xany.window == dspl_descr->sb_data->vert_window)
                     redraw_needed = move_vert_slider(&event_union, dspl_descr);
                  else
                     redraw_needed = move_horiz_slider(&event_union, dspl_descr);

                  if (redraw_needed)
                     process_redraw(dspl_descr, redraw_needed, False);  /* redo the pointer move */
               }
            dspl_descr->cursor_buff->up_to_snuff  = False;
            dspl_descr->tdm_mark.mark_set = False;  
            dspl_descr->root_mark.mark_set = False;  /* cursor moving in main window, erase markp mark */
            break;
         }

      /****************************************************************************************
      *  If this is a leave event and a mouse button is down and we are highlighting a region
      *  and we are exiting through the top or bottom of the window, start scrolling.
      *  RES 11/2004
      ****************************************************************************************/
      if ((event_union.type == LeaveNotify) &&
          dspl_descr->button_down && dspl_descr->mark1.mark_set && dspl_descr->echo_mode  &&
          (event_union.xany.window ==  dspl_descr->main_pad->x_window))
         if ((event_union.xcrossing.y <= 0) || (event_union.xcrossing.y >= dspl_descr->main_pad->window->height))
            {
               DEBUG23(
                   fprintf(stderr, "Main window exit at %d triggering vertical scrolling\n", event_union.xcrossing.y);
               )
               dspl_descr->drag_scrolling = True;
               redraw_needed |= build_in_arrow_press(&event_union, dspl_descr);
            }
         else
            if ((event_union.xcrossing.x <= 0) || (event_union.xcrossing.x >= dspl_descr->main_pad->window->width))
               {
                  DEBUG23(
                      fprintf(stderr, "Main window exit at %d triggering horizontal scrolling\n", event_union.xcrossing.x);
                  )
                  redraw_needed |= build_in_arrow_press(&event_union, dspl_descr);
                  dspl_descr->drag_scrolling = True;
               }

      /****************************************************************************************
      *  If this is an enter event and a mouse button is down and we are highlighting a region
      *  STOP scrolling.
      ****************************************************************************************/
      if ((event_union.type == EnterNotify) &&
          dspl_descr->button_down && dspl_descr->mark1.mark_set && dspl_descr->echo_mode  &&
          (event_union.xany.window ==  dspl_descr->main_pad->x_window) &&
          (dspl_descr->drag_scrolling))
         {
            DEBUG23(
                fprintf(stderr, "Main window enter at (%d,%d) stopping scrolling\n", event_union.xcrossing.x, event_union.xcrossing.y);
            )
            dspl_descr->drag_scrolling = False;
            redraw_needed |= build_in_arrow_rlse(dspl_descr);
         }


      /***************************************************************
      *  Use real enter and leave events to keep track of whether the 
      *  cursor is somewhere in our window structure.
      ***************************************************************/
      if ((event_union.type == EnterNotify) || (event_union.type == LeaveNotify))
         window_crossing(&event_union, dspl_descr);

      /***************************************************************
      *  Use real enter and leave events to keep track of whether the 
      *  cursor is somewhere in our window structure.
      ***************************************************************/
      if ((event_union.type == EnterNotify) && (in_window(dspl_descr) == False) && (event_union.xcrossing.focus == True))
         fake_focus(dspl_descr, event_union.xcrossing.focus);

      /***************************************************************
      *  If we are in mouse off mode and this is not a sent event, ignore it.
      *  However, we still do the flush if there are cc windows.
      ***************************************************************/
      if (!MOUSEON && !event_union.xany.send_event)
         break;

      /***************************************************************
      *  If we are in vt100 mode off mode and this is not a sent event,
      *  in the main window we will not process it.  However, if it
      *  is an enter notify into the main window, we will retransmit
      *  it so it goes to the right place in the main window.
      ***************************************************************/
      if (dspl_descr->vt100_mode && !event_union.xany.send_event && (event_union.xany.window == dspl_descr->main_pad->x_window))
         {
            if (event_union.type == EnterNotify)
               {
                  if (event_union.xmotion.y > dspl_descr->dmoutput_pad->window->y)  /* entering through the bottom of the window */
                     break;
                  dm_position(dspl_descr->cursor_buff, dspl_descr->main_pad, dspl_descr->main_pad->file_line_no, dspl_descr->main_pad->file_col_no);
                  process_redraw(dspl_descr, 0, True);  /* redo the pointer move */
                  break;
               }
            else
               if ((event_union.type == MotionNotify) && (!dspl_descr->mark1.mark_set))  /* residual event from the pipe */
                  break;                                                     /* ignore it, mark set is for dr/xc processing  */
         }

      /***************************************************************
      *  In click to change focus mode, we would like to ignore the
      *  mouse click if the user clicked m1 in the main window.  It
      *  turns out we get an extra LeaveNotify and EnterNotify with
      *  mode = NotifyGrab and NotifyUngrab respectively.  We will
      *  set a flag to ignore the next click.
      ***************************************************************/
      if ((event_union.type             == LeaveNotify) &&
          (event_union.xcrossing.window == dspl_descr->main_pad->x_window) &&
          (event_union.xcrossing.focus  == False) && /* res 11/11/2005 Added for Linux KDE, button click always generates leave and enter */
          (event_union.xcrossing.mode   == NotifyGrab))
         {
            dspl_descr->expecting_focus_mouseclick = True;
         }

      /***************************************************************
      *  Eat up all but the last motion event unless we are waiting for one.
      ***************************************************************/
      DEBUG9(XERRORPOS)
      if (!expecting_warp(dspl_descr))
         while (ce_XCheckIfEvent(dspl_descr->display,
                                 &event_union,
                                 find_event,
                                 (char *)&motion_event))
               DEBUG14( fprintf(stderr, "Previous event IGNORED\n\n");dump_Xevent(stderr, &event_union, debug);)

      /***************************************************************
      *  Determine which window we are dealing with.
      ***************************************************************/
      from_buffer = window_to_buff_descr(dspl_descr, event_union.xany.window);
#ifdef PAD
      /***************************************************************
      *  Ignore motion in the unixcmd window when it does not exist.
      *  This is due to residual events after the window is unmapped or destroyed.
      ***************************************************************/
      if ((from_buffer->which_window == UNIXCMD_WINDOW) && (dspl_descr->vt100_mode || !dspl_descr->pad_mode))
         break;
#endif
      dspl_descr->cursor_buff->x = event_union.xmotion.x;
      dspl_descr->cursor_buff->y = event_union.xmotion.y;
      dspl_descr->cursor_buff->root_x = event_union.xmotion.x_root;
      dspl_descr->cursor_buff->root_y = event_union.xmotion.y_root;
      dspl_descr->cursor_buff->which_window = from_buffer->which_window;
      if (event_union.type != LeaveNotify) /* therefore must be an enter or motion */
         {
            if (event_union.xcrossing.subwindow != 0)
               break; /* ignore the rest of the event */
            if (from_buffer->which_window == MAIN_PAD)
               {
                  /* cursor moving in main window, erase tdm mark */
                  dspl_descr->tdm_mark.mark_set = False;  
                  dspl_descr->root_mark.mark_set = False;  /* cursor moving in main window, erase markp mark */
               }                                    

            if (dspl_descr->echo_mode && dspl_descr->mark1.buffer == from_buffer)
               window_echo = True;
            else
               window_echo = False;
         }
      else
         {
            /***************************************************************
            *  Must be a leave event.  Don't do text highlighting.
            *  Set a TDM mark if none is set to the last place a mark was set.
            ***************************************************************/
            window_echo = False;
            /* RES 8/29/95 - release line when cursor leaves window */
            if (cc_ce) /* bad idea*/
               flush(dspl_descr->main_pad);
            if (!dspl_descr->tdm_mark.mark_set &&
                ((from_buffer->which_window == MAIN_PAD) || (from_buffer->which_window == UNIXCMD_WINDOW)))
               {
#ifdef PAD
                  if ((from_buffer->which_window == UNIXCMD_WINDOW) ||
                      (dspl_descr->pad_mode && !dspl_descr->vt100_mode))
                     {
                        dspl_descr->tdm_mark.mark_set = True;
                        dspl_descr->tdm_mark.file_line_no = dspl_descr->unix_pad->file_line_no;
                        dspl_descr->tdm_mark.col_no = dspl_descr->unix_pad->file_col_no;
                        dspl_descr->tdm_mark.buffer = dspl_descr->unix_pad;
                     }
                  else
                     {
                        dspl_descr->tdm_mark.mark_set = True;
                        dspl_descr->tdm_mark.file_line_no = dspl_descr->main_pad->file_line_no;
                        dspl_descr->tdm_mark.col_no = dspl_descr->main_pad->file_col_no;
                        dspl_descr->tdm_mark.buffer = dspl_descr->main_pad;
                     }
#else
                  dspl_descr->tdm_mark.mark_set = True;
                  dspl_descr->tdm_mark.file_line_no = dspl_descr->main_pad->file_line_no;
                  dspl_descr->tdm_mark.col_no = dspl_descr->main_pad->file_col_no;
                  dspl_descr->tdm_mark.buffer = dspl_descr->main_pad;
#endif
               }
         } /* end of leave event processing */

      /***************************************************************
      *  Don't draw unless we have the focus.
      *  RES 8/26/94
      ***************************************************************/
      if (!in_window(dspl_descr))
         {
            erase_text_cursor(dspl_descr); /* make sure it is gone */
            dspl_descr->cursor_buff->up_to_snuff  = False;
            break;
         }
      /***************************************************************
      *  Convert the cursor position to a row, column.
      *  This is a window row and a window column.
      ***************************************************************/
      text_cursor_motion(from_buffer->window,      /* input  */
                         window_echo,              /* input  */
                         &event_union,             /* input  */
                         &cursor_row,              /* output */
                         &cursor_col,              /* output */
                         from_buffer->win_lines,   /* input  */
                         dspl_descr);              /* input  */
      if (window_echo)
         text_highlight(from_buffer->x_window, from_buffer->window, dspl_descr);

      dspl_descr->cursor_buff->win_col_no   = cursor_col;
      /***************************************************************
      *  If we are still in the same pad window, on the same line,  and the
      *  real file line numbers match, we are still up to snuff.
      *  If a mark is set, we cannot leave the up_to_snuff slide because
      *  we are marking a range of text.
      ***************************************************************/
      DEBUG14(fprintf(stderr, "motion send event = %d\n", event_union.xany.send_event);)
      if ((!event_union.xany.send_event) || 
          (dspl_descr->cursor_buff->which_window != from_buffer->which_window) || 
          (dspl_descr->cursor_buff->win_line_no != cursor_row) || 
          ((dspl_descr->cursor_buff->current_win_buff->file_line_no - dspl_descr->cursor_buff->current_win_buff->first_line)  != cursor_row))
         {
            DEBUG8(fprintf(stderr, "Main: Motion event:  Resetting window row from %d to %d\n", dspl_descr->cursor_buff->win_line_no, cursor_row);)
            dspl_descr->cursor_buff->win_line_no  = cursor_row;
            dspl_descr->cursor_buff->which_window = from_buffer->which_window;
            dspl_descr->cursor_buff->up_to_snuff  = False;
         }

      break;

   /***************************************************************
   *
   *  MapNotify covers the case of the window first being mapped.
   *  We use this to do position the curor initially.
   *  We also use this to detect the window has been uniconned.
   *
   ***************************************************************/
   case MapNotify:
      /* figure out where we are really */
      get_root_location(dspl_descr->display,
                        dspl_descr->main_pad->x_window,
                        &dspl_descr->main_pad->window->x,
                        &dspl_descr->main_pad->window->y);
      if (dspl_descr->first_expose)
         {
            /***************************************************************
            *  Register the file in the CC list.
            *  This is what supports the cc option, xdmc, and other
            *  inter process communication features.
            ***************************************************************/
            dspl_descr->currently_mapped = True;
            cc_register_file(dspl_descr, padmode_shell_pid, edit_file);
            if (getenv(DMERROR_MSG_WINDOW) != NULL)
               {
                  snprintf(msg, sizeof(msg), "%s=", DMERROR_MSG_WINDOW);
                  p = malloc_copy(msg);
                  if (p)
                     putenv(p);
               }
            if (dspl_descr->pad_mode)
               {

#ifndef WIN32
                  /* 3/12/97 RES turn off signal for Bryan Harding at Cadence */
                  DEBUG16(fprintf(stderr, "Signalling child %d with SIGUSR1\n", padmode_shell_pid);)
                  i = kill(padmode_shell_pid, SIGUSR1);
                  if (i < 0)
                     {
                        snprintf(msg, sizeof(msg), "Could not signal child %d process with SIGUSR1", padmode_shell_pid);
                        dm_error(msg, DM_ERROR_LOG);
                     }
#ifdef ultrix
                  if (getuid() && !geteuid())
                     {
                        DEBUG16(fprintf(stderr, "Setting uid to %d in main process after map\n", getuid());)
                        setreuid(0, getuid());
                     }
#endif
#endif
               }
            /***************************************************************
            *  If commands were passed in via the -cmd argument, send a
            *  keystroke event to trigger their execution.
            ***************************************************************/
            if (CMDS_FROM_ARG)
               fake_keystroke(dspl_descr);  /* in getevent.c */

            dspl_descr->first_expose = False;
            force_in_window(dspl_descr); /* in window.c, allows warp in process redraw, even if cursor is not in the window */

#ifdef PAD
            if (dspl_descr->pad_mode)
               {
                  dm_position(dspl_descr->cursor_buff, dspl_descr->unix_pad, 0, 0);
                  process_redraw(dspl_descr, 0 /* no redraw */, True /* force warp */);
                  dspl_descr->cursor_buff->up_to_snuff  = False;
                  vi_set_size(shell_fd, dspl_descr->vt100_mode,
                              dspl_descr->main_pad->window->lines_on_screen+1, /* number of lines on screen (include unix cmd window line, RES 4/8/94) */
                              dspl_descr->main_pad->window->sub_width / dspl_descr->main_pad->window->font->max_bounds.width,  /* number of cols on window */
                              dspl_descr->main_pad->window->sub_width,    /* number of horizontal pixels available */
                              dspl_descr->main_pad->window->sub_height);  /* number of verticle pixels */
               }
            else
               {
                  bring_up_to_snuff(dspl_descr);
                  redraw_needed = dm_position(dspl_descr->cursor_buff, dspl_descr->main_pad, dspl_descr->main_pad->file_line_no, dspl_descr->main_pad->file_col_no);
                  process_redraw(dspl_descr, redraw_needed, True /* force warp */);
               }
#else
            dspl_descr->cursor_buff->x = dspl_descr->cursor_buff->current_win_buff->window->sub_x + 1;
            dspl_descr->cursor_buff->y = dspl_descr->cursor_buff->current_win_buff->window->sub_y + 1;
            process_redraw(dspl_descr, 0 /* no redraw */, True /* force warp */);
#endif
         }
      else
         if (event_union.xmap.window == dspl_descr->main_pad->x_window)
            {
               if (dspl_descr->x_pixmap == None)
                  {
                     dspl_descr->currently_mapped = True;
                     redraw_needed = realloc_pixmap(dspl_descr->main_pad->window, dspl_descr->display, &dspl_descr->x_pixmap, dspl_descr->main_pad->x_window);
                     process_redraw(dspl_descr, redraw_needed, False /* no warp */);
                  }
               if (dspl_descr->Hummingbird_flag) /* RES 7/9/2003 add Hummingbird flag */
                  cc_register_file(dspl_descr, padmode_shell_pid, edit_file);
            }
      cc_icon(False);
      break;


   /***************************************************************
   *
   *  UnmapNotify covers the case of the window being iconned.
   *
   ***************************************************************/
   case UnmapNotify:
      if (event_union.xunmap.window == dspl_descr->main_pad->x_window)
         {
            force_out_window(dspl_descr);
            cc_icon(True);

            dspl_descr->currently_mapped = False;
            if (dspl_descr->x_pixmap != None)
               {
                  DEBUG9(XERRORPOS)
                  XFreePixmap(dspl_descr->display, dspl_descr->x_pixmap);
                  DEBUG1(fprintf(stderr, "Pixmap 0x%X freed due to Unmap of main window\n", dspl_descr->x_pixmap);)
                  dspl_descr->x_pixmap = None;
               }
         }
      break;


   /***************************************************************
   *
   *  ReparentNotify means the window manager was restarted. 
   *  Redo the registering.
   *
   ***************************************************************/
   case ReparentNotify:
      cc_register_file(dspl_descr, padmode_shell_pid, edit_file);
      break;


   /***************************************************************
   *
   *  ConfigureNotify covers the case of the window changing shape.
   *
   ***************************************************************/
   case ConfigureNotify:

       OPTION_FROM_PARM[GEOMETRY_IDX] = True;

#ifndef apollo
  /*  removed 11/11/93, breaks things on the apollo due to suspected
      compiler bug when optimize is on and debug is off */
      /***************************************************************
      *  Eat up all but the last configure event
      ***************************************************************/
      DEBUG9(XERRORPOS)
      while (ce_XCheckIfEvent(dspl_descr->display,
                              &event_union,
                              find_event,     /* in getevent.c */
                              (char *)&conf_event))
         DEBUG15(fprintf(stderr, "Previous event IGNORED\n\n");dump_Xevent(stderr, &event_union, debug);)
#endif
  
      if (event_union.xany.window == dspl_descr->main_pad->x_window)
         window_resized = resize_pixmap(&event_union,                        /* input */
                                        &dspl_descr->x_pixmap,               /* input / output */
                                        dspl_descr->main_pad->window,        /* input / output */
                                        ForgetGravity);                      /* input leave screen blank. */
      else
         {
            DEBUG(
            if (event_union.xany.window == dspl_descr->dminput_pad->x_window)
               fprintf(stderr, "?main:  Unexpected window being reconfigured: DM_input window\n");
            else
               if (event_union.xany.window == dspl_descr->dmoutput_pad->x_window)
                  fprintf(stderr, "?main:  Unexpected window being reconfigured: DM_output window\n");
               else
                  fprintf(stderr, "?main:  Unexpected window being reconfigured: 0x%X\n", event_union.xany.window);
            )
         }

      get_root_location(dspl_descr->display,
                        dspl_descr->main_pad->x_window,
                        &dspl_descr->main_pad->window->x,
                        &dspl_descr->main_pad->window->y);

      clear_text_cursor(None, dspl_descr);

      /***************************************************************
      *  Save the new width and height in the window description and
      *  call the routines to resize the other sub areas and sub windows.
      ***************************************************************/

      /*dspl_descr->main_pad->window->width  = dspl_descr->main_pad->drawing_area->width;*/
      /*dspl_descr->main_pad->window->height = dspl_descr->main_pad->drawing_area->height;*/

      if (dspl_descr->pd_data->menubar_on)
         resize_menu_bar(dspl_descr->display,                 /* input   */
                      dspl_descr->pd_data->menubar_x_window,  /* input   */
                      dspl_descr->main_pad->window,           /* input   */
                      dspl_descr->title_subarea,              /* input   */
                      &dspl_descr->pd_data->menu_bar);        /* input / output  */


      resize_titlebar(dspl_descr->main_pad->window,        /* input   */
                      (dspl_descr->pd_data->menubar_on ? &dspl_descr->pd_data->menu_bar : NULL),
                      dspl_descr->title_subarea);          /* input / output  */

      resize_dm_windows(dspl_descr->main_pad->window,
                        dspl_descr->dminput_pad->window,
                        dspl_descr->dmoutput_pad->window,
                        dspl_descr->display,
                        dspl_descr->main_pad->x_window,
                        dspl_descr->dminput_pad->x_window,
                        dspl_descr->dmoutput_pad->x_window);
      dm_subarea(dspl_descr,
                 dspl_descr->dminput_pad->window,               /* input  in dmwin.c */
                 dspl_descr->dminput_pad->pix_window);          /* output */
      dm_subarea(dspl_descr,
                 dspl_descr->dmoutput_pad->window,              /* input  */
                 dspl_descr->dmoutput_pad->pix_window);         /* output */

#ifdef PAD
      if (dspl_descr->pad_mode && !(dspl_descr->vt100_mode))
         {
            resize_unix_window(dspl_descr, total_lines(dspl_descr->unix_pad->token));
            unix_subarea(dspl_descr->unix_pad->window,
                         dspl_descr->unix_pad->pix_window);
            old_lines_on_screen = dspl_descr->main_pad->window->lines_on_screen;
         }
#endif

      if (dspl_descr->show_lineno)
         setup_lineno_window(dspl_descr->main_pad->window,                        /* input  */
                             dspl_descr->title_subarea,                           /* input  */
                             dspl_descr->dminput_pad->window,                     /* input  */
                             (dspl_descr->pad_mode ? dspl_descr->unix_pad->window : NULL),    /* input  */
                             dspl_descr->lineno_subarea);                         /* output  */

      if (main_subarea(dspl_descr))
         break; /* returns true if a window was made too small */

      /***************************************************************
      *  If scroll bars are on, resize them as well.
      ***************************************************************/
      if (dspl_descr->sb_data->vert_window_on || dspl_descr->sb_data->horiz_window_on)
         resize_sb_windows(dspl_descr->sb_data, dspl_descr->main_pad->window, dspl_descr->display);

#ifdef PAD
      /* RES 11/12/97  Last test in following if  added to avoid window jump when scroll turns off horizontal scrollbar */
      if (dspl_descr->pad_mode && !event_union.xconfigure.send_event)
         {
            if (old_lines_on_screen + dspl_descr->main_pad->first_line + get_background_work(BACKGROUND_SCROLL) >= total_lines(dspl_descr->main_pad->token))
               { /* RES 9/26/95 only do the scroll some if we did the pad to bottom */
                  pad_to_bottom(dspl_descr->main_pad);
                  /***************************************************************
                  *  We are at the bottom, no need to scroll any more.
                  ***************************************************************/
                  if (dspl_descr->scroll_mode)
                     scroll_some(NULL, dspl_descr, 0);
               }
         }

#endif


      /***************************************************************
      *  If the lines on the window is greater than the number of
      *  lines on the display, resize the winlines data for the main pad.
      ***************************************************************/
      setup_winlines(dspl_descr->main_pad);

#ifdef PAD
      /***************************************************************
      *  In pad mode, tell the shell, how big the window is for vi and such.
      ***************************************************************/
      if (dspl_descr->pad_mode)
         {
            if (dspl_descr->vt100_mode)
               vt100_resize(dspl_descr->main_pad);
            if (window_resized || event_union.xany.send_event) /* 10/1/97, can when going into vt100 mode */
               vi_set_size(shell_fd, dspl_descr->vt100_mode,
                           dspl_descr->main_pad->window->lines_on_screen+1, /* number of lines on screen (include unix cmd window line, RES 4/8/94) */
                           dspl_descr->main_pad->window->sub_width / dspl_descr->main_pad->window->font->max_bounds.width,  /* number of cols on window */
                           dspl_descr->main_pad->window->sub_width,    /* number of horizontal pixels available */
                           dspl_descr->main_pad->window->sub_height);  /* number of verticle pixels */
         }

      /***************************************************************
      *  Figure out where to put the cursor.
      *  In pad mode, if the cursor is in the main window, put it in the
      *  Unix cmd window.  Otherwise, put it back where it was.
      *  Up to snuff and test added RES 3/28/94, to deal with a cursor jump, 
      *  redo_cursor_in_configure test added RES 5/15/94, to deal window moving;
      ***************************************************************/
      if ((!window_resized && in_window(dspl_descr) && !dspl_descr->redo_cursor_in_configure) || dspl_descr->first_expose)
         bring_up_to_snuff(dspl_descr);
         
      /* RES 10/20/97  Last test added to keep cursor from going to UNIX window when scroll bar turns on */
      if (dspl_descr->pad_mode && !(dspl_descr->vt100_mode) && (dspl_descr->cursor_buff->which_window == MAIN_PAD) && !event_union.xconfigure.send_event)
            dm_position(dspl_descr->cursor_buff, dspl_descr->unix_pad, dspl_descr->unix_pad->file_line_no, dspl_descr->unix_pad->file_col_no);
      else
         if (!dspl_descr->cursor_buff->current_win_buff->formfeed_in_window || (dspl_descr->cursor_buff->current_win_buff->formfeed_in_window > dspl_descr->cursor_buff->win_line_no)) 
            dm_position(dspl_descr->cursor_buff, dspl_descr->cursor_buff->current_win_buff, dspl_descr->cursor_buff->current_win_buff->file_line_no, dspl_descr->cursor_buff->current_win_buff->file_col_no);

#else
      if (!dspl_descr->cursor_buff->current_win_buff->formfeed_in_window || (dspl_descr->cursor_buff->current_win_buff->formfeed_in_window > dspl_descr->cursor_buff->win_line_no)) 
         dm_position(dspl_descr->cursor_buff, dspl_descr->cursor_buff->current_win_buff, dspl_descr->cursor_buff->current_win_buff->file_line_no, dspl_descr->cursor_buff->current_win_buff->file_col_no);
#endif
      /***************************************************************
      *  Force a full redraw of the screen - all windows.
      *  Make sure the cursor is drawn to the new window location if
      *  this was a geo command being issued.
      ***************************************************************/
      if (dspl_descr->redo_cursor_in_configure)
         force_in_window(dspl_descr); /* in window.c, allows warp in process redraw, even if cursor is not in the window */
      process_redraw(dspl_descr, FULL_REDRAW, dspl_descr->redo_cursor_in_configure); /* RES 1/17/94 switch warp flag from True */
      if (!dspl_descr->redo_cursor_in_configure && in_window(dspl_descr)) /* RES 3/28/94 If not warping cursor, redraw it */
         fake_cursor_motion(dspl_descr);
      dspl_descr->redo_cursor_in_configure = False;
      reset_geo_parm(dspl_descr->main_pad->window, &GEOMETRY);
      break;

   /***************************************************************
   *
   *  If we get a MappingNotify event, the keyboard mapping has
   *  changed.  We call XRefreshKeyboardMapping to fix some fields
   *  in the Display structure.  This is one of the "Nobrainers"
   *  that the instructor talked about.
   *
   ***************************************************************/
   case MappingNotify:
      DEBUG9(XERRORPOS)
      XRefreshKeyboardMapping((XMappingEvent *)&event_union);
      break;

   /***************************************************************
   *
   *  If we get a VisibilityNotify event, the window visibilty has
   *  changed.  Something has been obscurred or un obscurred.
   *
   ***************************************************************/
   case VisibilityNotify:
      if (event_union.xvisibility.state == VisibilityUnobscured)
         raised_window(dspl_descr);
      else
         obscured_window(dspl_descr);
      if (event_union.xvisibility.window == dspl_descr->main_pad->x_window)
         {
            if (event_union.xvisibility.state == VisibilityFullyObscured)
               force_out_window(dspl_descr);
            if ((event_union.xvisibility.state == VisibilityFullyObscured) && (dspl_descr->x_pixmap != None))
               {
                  DEBUG9(XERRORPOS)
                  XFreePixmap(dspl_descr->display, dspl_descr->x_pixmap);
                  DEBUG1(fprintf(stderr, "Pixmap 0x%X freed due to VisibilityFullyObscured of main window\n", dspl_descr->x_pixmap);)
                  dspl_descr->x_pixmap = None;
               }
            else
               {
                  if (dspl_descr->x_pixmap == None)
                     {
                        redraw_needed = realloc_pixmap(dspl_descr->main_pad->window, dspl_descr->display, &dspl_descr->x_pixmap, dspl_descr->main_pad->x_window);
                        process_redraw(dspl_descr, redraw_needed, False /* no warp */);
                     }
               }
         }
      break;

   /***************************************************************
   *
   *  If we get a SelectionClear event, some other program has 
   *  requested ownership of the X 
   *
   ***************************************************************/
   case SelectionClear:
      if (event_union.xselectionclear.selection != dspl_descr->properties->ce_cclist_atom)
         clear_pastebuf_owner(&event_union);
      break;

   /***************************************************************
   *
   *  If we get a SelectionRequest event, some other program has 
   *  requested transfer of the data in the unnamed paste buffer.
   *  Format the data, send it, and signal that it has been sent.
   *
   ***************************************************************/
   case SelectionRequest:
      send_pastebuf_data(&event_union);
      break;

   /***************************************************************
   *
   *  If we get a SelectionRequest event, some other program has 
   *  requested transfer of the data in the unnamed paste buffer.
   *  Format the data, send it, and signal that it has been sent.
   *
   ***************************************************************/
   case PropertyNotify:
      DEBUG24(dump_atom(&event_union);)
      if (event_union.xproperty.atom == dspl_descr->properties->cekeys_atom)
         get_changed_keydef(&event_union, dspl_descr->properties, dspl_descr->hsearch_data);
      else
         if (event_union.xproperty.atom == dspl_descr->properties->ce_cc_line_atom)
            {
               flush(dspl_descr->main_pad);
               redraw_needed = cc_get_update(&event_union,
                                             dspl_descr,
                                             &warp_needed);
               if (redraw_needed || warp_needed)
                  process_redraw(dspl_descr, redraw_needed, warp_needed);
            }
         else
            if (event_union.xproperty.atom == dspl_descr->properties->ce_cc_atom)
               {
                  /***************************************************************
                  *  This property is set by another copy of cc in routine 
                  *  cc_check_other_windows when it determines that we need a cc
                  *  instead of opening a new ce session.  cc_receive_cc_request
                  *  returns 0 if the request was successful.
                  ***************************************************************/
                  cc_receive_cc_request(&event_union, dspl_descr, edit_file, resource_class, argv[0]);
               }
            else
               if (event_union.xproperty.atom != dspl_descr->properties->ce_cclist_atom)
                  more_pastebuf_data(&event_union); /* actually returns True if event was used, otherwise we can do something else with it. */
      break;

   /***************************************************************
   *
   *  If we get a ClientMessage, It may be the SAVE_YOURSELF message
   *  from the window manager.  If it is, do a pad write if we
   *  are in edit mode and the file is dirty.  Then confirm that
   *  we are ready with a zero length append to the XA_WM_COMMMAND property.
   *
   ***************************************************************/
   case ClientMessage:
      if (event_union.xclient.message_type == dspl_descr->properties->wm_protocols_atom)
         {
            if ((unsigned)*event_union.xclient.data.l == dspl_descr->properties->save_yourself_atom)
               {
                  DEBUG(fprintf(stderr, "\nSAVE_YOURSELF client message received\n");)
                  if (!dspl_descr->pad_mode)  /* no saving done in pad mode */
                     if (dirty_bit(dspl_descr->main_pad->token))
                        dm_pw(dspl_descr, edit_file, False);

                  save_restart_state(dspl_descr,   /* in init.c */
                                     edit_file,
                                     argv[0]);
               }
            if ((unsigned)*event_union.xclient.data.l == dspl_descr->properties->delete_window_atom)
               {
                  DMC_wc  fake_window_close;
                  memset(&fake_window_close, 0, sizeof(DMC_wc));
                  fake_window_close.next   = NULL;
                  fake_window_close.cmd    = DM_wc;
                  fake_window_close.temp   = False;
                  fake_window_close.type   = 'w';
                  DEBUG(fprintf(stderr, "\nDELETE_WINDOW client message received\n");)
                  flush(dspl_descr->main_pad);
                  redraw_needed |= dm_wc((DMC *)&fake_window_close,
                                         dspl_descr,
                                         edit_file,
                                         NULL,
                                         True,          /* no prompt */
                                         MAKE_UTMP);
                  /* does not return when  passed -f unless more displays are open */
               }
         }
      else
         if (event_union.xclient.message_type == dspl_descr->properties->ce_cclist_atom)
            send_api_stats(dspl_descr->main_pad, &event_union);
      break;

   /***************************************************************
   *
   *  If we get a focus in event.  record this fact with routine
   *  focus change.  Then make sure the cursor is in the window and
   *  
   *
   ***************************************************************/
   case FocusOut:
      /* 10/12/2004 RES, clear the new drag_scrolling flag */
      if (dspl_descr->drag_scrolling)
         {
            DEBUG23(
                fprintf(stderr, "Focus loss stopping scrolling\n");
            )
            dspl_descr->drag_scrolling  = False;
            redraw_needed |= build_in_arrow_rlse(dspl_descr);
         }

      /* 4/15/1999 RES, change for Ken Banas on Ultra 10.  Getting
         extra Focus events compared to other nodes.  The extra
         events are not NotifyNormal */
      if ((event_union.xfocus.mode == NotifyUngrab) || (event_union.xfocus.mode == NotifyGrab))
         break;
      if (dspl_descr->pd_data->pulldown_active)
         pd_remove(dspl_descr, 0);
      /*FALLTHRU*/

   case FocusIn:
      /* 4/15/1999 RES, change for Ken Banas on Ultra 10.  Getting
         extra Focus events compared to other nodes.  The extra
         events are not NotifyNormal */
      if ((event_union.xfocus.mode == NotifyUngrab) || (event_union.xfocus.mode == NotifyGrab))
         break;
      focus_change(&event_union, dspl_descr);
      if (event_union.type == FocusIn)
         fake_cursor_motion(dspl_descr);
      else
         erase_text_cursor(dspl_descr);

      /***************************************************************
      *  02/17/2005  - RES, add check for file change on focus in.
      ***************************************************************/
      if ((dspl_descr->pad_mode == False) && (dspl_descr->reload_off == False) && (file_has_changed(edit_file)))
         {
            DMC             *prompt;
            strcpy(msg, "reload -&'File externally changed, reload? ' ");
            prompt = prompt_prescan(msg, True, dspl_descr->escape_char);  /* in kd.c */
            if (prompt != NULL)
               {
                  ce_XBell(dspl_descr, 0);

                  redraw_needed = dm_prompt(prompt, dspl_descr);
                  if (redraw_needed)
                     process_redraw(dspl_descr, redraw_needed, False);
               }
         }
      break;

   /***************************************************************
   *
   *  Someone pressed a key.  Decide what to do with it.
   *  The do while loop allows us to process several pent up
   *  keystrokes without redrawing the screen.  For example, if the
   *  user presses page down three times rapidly in succession,
   *  we will move the current line pointer down three times before
   *  the screen is redrawn.  This can be done because of the
   *  batching of events sent from the server to the program.
   *
   *  The scroll bars are treated as a special case.
   *
   *  <find marker>
   *
   *
   ***************************************************************/
   case ButtonPress:
   case ButtonRelease:
   case KeyRelease:
   case KeyPress:
      /* RES 10/11/2004 Keep track of what buttons are down */
      if (event_union.type == ButtonPress)
         dspl_descr->button_down |= (1 << event_union.xbutton.button);
      else
      if (event_union.type == ButtonRelease)
         dspl_descr->button_down &= ~(1 << event_union.xbutton.button);

      if (dspl_descr->expecting_focus_mouseclick)
         {
            if (event_union.type == ButtonPress)
               break;
            if (event_union.type == ButtonRelease)
               {
                  dspl_descr->expecting_focus_mouseclick = False;
                  break;
               }
         }

      if ((event_union.xany.window == dspl_descr->sb_data->vert_window) ||
          (event_union.xany.window == dspl_descr->sb_data->horiz_window))
         {
            if ((event_union.type == ButtonPress) || (event_union.type == ButtonRelease))
               {
                  if (event_union.xany.window == dspl_descr->sb_data->vert_window)
                     redraw_needed = vert_button_press(&event_union, dspl_descr);
                  else
                     redraw_needed = horiz_button_press(&event_union, dspl_descr);

                  if (redraw_needed)
                     process_redraw(dspl_descr, redraw_needed, False);  /* redo the pointer move */
               }
#ifdef PAD
            else
               if (dspl_descr->pad_mode)
                  {
                     DEBUG5(fprintf(stderr, "Moving cursor to MAIN window for key %s\n", ((event_union.type == KeyPress) ? "KeyPress" : "KeyRelease"));)
                     dspl_descr->cursor_buff->which_window = MAIN_PAD;
                     dspl_descr->cursor_buff->win_line_no  = dspl_descr->main_pad->file_line_no - dspl_descr->main_pad->first_line;
                     dspl_descr->cursor_buff->win_col_no   = dspl_descr->main_pad->file_col_no  - dspl_descr->main_pad->first_char;
                     bring_up_to_snuff(dspl_descr);
                     set_window_col_from_file_col(dspl_descr->cursor_buff);
                     dspl_descr->cursor_buff->y = dspl_descr->cursor_buff->current_win_buff->window->sub_y + (dspl_descr->cursor_buff->win_line_no * dspl_descr->cursor_buff->current_win_buff->window->line_height);

                     keypress(dspl_descr,
                              &event_union,
                              read_locked,
                              edit_file,
                              argv[0]);
                  }
#endif
         }
      else
         keypress(dspl_descr,
                  &event_union,
                  read_locked,
                  edit_file,
                  argv[0]);
   }     /* end switch on event_union.type */
}     /* end while 1  */

#if defined(__lint) || defined(WIN32)
return(0); /* keeps lint happy */
#endif

}  /* end main   */


/************************************************************************

NAME:      realloc_pixmap - reallocate the pixmap in case it was deleted.

PURPOSE:    This routine gets called when the the file becomes unobscured
            or iconned and the pixmap has been deleted to save space in 
            the server.

PARAMETERS:
   1.  pix         -  pointer to DRAWABLE_DESCR (INPUT / OUTPUT)
                      This is the drawable description for the pixmap which backs the
                      main window.

   2.  x_pixmap     -  pointer to Pixmap (Xlib type) (INPUT/OUTPUT)
                       This is the area to return the pixmap in.  It must have the
                      value None (Xlib define) to show it does not currently exist.

   3.  main_window  -  Window (Xlib type) (INPUT)
                       This is the main window.  It is needed to create the pixmap.


FUNCTIONS :

   1.   If the pixmap drawable is not null, just return.

   2.   Create the pixmap to the size specified in the drawable description.


RETURNED VALUE:
   redraw_needed - The redraw needed bitmask is returned.  This is always a full
                   redraw if the pixmap was created.

*************************************************************************/

static int realloc_pixmap(DRAWABLE_DESCR   *pix,
                          Display          *display,
#ifdef WIN32
                          Drawable         *x_pixmap,
#else
                          Pixmap           *x_pixmap,
#endif
                          Window            main_window)
{

if (*x_pixmap != None)
   {
      DEBUG1(fprintf(stderr, "realloc_pixmap:  Pixmap already exists\n");)
      return(0);
   }

DEBUG9(XERRORPOS)
*x_pixmap = XCreatePixmap(display,
                          main_window,
                          pix->width,
                          pix->height,
                          DefaultDepth(display, DefaultScreen(display)));

DEBUG1(fprintf(stderr, "realloc_pixmap:  Pixmap 0x%X allocated %dx%d\n", *x_pixmap, pix->width, pix->height);)

return(FULL_REDRAW);


} /* end of realloc_pixmap */


/************************************************************************

NAME:      reset_geo_parm - reset the value in the geometry parm value from
                            the current geometry.

PURPOSE:    This routine gets called for configure events to save the current
            window geometry in the parm value.  It is used from here by
            save_restart_state (in init.c) to tell the session manager how
            to restart the window.

PARAMETERS:
   1.  main_window  -  pointer to DRAWABLE_DESCR (INPUT)
               This is the drawable description for the main window.  It is needed
               to get the size information.

   2.  option_value  -  pointer to pointer to char (INPUT/OUTPUT)
               This is the address of the pointer which points to the geometry
               parm in the parameter values array.


FUNCTIONS :
   1.   Build the geometry string in pixels from the main window description.

   2.   Realloc the storage for the parm value if the string has gotten longer.

   3.   Copy the new geometry to the parm value.


*************************************************************************/

static void reset_geo_parm(DRAWABLE_DESCR   *main_window,
                           char            **option_value)
{
char            msg[80];

snprintf(msg, sizeof(msg), "%dx%d%+d%+d", main_window->width, main_window->height, main_window->x, main_window->y);

if (*option_value)
   free(*option_value);
*option_value = malloc_copy(msg);

} /* end of reset_geo_parm */


static void check_differentials(int *m)
{
int checksum = 0;
#ifdef CE_LITTLE_ENDIAN
#define STD_CHECKSUM 37
#else
#define STD_CHECKSUM 22
#endif

while(*m)
  checksum += *m++ % 13;

if (checksum != STD_CHECKSUM)
   exit(32);

}


#ifdef DebuG
/************************************************************************

NAME:      dump_atom - Debugging routine - dump atom name

PURPOSE:    This routine is called for PropertyNotify events.
            We get the name of the property which changed and dump it's
            new value.

PARAMETERS:
   1.  event -  pointer to XEvent (INPUT)
                This is the PropertyNotify event to process.

FUNCTIONS :
   1.   Retrieve the property mentioned in the event.

   2.   Retrieve the name of the property.

   3.   Dump the relevant property information.

   4.   If the property has data, produce a hex dump of the data.

*************************************************************************/

static void dump_atom(XEvent  *event)
{
Atom              actual_type;
int               actual_format;
int               keydef_property_size;
unsigned long     nitems;
unsigned long     bytes_after;
char             *buff = NULL;
char             *p;
char             *atom_name;

DEBUG9(XERRORPOS)
XGetWindowProperty(event->xproperty.display,
                   event->xproperty.window,
                   event->xproperty.atom,
                   0, /* offset zero from start of property */
                   INT_MAX / 4,
                   False, /* do not delete the property */
                   AnyPropertyType,
                   &actual_type,
                   &actual_format,
                   &nitems,
                   &bytes_after,
                   (unsigned char **)&buff);

keydef_property_size = nitems * (actual_format / 8);

DEBUG9(XERRORPOS)
atom_name = XGetAtomName(event->xproperty.display, event->xproperty.atom);
if (actual_type != None)
   p = XGetAtomName(event->xproperty.display, actual_type);
else
   p = "None";
fprintf(stderr, "dump_atom:  Name=\"%s\", Window=0x%X, Size=%d, actual_type=%d (%s), actual_format=%d, nitems=%d, &bytes_after=%d\n",
                atom_name, event->xproperty.window, keydef_property_size, actual_type, p,
                actual_format, nitems, bytes_after);
if (actual_type != None)
   XFree(p);
XFree(atom_name);

if (keydef_property_size > 0)
   hexdump(stderr, atom_name, buff, keydef_property_size, 0);

XFree(buff);

} /* end of dump_atom */

#endif

#ifdef Encrypt
void dm_crypt(DMC   *dmc)
{
   if (dmc->crypt.parm != NULL)
      {
         free(OPTION_VALUES[ENCRYPT_IDX]);
         if (dmc->crypt.parm[0] != '\0')
            {
               OPTION_VALUES[ENCRYPT_IDX] = CE_MALLOC(strlen(dmc->crypt.parm)+1);
               if (OPTION_VALUES[ENCRYPT_IDX] == NULL)
                  return;
               strcpy(OPTION_VALUES[ENCRYPT_IDX], dmc->crypt.parm);
               encrypt_init(OPTION_VALUES[ENCRYPT_IDX]);

            }
         else
            OPTION_VALUES[ENCRYPT_IDX] == NULL;
      }
   else
      dm_error("(crypt) missing key", DM_ERROR_BEEP);
}  /* end dm_crypt */

#endif


/************************************************************************

NAME:      process_dash_e_parm - Process the parms collected after -e

PURPOSE:    The -e parm is followed by a command string which is to be
            executed in the ceterm.  Everything past -e is NOT a ceterm
            paramter and needs to be clumped into one big parameter.

PARAMETERS:
   1.  argc        -  pointer to int (INPUT / OUTPUT)
                      Argc passed in from main.  Value modified if -e is found.

   2.  argv        -  pointer to array of ponter to char (INPUT / OUTPUT)
                      argv passed in from main.  Values modified if -e is found.

FUNCTIONS :
   1.   Scan the parm list for the -e or -E parameter.  If found
        figure out how much space all the followng parms occupy.
        Include space for blanks between parms and spaces.

   2.   If -e was found, malloc space for all the parms and copy the
        parms following -e to the malloced space.

   3.   Replace the -e parameter with the new combined parm.

   4.   Adjust argc to account for the parameter change.

*************************************************************************/

static void process_dash_e_parm(int  *argc, char *argv[])
{
int                   i;
int                   dash_e_idx = -1;
int                   malloc_len = 0;
char                 *new_arg;

/***************************************************************
*  Look at the parm list for the -e argument.  Just the -e
*  by itself.  If found, everthing past it gets grouped into
*  one big argument.  Thus we collect the lengths.
***************************************************************/
for (i = 1; i < *argc; i++)
{
if (dash_e_idx > 0)
   {
      malloc_len += strlen(argv[i]) + 1;
      if (strchr(argv[i], ' ') != NULL)
         malloc_len += 2; /* We will need to add quotes */
   }
else
   if ((strcmp(argv[i], "-e") == 0) || (strcmp(argv[i], "-E") == 0))
      {
         dash_e_idx = i;
      }
} /* first for loop */

/***************************************************************
*  If we found something, get space for the new parm and collect
*  all the arguments past the -e into this argument and then
*  replace the -e argument with the new string.  If there were
*  blanks in the arguments, we will quote the arguments.
***************************************************************/
if ((dash_e_idx >= 0) && (malloc_len > 0))
   {
      new_arg = (char *)malloc(malloc_len + 1);
      *new_arg = '\0';
      for (i = dash_e_idx+1; i < *argc; i++)
      {
         if (strchr(argv[i], ' ') != NULL)
            sprintf(&new_arg[strlen(new_arg)], "\"%s\" ", argv[i]);
         else
            sprintf(&new_arg[strlen(new_arg)], "%s ", argv[i]);
      }

      new_arg[strlen(new_arg)-1] = '\0'; /* kill off the last blank */
      argv[dash_e_idx] = new_arg;
      *argc = dash_e_idx +1;
   }

} /* end of process_dash_e_parm */


#ifdef WIN32
/************************************************************************

NAME:      dash_term_present - Scan windows parm list for term option

PURPOSE:    This routine gets called on the windows platform  It scans
            the parm list to see if the xterm styl -e or -E parm has been
            passed for the ceterm option -TERM or -term.

PARAMETERS:
   1.  argc        -  int (INPUT)
                      Argc passed in from main.

   2.  argv        -  pointer to array of ponter to char (INPUT)
                      argv passed in from main.

FUNCTIONS :
   1.   Scan the parm list for the values indicating that this is a term and
        return true if this is the case.

RETURNED VALUE:
   dash_term_present - int
                       True  - terminal parm found
                       False - terminal parm NOT found

*************************************************************************/

static int dash_term_present(int  argc, char *argv[])
{
int                   i;

for (i = 1; i < *argc; i++)
{
   if ((strcmp(argv[i], "-e") == 0) || (strcmp(argv[i], "-E") == 0))
      return(False);
   if ((strcmp(argv[i], "-term") == 0) || (strcmp(argv[i], "-TERM") == 0))
      return(True);
}
return(False);
} /* end of dash_term_present */
#endif


#ifndef WIN32
/************************************************************************

NAME:      setup_signal_handlers - Tell Unix to catch signals

PURPOSE:    This routine gets called on UNIX machines from the main routine.
            It sets up signal handlers to catch signals so we can create
            a crach file.

PARAMETERS:
   None

FUNCTIONS :
   1.   Set up catch_quit to be called for most signals.  Catch quit creates the
        crash file.

*************************************************************************/

static void setup_signal_handlers(void)
{
int                   i;
void                (*state)();

#ifndef NSIG
#define NSIG 128
#endif

signal(SIGHUP, catch_quit);
signal(SIGTERM, catch_quit);
signal(SIGINT,  catch_quit);
#ifdef SIGQUIT
signal(SIGQUIT, catch_quit);
#endif 
if (debug == 0)
   {
      signal(SIGSEGV, catch_quit);
#ifdef SIGBUS
      signal(SIGBUS, catch_quit);
#endif 
#ifdef SIGXCPU
      signal(SIGXCPU, catch_quit);
#endif 

      for (i=1; i < NSIG; i++)
      {
          errno = 0;
          if ((i == SIGCHLD) ||
              (i == SIGHUP) ||
              (i == SIGTERM) ||
              (i == SIGINT) ||
              (i == SIGSEGV) ||
#ifdef SIGEMT
              (i == SIGQUIT) ||
#endif
#ifdef SIGXCPU
              (i == SIGXCPU) ||
#endif
#ifdef SIGBUS
              (i == SIGBUS) ||
#endif
#ifdef SIGCONT
              (i == SIGCONT) ||
#endif
#ifdef SIGVTALRM
              (i == SIGVTALRM) ||
#endif
#ifdef SIGTTIN
              (i == SIGTTIN) ||
#endif
#ifdef SIGTTOU
              (i == SIGTTOU) ||
#endif
#ifdef SIGALRM
              (i == SIGALRM) ||
#endif
#ifdef SIGPIPE
              (i == SIGPIPE) ||
#endif
              (i == SIGWINCH)) continue;
      
          state = signal(i, catch_quit);
          if (errno == EINVAL) continue;
          if ((state != SIG_DFL) || (state == SIG_IGN)) signal(i, state); /* reset it */
      }  /* for */

   }
} /* end of setup_signal_handlers */
#endif

