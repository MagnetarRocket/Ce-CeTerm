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
*  Email:  styma@cox.net
*  
***************************************************************/

/***************************************************************
*
*  module cc.c
*
*  This module provides the cc (carbon copy) DM command and the routines
*  it detect, identify, and syncronize the multiple copies of the editor
*  open on the same file.
*
*  The basic idea is that a cc open the display a second time and creates
*  a new set of windows.  The main window in each case is backed by the
*  same memdata database.  Other windows are independent.
*  A separate display description is used for each set of windows.  
*  
*  If someone types ce on a file from the command line for a file that is already
*  open, routine initial_file_open calls the check other window routine to see
*  if there is a ce session already open on this file.  If so, a cc request is
*  sent to the window which already has the file open.
*
*  The information about what file a ce edit session has open, and about what
*  control terminal and shell processes a ceterm session has open is put in a
*  resource hung off the main window.  The main windows on the screen are scanned
*  for this information.
*
*  Routines:
*     cc_check_other_windows -   check other windows to see if they are open on this file.
*     cc_set_pad_mode        -   Set the cc configuration to pad mode.
*     cc_register_file       -   register a file with the X server.
*     dm_cc                  -   perform the cc command
*     cc_shutdown            -   Remove the cclist property from the WM window
*     cc_get_update          -   process an update sent via property change from another window
*     cc_plbn                -   send a line as a result of a PutLine_By_Number
*     cc_ca                  -   Tranmit color impacts to other displays
*     cc_dlbn                -   Tell the other windows to delete a line  (Delete Line By Number)
*     cc_joinln              -   Flush cc windows prior to a join
*     cc_titlebar            -   Tell the other windows about a titlebar change
*     cc_line_chk            -   Check if a line to be processed is already in use
*     cc_typing_chk          -   Update typing on a CC screens
*     cc_padname             -   Tell the other windows about a pad_name command
*     cc_ttyinfo_send        -   Send the slave tty name in pad mode to the main process
*     cc_ttyinfo_receive     -   Receive the slave tty name from cc_ttyinfo_send and save in the CC_LIST
*     ce_gethostid           -   Get the Internet host id
*     cc_xdmc                -   Send a dm command to a window.
*     cc_debug_cclist_data   -   Print this windows cc_list info
*     ccdump                 -   Print the cc_list (called from xdmc.c)
*     cc_to_next             -   Move to the next window.
*     cc_icon                -   Flag the window iconned or not iconned
*     cc_is_iconned          -   Check if the window is iconned
*     cc_obscured            -   Mark a window obscurred or non-obscurred
*     cc_receive_cc_request  -   Process a request to cc sent via property change from another window
*     send_api_stats         -   Send current stats to the ceapi
*     dm_delcc               -   Delete the cc list
*
* Internal:
*     get_cclist_data        -   Read the CC list property for this window
*     put_cclist_data        -   Write the CC list property for this window
*     chk_cclist_data        -   Validate that a window manager window has good cclist data
*     child_of_root          -   Find the window which is a child of the root window
*     check_atoms            -   Make sure we have all the atoms we need to get.
*     adjust_tty_inode       -   Special manipulation on the tty inode
*     dumpcc                 -   Dump all the cclist data hung off the main window.
*     dump_saved_cmd         -   Dump the window manager saved command and machine
*     dump_cc_data           -   Dump the sessions manager saved command
*
*
* OVERVIEW of CC operations:
*     When Ce comes up it checks to see if there is another edit session open on 
*     this file (details later).  If this is false, as part of start up it registers
*     the file in a property hung off the immeditate child of the root window named
*     which contains the main window.  The property's name is CE_CCLIST.  The 
*     format of the property is described in structure CC_LIST.
*
*     If Ce checks the window list and finds this file is being edited by another Ce
*     Ce session, it sets a property on the first window which has the file open to
*     request a cc. The value put into
*     the property is the cc command which would be entered to achieve the desired result.
*
*
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h   /usr/include/sys/errno.h      */
#include <ctype.h>          /* /usr/include/ctype.h      */
#include <stdlib.h>
#include <sys/types.h>      /* /usr/include/sys/types.h */
#ifdef OMVS
typedef  unsigned short ushort;
#endif
#include <sys/stat.h>       /* /usr/include/sys/types.h */
#include <time.h>
#ifndef WIN32
#include <netinet/in.h>     /* /usr/include/netinet/in.h */
#include <unistd.h>         /* /usr/include/unistd.h */
#ifndef OMVS
#include <arpa/inet.h>      /* /usr/include/arpa/inet.h    */
#endif
#include <sys/socket.h>     /* /usr/include/sys/socket.h */
#include <netdb.h>          /* /usr/include/netdb.h      */
#include <fcntl.h>          /* /usr/include/fcntl.h */
#else
#include <winsock.h>
#include <time.h>
#include <process.h>
#endif


#include <signal.h>         /* /usr/include/signal.h */
#include <limits.h>         /* /usr/include/limits.h     */
#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif


#ifdef WIN32
#define GETPGRP(a) _getpid()
#define GETPPID(a) _getpid()

#define getuid() -1

#else
#define GETPPID(a)  getppid()
#define GETPGRP(a)  getpgrp()
#endif


/***************************************************************
*  
*  PGM_XDMC is defined in xdmc.c which includes this entire
*  .c file.
*  
***************************************************************/


#ifndef PGM_XDMC
#include "apistats.h"
#endif
#include "cc.h"
#include "netlist.h"
#ifndef PGM_XDMC
#include "dmwin.h"
#include "dmsyms.h"
#include "display.h"
#include "emalloc.h"
#include "getevent.h"
#include "getxopts.h"
#include "hsearch.h"
#include "ind.h"
#include "init.h"
#include "kd.h"      /* needed for HASH_SIZE */
#include "mvcursor.h"
#include "pad.h"
#include "parms.h"
#include "parsedm.h"
#include "pastebuf.h"
#include "pw.h"      /* needed for STDIN_FILE_STRING */
#include "redraw.h"
#include "sendevnt.h"
#include "serverdef.h"
#include "txcursor.h"
#include "typing.h"   /* needed for flush */
#include "undo.h"
#include "window.h"
#include "windowdefs.h"
#include "winsetup.h"
#endif
#include "xerror.h"
#include "xerrorpos.h"
#include "hexdump.h"

/***************************************************************
*  
*  When compiled for xdmc we do not use the fancy malloc nor
*  do we use dm_error.
*  
***************************************************************/

#ifdef PGM_XDMC
#define CE_MALLOC(size) malloc(size)
#define dm_error(m,j) fprintf(stderr, "%s\n", m)

#define CE_PROPERTY(a)   a
static Atom  ce_cc_line_atom = 0;
static Atom  ce_cc_atom = 0;
static Atom  ce_cclist_atom = 0;
static Atom  ce_cc_visibility_atom = 0;

#else
#define CE_PROPERTY(a)   dspl_descr->properties->a
#endif

#if !defined(IBMRS6000) && !defined(linux)
void   sleep(int secs);
void   usleep(int usecs);
#endif

/*void   srand(int seed);  remove for freebsd, can't see where this is needed */
/*int    rand(void);       remove for freebsd, can't see where this is needed */

#ifndef WIN32
char  *ttyname(int fd);
#endif

/***************************************************************
*  
*  Structure used to describe the CC_LIST property saved in the
*  server.  These are all 4 type (32 bit) quantities, we let
*  the X server do the big endian/little endian conversions.
*
*  Window_lcl is defined for the DEC Alpha where Window is 8
*  bytes long but only the low 4 are used.
*  
***************************************************************/

#ifdef WIN32
#define    Window_lcl Window
#else
typedef unsigned int    Window_lcl;
#endif

#define CC_LIST_VERSION 1
/* Value of CC_LIST_VERSION is 0 */

typedef struct {
   unsigned int       dev;                    /* device id for the file               */
   unsigned int       inode;                  /* inode number for the file            */
   unsigned int       host_id;                /* 4 part internet id of the host       */
   unsigned int       uid;                    /* uid of the person opening the file   */
   Window_lcl         main_window;            /* main window for the session          */
   int                shpid;                  /* pid of shell run from a ceterm       */
   int                shgrp;                  /* pgrp of shell run from a ceterm      */
   int                pgrp;                   /* process group id for pid             */
   int                cepid;                  /* pid of ce or ceterm                  */
   int                obscured;               /* True if window is partially or fully obscured */
   int                iconned;                /* True if window is unmapped, iconned  */
   unsigned short     version;                /* CC_LIST structure version number = 1 */
   unsigned short     display_no;             /* Display number                       */
   Window_lcl         wm_window;              /* window manager window for the session*/
   unsigned int       create_time;            /* Create time for file                 */
   unsigned int       file_size;              /* Current disk file size               */

} CC_LIST;

/***************************************************************
*  
*  Structure used to send a put line to other windows.
*  total_lines_infile is used to make deletes are idempotent.
*
*  Ints are stored in the server using htonl (network order).
*  
***************************************************************/

typedef struct {
   int                msg_len;                /* len of this message, is padded to mult of 4     */
   int                lineno;                 /* line to be replaced, deleted, or inserted after */
   short int          type;                   /* one of the below constants (CMD_XDMC or CMD_TOWINDOW)*/
   short int          count;                  /* count of lines to be deleted for DELLINE (always 1) */
   int                total_lines_infile;     /* total lines for the file at this time.          */
   char               line[4];                /* the array                                       */
} CC_MSG;


#define CMD_XDMC          1
#define CMD_TOWINDOW      2
#define CMD_PING          3



/***************************************************************
*  
*  List of cc windows known to this routine.  Includes current session.
*  
***************************************************************/

#define MAX_CC_WINDOWS 100
#define MAX_CCLIST_TRIES 3
#define MAX_CCLIST_LOOPS 30

#define PAD_MODE_DEV     -1
#define PAD_MODE_INODE   -1
#define PAD_MODE_HASH    -1

static unsigned int  saved_dev = (unsigned)PAD_MODE_DEV;
static unsigned int  saved_inode = (unsigned)PAD_MODE_INODE;
static unsigned int  saved_host_id = (unsigned)PAD_MODE_HASH;
static unsigned int  saved_create_time = 0;
static unsigned int  saved_file_size = 0;
/*static int           this_shpid; RES 6/27/95 get rid of lint msg */

#define MAX_CC_ARGS 256

#define MASK_4     (UINT_MAX & (UINT_MAX << 2))
#define ROUND4(n)  ((n + 3) & MASK_4)


/***************************************************************
*  
*  Atoms for communicating with other windows
*  CE_CC_LINE  is the property used to pass off line updates to 
*              other windows.  Properties are hung off CE windows.
*  CE_CC       is the property used to pass a cc request to
*              another window.  This is done during initialization.
*              This property is humg off the CE window of the 
*              session we are requesting to do a CC
*  CE_CCLIST   is used as a selection and a property to keep a list
*              of the CE edit sessions currently in use.  This
*              property is hung off the root window.
*  CE_CC_VISIBILITY  is exists on the main window for the ce
*              session.  It contains the flags showing whether the
*              window is iconned or obscured.
*
*  The atom declarations are held in the cc.h file since they are
*  needed in the main event loop.
*  
***************************************************************/

#define  CE_CC_LINE_ATOM_NAME           "CE_CC_LINE"
#define  CE_CC_ATOM_NAME                "CE_CC"
#define  CE_CCLIST_ATOM_NAME            "CE_CCLIST"
#define  CE_CC_VISIBILITY_ATOM_NAME     "CE_CC_VISIBILITY"


/***************************************************************
*  
*  System routines which have no built in defines
*  
***************************************************************/

char   *getenv(const char *name);
int    fork(void);
int    getpid(void);
#if !defined(_XOPEN_SOURCE_EXTENDED) && !defined(linux)
int    setpgrp(int pid, int pgrp);  /* bsd version */
#endif
int    isatty(int fd);
#if !defined(WIN32) && !defined(_XOPEN_SOURCE_EXTENDED) && !defined(linux)
int    gethostname(char  *target, int size);
#endif


/***************************************************************
*  
*  Local prototypes
*  
***************************************************************/


static CC_LIST  *get_cclist_data(Display       *display,
                                 Window         wm_window,
                                 int           *new); 

static void put_cclist_data(Display       *display,
                            Window         wm_window,
                            CC_LIST       *buff);

static int  chk_cclist_data(Display       *display,
                            Window         wm_window,
                            Window         main_window);

static Window  child_of_root(Display   *display,
                             Window     window);

static void check_atoms(Display *display);

#ifndef PGM_XDMC
static ino_t   adjust_tty_inode(char     *slave_tty_name,
                                ino_t     inode);
#endif


#if defined(DebuG) || defined(PGM_XDMC)
static void dump_cc_data(char   *buff,
                         int     buff_len,
                         Window  wm_window,
                         FILE   *stream);

#endif
#if defined(DebuG) || defined(PGM_XDMC)
static void dump_saved_cmd(Display *display,
                           Window   ce_window,
                           FILE    *stream);

#endif

#ifdef CE_LITTLE_ENDIAN
static void to_net_cc_list(CC_LIST   *buff,
                           int        len);
static void to_host_cc_list(CC_LIST   *buff,
                            int        len);
#endif


#ifndef PGM_XDMC
/************************************************************************

NAME:      cc_check_other_windows -   check other windows to see if they are open on this file.


PURPOSE:    This routine detects if the passed file is already open in another window
            and signals that window to do the cc.
            NOTE: This routine is always called first, before any other routines in this module.
                  The exception is pad mode, when this routine does not get called at all.

PARAMETERS:

   1.  stat_buff   - pointer to (struct stat) (INPUT)
                     This is a stat buff for the passed edit file.
                     It so happens that the routine with calls this routine
                     (intial_file_open), has already done a stat.  There is
                     no point in pretending we are Interleaf and doing extra stats.

   2.  dspl_descr  - pointer to DISPLAY_DESCR (INPUT)
                     This is the current display description.

   3.  writable    - int (INPUT)
                     This flag is true if the open it to be writable.

GLOBAL DATA:

saved_dev        -  int (OUTPUT)
                    The device number is saved globally

saved_inode      -  int (OUTPUT)
                    The inode number is saved globally


saved_host_id    -  int (OUTPUT)
                    The host net id is saved.

saved_create_time  -  int (OUTPUT)
                    The create_time from the stat buff is saved

saved_file_size  -  int (OUTPUT)
                    The file size from the stat buff is saved



FUNCTIONS :

   1.   Get the host name from the system.

   2.   Build the file key from the device and inode of the file and the
        hashed host name and the users uid.

   3.   Save the device, inode, create_time, file size, and host_id
        in the area global to this module.

   4.   Get the CC_LIST property off the root window of the server.  If it
        does not exist, we are done.

   5.   Scan the cc list to see if this file exists there.  If not, we are done.

   6.   If the file exist there but is opened under another uid, flag an error
        and return.  We will have to bail out.

   7.   Build the cc command into a string and attempt to append it to the
        CE_CC_REQUEST property on the window from the cc list.  Flush the X queue and
        reread the property.  This will trigger a bad window error which we
        trap (in xerror.c) and let us know the window has gone away.

   9.   If the window is good, sleep 4 seconds and reread the property.  The window
        list should have changed in size or content.  If not, the there is
        something wrong with the server?  Or could the window be icon'ed.
        This will have to be tried out.  If the list changes as expected,
        exit the program,  control has been passed to the other window.

RETURNED VALUE:

   If a match is found, this routine does not return.

   rc  -  return code
          0  -  No match found
         -1  -  Error detected, message already output.  must abort.


*************************************************************************/

int  cc_check_other_windows(struct stat   *file_stats,
                            DISPLAY_DESCR *dspl_descr,
                            int            writable)
{
int               i;
int               window_idx;
Atom              actual_type;
int               actual_format;
int               keydef_property_size;
unsigned long     nitems;
unsigned long     bytes_after;
char             *buff = NULL;
CC_LIST          *buff_pos;
char             *buff2 = NULL;
char              cmd[1024];
Window            root_return;
Window            parent_return;
Window           *child_windows = NULL;
unsigned int      nchildren;
char             *p;
int               argc;
char             *argv[256];
char              arg0[10];

/***************************************************************
*  
*  Make sure all the atoms are defined.
*  Save the device, inode, and hashed host name for the
*  file being edited.
*
*  We set this_window to the root window until cc_register_file
*  sets it to the window we will create.  So far we do not
*  have a window to use.  This value only gets used in the
*  case of bad_window errors.
*  
***************************************************************/

check_atoms(dspl_descr->display);
saved_dev     = file_stats->st_dev;
saved_inode   = file_stats->st_ino;
saved_host_id = ce_gethostid();
saved_create_time = file_stats->st_ctime;
saved_file_size   = file_stats->st_size;
DEBUG18(
   fprintf(stderr, "@cc_check_other_windows: file id:  0x%X 0x%X 0x%X 0x%X %d\n",
           saved_dev, saved_inode, saved_host_id, saved_create_time, saved_file_size);
)

/***************************************************************
*  
*  See if there is another window open on the file and try
*  to signal it if there is.
*  1.  Get the current list windows.
*  2.  Get the cclist property from each window and see if it matches.
*  
***************************************************************/

DEBUG9(XERRORPOS)
if (!XQueryTree(dspl_descr->display,     /* input */
                RootWindow(dspl_descr->display, DefaultScreen(dspl_descr->display)), /* input */
                &root_return,            /* output */
                &parent_return,          /* output */
                &child_windows,          /* output */
                &nchildren))             /* output */

   {
      dm_error("Could not get window list from server", DM_ERROR_BEEP);
      return(0);
   }

for (window_idx = 0; window_idx < (int)nchildren; window_idx++)
{
   /***************************************************************
   *  buff may be dirty from a previous loop
   ***************************************************************/
   if (buff)
      {
         XFree(buff);
         buff = NULL;
      }
   /***************************************************************
   *  Deal with windows which have just gone away.
   ***************************************************************/
   ce_error_bad_window_flag = False;
   ce_error_generate_message = False;

   DEBUG9(XERRORPOS)
   XGetWindowProperty(dspl_descr->display,
                      child_windows[window_idx],
                      CE_PROPERTY(ce_cclist_atom),
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

   if (ce_error_bad_window_flag || (keydef_property_size != sizeof(CC_LIST)))
      {
         DEBUG18(
            if (ce_error_bad_window_flag)
               fprintf(stderr, "Can't use window 0x%X, Bad window\n", child_windows[window_idx]);
            else
               if (keydef_property_size == 0)
                  fprintf(stderr, "Can't use window 0x%X, Not registered as Ce\n", child_windows[window_idx]);
               else
                  fprintf(stderr, "Can't use window 0x%X, Property wrong size, is %d, should be %d\n", child_windows[window_idx], keydef_property_size, sizeof(CC_LIST));
         )
         continue;
      }
   else
      buff_pos = (CC_LIST *)buff;

   DEBUG9(XERRORPOS)
   DEBUG18(
   if (actual_type != None)
      p = XGetAtomName(dspl_descr->display, actual_type);
   else
      p = "None";
   fprintf(stderr, "cc_check_other_windows:  Window = 0x%X, Size = %d, actual_type = %d (%s), actual_format = %d, nitems = %d, &bytes_after = %d\n",
                   child_windows[window_idx],
                   keydef_property_size, actual_type, p,
                   actual_format, nitems, bytes_after);
   if (actual_type != None)
      XFree(p);
   )

#ifdef CE_LITTLE_ENDIAN
   to_host_cc_list(buff_pos, keydef_property_size);
#endif

   /***************************************************************
   *  See if this window is the one we want.
   ***************************************************************/
   DEBUG18(dump_cc_data(buff, keydef_property_size, child_windows[window_idx], stderr);)
   if (buff_pos->version == 0) /* Ce Release 2.3 and 2.3.1 */
      {
         if ((buff_pos->dev != file_stats->st_dev) || (buff_pos->inode != file_stats->st_ino) || (buff_pos->host_id != saved_host_id))
            continue;
      }
   else
      {
         if ((buff_pos->inode       != file_stats->st_ino) ||
             (buff_pos->create_time != saved_create_time)  ||
             (buff_pos->file_size   != saved_file_size))
            continue;
      }

   DEBUG18(
      fprintf(stderr, "cc_check_other_windows:  Found window for dev 0x%X inode 0x%X host 0x%X ctime 0x%X size %d\n",
              file_stats->st_dev, file_stats->st_ino, saved_host_id, buff_pos->create_time, buff_pos->file_size);
      )

   if (chk_cclist_data(dspl_descr->display, child_windows[window_idx], buff_pos->main_window) == False)
      continue;

#ifndef WIN32
   /***************************************************************
   *  Make sure the uid matches.
   ***************************************************************/
   if (buff_pos->uid != getuid())
      {
         dm_error("File is open under another userid", DM_ERROR_BEEP);
         return(-1);
      }
#endif

   /***************************************************************
   *  Get the args we need to use
   ***************************************************************/
   arg0[0] = 'c';
   if (writable)
      arg0[1] = 'e';
   else
      arg0[1] = 'v';
   arg0[2] = '\0';

   build_cmd_args(dspl_descr,
                  "",
                  arg0,
                  &argc,
                  argv);

   /***************************************************************
   *  Build the cc command
   ***************************************************************/
   cmd[0] = 'c';
   if (writable)
      cmd[1] = 'e';
   else
      cmd[1] = 'v';
   cmd[2] = ' ';
   cmd[3] = '\0';
   p = &cmd[3];

   for (i = 1; i < argc; i++)
   {
      if (argv[i][0] != '\0')
         if (argv[i][0] == '-')
            snprintf(p, (sizeof(cmd)-(p-cmd))," %s", argv[i]);
         else
            snprintf(p, (sizeof(cmd)-(p-cmd)), " \"%s\"", argv[i]);
      p += strlen(p);
   }

   DEBUG18(fprintf(stderr, "cc_check_other_windows:  sending cmd '%s'\n", cmd);)

   /***************************************************************
   *  Send the property to the other window.
   ***************************************************************/

   ce_error_bad_window_flag = False;
   ce_error_generate_message = False;
   DEBUG18(ce_error_generate_message = True;)

   DEBUG9(XERRORPOS)
   XChangeProperty(dspl_descr->display,
                   buff_pos->main_window,
                   CE_PROPERTY(ce_cc_atom),
                   XA_STRING,
                   8, /* 8 bit quantities, we handle littleendin ourselves */
                   PropModeAppend,
                   (unsigned char *)cmd, 
                   strlen(cmd)+1);

   XFlush(dspl_descr->display);

   /***************************************************************
   *  Reread the property, or trap the error check to see if the
   *  badwindow flag went on,  It is set in xerror.c.  If so,
   *  this window just went away.  Skip it.  Otherwise, we have
   *  just sent the cc request to the file.  We are done.
   ***************************************************************/
   DEBUG9(XERRORPOS)
   XGetWindowProperty(dspl_descr->display,
                      buff_pos->main_window,
                      CE_PROPERTY(ce_cc_atom),
                      0, /* offset zero from start of property */
                      INT_MAX / 4,
                      False, /* do not delete the property */
                      XA_STRING,
                      &actual_type,
                      &actual_format,
                      &nitems,
                      &bytes_after,
                      (unsigned char **)&buff2);


 
   if (!ce_error_bad_window_flag)
      {
         DEBUG(fprintf(stderr, "Passing cc off to window 0x%X WM window 0x%X and exiting\n", buff_pos->main_window, child_windows[window_idx]);)
         exit(0);
      }
   else
      {
         DEBUG18(fprintf(stderr, "Bad window flag was true for window 0x%X WM window 0x%X\n", buff_pos->main_window, child_windows[window_idx]);)
      }

   /***************************************************************
   *  Free up storage allocated.
   ***************************************************************/
   if (buff)
      {
         XFree(buff);
         buff = NULL;
      }
   if (buff2)
      {
         XFree(buff2);
         buff = NULL;
      }

} /* for in the list of child windows off the Root window */

ce_error_generate_message = True;

if (child_windows)
   {
      XFree((char *)child_windows);
      buff = NULL;
   }

return(0);


} /* end of cc_check_other_windows */


/************************************************************************

NAME:      cc_set_pad_mode  -  Set variables to show we are in pad mode


PURPOSE:    This routine takes care of showing we are in pad mode.
            It sets the device and inode numbers to their default values.
            It is required when doing a cp from an edit session.
            Note: for pad mode, this routine is called in place of cc_check_other_windows

PARAMETERS:

   1.  display     - pointer to Display (INPUT)
                     This is the display handle needed for X calls.


GLOBAL DATA:

saved_dev        -  int (INPUT)
                    The device number of the file being edited is used.

saved_inode      -  int (INPUT)
                    The inode number of the file being edited is used.

saved_host_id    -  int (INPUT)
                    The internet id of the host of the file being edited is used.



FUNCTIONS :
   1.   Set the global variables to their default values.


*************************************************************************/

void  cc_set_pad_mode(Display       *display)
{

check_atoms(display);

saved_dev         = (unsigned int)PAD_MODE_DEV;
saved_inode       = (unsigned int)PAD_MODE_INODE;
saved_host_id     = ce_gethostid();
saved_create_time = (unsigned int)time(NULL);
saved_file_size   = 0;
DEBUG18(
   fprintf(stderr, "@cc_set_pad_mode: file id:  0x%X 0x%X 0x%X 0x%X %d\n",
           saved_dev, saved_inode, saved_host_id, saved_create_time, saved_file_size);
)

} /* end of cc_set_pad_mode */


/************************************************************************

NAME:      cc_register_file          -   register a file with the X server.


PURPOSE:    This routine takes care of registering a file with the X server.

PARAMETERS:

   1.  dspl_descr     - pointer to DISPLAY_DESCR (INPUT/OUTPUT)
                        This is the display description being set up.

   2.  main_window    - Window (INPUT - Xlib type)
                        This is the window to be registered.  It is the main window for
                        the ce session.

   3.  shell_pid      - int (INPUT)
                        This is the process id of the shell process started by a ceterm.
                        For non ceterm runs, zero is passed.

GLOBAL DATA:

saved_dev        -  int (INPUT)
                    The device number of the file being edited is used.

saved_inode      -  int (INPUT)
                    The inode number of the file being edited is used.

saved_host_id    -  int (INPUT)
                    The network id of the host of the file being edited is used.



FUNCTIONS :
   1.   Save the main window in the variable global to this module.

   2.   Call the routine to update the CC_LIST adding a window.

   3.   Put this window in the window list.
  

RETURNED VALUE:

   An X error which prevents saving the property causes a non-zero returned value.


*************************************************************************/

int  cc_register_file(DISPLAY_DESCR *dspl_descr,
                      int            shell_pid,
                      char          *edit_file)
{
int               rc;
int               new;
CC_LIST          *buff_pos;

/***************************************************************
*  
*  In pad mode, cc_check_other_windows is never called.
*  we use the special dev name and the inode is the pid.
*  
***************************************************************/

if (strcmp(edit_file, STDIN_FILE_STRING) == 0)  /* takes care of stdin */
   {
      saved_inode = getpid();
      saved_host_id = ce_gethostid();
      saved_create_time = (unsigned int)time(NULL);
   }

/*this_shpid = shell_pid; RES 6/27/95 get rid of lint msg */

DEBUG18(
   fprintf(stderr, "@cc_register_file: file id:  0x%X 0x%X 0x%X 0x%X %d  shell pid = %d\n",
           saved_dev, saved_inode, saved_host_id, saved_create_time, saved_file_size, shell_pid);
)

dspl_descr->wm_window = child_of_root(dspl_descr->display, dspl_descr->main_pad->x_window);

DEBUG18(
   fprintf(stderr, "cc_register_file: window 0x%X,   FILE ID:  0x%X 0x%X 0x%X 0x%X %d   - WM window 0x%X\n",
           dspl_descr->main_pad->x_window, saved_dev, saved_inode, saved_host_id,
           saved_create_time, saved_file_size, dspl_descr->wm_window);
)


buff_pos =  get_cclist_data(dspl_descr->display,  dspl_descr->wm_window, &new);
if (buff_pos)
   {
      buff_pos->dev         = saved_dev;
      buff_pos->inode       = saved_inode;
      buff_pos->host_id     = saved_host_id;
      buff_pos->create_time = saved_create_time;
      buff_pos->file_size   = saved_file_size;
      buff_pos->uid         = getuid();
      buff_pos->main_window = dspl_descr->main_pad->x_window;
      buff_pos->shpid       = shell_pid;  
      buff_pos->shgrp       = GETPGRP(shell_pid);
      buff_pos->pgrp        = GETPGRP(0);
      buff_pos->cepid       = getpid();
      buff_pos->display_no  = dspl_descr->display_no;
      buff_pos->version     = CC_LIST_VERSION;
      buff_pos->wm_window   = dspl_descr->wm_window;
      buff_pos->iconned     = (dspl_descr->currently_mapped == 0);

      put_cclist_data(dspl_descr->display, dspl_descr->wm_window, buff_pos);
      if (new)
         free((char *)buff_pos);
      else
         XFree((char *)buff_pos);
      rc = 0;
   }
else
   rc = -1;

#ifdef blah
/**************** DEBUG START *************************/
{
Atom   ws_atom;
      Atom             actual_type;
      int              actual_format;
      int              property_size;
      int              i;
      unsigned long    nitems;
      unsigned long    bytes_after;
      char            *buff = NULL;
      char            *p = NULL;
      FILE            *cedump = NULL;
      char             name[50];

snprintf(name, sizeof(name), "/tmp/cedump.%d", getpid());
cedump = fopen(name, "w");

DEBUG9(XERRORPOS)
ws_atom  = XInternAtom(dspl_descr->display, "_DT_WORKSPACE_PRESENCE", True);
if (ws_atom != None)
   {
        fprintf(cedump, "\n    _DT_WORKSPACE_PRESENCE is %d\n", ws_atom);
        DEBUG9(XERRORPOS)
        XGetWindowProperty(dspl_descr->display,
                           dspl_descr->main_pad->x_window,
                           ws_atom,
                           0, /* offset zero from start of property */
                           INT_MAX / 4,
                           False, /* do not delete the property */
                           AnyPropertyType,
                           &actual_type,
                           &actual_format,
                           &nitems,
                           &bytes_after,
                           (unsigned char **)&buff);

      property_size = nitems * (actual_format / 8);
      if (cedump && actual_type != None)
          {
             DEBUG9(XERRORPOS)
             p = XGetAtomName(dspl_descr->display, actual_type);
             fprintf(cedump, "\n    For main window 0x%X, Actual_type = %d (%s), actual_format = %d, len = %d\n",
                             dspl_descr->main_pad->x_window, actual_type, p, actual_format, property_size);
             XFree(p);
             hexdump(cedump, "main _DT_WORKSPACE_PRESENCE", buff, property_size, 0);
          }

        DEBUG9(XERRORPOS)
        XGetWindowProperty(dspl_descr->display,
                           dspl_descr->wm_window,
                           ws_atom,
                           0, /* offset zero from start of property */
                           INT_MAX / 4,
                           False, /* do not delete the property */
                           AnyPropertyType,
                           &actual_type,
                           &actual_format,
                           &nitems,
                           &bytes_after,
                           (unsigned char **)&buff);

      property_size = nitems * (actual_format / 8);
      if (cedump && actual_type != None)
          {
             DEBUG9(XERRORPOS)
             p = XGetAtomName(dspl_descr->display, actual_type);
             fprintf(cedump, "\n    For wm window 0x%X, Actual_type = %d (%s), actual_format = %d, len = %d\n",
                             dspl_descr->wm_window, actual_type, p, actual_format, property_size);
             XFree(p);
             hexdump(cedump, "wm _DT_WORKSPACE_PRESENCE", buff, property_size, 0);
          }

    }

DEBUG9(XERRORPOS)
ws_atom  = XInternAtom(dspl_descr->display, "_DT_WORKSPACE_HINTS", True);
if (ws_atom != None)
   {
        fprintf(cedump, "\n    _DT_WORKSPACE_HINTS is %d\n", ws_atom);
        DEBUG9(XERRORPOS)
        XGetWindowProperty(dspl_descr->display,
                           dspl_descr->main_pad->x_window,
                           ws_atom,
                           0, /* offset zero from start of property */
                           INT_MAX / 4,
                           False, /* do not delete the property */
                           AnyPropertyType,
                           &actual_type,
                           &actual_format,
                           &nitems,
                           &bytes_after,
                           (unsigned char **)&buff);

      property_size = nitems * (actual_format / 8);
      if (actual_type != None)
          {
             DEBUG9(XERRORPOS)
             p = XGetAtomName(dspl_descr->display, actual_type);
             fprintf(cedump, "\n    For main window 0x%X, Actual_type = %d (%s), actual_format = %d, len = %d\n",
                             dspl_descr->main_pad->x_window, actual_type, p, actual_format, property_size);
             XFree(p);
             hexdump(cedump, "main _DT_WORKSPACE_PRESENCE", buff, property_size, 0);
          }

        DEBUG9(XERRORPOS)
        XGetWindowProperty(dspl_descr->display,
                           dspl_descr->wm_window,
                           ws_atom,
                           0, /* offset zero from start of property */
                           INT_MAX / 4,
                           False, /* do not delete the property */
                           AnyPropertyType,
                           &actual_type,
                           &actual_format,
                           &nitems,
                           &bytes_after,
                           (unsigned char **)&buff);

      property_size = nitems * (actual_format / 8);
      if (actual_type != None)
          {
             DEBUG9(XERRORPOS)
             p = XGetAtomName(dspl_descr->display, actual_type);
             fprintf(cedump, "\n    For wm window 0x%X, Actual_type = %d (%s), actual_format = %d, len = %d\n",
                             dspl_descr->wm_window, actual_type, p, actual_format, property_size);
             XFree(p);
             hexdump(cedump, "wm _DT_WORKSPACE_PRESENCE", buff, property_size, 0);
          }

    }
   if (cedump)
      fclose(cedump);
}
/**************** DEBUG END   *************************/
#endif


return(rc);

} /* end of cc_register_file */


/************************************************************************

NAME:      get_cclist_data          -   get cclist data from the file.


PURPOSE:    This routine takes care of registering a file with the X server.

PARAMETERS:

   1.  display        - pointer to Display (INPUT)
                        This is the display we are talking to.

   2.  wm_window      - Window (INPUT - Xlib type)
                        This is the window manager window to get the data from

   3.  new            - int (OUTPUT)
                        This flag specifies whether data existed or was just created.

FUNCTIONS :
   1.   Read the CC_LIST property from the passed window.

   2.   If the property does not exist, create one and fill in the parameters.

   3.   Put this window in the window list.
  

RETURNED VALUE:
   The pointer to the CC_LIST property is returned.


*************************************************************************/

static CC_LIST  *get_cclist_data(Display       *display,
                                 Window         wm_window,
                                 int           *new)
{
Atom              actual_type;
int               actual_format;
int               keydef_property_size;
unsigned long     nitems;
unsigned long     bytes_after;
char             *buff = NULL;
CC_LIST          *buff_pos;
char             *p;

check_atoms(display);

/***************************************************************
*  Read the property for this window.  If not found, make one.
***************************************************************/
ce_error_bad_window_flag = False; /* in xerrorpos.h */
ce_error_generate_message = False;
DEBUG18(ce_error_generate_message = True;)

DEBUG9(XERRORPOS)
XGetWindowProperty(display,
                   wm_window,
                   CE_PROPERTY(ce_cclist_atom),
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
ce_error_generate_message = True;

if (keydef_property_size != sizeof(CC_LIST))
   {
      DEBUG18(
         if (keydef_property_size == 0)
               fprintf(stderr, "get_cclist_data:  Creating new cc data\n");
            else
               fprintf(stderr, "get_cclist_data:  CC data is wrong size, is %d, should be %d\n", keydef_property_size, sizeof(CC_LIST));
      )
      if (buff)
         XFree(buff);
      *new = True;
      buff = CE_MALLOC(sizeof(CC_LIST));
      if (!buff)
         return(NULL);
      memset(buff, 0, sizeof(CC_LIST));
      buff_pos = (CC_LIST *) buff;
      buff_pos->uid         = getuid();
      buff_pos->main_window = None;
      buff_pos->version     = CC_LIST_VERSION;
      buff_pos->pgrp        = GETPGRP(0);
   }
else
   {
      DEBUG9(XERRORPOS)
      DEBUG18(
      if (actual_type != None)
         p = XGetAtomName(display, actual_type);
      else
         p = "None";
      fprintf(stderr, "get_cclist_data:  Window = 0x%X, Size = %d, actual_type = %d (%s), actual_format = %d, nitems = %d, &bytes_after = %d\n",
                      wm_window,
                      keydef_property_size, actual_type, p,
                      actual_format, nitems, bytes_after);
      if (actual_type != None)
         XFree(p);
      )

      *new = False;
      buff_pos = (CC_LIST *) buff;
#ifdef CE_LITTLE_ENDIAN
      to_host_cc_list((CC_LIST *)buff, keydef_property_size);
#endif
   }

return(buff_pos);

} /* end of get_cclist_data  */


/************************************************************************

NAME:      put_cclist_data          -   Write the CC list property for this window


PURPOSE:    This routine takes care of registering a file with the X server.

PARAMETERS:

   1.  display        - pointer to Display (INPUT)
                        This is the display we are talking to.

   2.  wm_window      - Window (INPUT - Xlib type)
                        This is the window manager window to get the data from

   3.  buff           - pointer to CC_LIST (INPUT)
                        This data to write out.

FUNCTIONS :
   1.   Read the CC_LIST property from the passed window.

   2.   If the property does not exist, create one and fill in the parameters.

   3.   Put this window in the window list.
  

RETURNED VALUE:
   The pointer to the CC_LIST property is returned.


*************************************************************************/

static void put_cclist_data(Display       *display,
                            Window         wm_window,
                            CC_LIST       *buff)
{
Window      hold_window;  /* needed because to_net_cc_list changes buff contents */

DEBUG18(fprintf(stderr, "@put_cclist_data\n");dump_cc_data((char *)buff, sizeof(CC_LIST), wm_window, stderr);)

hold_window = (Window)buff->main_window;

#ifdef CE_LITTLE_ENDIAN
to_net_cc_list((CC_LIST *)buff, sizeof(CC_LIST));
#endif

DEBUG9(XERRORPOS)
XChangeProperty(display,
                wm_window,
                CE_PROPERTY(ce_cclist_atom),
                XA_STRING,
                8, /* 8 bit quantities, we handle littleendin ourselves */
                PropModeReplace,
                (unsigned char *)buff,
                sizeof(CC_LIST));

if (wm_window != (Window)buff->main_window)
   {
      DEBUG9(XERRORPOS)
      XChangeProperty(display,  /* attach to the local window too */
                      hold_window,
                      CE_PROPERTY(ce_cclist_atom),
                      XA_STRING,
                            8, /* 8 bit quantities, we handle littleendin ourselves */
                      PropModeReplace,
                      (unsigned char *)buff,
                      sizeof(CC_LIST));
   }

XFlush(display);

} /* end of put_cclist_data  */


/************************************************************************

NAME:      cc_shutdown            -   Remove the cclist property from the WM window


PURPOSE:    This routine removes the cclist property from the window manager
            window during a controlled shutdown.  Some window managers reuse the
            window manager windows and thus leave this garbage.

PARAMETERS:

   1.  dspl_descr     - pointer to DISPLAY_DESCR (INPUT/OUTPUT)
                        This is the display description being shut down.


FUNCTIONS :
   1.   Read the property with delete.

*************************************************************************/

void cc_shutdown(DISPLAY_DESCR *dspl_descr)
{
Atom              actual_type;
int               actual_format;
unsigned long     nitems;
unsigned long     bytes_after;
char             *buff = NULL;


if (dspl_descr->wm_window != None)
   {
      DEBUG9(XERRORPOS)
      XGetWindowProperty(dspl_descr->display,
                         dspl_descr->wm_window,
                         CE_PROPERTY(ce_cclist_atom),
                         0, /* offset zero from start of property */
                         INT_MAX / 4,
                         True, /* do delete the property */
                         AnyPropertyType,
                         &actual_type,
                         &actual_format,
                         &nitems,
                         &bytes_after,
                         (unsigned char **)&buff);
      if (buff)
         XFree(buff);
   }

} /* end of cc_shutdown */
                          

/************************************************************************

NAME:      child_of_root       -   Find the window to register the CC list property on

PURPOSE:    This routine finds the window whose parent is the root window starting
            at the passed window.  This may the the passed window or one of
            its ancestors.

PARAMETERS:

   1.  display     - pointer to Display (INPUT)
                     This is the display name needed for X calls.

   2.  window      - Window (INPUT - X type)
                     This is the window to start with.

FUNCTIONS :

   1.   Call XQueryTree to find the parent of the passed window.

   2.   Follow this list of parents till the window whose parent
        is the root window is found.  This compensates for window managers.

RETURNED VALUE:

   wm_window   -  Window
                  The window whose window is the root window is returned.

*************************************************************************/

static Window  child_of_root(Display   *display,
                             Window     window)
{
int               ok;
Window            root_return;
Window            parent_return;
Window           *child_windows = NULL;
unsigned int      nchildren;

DEBUG18(fprintf(stderr, "@child_of_root:  Ce window: 0x%X ", window);)

/***************************************************************
*  
*  Find the window whose parent is the root window.  This may
*  be the passed window or a parent of the passed window.
*  
***************************************************************/

DEBUG9(XERRORPOS)
ok = XQueryTree(display,                 /* input */
                window,                  /* input */
                &root_return,            /* output */
                &parent_return,          /* output */
                &child_windows,          /* output */
                &nchildren);             /* output */

while(ok && (root_return != parent_return))
{
   if (child_windows)
      XFree((char *)child_windows);
   child_windows = NULL;
   window = parent_return;
   ok = XQueryTree(display,                 /* input */
                   window,                  /* input */
                   &root_return,            /* output */
                   &parent_return,          /* output */
                   &child_windows,          /* output */
                   &nchildren);             /* output */
}

DEBUG18(fprintf(stderr, "WM window: 0x%X\n", window);)

if (child_windows)
   XFree((char *)child_windows);
return(window);

} /* end of child_of_root */


/************************************************************************

NAME:      cc_get_update    -   process an update sent via property change from another window

PURPOSE:    This routine receives commands sent by other windows.

PARAMETERS:

   1.  event       - pointer to XEvent (INPUT)
                     The property notify event which caused this routine to be called.

   2.  dspl_descr  - pointer to DISPLAY_DESCR (INPUT)
                     This is the current display description which contains the pointers
                     to all the pads.

   3.  warp_needed - pointer to int (OUTPUT)
                     This flag specifies whether a warp is needed int he original window.


FUNCTIONS :

   1.   If the property notify indicates deleted, return.

   2.   Read the passed property with a Delete after read option.

   3.   Walk the list of events processing each one.


RETURNED VALUE:
   redraw -  If the screen needs to be redrawn, the mask of which screen
             and what type of redrawing is returned.


*************************************************************************/

int  cc_get_update(XEvent         *event,
                   DISPLAY_DESCR  *dspl_descr,             /* input  */
                   int            *warp_needed)            /* output */
{
char                 *p;
Atom                  actual_type;
int                   actual_format;
int                   keydef_property_size;
unsigned long         nitems;
unsigned long         bytes_after;
char                 *buff = NULL;
char                 *buff_end;
CC_MSG               *buff_pos;
char                  msg[256];
int                   scrap_window;
int                   redraw_needed = 0;

check_atoms(event->xproperty.display); /* make sure the atoms exist */
*warp_needed = False;

/***************************************************************
*
*  Make sure this is not a delete property change.  We are not
*  interested in the property going away.
*
***************************************************************/
if (event->xproperty.state == PropertyDelete)
   return(0);

DEBUG9(XERRORPOS)
DEBUG18(fprintf(stderr, "cc_get_update:  pid %d - Received update to window 0x%X, property =  %d (%s), state = %s\n",
               getpid(), event->xproperty.window,
               event->xproperty.atom, (p = XGetAtomName(event->xproperty.display, event->xproperty.atom)),
               ((event->xproperty.state == PropertyDelete) ? "Deleted" : "NewValue"));
       XFree(p);
)

/***************************************************************
*  
*  Read the property in and delete it.
*  
***************************************************************/

DEBUG9(XERRORPOS)
XGetWindowProperty(event->xproperty.display,
                   event->xproperty.window,
                   event->xproperty.atom,
                   0, /* offset zero from start of property */
                   INT_MAX / 4,
                   True, /* delete the property after reading it */
                   XA_STRING,
                   &actual_type,
                   &actual_format,
                   &nitems,
                   &bytes_after,
                   (unsigned char **)&buff);

keydef_property_size = nitems * (actual_format / 8);
buff_pos = (CC_MSG *)buff;
buff_end = buff + keydef_property_size;


DEBUG9(XERRORPOS)
DEBUG18(
   if (actual_type != None)
      p = XGetAtomName(event->xproperty.display, actual_type);
   else
      p = "None";
   fprintf(stderr, "cc_get_update:  Size = %d, actual_type = %d (%s), actual_format = %d, nitems = %d, &bytes_after = %d\n",
                   keydef_property_size, actual_type, p,
                   actual_format, nitems, bytes_after);
   if (actual_type != None)
      XFree(p);
   )

if (keydef_property_size == 0)
   {
      if (buff)
         XFree(buff);
      return(0);
   }

/***************************************************************
*  
*  Walk the list of message doing the inserts, deleted, and replaces.
*  
***************************************************************/

while ((char *)buff_pos < buff_end)
{
   /***************************************************************
   *  First go to host format.
   ***************************************************************/
   buff_pos->msg_len            = ntohl(buff_pos->msg_len);
   buff_pos->lineno             = ntohl(buff_pos->lineno);
   buff_pos->type               = ntohs(buff_pos->type);
   buff_pos->count              = ntohs(buff_pos->count);
   buff_pos->total_lines_infile = ntohl(buff_pos->total_lines_infile);

   switch(buff_pos->type)
   {
   /***************************************************************
   *  
   *  Request to execute a dm command.
   *  CMDS_FROM_ARG is in parms.h.
   *  
   ***************************************************************/

   case CMD_XDMC:
      DEBUG18(
        fprintf(stderr, "cc_get_update:  Request to do execute a dm command, cmd = \"%s\"\n",
        buff_pos->line);
      )
      store_cmds(dspl_descr, buff_pos->line, False);  /* in getevent.c */
      fake_keystroke(dspl_descr);                     /* in getevent.c */
      break; 

   /***************************************************************
   *  
   *  Request to execute a ping back to the sender
   *  append this window id to the requested atom on the callers window.
   *  lineno is the window id
   *  total_lines_infile is the atom name
   *  
   ***************************************************************/

   case CMD_PING:
      DEBUG18(
        fprintf(stderr, "cc_get_update:  Request to do execute a ping to window 0x%X, atom %d\n",
        buff_pos->lineno, buff_pos->total_lines_infile);
      )
      scrap_window = htonl((unsigned int)dspl_descr->main_pad->x_window);
      XChangeProperty(dspl_descr->display,
                      (Window)buff_pos->lineno,
                      (Atom)buff_pos->total_lines_infile,
                      XA_STRING,
                      8, /* 8 bit quantities, we handle littleendin ourselves */
                      PropModeAppend,
                      (unsigned char *)&scrap_window,
                      4);
      break; 

   /***************************************************************
   *  
   *  Request to warp to one of these windows.
   *  Count is non-zero if we want to go to the input window.
   *  The input window is the unixcmd if available and the dm window
   *  for a read only file, and the main window for a writeable file,
   *  If count is zero, we always go to the main window.
   *  
   ***************************************************************/

   case CMD_TOWINDOW:
      DEBUG18(fprintf(stderr, "cc_get_update:  Request to go to window, type %s\n", (buff_pos->count ? "Input" : "Main"));)
      force_in_window(dspl_descr); /* in window.c, allows warp in process redraw, even if cursor is not in the window */
      /* following ifdef added 5/3/94 by RES */
#ifndef apollo
      /* following call added 10/20/93 by RES */
      XSetInputFocus(dspl_descr->display, dspl_descr->main_pad->x_window, RevertToPointerRoot, CurrentTime);
#endif
      /* count carries the flag which when True sayes we want an writable window */
      if (!buff_pos->count || WRITABLE(dspl_descr->main_pad->token))
         redraw_needed = dm_position(dspl_descr->cursor_buff, dspl_descr->main_pad, dspl_descr->main_pad->file_line_no, dspl_descr->main_pad->file_col_no);
      else
         if (dspl_descr->pad_mode)
            redraw_needed = dm_position(dspl_descr->cursor_buff, dspl_descr->unix_pad, dspl_descr->unix_pad->file_line_no, dspl_descr->unix_pad->file_col_no);
         else
            redraw_needed = dm_position(dspl_descr->cursor_buff, dspl_descr->dminput_pad, dspl_descr->dminput_pad->file_line_no, dspl_descr->dminput_pad->file_col_no);
      *warp_needed = True;
      /***************************************************************
      *  Make sure we are on top.
      ***************************************************************/
      if (window_is_obscured(dspl_descr)) /* in window.c */
         {
            DEBUG9(XERRORPOS)
            XRaiseWindow(dspl_descr->display, dspl_descr->main_pad->x_window);
         }
      break; 


   /***************************************************************
   *  
   *  Unknown message type.  Flag an error.
   *  
   ***************************************************************/

   default:
      snprintf(msg, sizeof(msg), "cc_get_update:  Invalid CC msg type received (%d)\n", buff_pos->type);
      dm_error(msg, DM_ERROR_LOG);
      redraw_needed |= (dspl_descr->main_pad->redraw_mask & FULL_REDRAW);
      break;

    }  /* end of switch on message type */

      
   /***************************************************************
   *  Bump to the next entry.
   ***************************************************************/
   DEBUG18(fprintf(stderr, "cc_get_update:  Bump to next ent, cur len = %d, buff_pos = 0x%X, buff_end = 0x%X\n", buff_pos->msg_len, buff_pos, buff_end);)
   buff_pos = (CC_MSG *)((char *)buff_pos + buff_pos->msg_len);

} /* while walking the message list */

XFree(buff);

return(redraw_needed);

} /* end of cc_get_update */


/************************************************************************

NAME:      cc_plbn  -   send a line as a result of a PutLine_By_Number

PURPOSE:    This routine looks at what type of redrawing is needed in other
            windows when a put line by number is done in cc mode.

PARAMETERS:

   1.  token       - pointer to DATA_TOKEN (INPUT)
                     This opaque pointer references the data token for the
                     window being updated.  This is always the main window.

   2.  line_no     - int (INPUT)
                     This is the line number being replaced or the line just before
                     the line being added.

   3.  line        - pointer to char (INPUT)
                     This is the line being replaced or added.

   4.  flag        - int (INPUT)
                     This flag specifies whether this is an insert replace.
                     It is one of the following two values defined in
                     memdata.h: OVERWRITE or INSERT .
                    
GLOBAL DATA:

dspl_descr       -  pointer to DISPLAY_DESCR (INPUT)
                    The current display is accessed globally through windowdefs.h
                    We deal with all displays except the current one.


FUNCTIONS :

   1.   Verify that this request is for the main window.

   2.   Put the request into the structure.

   3.   Send the structure to each window in the CC list except this one.


*************************************************************************/

void     cc_plbn(DATA_TOKEN *token,       /* opaque */
                 int         line_no,     /* input */
                 char       *line,        /* input */ 
                 int         flag)        /* input;   0=overwrite 1=insert after */
{
DISPLAY_DESCR        *walk_dspl;
int                   redraw_needed;
int                   window_line;
int                   last_line;
int                   bottom_displayed;
int                   shifted = False;

/***************************************************************
*  Make sure this request is for the main window.
***************************************************************/
if (token != dspl_descr->main_pad->token)
   return;

DEBUG18(fprintf(stderr, "cc_plbn: %s at line %d, display %d, line = \"%s\"\n", (flag ? "Insert" : "Overwrite"), line_no, dspl_descr->display_no, line);)


for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
{
   /***************************************************************
   *  Find out some stuff about this display.
   *  What lines are being displayed, are we in the window and is
   *  this the special case of a partial display of the end
   *  of the data.
   ***************************************************************/
   redraw_needed = TITLEBAR_MASK;
   window_line = line_no - walk_dspl->main_pad->first_line;
   last_line   = walk_dspl->main_pad->first_line+walk_dspl->main_pad->window->lines_on_screen;
   bottom_displayed = (last_line >= total_lines(walk_dspl->main_pad->token));

   /***************************************************************
   *  First off, 
   *  If line numbers are on and this insert is above the first
   *  line in the file, a lineno redraw is needed.  In the impacted
   *  redraw mask, this is the same as scroll redraw.
   ***************************************************************/
   if (walk_dspl->show_lineno && (flag == INSERT) && (line_no <= last_line))
      redraw_needed |= MAIN_PAD_MASK & LINENO_REDRAW;

   /***************************************************************
   *  Next,
   *  Adjust the first line displayed if needed and also the
   *  current line.  The net result is that the position on the
   *  window dows not change for the impacted screen and we do
   *  not have to move the cursor.
   ***************************************************************/
   if (flag == INSERT)
      {
         if (line_no < walk_dspl->main_pad->file_line_no)
            {
               walk_dspl->main_pad->file_line_no++;
               walk_dspl->main_pad->first_line++;
               walk_dspl->cursor_buff->up_to_snuff = False;
               shifted++;
            }
      }
   else
      /***************************************************************
      *  If the other display is snuffed on the replaced line, it ain't
      *  no more.  The cc_line_ck keeps both displays from modifying
      *  the same line.
      ***************************************************************/
      if ((walk_dspl->main_pad->file_line_no == line_no) && (walk_dspl->cursor_buff->which_window == MAIN_PAD))
         {
            DEBUG18(fprintf(stderr, "cc_plbn: hit on line %d, unsnuffing display %d\n", line_no, walk_dspl->display_no);)
            walk_dspl->cursor_buff->up_to_snuff = False;
         }

   /***************************************************************
   *  Next,
   *  If this is an insert on the screen, everything from that point
   *  on down has to be redrawn.  If this is an overwrite, we can
   *  replace just the line unless we are already impacted.  In that
   *  case we also do a partial screen redraw.
   ***************************************************************/
   if ((window_line >= 0) && (line_no < last_line))
      if ((strchr(line, '\f') != NULL) || bottom_displayed || walk_dspl->main_pad->formfeed_in_window || (line_no < walk_dspl->main_pad->file_line_no))
         {
            walk_dspl->main_pad->impacted_redraw_line = 0;
            redraw_needed |= (MAIN_PAD_MASK & FULL_REDRAW);
         }
      else
         if ((flag == OVERWRITE) && (walk_dspl->main_pad->impacted_redraw_line == -1))  /* not set yet */
            {
               walk_dspl->main_pad->impacted_redraw_line = window_line;
               redraw_needed |= (MAIN_PAD_MASK & PARTIAL_LINE);
            }
         else
            {
               if ((walk_dspl->main_pad->impacted_redraw_line > window_line) || (walk_dspl->main_pad->impacted_redraw_line == -1))
                  walk_dspl->main_pad->impacted_redraw_line = window_line;
               redraw_needed |= (MAIN_PAD_MASK & PARTIAL_REDRAW);
            }
      
   if (shifted)
      redraw_needed |= TITLEBAR_MASK & FULL_REDRAW;
   
   walk_dspl->main_pad->impacted_redraw_mask |= redraw_needed;
   DEBUG18(fprintf(stderr, "cc_plbn: impacted display %d, redraw 0x%X, line no %d, first line %d\n", walk_dspl->display_no, redraw_needed, walk_dspl->main_pad->impacted_redraw_line,  walk_dspl->main_pad->first_line);)

} /* end of walking list of secondary display descriptions */


} /* end of cc_plbn */


/************************************************************************

NAME:      cc_ca  -   Tranmit color impacts to other displays

PURPOSE:    This routine looks at what type of redrawing is needed in other
            windows when an area is colored.

PARAMETERS:

   1.  dspl_descr  - pointer to DISPLAY_DESCR (INPUT/OUTPUT)
                     This is the current display description.
                     It is needed to access the other windows.

   2.  top_line    - int (INPUT)
                     This is the first line impacted by the color area.

   3.  bottom_line - int (INPUT)
                     This is the last line impacted by the color area.

FUNCTIONS :
   1.   Verify that this request is for the main window.

   2.   Put the request into the structure.

   3.   Send the structure to each window in the CC list except this one.


*************************************************************************/

void     cc_ca(DISPLAY_DESCR   *dspl_descr,   /* input/output */
               int              top_line,     /* input        */
               int              bottom_line)  /* input        */
{
DISPLAY_DESCR        *walk_dspl;
int                   last_line;


for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
{
   /***************************************************************
   *  Find out some stuff about this display.
   *  What lines are being displayed, are we in the window and is
   *  this the special case of a partial display of the end
   *  of the data.
   ***************************************************************/
   last_line   = walk_dspl->main_pad->first_line+walk_dspl->main_pad->window->lines_on_screen;

   /***************************************************************
   *  First, see if there is any impact to the displayed screen.
   *  Look for overlap between the colored range and the displayed
   *  range of lines.
   ***************************************************************/
   if ((walk_dspl->main_pad->first_line <= bottom_line) &&
       (last_line > top_line))
      {
         /*event_do(dspl_descr->main_pad->token, KS_EVENT, 0, 0, 0, 0);*/
         flush(walk_dspl->main_pad); /* in typing.c */
         /*event_do(dspl_descr->main_pad->token, KS_EVENT, 0, 0, 0, 0);*/

         walk_dspl->main_pad->impacted_redraw_mask |= MAIN_PAD_MASK & FULL_REDRAW;
         DEBUG18(
            fprintf(stderr, "cc_ca: impacted display %d, range %d->%d, window %d->%d\n",
            walk_dspl->display_no, top_line, bottom_line, walk_dspl->main_pad->first_line, last_line);
         )
      }
} /* end of walking list of secondary display descriptions */


} /* end of cc_ca */


/************************************************************************

NAME:      cc_dlbn  -   tell the other windows to delete a line  (Delete Line By Number)

PURPOSE:    This routine looks at what type of redrawing is needed in other
            windows when a delete line by number is done in cc mode.

PARAMETERS:

   1.  token       - pointer to DATA_TOKEN (INPUT)
                     This opaque pointer references the data token for the
                     window being updated.  This is always the main window.

   2.  line_no     - int (INPUT)
                     This is the line number being replaced or the line just before
                     the line being added.

   3.  count       - int (INPUT)
                     Count of lines to be deleted.  Always 1.
                    


FUNCTIONS :

   1.   Verify that this request is for the main window.

   2.   Put the request into the structure.

   3.   Send the structure to each window in the CC list except this one.


*************************************************************************/

/* ARGSUSED2 */
void     cc_dlbn(DATA_TOKEN *token,       /* opaque */
                 int         line_no,     /* input */
                 int         count)       /* input;   (always 1) */
{
DISPLAY_DESCR        *walk_dspl;
DISPLAY_DESCR        *hold_dspl;
int                   redraw_needed = 0;
int                   window_line;
int                   last_line;
int                   shifted = False;
char                  msg[256];

/***************************************************************
*  Make sure this request is for the main window.
***************************************************************/
if (token != dspl_descr->main_pad->token)
   return;

DEBUG18(fprintf(stderr, "cc_dlbn: delete line %d, display %d\n", line_no, dspl_descr->display_no);)


for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
{
   /***************************************************************
   *  Find out some stuff about this display.
   *  What lines are being displayed, are we in the window and is
   *  this the special case of a partial display of the end
   *  of the data.
   ***************************************************************/
   window_line = line_no - walk_dspl->main_pad->first_line;
   last_line   = walk_dspl->main_pad->first_line+walk_dspl->main_pad->window->lines_on_screen;

   /***************************************************************
   *  First off, 
   *  If line numbers are on and this delete is above the first
   *  line in the file, a lineno redraw is needed.  In the impacted
   *  redraw mask, this is the same as scroll redraw.
   ***************************************************************/
   if (walk_dspl->show_lineno && (line_no <= last_line))
      redraw_needed |= MAIN_PAD_MASK & LINENO_REDRAW;

   /***************************************************************
   *  Next,
   *  Adjust the first line displayed if needed and also the
   *  current line.
   ***************************************************************/
   if (line_no < walk_dspl->main_pad->file_line_no)
      {
         walk_dspl->main_pad->file_line_no--;
         if (walk_dspl->main_pad->first_line > 0)
            walk_dspl->main_pad->first_line--;
         else
            {
               /***************************************************************
               *  Just deleted a line above the line the guy is typing on 
               *  and we have no choice but to shift his cursor up one.
               ***************************************************************/
               walk_dspl->cursor_buff->y -= walk_dspl->main_pad->window->line_height;
               walk_dspl->cursor_buff->win_line_no--;
               if (walk_dspl->main_pad->buff_modified)
                  process_redraw(walk_dspl, 0, True); /* move up one line */
            }
         walk_dspl->cursor_buff->up_to_snuff = False;
         shifted++;
      }

   if ((line_no == walk_dspl->main_pad->file_line_no) && (walk_dspl->main_pad->buff_modified))
      {
         snprintf(msg, sizeof(msg), "Deleted line %d was being modified in another CC window", line_no+1);
         dm_error(msg, DM_ERROR_BEEP);
         event_do(dspl_descr->main_pad->token, KS_EVENT, 0, 0, 0, 0);
         flush(walk_dspl->main_pad);
         event_do(dspl_descr->main_pad->token, KS_EVENT, 0, 0, 0, 0);
         hold_dspl = dspl_descr;
         set_global_dspl(walk_dspl);
         snprintf(msg, sizeof(msg), "Current line %d was just deleted in another window", line_no+1);
         dm_error(msg, DM_ERROR_BEEP);
         set_global_dspl(hold_dspl);
      }

   if (line_no <= walk_dspl->main_pad->file_line_no)
      {
         /*walk_dspl->main_pad->file_line_no++; RES 8/19/94 */
         walk_dspl->cursor_buff->up_to_snuff = False;
         shifted++;
      }

   /***************************************************************
   *  If the other display is snuffed on the deleted line, it ain't
   *  no more.  The cc_line_ck keeps both displays from modifying
   *  the same line.
   ***************************************************************/
   if ((walk_dspl->main_pad->file_line_no == line_no) && (walk_dspl->cursor_buff->which_window == MAIN_PAD))
      {
         DEBUG18(fprintf(stderr, "cc_plbn: hit on line %d, unsnuffing display %d\n", line_no, walk_dspl->display_no);)
         walk_dspl->cursor_buff->up_to_snuff = False;
      }

   /***************************************************************
   *  Next,
   *  If the line being deleted is on the screen. we need a partial
   *  redraw.
   ***************************************************************/
   if ((window_line >= 0) && (window_line < last_line))
      if (walk_dspl->main_pad->formfeed_in_window)  /* screens with a form feed require a full redraw */
         {
            walk_dspl->main_pad->impacted_redraw_line = 0;
            redraw_needed |= (MAIN_PAD_MASK & FULL_REDRAW);
         }
      else
         {
            if ((walk_dspl->main_pad->impacted_redraw_line > window_line) || (walk_dspl->main_pad->impacted_redraw_line == -1))
               walk_dspl->main_pad->impacted_redraw_line = window_line;
            redraw_needed |= (MAIN_PAD_MASK & PARTIAL_REDRAW);
         }
   
   if (shifted)
      redraw_needed |= TITLEBAR_MASK & FULL_REDRAW;
   walk_dspl->main_pad->impacted_redraw_mask |= redraw_needed;
   DEBUG18(fprintf(stderr, "cc_dlbn: impacted display %d, redraw 0x%X, line no %d, first line %d\n", walk_dspl->display_no, redraw_needed, walk_dspl->main_pad->impacted_redraw_line, walk_dspl->main_pad->first_line);)

} /* end of walking list of secondary display descriptions */

} /* end of cc_dlbn */


/************************************************************************

NAME:      cc_joinln  -   Flush cc windows prior to a join

PURPOSE:    This routin flushes a typing work buffer in a cc window if the
            line is about to be joined to another line.

PARAMETERS:

   1.  token       - pointer to DATA_TOKEN (INPUT)
                     This opaque pointer references the data token for the
                     window being updated.  This is always the main window.

   2.  line_no     - int (INPUT)
                     This is the line number being joined.  It is one before
                     the impacted line.  If -1 is passed, all buffers are flushed.

FUNCTIONS :

   1.   Verify that this request is for the main window.

   2.   Check each display to see if the line number passed plus one is affected.

   3.   Flush affected lines.


*************************************************************************/

void     cc_joinln(DATA_TOKEN *token,       /* opaque */
                   int         line_no)     /* input */
{
DISPLAY_DESCR        *walk_dspl;
int                   affected_line = line_no + 1;

/***************************************************************
*  Make sure this request is for the main window.
***************************************************************/
if (token != dspl_descr->main_pad->token)
   return;

DEBUG18(fprintf(stderr, "cc_joinln: affected line line %d, display %d\n", affected_line, dspl_descr->display_no);)


for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
{

   /***************************************************************
   *  First off, 
   *  If line numbers are on and this delete is above the first
   *  line in the file, a lineno redraw is needed.  In the impacted
   *  redraw mask, this is the same as scroll redraw.
   ***************************************************************/
   if (walk_dspl->main_pad->file_line_no == affected_line || (line_no < 0))
      {
         DEBUG18(fprintf(stderr, "cc_joinln: Flushing display %d\n", walk_dspl->display_no);)
         event_do(dspl_descr->main_pad->token, KS_EVENT, 0, 0, 0, 0);
         flush(walk_dspl->main_pad); /* in typing.c */
         event_do(dspl_descr->main_pad->token, KS_EVENT, 0, 0, 0, 0);
      }
} /* end of walking list of secondary display descriptions */

} /* end of cc_joinln */


/************************************************************************

NAME:      cc_padname  -   Tell the other windows about a pad_name command

PURPOSE:    This routine iresets the dev and inode in the window definition.

PARAMETERS:

   1.  dspl_descr  - pointer to DISPLAY_DESCR (INPUT)
                     This is the current display description.

   2.  edit_file   - pointer to char (INPUT)
                     This is the new file name


FUNCTIONS :

   1.   Reset the saved dev and inode values for the file.

   2.   Change the CC_CCLIST property to reflect the new value

   3.   Update any cc windows to reexpose the titlebar.

*************************************************************************/

void     cc_padname(DISPLAY_DESCR *dspl_descr,    /* input */
                    char          *edit_file)     /* input */
{
int                   rc;
struct stat           file_stats;
char                  msg[256];
int                   new;
CC_LIST              *buff_pos;
DISPLAY_DESCR        *walk_dspl;

DEBUG18(fprintf(stderr, "cc_padname:  file %s\n", edit_file);)
if (strcmp(edit_file, STDIN_FILE_STRING) == 0)
   {
      saved_dev = (unsigned int)PAD_MODE_DEV;
      saved_inode = getpid();
      saved_host_id = ce_gethostid();
      saved_create_time = (unsigned int)time(NULL);
   }
else
   {
      if (LSF)
         rc = ind_stat(&(dspl_descr->ind_data), dspl_descr->hsearch_data, edit_file, &file_stats);
      else
         rc = stat(edit_file, &file_stats);
      if (rc != 0)
          {
            snprintf(msg, sizeof(msg), "Cannot stat %s, (%s)", edit_file, strerror(errno));
            dm_error(msg, DM_ERROR_BEEP);
            return;
         }
      saved_dev         = file_stats.st_dev;
      saved_inode       = file_stats.st_ino;
      saved_create_time = (unsigned int)file_stats.st_ctime;
      saved_file_size   = file_stats.st_size;
   }

walk_dspl = dspl_descr;
do
{
   if (walk_dspl->wm_window == 0)
      walk_dspl->wm_window = child_of_root(walk_dspl->display, walk_dspl->main_pad->x_window);
   
   buff_pos =  get_cclist_data(walk_dspl->display,  walk_dspl->wm_window, &new);
   if (buff_pos)
      {
         buff_pos->dev         = saved_dev;
         buff_pos->inode       = saved_inode;
         buff_pos->host_id     = saved_host_id;
         buff_pos->create_time = saved_create_time;
         buff_pos->file_size   = saved_file_size;
         buff_pos->uid         = getuid();
         buff_pos->main_window = walk_dspl->main_pad->x_window;
         if (!walk_dspl->pad_mode)
            buff_pos->shpid       = 0;
         buff_pos->shgrp       = GETPGRP(0);
         buff_pos->pgrp        = GETPGRP(0);
         buff_pos->cepid       = getpid();
         buff_pos->display_no  = walk_dspl->display_no;
         buff_pos->version     = CC_LIST_VERSION;
         buff_pos->wm_window   = walk_dspl->wm_window;
   
         put_cclist_data(walk_dspl->display, walk_dspl->wm_window, buff_pos);
         if (new)
            free((char *)buff_pos);
         else
            XFree((char *)buff_pos);
      }

   DEBUG18(fprintf(stderr, "display %d\n",walk_dspl->display_no);dump_cc_data((char *)buff_pos, sizeof(CC_LIST), walk_dspl->wm_window, stderr);)
   walk_dspl = walk_dspl->next;
} while(walk_dspl != dspl_descr);

cc_titlebar();
} /* end of cc_padname */


/************************************************************************

NAME:      cc_titlebar  -   Tell the other windows about a titlebar change

PURPOSE:    Scan the list of other windows and mark them all for titlebar 
            refresh.

PARAMETERS:

   none

FUNCTIONS :

   1.   Mark the titlebar refresh flag in each CC window.


*************************************************************************/

void     cc_titlebar(void)
{
DISPLAY_DESCR        *walk_dspl;

DEBUG18(fprintf(stderr, "@cc_titlebar:\n");)

for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
    walk_dspl->main_pad->impacted_redraw_mask |= TITLEBAR_MASK & FULL_REDRAW;

} /* end of cc_titlebar */


/************************************************************************

NAME:      cc_line_chk  -   Check if a line to be processed is already in use


PURPOSE:    Scan the list of other windows and look for the passed line with
            the buffers set up for modification.

PARAMETERS:

   1.   dspl_descr   - pointer to DISPLAY_DESCR (INPUT)
                       This is the display description to be modified

   2.   line_no      - int (INPUT)
                       This is the file line number to be modified.

FUNCTIONS :

   1.   Check each CC window to see if the file line number is in use

RETURNED VALUE:
   in_use    -  int
                True   -  Line number is in use by another window.
                False  -  Line number is available

*************************************************************************/

int      cc_line_chk(DATA_TOKEN    *token,       /* opaque */
                     DISPLAY_DESCR *dspl_descr,
                     int            line_no)
{
DISPLAY_DESCR        *walk_dspl;

/***************************************************************
*  Make sure this request is for the main window.
***************************************************************/
if (token != dspl_descr->main_pad->token)
   return(False);

for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
    if ((walk_dspl->main_pad->file_line_no == line_no) && walk_dspl->main_pad->buff_modified)
       {
          DEBUG18(fprintf(stderr, "cc_line_chk: hit on line %d, display %d wants, display %d owns\n", line_no, dspl_descr->display_no, walk_dspl->display_no);)
          return(True);
       }

return(False);

} /* end of cc_line_chk */


/************************************************************************

NAME:      cc_typing_chk  -   Update typing on a CC screens

PURPOSE:    This routine looks at what type of redrawing is needed in other
            windows when a put line by number is done in cc mode.

PARAMETERS:

   1.  token       - pointer to DATA_TOKEN (INPUT)
                     This opaque pointer references the data token for the
                     window being updated.  This is always the main window.

   2.  dspl_descr  - pointer to DISPLAY_DESCR (INPUT)
                     This is the display description for the pad being
                     modified.
                    
   3.  line_no     - int (INPUT)
                     This is the line number being replaced or the line just before
                     the line being added.

   4.  line        - pointer to char (INPUT)
                     This is the line being replaced or added.

FUNCTIONS :

   1.   Verify that this request is for the main window.

   2.   Check for other windows which are showing the line being modified.

   3.   Set the typing impacted redraw fields for the impacted displays


*************************************************************************/

void     cc_typing_chk(DATA_TOKEN     *token,       /* opaque */
                       DISPLAY_DESCR  *dspl_descr,   /* input  */
                       int             line_no,      /* input */
                       char           *line)         /* input */ 
{
DISPLAY_DESCR        *walk_dspl;
int                   redraw_needed;
int                   window_line;
int                   last_line;

/***************************************************************
*  Make sure this request is for the main window.
***************************************************************/
if (token != dspl_descr->main_pad->token)
   return;

DEBUG18(fprintf(stderr, "cc_typing_chk: line %d, line = \"%s\"\n", line_no, line);)


for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
{
   /***************************************************************
   *  Find out some stuff about this display.
   *  What lines are being displayed, are we in the window and is
   *  this the special case of a partial display of the end
   *  of the data.
   ***************************************************************/
   redraw_needed = 0;
   window_line = line_no - walk_dspl->main_pad->first_line;
   last_line   = walk_dspl->main_pad->first_line+walk_dspl->main_pad->window->lines_on_screen;

   /***************************************************************
   *  If this is an insert on the screen, everything from that point
   *  on down has to be redrawn.  If this is an overwrite, we can
   *  replace just the line unless we are already impacted.  In that
   *  case we also do a partial screen redraw.
   ***************************************************************/
   if ((window_line >= 0) && (line_no < last_line))
      {
         if ((strchr(line, '\f') != NULL)  && (line_no <= walk_dspl->main_pad->file_line_no))
            {
               redraw_needed |= (MAIN_PAD_MASK & FULL_REDRAW);
            }
         else
            if ((walk_dspl->main_pad->impacted_redraw_line == -1) ||         /* not set yet or */
                (walk_dspl->main_pad->impacted_redraw_line == window_line))  /* same line as previous es */
               {
                  walk_dspl->main_pad->impacted_redraw_line = window_line;
                  redraw_needed |= (MAIN_PAD_MASK & PARTIAL_LINE);
               }
            else
               {
                  if ((walk_dspl->main_pad->impacted_redraw_line > window_line) || (walk_dspl->main_pad->impacted_redraw_line == -1))
                     walk_dspl->main_pad->impacted_redraw_line = window_line;
                  redraw_needed |= (MAIN_PAD_MASK & PARTIAL_REDRAW);
               }
      
         /* for typing always set partial line for impacted redraw */
         redraw_needed |= (MAIN_PAD_MASK & PARTIAL_LINE);
      }

   walk_dspl->main_pad->impacted_redraw_mask |= redraw_needed;
   DEBUG18(fprintf(stderr, "cc_typing_chk: display %d, impacted redraw 0x%X, line no %d\n", walk_dspl->display_no, redraw_needed, walk_dspl->main_pad->impacted_redraw_line);)

} /* end of walking list of secondary display descriptions */


} /* end of cc_typing_chk */


#if defined(PAD) && !defined(WIN32)
/************************************************************************

NAME:      cc_ttyinfo_send  -   Send the slave tty name in pad mode to the main process

PURPOSE:    This routine sends the tty name to the main process.
            It is called in the subtask which will exec the shell in ceterm.

PARAMETERS:

   1.  slave_tty_fd    - int (INPUT)
                         This is the file descriptor for the slave tty.


FUNCTIONS :

   1.   Get the tty name for the passed file descriptor.

   2.   send the name with a special header down the file descriptor pipe.


*************************************************************************/

void     cc_ttyinfo_send(int        slave_tty_fd)   /* input */
{
char                  tty_buff[MAXPATHLEN+2];
char                 *slave_tty_name;

slave_tty_name = ttyname(slave_tty_fd);

DEBUG16(
   if (!slave_tty_name)
      fprintf(stderr, "cc_ttyinfo_send: PID %d, slave tty name pointer is NULL (%s)\n", getpid(), strerror(errno));
   else
      fprintf(stderr, "cc_ttyinfo_send: PID %d, slave tty name to %s\n", getpid(), slave_tty_name);
)


/***************************************************************
*  Make sure we have a name to look at.
*  Then do a stat of that name.
***************************************************************/
if (!slave_tty_name)
   slave_tty_name = "";

snprintf(tty_buff, sizeof(tty_buff), "%c%s\n", CC_TTYINFO_CHAR, slave_tty_name);
write(slave_tty_fd, tty_buff, strlen(tty_buff));

} /* end of cc_ttyinfo_send */


/************************************************************************

NAME:      cc_ttyinfo_receive  -   Receive the slave tty name from cc_ttyinfo_send and save in the CC_LIST

PURPOSE:    This routine runs in the main ceterm process and receives the
            ttyname sent by cc_ttyinfo_send.

PARAMETERS:

   1.  slave_tty_name   - pointer to char (INPUT)
                         This is the name of the slave tty being passed to
                         the main process.


FUNCTIONS :

   1.   Do a stat on the passed ttyname.

   2.   Change the dev and inode for this window locally.
        Do not change the cclist.  It the window may not have been mapped
        and the window manager reparenting may not have taken place.

*************************************************************************/

void     cc_ttyinfo_receive(char       *slave_tty_name)   /* input */
{
struct stat           file_stats;
int                   rc;

DEBUG16(fprintf(stderr, "cc_ttyinfo_receive:  Got slave tty name name = \"%s\"\n", slave_tty_name);)

/***************************************************************
*  Make sure we have a name to look at.
*  Then do a stat of that name.
***************************************************************/
if (!slave_tty_name || (*slave_tty_name == '\0'))
   return;

rc = stat(slave_tty_name, &file_stats);
if (rc != 0)
   {
      DEBUG16(fprintf(stderr, "Cannot stat \"%s\", (%s)\n", slave_tty_name, strerror(errno));)
      return;
   }

DEBUG16(fprintf(stderr, "dev = 0x%X inode = 0x%X  ", file_stats.st_dev, file_stats.st_ino);)

file_stats.st_ino = adjust_tty_inode(slave_tty_name, file_stats.st_ino);

DEBUG16(fprintf(stderr, "adjusted inode = 0x%X\n", file_stats.st_ino);)


saved_dev         = file_stats.st_dev;
saved_inode       = file_stats.st_ino;
saved_host_id     = ce_gethostid();
saved_create_time = (unsigned int)time(NULL);
saved_file_size   = file_stats.st_size;

} /* end of cc_ttyinfo_receive */
#endif /* PAD and not WIN32 */

#endif /* ifndef PGM_XDMC */


/************************************************************************

NAME:      chk_cclist_data          -   Validate that a window manager window has good cclist data


PURPOSE:    This routine takes care of the case where the window manager keeps
            the reparenting (border) windows around and a previous Ce came down
            without being able to clear the property.

PARAMETERS:

   1.  display        - pointer to Display (INPUT)
                        This is the display we are talking to.

   2.  wm_window      - Window (INPUT - Xlib type)
                        This is the window manager window to get the data from

   2.  main_window      - Window (INPUT - Xlib type)
                        This is the Main window id for Ce

FUNCTIONS :
   1.   Read the CC_LIST property from the passed main window.

   2.   Check that the window exists and the property exists

   3.   Check that the property is the correct size

   4.   Check that the wm window in the property matches the passed window manager window.

   5.   If any check fails, delete the window manager window property
  

RETURNED VALUE:
   ok      -    int flag
                True   -   Window is ok
                False  -   Window is no good

*************************************************************************/

static int  chk_cclist_data(Display       *display,
                            Window         wm_window,
                            Window         main_window)
{
Atom              actual_type;
int               actual_format;
int               keydef_property_size;
unsigned long     nitems;
unsigned long     bytes_after;
char             *buff = NULL;
CC_LIST          *buff_pos;
int               rc;

check_atoms(display);

/***************************************************************
*  Read the property for the actual ce window.  It is a copy of
*  what is in the window manager window.
***************************************************************/
ce_error_bad_window_flag = False; /* in xerrorpos.h */
ce_error_generate_message = False;
DEBUG18(ce_error_generate_message = True;)

DEBUG9(XERRORPOS)
XGetWindowProperty(display,
                   main_window,
                   CE_PROPERTY(ce_cclist_atom),
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
ce_error_generate_message = True;
buff_pos = (CC_LIST *) buff;

#ifdef CE_LITTLE_ENDIAN
if (keydef_property_size == sizeof(CC_LIST))
   to_host_cc_list((CC_LIST *)buff, keydef_property_size);
#endif

if (ce_error_bad_window_flag || (keydef_property_size == 0) || (keydef_property_size != sizeof(CC_LIST)) ||
    ((buff_pos->wm_window != None) && (buff_pos->wm_window != wm_window) && !(IS_HUMMINGBIRD_EXCEED(display))))
   {
      DEBUG18(
          fprintf(stderr, "chk_cclist_data:  Window manager has bad CCLIST data, deleting it\n");
          fprintf(stderr, "ce_error_bad_window_flag= %d,  \nkeydef_property_size = %d, expecting %d\nwm_window from property = 0x%X, expecting 0x%X\n",
                          ce_error_bad_window_flag, keydef_property_size, sizeof(CC_LIST),
                          buff_pos->wm_window, wm_window);
      )
      if (buff)
         XFree(buff);
      buff = NULL;
      DEBUG9(XERRORPOS)
      XGetWindowProperty(display,
                         wm_window,
                         CE_PROPERTY(ce_cclist_atom),
                         0, /* offset zero from start of property */
                         INT_MAX / 4,
                         True, /* do delete the property */
                         AnyPropertyType,
                         &actual_type,
                         &actual_format,
                         &nitems,
                         &bytes_after,
                         (unsigned char **)&buff);
      rc = False;
   }
else
   {
      DEBUG18(fprintf(stderr, "chk_cclist_data:  Window ok\n");)
      rc = True;
   }

if (buff)
   XFree(buff);

return(rc);

} /* end of chk_cclist_data  */


/************************************************************************

NAME:      ce_gethostid   -   Get the Internet host id

PURPOSE:    This routine hashes the host name to make a reasonably unique number.

STATIC DATA:

   host_id    -  int
                 The host internet address is calculated once and saved.


FUNCTIONS :

   1.   If the host id is already calculated, return it.

   2.   Get the host ip address


RETURNED VALUE:

   host_id    -  int
                 The host internet address is returned.

*************************************************************************/

unsigned int  ce_gethostid(void)
{
static int            host_id = 0;
#define MAX_HOSTS 2
unsigned long  host_ids[MAX_HOSTS];
unsigned long  netmask[MAX_HOSTS];
int            count;

if (host_id)
   return(host_id);

count = netlist(host_ids, MAX_HOSTS, netmask);
if (count == 0)
   {
      DEBUG18(fprintf(stderr, "ce_gethostid: Can't get the network address (%s)\n", strerror(errno));)
      host_id = 5827323;  /* can't get the name, use some arbitrary value */
   }
else
   host_id = host_ids[0];
   
return(host_id);

} /* end of ce_gethostid */


#ifdef PAD
/************************************************************************

NAME:      adjust_tty_inode  -  special manipulation on the tty inode

PURPOSE:    This routine does some special manipulation on the slave tty
            inode in pad_mode to get a unique name on the IBM RS600 which
            has the same inode for all files.

PARAMETERS:

   1.  slave_tty_name      - pointer to char (INPUT)
                            This is the slave tty name for the shell task.
                            It is the control terminal name for the shell.

   2.  inode              - ino_t (INPUT)
                            This is the inode for the slave_tty_name from stat(2).

FUNCTIONS :

   1.   Add the tty number to the inode number to get a unique value.


*************************************************************************/

static ino_t   adjust_tty_inode(char     *slave_tty_name,
                                ino_t     inode)
{
int               len;
char             *p;
int               tty_no;

/***************************************************************
*  Add the pty number to the inode number.  This makes
*  the inode number unique on the RS6000, where all the
*  tty's have the same inode number (they are all soft links
*  to the same place).  It does not hurt the other boxes.
***************************************************************/
len = strlen(slave_tty_name);
if (len > 0)
   {
      p = &slave_tty_name[strlen(slave_tty_name)-1];
      tty_no = *p;
      if (len > 1)
         tty_no += *(p-1);
   }
else
   tty_no = 0;

inode += tty_no;

return(inode);

} /* end of adjust_tty_inode */
#endif /* PAD */


/************************************************************************

NAME:      cc_xdmc  -   Send a window a dm command.

PURPOSE:    This routine sends a dm command line to the window specified
            by a process id, or the dm window associated with this process.
            It is called only in program xdmc.

PARAMETERS:

   1.  display     - pointer to Display (INPUT)
                     This is the display handle needed for X calls.

   2.  pid         - int (INPUT)
                     This is the process id whose window we will look for.
                     VALUES:
                      0   -  send to window with the same process group or terminal as this process
                     >0   -  send to window owning the process group passed.

   3.  cmd_line    - pointer to char (INPUT)
                     This is the dm command to send.

   4.  window      - pointer to Window (INPUT/OUTPUT)
                     Send the commands to this window.
                     None  -  Do not use this parm
                     Window id 
                     On output, the window value is set to the window we send to.
                     If a window id is specified, this will be the same window.
                     On failure, the window id is set to null.

   5.  quiet       - int (INPUT)
                     Flag to show we do not want error messages.
                     VALUES:
                     True   -   Do not generate error messages
                     False  -   Generate error messages


FUNCTIONS :

   1.   If not pid was specified, use the process group id of this process.
 
   2.   Get the cc_list data from the X server.

   3.   Scan the cc list data to find the requested window.

   4.   Put the dm command into a request structure.

   5.   Send the structure to the desired window.

   6.   Check that the window was good.

RETURNED VALUE:
   rc   -  return code (int)
           0  -  command sent to a window
          -1  -  command not sent due to error

*************************************************************************/

int      cc_xdmc(Display    *display,    /* input */
                 int         pid,        /* input */
                 char       *cmd_line,   /* input */
                 Window     *window,     /* input */
                 int         quiet)      /* input */
{
CC_MSG               *request;
int                   request_size;
int                   i;
char                 *p;
int                   window_uid;
int                   ctl_term_window_uid;
Atom                  actual_type;
int                   actual_format;
int                   keydef_property_size;
unsigned long         nitems;
unsigned long         bytes_after;
char                 *buff = NULL;
CC_LIST              *buff_pos;
char                  msg[256];
int                   pgrp;
char                 *tty_name = NULL;
#if defined(PAD) && !defined(WIN32)
int                   rc;
struct stat           file_stats;
#endif
dev_t                 tty_dev = 0;
ino_t                 tty_inode = 0;
int                   host_id = 0;
static Window         last_validated_window = None;
int                   pid_passed = False;
Window                ctl_term_window = None;
Window                match_window = None;
Window                root_return;
Window                parent_return;
Window               *child_windows = NULL;
unsigned int          nchildren;


/***************************************************************
*  
*  If we have already validated this window, skip looking for it.
*  
***************************************************************/

if ((*window == None) || (*window != last_validated_window))
   {
      /***************************************************************
      *  
      *  If zero was passed as the pid, get the current process group.
      *  
      ***************************************************************/

      if (pid == 0)  /* added check for passid pid 3/10/94 RES */
         {
            pid = GETPPID(0);
            pgrp = GETPGRP(PID);
         }
      else
         {
            pid_passed = True;
            pgrp = -1;
         }

      DEBUG18(fprintf(stderr, "cc_xdmc: send to %d, group %d, cmd \"%s\"\n", pid, pgrp, cmd_line);)

      check_atoms(display);

#if defined(PAD) && !defined(WIN32)
      /***************************************************************
      *  
      *  See if we can find the control terminal.
      *  
      ***************************************************************/

      if (!pid_passed)
         {
            for (i = 0; (i < 3) && (tty_name == NULL); i++)
               tty_name = ttyname(i);

            if (tty_name)
               {
                  DEBUG18(fprintf(stderr, "cc_xdmc: tty %s  -  ", tty_name);)
                  rc = stat(tty_name, &file_stats);
                  if (rc != 0)
                     {
                        DEBUG18(fprintf(stderr, "Cannot stat %s, (%s)\n", tty_name, strerror(errno));)
                     }
                  else
                     {
                        tty_dev   = file_stats.st_dev;
                        tty_inode = adjust_tty_inode(tty_name, file_stats.st_ino);
                        host_id   = ce_gethostid();
                        DEBUG18(fprintf(stderr, "dev 0x%x inode 0x%X host 0x%X\n", tty_dev, tty_inode, host_id);)
                     }
               }
         }
#endif

      DEBUG18(
         if (*window == None)
            fprintf(stderr, "looking for pid = %d or pgrp = %d\n", pid, pgrp);
         else
            fprintf(stderr, "looking for window 0x%X\n", *window);
      )

      /***************************************************************
      *  
      *  Get the list of  windows which are "children of the root"
      *  (Could be the title of a new book)
      *
      ***************************************************************/
      DEBUG9(XERRORPOS)
      if (!XQueryTree(display,     /* input */
                      RootWindow(display, DefaultScreen(display)), /* input */
                      &root_return,            /* output */
                      &parent_return,          /* output */
                      &child_windows,          /* output */
                      &nchildren))             /* output */

         {
            dm_error("Could not get window list from server", DM_ERROR_BEEP);
            return(-1);
         }
      DEBUG18(fprintf(stderr, "cc_xdmc: %d windows hung off the root window, sizeof(Window) = %d\n", nchildren, sizeof(Window));)

      for (i = 0; i < (int)nchildren && match_window == None; i++)
      {
         /***************************************************************
         *  Get the cclist property for the window to see if it is a 
         *  Ce window.  Be prepared to deal
         *  with windows which have just gone away.
         ***************************************************************/
         ce_error_bad_window_flag = False;
         ce_error_generate_message = False;

         if (buff) /* may be set on second or later loops */
            XFree(buff);

         DEBUG9(XERRORPOS)
         XGetWindowProperty(display,
                            child_windows[i],
                            CE_PROPERTY(ce_cclist_atom),
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
         buff_pos = (CC_LIST *) buff;
         DEBUG18(fprintf(stderr, "window[%d]  0x%X  CCLIST size: %d, bad_window: %s\n", i, child_windows[i], keydef_property_size, (ce_error_bad_window_flag ? "True" : "False"));)

         if (ce_error_bad_window_flag || (keydef_property_size != sizeof(CC_LIST)))
            {
               DEBUG18(
                  if (ce_error_bad_window_flag)
                     fprintf(stderr, "Can't use window 0x%X, Bad window\n", child_windows[i]);
                  else
                     if (keydef_property_size == 0)
                        fprintf(stderr, "Can't use window 0x%X, Not registered as Ce\n", child_windows[i]);
                     else
                        fprintf(stderr, "Can't use window 0x%X, Property wrong size, is %d, should be %d\n", child_windows[i], keydef_property_size, sizeof(CC_LIST));
               )
               continue;
            }

         DEBUG9(XERRORPOS)
         DEBUG18(
         if (actual_type != None)
            p = XGetAtomName(display, actual_type);
         else
            p = "None";
         fprintf(stderr, "cc_xdmc:  Window = 0x%X, Size = %d, actual_type = %d (%s), actual_format = %d, nitems = %d, &bytes_after = %d\n",
                         child_windows[i],
                         keydef_property_size, actual_type, p,
                         actual_format, nitems, bytes_after);
         if (actual_type != None)
            XFree(p);
         )

#ifdef CE_LITTLE_ENDIAN
         to_host_cc_list((CC_LIST *)buff, keydef_property_size);
#endif

         if (chk_cclist_data(display, child_windows[i], buff_pos->main_window) == False)
             continue;

         /***************************************************************
         *  See if this window is the one we want from the pid or window.
         ***************************************************************/
         DEBUG18(
            fprintf(stderr, "Window 0x%X: WM window 0x%X pgrp: %d, shpid: %d, shpgrp: %d cepid: %d id 0x%X\n",
                    buff_pos->main_window, child_windows[i],
                    buff_pos->pgrp,
                    buff_pos->shpid, buff_pos->shgrp, buff_pos->cepid,
                    buff_pos->main_window);
                )

         if (*window == None)
            {
               if ((buff_pos->pgrp  == pid)  || (buff_pos->cepid == pid)  ||
                   (buff_pos->shgrp == pid)  || (buff_pos->shpid == pid)  ||
                   (buff_pos->pgrp  == pgrp) || (buff_pos->cepid == pgrp) ||
                   (buff_pos->shgrp == pgrp) || (buff_pos->shpid == pgrp))
                  {
                     DEBUG18(fprintf(stderr, "Found matching process\n");)
                     match_window = buff_pos->main_window;
                     window_uid = buff_pos->uid;
                  }
            }
         else
            if ((buff_pos->main_window == *window) || (buff_pos->wm_window == *window))
               {
                  DEBUG18(fprintf(stderr, "Found matching window\n");)
                  match_window = buff_pos->main_window;
                  window_uid = buff_pos->uid;
               }
            else
               if (ANY_WINDOW  == *window)
                  {
                     DEBUG18(fprintf(stderr, "Matched on any  window\n");)
                     match_window = buff_pos->main_window;
                     window_uid = buff_pos->uid;
                  }

         /***************************************************************
         *  If not found, check the control terminal device and inode
         ***************************************************************/
         DEBUG18(
            fprintf(stderr, "dev 0x%x inode 0x%X host 0x%X create 0x%X size %d\n",
                    buff_pos->dev, buff_pos->inode, buff_pos->host_id, buff_pos->create_time, buff_pos->file_size);
         )
         if ((buff_pos->dev == tty_dev) && (buff_pos->inode == tty_inode) && (buff_pos->host_id == (unsigned)host_id))
            {
               DEBUG18(fprintf(stderr, "Found matching control terminal\n");)
               ctl_term_window = buff_pos->main_window;
               ctl_term_window_uid = buff_pos->uid;
            }

#ifndef WIN32
         /***************************************************************
         *  Make sure the uid matches.
         ***************************************************************/
         if ((match_window != None) && ((geteuid()) && (window_uid != getuid()) && (window_uid != geteuid())))
            if (ANY_WINDOW  == *window)
               match_window = None;
            else
               {
                  dm_error("File is open under another userid", DM_ERROR_BEEP);
                  return(-1);
               }
#endif
      } /* end of for loop on i walking through children of the root*/

      /***************************************************************
      *  If we did not find the process id but found the control terminal
      *  use it.
      ***************************************************************/
      if ((match_window == None) && (ctl_term_window != None))
         {
            match_window = ctl_term_window;
            window_uid = ctl_term_window_uid;
         }

      /***************************************************************
      *  Make sure we found something.
      ***************************************************************/
      if (match_window == None)
         {
            if (*window == None)
               snprintf(msg, sizeof(msg), "Could not find window for pid %d", pid);
            else
               snprintf(msg, sizeof(msg), "Could not find window for window id 0x%X", *window);
            if (!quiet)
               dm_error(msg, DM_ERROR_BEEP);
            return(-1);
         }

      *window = match_window;
#ifndef WIN32
      if ((geteuid()) && (window_uid != getuid()) && (window_uid != geteuid()))
         {
            snprintf(msg, sizeof(msg), "Window is owned by another user, xdmc request cancelled");
            if (!quiet)
               dm_error(msg, DM_ERROR_BEEP);
            return(-1);
         }
#endif

      last_validated_window = *window;

      /***************************************************************
      *  We are done with the property, free it.
      ***************************************************************/
      if (buff)
         XFree(buff);

   } /* end of need to validate the window */

/***************************************************************
*  Get space for the request.  If the CE_MALLOC fails, return,
*  the message has already been output.
***************************************************************/

request_size = sizeof(CC_MSG) + strlen(cmd_line);
request_size = ROUND4(request_size);

request = (CC_MSG *)CE_MALLOC(request_size);
if (request == NULL)
   return(-1);

/***************************************************************
*  Copy the data from the parm list to the request.
***************************************************************/

request->msg_len = htonl(request_size);
request->lineno  = 0; /* arbitrary value */
request->type    = htons(CMD_XDMC);
request->count   = htons(1);
request->total_lines_infile = 0;
strcpy(request->line, cmd_line);

/***************************************************************
*  
*  Ship the request to the desired window.
*  
***************************************************************/

DEBUG18(fprintf(stderr, "cc_xdmc: sending cmd to window 0x%X len %d, \"%s\"\n", *window, request_size, cmd_line);)
ce_error_bad_window_flag = False;
DEBUG9(XERRORPOS)
XChangeProperty(display,
                *window,
                CE_PROPERTY(ce_cc_line_atom),
                XA_STRING,
                8, /* 8 bit quantities, we handle little endian ourselves */
                PropModeAppend,
                (unsigned char *)request, 
                request_size);
XFlush(display);
if (ce_error_bad_window_flag)  /* set in xerror.c */
   {
      snprintf(msg, sizeof(msg), "Bad window detected: 0x%X for pid %d.\n", *window, pid);
      if (!quiet)
         dm_error(msg, DM_ERROR_BEEP);
      return(-1);
   }

ce_error_generate_message = True;

if (child_windows)
   XFree((char *)child_windows);

free(request);
return(0);

} /* end of cc_xdmc */


#ifdef PGM_XDMC
/************************************************************************

NAME:      ccdump  -   Print the cc_list (called from xdmc.c)

PURPOSE:    This routine just dumps the cc list in the server.

PARAMETERS:

   1.  display     - pointer to Display (INPUT)
                     This is the display handle needed for X calls.

   2.  print_cmds  - int (flag) (INPUT)
                     If true, the commmand line for the window is dumped.


FUNCTIONS :

   1.   If the atoms are not already gotten, get them.

   2.   Get the cclist property hung off the root.

   3.   Print it's size and its contents.


*************************************************************************/

void ccdump(Display *display,
            int      print_cmds)
{
Atom                  actual_type;
int                   actual_format;
int                   keydef_property_size;
unsigned long         nitems;
unsigned long         bytes_after;
char                 *buff = NULL;
Window                root_return;
Window                parent_return;
Window               *child_windows = NULL;
unsigned int          nchildren;
int                   found_one = False;
int                   i;

check_atoms(display); /* make sure the atoms exist */

/***************************************************************
*  
*  Get the list of  windows which are "children of the root"
*  (Could be the title of a new book)
*
***************************************************************/
DEBUG9(XERRORPOS)
if (!XQueryTree(display,     /* input */
                RootWindow(display, DefaultScreen(display)), /* input */
                &root_return,            /* output */
                &parent_return,          /* output */
                &child_windows,          /* output */
                &nchildren))             /* output */
   {
      dm_error("Could not get window list from server", DM_ERROR_BEEP);
      return;
   }

DEBUG18(fprintf(stderr, "ccdump: %d windows hung off the root window, sizeof(Window) = %d\n", nchildren, sizeof(Window));)

for (i = 0; i < (int)nchildren; i++)
{
   /***************************************************************
   *  Get the cclist property for the window to see if it is a 
   *  Ce window.  Be prepared to deal
   *  with windows which have just gone away.
   ***************************************************************/
   ce_error_bad_window_flag = False;
   ce_error_generate_message = False;

   if (buff) /* may be set on second or later loops */
      {
         XFree(buff);
         buff = NULL;
      }

   DEBUG9(XERRORPOS)
   XGetWindowProperty(display,
                      child_windows[i],
                      CE_PROPERTY(ce_cclist_atom),
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
   DEBUG18(fprintf(stderr, "window[%d]  0x%X  CCLIST size: %d, bad_window: %s\n", i, child_windows[i], keydef_property_size, (ce_error_bad_window_flag ? "True" : "False"));)

   if (ce_error_bad_window_flag || (keydef_property_size != sizeof(CC_LIST)))
      continue;

#ifdef CE_LITTLE_ENDIAN
   to_host_cc_list((CC_LIST *)buff, keydef_property_size);
#endif

   if (keydef_property_size > 0)
      {
         if (chk_cclist_data(display, child_windows[i], ((CC_LIST *)buff)->main_window) == False)
            continue;
         found_one = True;
         dump_cc_data(buff, keydef_property_size, child_windows[i], stdout);
         if (print_cmds)
            dump_saved_cmd(display, ((Window)((CC_LIST *)buff)->main_window), stdout);
      }
} /* end of walking through the chilren of root */

if (!found_one)
   fprintf(stdout, "No Ce windows found\n");

if (child_windows)
   XFree((char *)child_windows);

ce_error_generate_message = True;

} /* end of ccdump */
#endif /* PGM_XDMC */


#if defined(DebuG) && !defined(PGM_XDMC)
/************************************************************************

NAME:      cc_debug_cclist_data  -   Print this windows cc_list info


PURPOSE:    This routine just dumps the current cc data for debugging.
            it is a operation of the debug command.

PARAMETERS:
   1.  display     - pointer to Display (INPUT)
                     This is the display handle needed for X calls.

   2.  wm_window   - Window (INPUT)
                     This is the Window manager window for this window.


FUNCTIONS :
   1.   Get the cclist data

   2.   Print the cclist data 

   3.   Print the saved command.


*************************************************************************/

void cc_debug_cclist_data(Display       *display,
                          Window         wm_window)
{
CC_LIST          *cc_data;
int               new;

cc_data = get_cclist_data(display, wm_window, &new);
if (new)
   fprintf(stderr, "get_cclist_data generated the following data\n");

dump_cc_data((char *)cc_data, sizeof(CC_LIST), wm_window, stderr);
dump_saved_cmd(display, ((Window)cc_data->main_window), stderr);

} /* end of cc_debug_cclist_data */
#endif  /* defined(DebuG) && !defined(PGM_XDMC) */


#if defined(DebuG) || defined(PGM_XDMC)
/************************************************************************

NAME:      dump_saved_cmd  -   Dump the sessions manager saved command


PURPOSE:    This routine just dumps the current cc data for debugging.
            it is a operation of the debug command.

PARAMETERS:
   1.  display     - pointer to Display (INPUT)
                     This is the display handle needed for X calls.

   2.  ce_window   - Window (INPUT)
                     This is the ce main window id to get the data for.


FUNCTIONS :
   1.   Get the Client machine and WM_command properties.

   2.   Print the data.

*************************************************************************/

static void dump_saved_cmd(Display *display,
                           Window   ce_window,
                           FILE    *stream)
{
Atom                  actual_type;
int                   actual_format;
int                   property_size;
unsigned long         nitems;
unsigned long         bytes_after;
char                 *buff = NULL;
char                 *buff_pos;
char                 *buff_end;

XGetWindowProperty(display,
                   ce_window,
                   XA_WM_CLIENT_MACHINE,
                   0, /* offset zero from start of property */
                   INT_MAX / 4,
                   False, /* do not delete the property */
                   AnyPropertyType,
                   &actual_type,
                   &actual_format,
                   &nitems,
                   &bytes_after,
                   (unsigned char **)&buff);

property_size = nitems * (actual_format / 8);

if (property_size != 0)
   fprintf(stream, "     (%s) ", buff);

if (buff)
   {
      XFree(buff);
      buff = NULL;
   }

XGetWindowProperty(display,
                   ce_window,
                   XA_WM_COMMAND,
                   0, /* offset zero from start of property */
                   INT_MAX / 4,
                   False, /* do not delete the property */
                   AnyPropertyType,
                   &actual_type,
                   &actual_format,
                   &nitems,
                   &bytes_after,
                   (unsigned char **)&buff);

property_size = nitems * (actual_format / 8);
DEBUG18(fprintf(stderr, "window 0x%X size: %d\n", ce_window, property_size);)

if (property_size != 0)
   {
      buff_end  = buff + property_size;
      buff_pos  = buff;

      while(buff_pos < buff_end)
      {
         fprintf(stream, "%s ", buff_pos);
         buff_pos += strlen(buff_pos) + 1;
      }
   }

fprintf(stream, "\n");

if (buff)
   {
      XFree(buff);
      buff = NULL;
   }

} /* end of dump_saved_cmd */

#endif /* defined(DebuG) || defined(PGM_XDMC) */


/************************************************************************

NAME:      check_atoms  -   Make sure the needed atom ids have been gotten.

PURPOSE:    This routine checks to see that the atom ids needed by cc have
            been gotten from the server.

PARAMETERS:

   1.  display     - pointer to Display (INPUT)
                     This is the display handle needed for X calls.


FUNCTIONS :

   1.   If the atoms are already gotten, do nothing.

   2.   Get each atom from the server.


*************************************************************************/

static void check_atoms(Display   *display)
{
/***************************************************************
*  If the cc_list buffer atom does not already exist, create it.
*  This is the property used to keep track of what files are being
*  edited.
***************************************************************/

if (CE_PROPERTY(ce_cc_atom) == 0)
   {
      DEBUG9(XERRORPOS)
      CE_PROPERTY(ce_cc_atom)             = XInternAtom(display, CE_CC_ATOM_NAME, False);
      DEBUG9(XERRORPOS)
      CE_PROPERTY(ce_cc_line_atom)        = XInternAtom(display, CE_CC_LINE_ATOM_NAME, False);
      DEBUG9(XERRORPOS)
      CE_PROPERTY(ce_cclist_atom)         = XInternAtom(display, CE_CCLIST_ATOM_NAME, False);

      DEBUG18(fprintf(stderr, "ce_cc_atom = %d, ce_cclist_atom = %d, ce_cc_line_atom = %d\n",
                              CE_PROPERTY(ce_cc_atom), CE_PROPERTY(ce_cclist_atom), CE_PROPERTY(ce_cc_line_atom));
      )
   }
} /* end of check_atoms */


#ifndef PGM_XDMC
/************************************************************************

NAME:      cc_to_next  -   Move to the next window.

PURPOSE:    This routine searches the cc list for the next window and 
            sends a request to the window for it to warp the pointer to
            itself.

PARAMETERS:

   1.  input_flag  - int (INPUT)
                     This flag is passed with the request, it tells the called
                     session to move an input window or just the main window.
                     True  -  Go to the input window.
                     False -  Go to the main window.


FUNCTIONS :

   1.   Get the CC_LIST property from the server root window.
 
   2.   Scan the list looking for this window.

   3.   Take the window after this list.

   4.   Send the "to window" request to the chosen window.

   5.   If the window is bad, try again.


*************************************************************************/

void     cc_to_next(int         input_flag)
{
CC_MSG               *request;
int                   request_size;
int                   i;
Atom                  actual_type;
int                   actual_format;
int                   keydef_property_size;
unsigned long         nitems;
unsigned long         bytes_after;
char                 *buff = NULL;
CC_LIST              *buff_pos;
char                  msg[256];
Window                to_window = None;
Window                first_window = None;
int                   bingo = False;
Window                root_return;
Window                parent_return;
Window               *child_windows = NULL;
unsigned int          nchildren;


DEBUG18(fprintf(stderr, "cc_to_next: To next %swindow\n", (input_flag ? "input " : ""));)

/***************************************************************
*  
*  Get the list of  windows which are "children of the root"
*  (Could be the title of a new book)
*
***************************************************************/
DEBUG9(XERRORPOS)
if (!XQueryTree(dspl_descr->display,     /* input */
                RootWindow(dspl_descr->display, DefaultScreen(dspl_descr->display)), /* input */
                &root_return,            /* output */
                &parent_return,          /* output */
                &child_windows,          /* output */
                &nchildren))             /* output */
   {
      dm_error("Could not get window list from server", DM_ERROR_BEEP);
      return;
   }

for (i = 0; i < (int)nchildren; i++)
{
   /***************************************************************
   *  Get the cclist property for the window to see if it is a 
   *  Ce window.  Be prepared to deal
   *  with windows which have just gone away.
   ***************************************************************/
   ce_error_bad_window_flag = False;
   ce_error_generate_message = False;

   if (buff) /* may be set on second or later loops */
      XFree(buff);

   DEBUG9(XERRORPOS)
   XGetWindowProperty(dspl_descr->display,
                      child_windows[i],
                      CE_PROPERTY(ce_cclist_atom),
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
   buff_pos = (CC_LIST *) buff;

   if (ce_error_bad_window_flag || (keydef_property_size != sizeof(CC_LIST)))
      continue;

#ifdef CE_LITTLE_ENDIAN
   to_host_cc_list((CC_LIST *)buff, keydef_property_size);
#endif

   DEBUG18(dump_cc_data(buff, keydef_property_size, child_windows[i], stderr);)

   if (chk_cclist_data(dspl_descr->display, child_windows[i], buff_pos->main_window) == False)
      continue;

   if ((first_window == None) && (buff_pos->iconned == False) && (buff_pos->obscured == False))
      first_window = buff_pos->main_window;

   if (bingo && (buff_pos->iconned == False) && (buff_pos->obscured == False))
      {
         to_window = buff_pos->main_window;
         break;
      }
   if (buff_pos->main_window == dspl_descr->main_pad->x_window)
      bingo = True;

} /* end of walking through the children of root */


/***************************************************************
*  
*  If the to_window was not set, this may have been the last
*  window in the list,  In this case we go to the first window.
*  If first window was not set, the property was not there and
*  we will flag an error.
*  
***************************************************************/

if (to_window == None)
   if (first_window != None)
      to_window = first_window;
   else
      {
         snprintf(msg, sizeof(msg), "cc_to_next:  Cannot find any Ce windows or all Ce windows are iconned or obscurred, command failed");
         dm_error(msg, DM_ERROR_BEEP);
         return;
      }

/***************************************************************
*  If the target window is this window (nobody else home),
*  forget the whole thing, we are in the correct place.
***************************************************************/
if (to_window == dspl_descr->main_pad->x_window)
   return;

/***************************************************************
*  Get space for the request.  If the CE_MALLOC fails, return,
*  the message has already been output.
***************************************************************/
request_size = sizeof(CC_MSG);
request_size = ROUND4(request_size);

request = (CC_MSG *)CE_MALLOC(request_size);
if (request == NULL)
   return;

/***************************************************************
*  Build the request.
***************************************************************/
request->msg_len = htonl(request_size);
request->lineno  = 0; /* arbitrary value */
request->type    = htons(CMD_TOWINDOW);
request->count   = htons((short)input_flag);
request->total_lines_infile = 0;
request->line[0] = '\0';

/***************************************************************
*  
*  Ship the request the target window so he will grab the cursor.
*  
***************************************************************/

DEBUG18(fprintf(stderr, "cc_to_next:  sending cmd to window 0x%X len %d\n", to_window, request_size);)
ce_error_bad_window_flag = False;
DEBUG9(XERRORPOS)
XChangeProperty(dspl_descr->display,
                to_window,
                CE_PROPERTY(ce_cc_line_atom),
                XA_STRING,
                8, /* 8 bit quantities, we handle little endian ourselves */
                PropModeAppend,
                (unsigned char *)request, 
                request_size);
XFlush(dspl_descr->display);
if (ce_error_bad_window_flag)
   DEBUG18(fprintf(stderr, "cc_to_next: bad window deteted: 0x%X\n", to_window);)
else
   force_out_window(dspl_descr);

if (child_windows)
   XFree((char *)child_windows);

free(request);

} /* end of cc_to_next */


/************************************************************************

NAME:      cc_icon  -   Mark a window iconned or non-iconned

PURPOSE:    This routine sets or clears the the iconned flag in the 
            ce_cc_visibility property.

PARAMETERS:

   1.  icon_flag   - int (INPUT)
                     This flag specifies whether the icon flag should be set
                     or unset.
                     True  -  Window is iconned. (unmapped)
                     False -  Window is un-iconned (mapped)


FUNCTIONS :

   1.   Rebuild the ce_cc_visibility property value and write it out.
 
   2.   Save the icon value statically within this module.



*************************************************************************/

void     cc_icon(int         icon_flag)
{
int               new;
CC_LIST          *buff_pos;

DEBUG18(fprintf(stderr, "@cc_icon, icon_flag: %d, display: %d, wm_window: 0x%X\n", icon_flag, dspl_descr->display_no, dspl_descr->wm_window);)

buff_pos =  get_cclist_data(dspl_descr->display,  dspl_descr->wm_window, &new);
if (buff_pos && (buff_pos->iconned != icon_flag))
   {
      buff_pos->iconned     = icon_flag;
      buff_pos->dev         = saved_dev;
      buff_pos->inode       = saved_inode;
      buff_pos->host_id     = saved_host_id;
      buff_pos->create_time = saved_create_time;
      buff_pos->file_size   = saved_file_size;
      buff_pos->uid         = getuid();
      buff_pos->main_window = dspl_descr->main_pad->x_window;
      buff_pos->cepid       = getpid();
      buff_pos->display_no  = dspl_descr->display_no;
      buff_pos->version     = CC_LIST_VERSION;
      buff_pos->wm_window   = dspl_descr->wm_window;

      put_cclist_data(dspl_descr->display, dspl_descr->wm_window, buff_pos);
      if (new)
         free((char *)buff_pos);
      else
         XFree((char *)buff_pos);
   }

} /* end of cc_icon */


/************************************************************************

NAME:      cc_is_iconned  -   Check if the window is iconned

PURPOSE:    This routine checks the iconned flag to see if the window
            is iconned.

PARAMETERS:
   none

GLOBAL DATA:
   The current display description defined in windowdefs.h is accessed
   by this routine.


FUNCTIONS :

   1.   Get the current cclist data for this window.
 
   2.   Return the iconned flag from this data.

RETURNED VALUE:
   iconned   -  int
                True   -  The window is iconned
                False  -  The window is not iconned

*************************************************************************/

int      cc_is_iconned(void)
{
int               new;
int               iconned = False;
CC_LIST          *buff_pos;

DEBUG18(fprintf(stderr, "@cc_is_iconned, display: %d, wm_window: 0x%X", dspl_descr->display_no, dspl_descr->wm_window);)

buff_pos =  get_cclist_data(dspl_descr->display,  dspl_descr->wm_window, &new);
if (buff_pos)
   {
      iconned = buff_pos->iconned;

      if (new)
         free((char *)buff_pos);
      else
         XFree((char *)buff_pos);
   }

return(iconned);

} /* end of cc_is_iconned */


/************************************************************************

NAME:      cc_obscured  -   Mark a window obscurred or non-obscurred

PURPOSE:    This routine sets or clears the the obscured flag in the 
            ce_cc_visibility property.   It is set when the window is either
            partially or fully obscured.

PARAMETERS:

   1.  obscured   - int (INPUT)
                     This flag specifies whether the obscured flag should be set
                     or unset.
                     True  -  Window is obscured (partially or fully obscured)
                     False -  Window is un-obscured (fully visible)


FUNCTIONS :

   1.   Rebuild the ce_cc_visibility property value and write it out.
 
   2.   Save the obscured value statically within this module.



*************************************************************************/

void     cc_obscured(int         obscured)
{
int               new;
CC_LIST          *buff_pos;

DEBUG18(fprintf(stderr, "@cc_obscured, icon_flag: %d, display: %d, wm_window: 0x%X\n", obscured, dspl_descr->display_no, dspl_descr->wm_window);)

buff_pos =  get_cclist_data(dspl_descr->display,  dspl_descr->wm_window, &new);
if (buff_pos && (buff_pos->obscured != obscured))
   {
      buff_pos->obscured    = obscured;
      buff_pos->dev         = saved_dev;
      buff_pos->inode       = saved_inode;
      buff_pos->host_id     = saved_host_id;
      buff_pos->create_time = saved_create_time;
      buff_pos->file_size   = saved_file_size;
      buff_pos->uid         = getuid();
      buff_pos->main_window = dspl_descr->main_pad->x_window;
      buff_pos->cepid       = getpid();
      buff_pos->display_no  = dspl_descr->display_no;
      buff_pos->version     = CC_LIST_VERSION;
      buff_pos->wm_window   = dspl_descr->wm_window;

      put_cclist_data(dspl_descr->display, dspl_descr->wm_window, buff_pos);
      if (new)
         free((char *)buff_pos);
      else
         XFree((char *)buff_pos);
   }

} /* end of cc_obscured */

/************************************************************************

NAME:      cc_receive_cc_request  -   process a request to cc sent via property change from another window


PURPOSE:    This routine processes a request to do a cc.  The request is recieved
            via property notify.

PARAMETERS:

   1.  event       - pointer to XEvent (INPUT)
                     The property notify event which caused this routine to be called.

   2.  dspl_descr  - pointer to DISPLAY_DESCR (INPUT / OUTPUT in the child process)
                     This is the current display description.  It is passed through to dm_cc.

   3.  edit_file   - pointer to char (INPUT)
                     This is the name of the file being edited.  It is needed if
                     a cc_request from another window is requesting us to open
                     for update when we are currently readonly.
                     It is passed through to dm_cc.

   4.  resource_class - pointer to char (INPUT)
                     This is the X resource class for this program used in parsing
                     the arguments with the X routines.  We happen to know the
                     resource class for ce is "Ce".
                     It is passed through to dm_cc.

   5.  arg0        - pointer to char (INPUT)
                     The command name, passed to lower level routines.

FUNCTIONS :
   1.   If the property notify indicates deleted, return.

   2.   Read the passed property with a Delete after read option.

   3.   For each null terminated string in the property, parse the DM line and
        call the CC command.

RETURNED VALUE:
   rc    -  The returned value indicates whether the cc was successfully created.
            In the event of a failure, a message has already been output
             0  -  CC worked.
            -1  -  Failure, message already output

*************************************************************************/

int  cc_receive_cc_request(XEvent          *event,                  /* input */
                           DISPLAY_DESCR   *dspl_descr,             /* input / output  in child process */
                           char            *edit_file,              /* input  */
                           char            *resource_class,         /* input  */
                           char            *arg0)                   /* input  */
{
char             *p;
Atom              actual_type;
int               actual_format;
int               keydef_property_size;
unsigned long     nitems;
unsigned long     bytes_after;
char             *buff = NULL;
char             *buff_pos;
char             *buff_end;
DMC              *dmc;
int               rc = 0;

check_atoms(event->xproperty.display); /* make sure the atoms exist */

/***************************************************************
*
*  Make sure this is not a delete property change.  We are not
*  interested in the property going away.
*
***************************************************************/
if (event->xproperty.state == PropertyDelete)
   return(-1);

DEBUG9(XERRORPOS)
DEBUG18(fprintf(stderr, "cc_receive_cc_request: PID %d, Received cc request to window 0x%X, property =  %d (%s), state = %s\n",
               getpid(), event->xproperty.window,
               event->xproperty.atom, (p = XGetAtomName(event->xproperty.display, event->xproperty.atom)),
               ((event->xproperty.state == PropertyDelete) ? "Deleted" : "NewValue"));
       XFree(p);
)

/***************************************************************
*  
*  Read the property in and delete it.
*  
***************************************************************/

DEBUG9(XERRORPOS)
XGetWindowProperty(event->xproperty.display,
                   event->xproperty.window,
                   event->xproperty.atom,
                   0, /* offset zero from start of property */
                   INT_MAX / 4,
                   True, /* delete the property after reading it */
                   XA_STRING,
                   &actual_type,
                   &actual_format,
                   &nitems,
                   &bytes_after,
                   (unsigned char **)&buff);

keydef_property_size = nitems * (actual_format / 8);
buff_pos = buff;
buff_end = buff + keydef_property_size;


DEBUG9(XERRORPOS)
DEBUG18(
   if (actual_type != None)
      p = XGetAtomName(event->xproperty.display, actual_type);
   else
      p = "None";
   fprintf(stderr, "cc_receive_cc_request:  Size = %d, actual_type = %d (%s), actual_format = %d, nitems = %d, &bytes_after = %d\n",
                   keydef_property_size, actual_type, p,
                   actual_format, nitems, bytes_after);
   if (actual_type != None)
      XFree(p);
)

if (keydef_property_size == 0)
   {
      if (buff)
         XFree(buff);
      return(-1);
   }

/***************************************************************
*  
* Handle the first request.  If there are more execute them later.
*  
***************************************************************/

if (buff_pos < buff_end)
   {
      DEBUG18(fprintf(stderr, "cc_line:  %s\n", buff_pos);)
      dmc = parse_dm_line(buff_pos, True, 0, dspl_descr->escape_char, dspl_descr->hsearch_data);
      if (dmc != NULL)
         rc = dm_cc(dmc, dspl_descr, edit_file, resource_class, arg0);

      buff_pos += strlen(buff_pos) + 1;
      if (buff_pos < buff_end)
         {
            store_cmds(dspl_descr, buff_pos, False); /* in getevent.c */
            fake_keystroke(dspl_descr);              /* in getevent.c */
         }
   }

XFree(buff);
return(rc); /* the parent process returns here */

} /* end of cc_receive_cc_request */


/************************************************************************

NAME:      dm_cc  -  perform the cc command (create copy)

PURPOSE:    This routine splits off a new process which is editing the same file.
            It registers the file with the X server to establish communication between
            copies of the file running.

PARAMETERS:

   1.  dmc         - pointer to DMC (INPUT)
                     This is the command structure for the ce or cv command.
                     The dash options are currently ignored.

   2.  dspl_descr  - pointer to DISPLAY_DESCR (INPUT / OUTPUT in the child process)
                     This is the current display description.  It is the root pointer
                     for information about all the windows.

   3.  edit_file   - pointer to char (INPUT)
                     This is the name of the file being edited.  It is needed if
                     a cc_request from another window is requesting us to open
                     for update when we are currently readonly.

   4.  resource_class - pointer to char (INPUT)
                     This is the X resource class for this program used in parsing
                     the arguments with the X routines.  We happen to know the
                     resource class for ce is "Ce".

   5.  arg0        - pointer to char (INPUT)
                     The command name, passed to lower level routines.


FUNCTIONS :
   1.   Initialize a new display description

   2.   Process the X and users supplied options for the new window.
        If the display is different, try to pick up the X options
        from the new display if possible.

   3.   Flag any bad options.

   4.   Initialize the key definitions for the new window.  These may
        be the same or different than the existing key definitions.

   5.   Add the new display description to the circular linked list of
        display descriptions.

RETURNED VALUE:
   rc    -  The returned value indicates whether the cc was successfully created.
            In the event of a failure, a message has already been output
             0  -  CC worked.
            -1  -  Failure, message already output
        

*************************************************************************/

int   dm_cc(DMC             *dmc,                    /* input  */
            DISPLAY_DESCR   *dspl_descr,             /* input / output  in child process */
            char            *edit_file,              /* input  */
            char            *resource_class,         /* input  */
            char            *arg0)                   /* input  */
{
int                   i;
int                   rc;
char                  link[MAXPATHLEN];
char                 *lcl_argv[MAX_CC_ARGS];
int                   lcl_argc;
DISPLAY_DESCR        *new_dspl;
int                   new_display = False;
char                 *old_display;
char                 *p;

DEBUG18(
   fprintf(stderr, "cmd = %s\n",  dmsyms[dmc->ce.cmd].name);
   for (i = 0; i < dmc->cv.argc; i++)
      fprintf(stderr, "argv[%02d] = %s\n", i, dmc->cv.argv[i]);
)

/***************************************************************
*  
*  Get a new display description
*  
***************************************************************/

new_dspl = init_display();
if (!new_dspl)
   return(-1);

/***************************************************************
*  Process any options which were passed in with the cc command.
*  Copy them to a local area which will get modified.  The parms 
*  do not get modified, but the array does.
***************************************************************/
lcl_argc = dmc->cc.argc;
if (lcl_argc > MAX_CC_ARGS)
   lcl_argc = MAX_CC_ARGS;

for (i = 0; i < lcl_argc; i++)
    lcl_argv[i] = dmc->cc.argv[i];

/***************************************************************
*  To get X resources correctly, lcl_argv[0] must be "ce" or "cv".
*  dm_cc can be called witha ce, cv, or cc command structure.
*  These are all the same structure.  They better be!
***************************************************************/
if (dmc->cc.cmd == DM_cc)
   if (dspl_descr->pad_mode)
      lcl_argv[0] = "ceterm";
   else
      if (WRITABLE(dspl_descr->main_pad->token))
         lcl_argv[0] = "ce";
      else
         lcl_argv[0] = "cv";

/***************************************************************
*  Make the current display the default for a little while.
*  This way, if a cc or cv is done in a window which was a cc
*  itself, the display we want is used.
***************************************************************/
old_display = getenv("DISPLAY");
if (!old_display)
   old_display = ""; /* avoid memory fault */
if (strcmp(old_display, DisplayString(dspl_descr->display)) != 0)
   {
      snprintf(link, sizeof(link), "DISPLAY=%s", DisplayString(dspl_descr->display));
      DEBUG18(fprintf(stderr, "cc: Temporarily Using %s\n", link);)
      p = malloc_copy(link);
      if (p)
         putenv(p);
      else
         return(-1); /* out of memory, msg already output */
   }
else
   old_display = NULL;

/***************************************************************
*  Process the options for the new window.
*  We will open a new display in this code.
***************************************************************/
getxopts(cmd_options,
         OPTION_COUNT,
         defaults,
         &lcl_argc,
         lcl_argv,
         new_dspl->option_values,
         new_dspl->option_from_parm,
         &new_dspl->display,
         DISPLAY_IDX,
         NAME_IDX,
         resource_class);

/***************************************************************
*  Put the DISPLAY environment variable back.
***************************************************************/
if (old_display != NULL)
   {
      snprintf(link, sizeof(link), "DISPLAY=%s", old_display);
      DEBUG18(fprintf(stderr, "cc: Reseting %s\n", link);)
      p = malloc_copy(link);
      if (p)
         putenv(p);
      else
         return(-1); /* out of memory, msg already output */
   }

/***************************************************************
*  See if the new DISPLAY opened.
***************************************************************/
if (!(new_dspl->display))
   {
      DEBUG9(XERRORPOS)
      snprintf(link, sizeof(link), "(%s) Can't open display '%s' (%s)",
                   dmsyms[dmc->cc.cmd].name, XDisplayName(new_dspl->option_values[DISPLAY_IDX]),
                   strerror(errno));
      dm_error(link, DM_ERROR_BEEP);
      del_display(new_dspl);
      return(-1);
   }

new_display = !same_display(new_dspl->display, dspl_descr->display); /* in pastebuf.c */
DEBUG18(fprintf(stderr, "dm_cc:  cc is to %s display\n", (new_display ? "different" : "same"));)

/***************************************************************
*  See if there were any left over (bad) options.
***************************************************************/
if (lcl_argc > 1)
   {
      for (i = 1; i < lcl_argc; i++)
         if (lcl_argv[i][0] == '-')
            {
               snprintf(link, sizeof(link), "Invalid or non-unique option \"%s\"", lcl_argv[i]);
               dm_error(link, DM_ERROR_BEEP);
            }
         else
            {
               snprintf(link, sizeof(link), "Extraneous parm  \"%s\"", lcl_argv[i]);
               dm_error(link, DM_ERROR_BEEP);
            }
      return(-1);
   }

if (new_dspl->option_values[VERSION_IDX] != NULL)
   {
      dm_error("-version option not available", DM_ERROR_BEEP);
      return(-1);
   }

/* RES 9/22/97, check for -cmd string in quotes */
if (new_dspl->option_values[CMD_IDX] != NULL)
   {
      if ((new_dspl->option_values[CMD_IDX][0] == '"') || (new_dspl->option_values[CMD_IDX][0] == '\''))
         {
            new_dspl->option_values[CMD_IDX][0] = ' ';
            i = strlen(new_dspl->option_values[CMD_IDX]) - 1;
            if ((new_dspl->option_values[CMD_IDX][i] == '"') || (new_dspl->option_values[CMD_IDX][i] == '\''))
               new_dspl->option_values[CMD_IDX][i] = '\0';
         }
   }

if (new_dspl->option_values[HELP_IDX] != NULL)
   {
      dm_error("-help option not available", DM_ERROR_BEEP);
      return(-1);
   }

if (new_dspl->option_values[RELOAD_IDX] != NULL)
   {
      dm_error("-reload option not available", DM_ERROR_BEEP);
      return(-1);
   }

if (new_dspl->option_values[LOAD_IDX] != NULL)
   {
      dm_error("-load option not available", DM_ERROR_BEEP);
      return(-1);
   }


/***************************************************************
*  
*  If a -display parameter was supplied, try to read the
*  server key definitions from the new server.  If we cannot,
*  just use the existing set.  init_display has given us a
*  hsearch_data field which is null to begin with.
*  
***************************************************************/

if (new_display)
   {
      DEBUG18(fprintf(stderr, "dm_cc:  Opening a new display\n");)
      new_dspl->hsearch_data = hcreate(HASH_SIZE);
      if (new_dspl->hsearch_data)
         {
            rc = load_server_keydefs(new_dspl->display, new_dspl->properties, KEYDEF_PROP, new_dspl->hsearch_data);
            if (rc == 0)
               {
                  DEBUG18(fprintf(stderr, "dm_cc:  Loaded new keydefs from display %s\n", XDisplayName(new_dspl->option_values[DISPLAY_IDX]));)
               }
            else
               {
                  hdestroy(new_dspl->hsearch_data);
                  new_dspl->hsearch_data = NULL;
               }
         }
   }

if (!new_dspl->hsearch_data)
   {
      DEBUG18(fprintf(stderr, "dm_cc:  Using existing key defs\n");)
      new_dspl->hsearch_data = dspl_descr->hsearch_data;
      if (!new_display) /* RES 2/25/1999 Don't copy properties for different display, atom numbers are different */
         memcpy((char *) new_dspl->properties, (char *) dspl_descr->properties, sizeof(PROPERTIES));
      /*new_dspl->properties   = dspl_descr->properties; RES 8/18/94 messes up event masks */
   }


/***************************************************************
*  
*  Add the new display description to the list.  Make the
*  main pad token be the same as the existing main pad token.
*  
***************************************************************/

add_display(dspl_descr, new_dspl);
new_dspl->main_pad->token   = dspl_descr->main_pad->token;
new_dspl->xsmp_private_data = dspl_descr->xsmp_private_data;
new_dspl->xsmp_active       = dspl_descr->xsmp_active;

/***************************************************************
*  Move the window so it is not right over the last window
*  and position us at the top of the dataset.  Use the geo parm if
*  it was supplied.
***************************************************************/
if (lcl_argc < 0 || OPTION_VALUES[GEOMETRY_IDX] == NULL)
   {
      snprintf(link, sizeof(link), "%dx%d+%d+%d", dspl_descr->main_pad->window->width, dspl_descr->main_pad->window->height, dspl_descr->main_pad->window->x+30, dspl_descr->main_pad->window->y+30);
      new_dspl->option_values[GEOMETRY_IDX] = malloc_copy(link);
   }

/***************************************************************
*  Put the CC main pad in the same place in the file
*  as the original to start.
***************************************************************/

new_dspl->main_pad->file_col_no     = 0;
new_dspl->main_pad->file_line_no    = dspl_descr->main_pad->file_line_no;
new_dspl->main_pad->first_line      = dspl_descr->main_pad->first_line;
new_dspl->main_pad->first_char      = dspl_descr->main_pad->first_char;
new_dspl->main_pad->buff_modified   = dspl_descr->main_pad->buff_modified;
new_dspl->main_pad->buff_ptr        = dspl_descr->main_pad->buff_ptr;
/*new_dspl->main_pad->window->display = &new_dspl->display;*/

new_dspl->cursor_buff->current_win_buff = new_dspl->main_pad;
new_dspl->cursor_buff->which_window = new_dspl->cursor_buff->current_win_buff->which_window;
new_dspl->cursor_buff->x            = dspl_descr->cursor_buff->x;
new_dspl->cursor_buff->y            = dspl_descr->cursor_buff->y;
new_dspl->cursor_buff->win_col_no   = 0;
new_dspl->cursor_buff->win_line_no  = new_dspl->main_pad->file_line_no - new_dspl->main_pad->first_line;
new_dspl->cursor_buff->up_to_snuff  = 0;

#ifdef WIN32
new_dspl->term_mainpad_mutex = dspl_descr->term_mainpad_mutex;
#endif


/***************************************************************
*  This is used when the command is passed in
*  via cc_receive_cc_request
***************************************************************/
if (lcl_argc > 0)
   {
      if ((dmc->cc.argv[0][1] == 'e') && !READLOCK)
         {
            if (!WRITABLE(new_dspl->main_pad->token) && ok_for_output(edit_file, &i/* not used */, dspl_descr))
               WRITABLE(new_dspl->main_pad->token) = True;
         }
   }

cc_ce = True;

store_cmds(new_dspl, "msg 'This is a CC window'", False);
dm_error("CC window created from this window", DM_ERROR_BEEP);

/***************************************************************
*  RES 2/3/1999 - moved into this routine from callers.
*  Go do the X requests to create the new windows.
***************************************************************/
win_setup(new_dspl,
          edit_file,
          resource_class,
          WRITABLE(new_dspl->main_pad->token),
          arg0);

return(0);

} /* end of dm_cc  */


/************************************************************************

NAME:      send_api_stats  -   Send current stats to the ceapi


PURPOSE:    This responds to a client message event from the ceapi by
            sending the window the current message data.

PARAMETERS:

   1.  main_pad        - pointer to PAD_DESCR (INPUT)
                         This is the main pad description from which the
                         sent data will be extracted.

   2.  event           - pointer to XEvent (INPUT / Modified)
                         This is the event which caused this routine to
                         be called.  It is filled in and sent back.

FUNCTIONS :

   1.  Copy the relevent data to the structure.

   2.  Send the data back to the api.


*************************************************************************/

void send_api_stats(PAD_DESCR   *main_pad,
                    XEvent      *event)
{

/***************************************************************
*  
*  We have been given a client message event.  Fill in the fields
*  and send it back to the window specified in the window parameter.
*  
***************************************************************/

event->TOTAL_LINES_INFILE = htonl(total_lines(main_pad->token));
event->CURRENT_LINENO     = htonl(main_pad->file_line_no+1); /* we are zero based */
event->CURRENT_COLNO      = htons((short)(main_pad->file_col_no+1));  /* we are zero based */
event->PAD_WRITABLE       = htons((short)WRITABLE(main_pad->token));

XSendEvent(event->xclient.display,
           event->xclient.window,
           False,
           None,
           event);


} /* end of send_api_stats */
#endif  /* ifndef PGM_XDMC            */


#ifdef CE_LITTLE_ENDIAN
/************************************************************************

NAME:      to_net_cc_list  -   Convert a CC List from host order (little endian)
                                to network order (big endian).


PURPOSE:    This routine converts the passed CC List which is an array of 4
            byte ints from host order to network order.  The #ifdefs around
            this routine make it only get included on little endian machines.

PARAMETERS:

   1.  buff        - pointer to char (INPUT)
                     This is the buffer containing the CC list.

   2.  buff_len    - int (INPUT)
                     This is the length of buffer buff.


FUNCTIONS :

   1.   For each int in the CC List, convert it to network order from host
        order using the system supplied routine.


*************************************************************************/

static void to_net_cc_list(CC_LIST   *buff,
                           int        len)
{
int              *num;
int              *end;

DEBUG18(fprintf(stderr, "@to_net_cc_list\n");)

num = (int *)buff;
end = (int *)((char *)num + len);

while (num < end)
{
   *num = htonl(*num);
   num++;
}

} /* end of tonet_cc_list */


/************************************************************************

NAME:      to_host_cc_list  -   Convert a CC List from network order (big endian)
                                to host order (little endian).


PURPOSE:    This routine converts the passed CC List which is an array of 4
            byte ints from network order to host order.  The #ifdefs around
            this routine make it only get included on little endian machines.

PARAMETERS:

   1.  buff        - pointer to char (INPUT)
                     This is the buffer containing the CC list.

   2.  buff_len    - int (INPUT)
                     This is the length of buffer buff.


FUNCTIONS :

   1.   For each int in the CC List, convert it to host order from network
        order using the system supplied routine.


*************************************************************************/

static void to_host_cc_list(CC_LIST   *buff,
                            int        len)
{
int              *num;
int              *end;

DEBUG18(fprintf(stderr, "@to_host_cc_list\n");)

num = (int *)buff;
end = (int *)((char *)num + len);

while (num < end)
{
   *num = ntohl(*num);
   num++;
}

} /* end of to_host_cc_list */

#endif  /* ifdef CE_LITTLE_ENDIAN     */




#if defined(DebuG) || defined(PGM_XDMC)
/************************************************************************

NAME:      dump_cc_data  -   debugging routine to the cclist structure.


PURPOSE:    This routine dumps the passed cclist structure

PARAMETERS:

   1.  buff        - pointer to char (INPUT)
                     This is the buffer containing the CC list.

   2.  buff_len    - int (INPUT)
                     This is the length of buffer buff.


FUNCTIONS :

   1.   For each CC_LIST entry dump the base data and the sets of windows.


*************************************************************************/

static void dump_cc_data(char   *buff,
                         int     buff_len,
                         Window  wm_window,
                         FILE   *stream)
{
CC_LIST          *buff_pos = (CC_LIST *)buff;

DEBUG18(fprintf(stream, "dump_cc_data: PID %d, len = %d\n", getpid(), buff_len);)

fprintf(stream, "\nID:  D 0x%X I 0x%X H 0x%X C 0x%X S %d (%d), window: 0x%X, WM window: 0x%X\n",
                buff_pos->dev, buff_pos->inode, buff_pos->host_id,
                buff_pos->create_time, buff_pos->file_size, buff_pos->display_no,
                buff_pos->main_window, wm_window);
fprintf(stream, "     UID: %d, pgrp %d, shpid %d, shpgrp %d cepid %d obscured %d iconned %d  ver %d\n",
                buff_pos->uid,
                buff_pos->pgrp, buff_pos->shpid, buff_pos->shgrp, buff_pos->cepid,
                buff_pos->obscured, buff_pos->iconned, buff_pos->version);

 } /* end of dump_cc_data */
#endif



