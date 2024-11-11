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
*  module pastebuf.c
*
*  Routines:
*     open_paste_buffer      -   Open a paste buffer (unnamed, named, or file)
*     init_paste_buff        -   Initialize default paste buffer name;
*     ce_fclose              -   Close a paste buffer
*     setup_x_paste_buffer   -   allocate memory for an X paste buffer, called from dm_xc
*     ce_fgets               -   Do an fgets, or the equivalent for the X paste buffer
*     ce_fputs               -   Do an fputs, or the equivalent for the X paste buffer
*     ce_fwrite              -   Do an fwrite, or the equivalent for the X paste buffer
*     clear_pastebuf_owner   -   clear the pastebuf ownership after receiving a selection clear
*     send_pastebuf_data     -   Send the paste buffer data to another program via a property
*     more_pastebuf_data     -   Send the pieces of the paste buffer data using INCR convention
*     closeup_paste_buffers  -   Save owned X paste buffers in the pastebuf files.
*     release_x_pastebufs    -   Release ownership flags for x paste buffers.
*     setup_paste_buff_dir   -   Verify the paste buffer directory exists and is accessible.
*     get_pastebuf_size      -   Return the total number of bytes in a paste buffer.
*     same_display           -   Determine if two displays are the same
*
* Internal:
*     open_X_paste_buffer    -   Set up the X paste buffer.
*     find_paste_buff_notify -   Predicate routine for XIfEvent to find response to XIfEvent
*     get_pastebuf_data      -   Get the data delivered with a selection event
*     get_paste_data_from_X  -   Get paste data for append mode xc
*     free_paste_area_ext    -   Free the paste buffe area extension.
*     read_cutbuff_data      -   (APOLLO ONLY) Read a cutbuff type property if we cannot get the X selection.
*     kill_cutbuff_data      -   (APOLLO ONLY) Get rid of cutbuf data when not needed
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h   /usr/include/sys/errno.h      */
#include <ctype.h>          /* /usr/include/ctype.h      */
#include <fcntl.h>          /* /usr/include/fcntl.h */
#include <limits.h>         /* /usr/include/limits.h     */
#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif
#include <sys/types.h>    /* /usr/include/sys/types.h   */
#include <sys/stat.h>     /* /usr/include/sys/stat.h    */

#ifdef WIN32
#include <stdlib.h>
#include <io.h>
#include <direct.h>
#else
#include <X11/Xatom.h>     /* /usr/include/X11/Xatom.h */
#endif


#include "dmsyms.h"
#include "dmwin.h"
#ifndef PGM_CEAPI
#include "dumpxevent.h"
#include "emalloc.h"
#include "netlist.h"
#endif
#include "normalize.h"
#include "pastebuf.h"
#ifndef PGM_CEAPI
#include "undo.h"        /* needed for undo_semafor in get_paste_data_from_X */
#include "windowdefs.h"  /* needed to get to property list */
#endif
#include "xerrorpos.h"

#ifdef WIN32
#define unlink _unlink
#else
#ifndef IBMRS6000
int    unlink(char *path);
#endif
#endif
#ifndef IBMRS6000
void   sleep(int secs);
#endif

#ifdef PGM_CEAPI
#include <malloc.h>
#define CE_MALLOC(size) malloc(size)
#define dm_error(m,j) fprintf(stderr, "%s\n", m)
#define malloc_copy(m) strdup(m)
#define same_display(m,n) (1)
#endif


/***************************************************************
*
*  Variables for file versions of paste buffers
*
***************************************************************/

#define  UNNAMED_PASTE_BUFF   "PRIMARY"  /* Changed from CLIPBOARD 6/10/97 */
#define  MAX_PASTE_TRIES   3

/***************************************************************
*
*  Linked list of default paste buffer names set up by
*  init_paste_buff.  This is used for the unnamed paste buffer.
*
***************************************************************/

struct PASTE_NAME_LIST  ;

struct PASTE_NAME_LIST {
        struct PASTE_NAME_LIST *next;              /* next data in list                          */
        char                   *name;              /* paste buffer name                          */
        Display                *display;           /* associated display                         */
  };

typedef struct PASTE_NAME_LIST PASTE_NAME_LIST;

PASTE_NAME_LIST   default_for_null_display = {NULL,  UNNAMED_PASTE_BUFF, NULL};

static PASTE_NAME_LIST  *paste_name_list_head = &default_for_null_display;

/***************************************************************
*
*  Linked list of data areas passed when we do a paste from
*  another client and the data comes over piecemeal via
*  INCR processing.
*
***************************************************************/

struct PASTE_DATA_LIST  ;

struct PASTE_DATA_LIST {
        struct PASTE_DATA_LIST *next;              /* next data in list                          */
        char                   *data;              /* paste buffer name                          */
        int                     len;               /* length of the data in this segment         */
  };

typedef struct PASTE_DATA_LIST PASTE_DATA_LIST;




/***************************************************************
*
*  Linked list of X paste buffer names plus a few hard coded
*  ones.
*
***************************************************************/

struct PASTEBUF_LIST  ;

struct PASTEBUF_LIST {
        unsigned long           marker;            /* header marker, set to X_PASTEBUF_MARKER    */
        struct PASTEBUF_LIST   *next;              /* next key in list                           */
        char                   *name;              /* paste buffer name                          */
        char                   *paste_area;        /* area where the paste buffer data is        */
        long                    paste_area_len;    /* length of the paste area                   */
        PASTE_DATA_LIST        *paste_area_ext;    /* list of pieces of paste buff gotten via INCR */
        char                   *paste_cur_pos;     /* current position for ce_fgets & ce_fputs   */
        unsigned int            paste_str_len;     /* length of the data in the paste buffer     */
        Display                *display;           /* display pointer for X calls                */
        Atom                    selection_atom;    /* atom id for the paste buffer selection     */
        Window                  sending_to_window; /* window id of target when sending via INCR  */
        Atom                    sending_to_prop;   /* property id of target when sending via INCR*/
        Atom                    sending_to_format; /* format atom of target when sending via INCR*/
        Time                    sending_lasttime;  /* timestamp from last send operation         */
        char                    own_selection;     /* current ownership flag                     */
        char                    Xfree_area;        /* Flag to show the area needs to be Xfree'ed */
        char                    local;             /* Flag to show local paste buffer, No X stuff*/
        char                    cut_buffer;        /* Flag set when we cannot get selection ownership, copy hangs property on root window */
        char                    append_mode;       /* Flag set to show we are in append mode     */
  };

typedef struct PASTEBUF_LIST PASTEBUF_LIST;

/***************************************************************
*
*  Fixed paste buffers
*  Note that the "PRIMARY" is used for the unnamed paste buffer.
*  The atom id is filled in when it is accessed.
*
***************************************************************/

static PASTEBUF_LIST     X_secondary_paste_buffer = {X_PASTEBUF_MARKER,          /*  marker             */
                                                     NULL,                       /*  *next;             */
                                                     "SECONDARY",                /*  *name;             */
                                                     NULL,                       /*  *paste_area;       */
                                                     0,                          /*   paste_area_len;   */
                                                     NULL,                       /*   paste_area_ext;   */
                                                     NULL,                       /*  *paste_cur_pos;    */
                                                     0,                          /*   paste_str_len;    */
                                                     NULL,                       /*  *display;          */
                                                     XA_SECONDARY,               /*   selection_atom;   */
                                                     None,                       /*   sending_to_window */
                                                     None,                       /*   sending_to_prop   */
                                                     None,                       /*   sending_to_format */
                                                     0,                          /*   sending_lasttime  */
                                                     False,                      /*   own_selection;    */
                                                     False,                      /*   Xfree_area;       */
                                                     False,                      /*   local             */
                                                     False,                      /*   cut_buffer;       */
                                                     False};                     /*   append_mode;      */

static PASTEBUF_LIST     X_primary_paste_buffer   = {X_PASTEBUF_MARKER,          /*  marker             */
                                                     &X_secondary_paste_buffer,  /*  *next;             */
                                                     "PRIMARY",                  /*  *name;             */
                                                     NULL,                       /*  *paste_area;       */
                                                     0,                          /*   paste_area_len;   */
                                                     NULL,                       /*   paste_area_ext;   */
                                                     NULL,                       /*  *paste_cur_pos;    */
                                                     0,                          /*   paste_str_len;    */
                                                     NULL,                       /*  *display;          */
                                                     XA_PRIMARY,                 /*   selection_atom;   */
                                                     None,                       /*   sending_to_window */
                                                     None,                       /*   sending_to_prop   */
                                                     None,                       /*   sending_to_format */
                                                     0,                          /*   sending_lasttime  */
                                                     False,                      /*   own_selection;    */
                                                     False,                      /*   Xfree_area;       */
                                                     False,                      /*   local             */
                                                     False,                      /*   cut_buffer;       */
                                                     False};                     /*   append_mode;      */

static PASTEBUF_LIST     unnamed_paste_buffer     = {X_PASTEBUF_MARKER,          /*  marker             */
                                                     &X_primary_paste_buffer,    /*  *next;             */
                                                     UNNAMED_PASTE_BUFF,         /*  *name;             */
                                                     NULL,                       /*  *paste_area;       */
                                                     0,                          /*   paste_area_len;   */
                                                     NULL,                       /*   paste_area_ext;   */
                                                     NULL,                       /*  *paste_cur_pos;    */
                                                     0,                          /*   paste_str_len;    */
                                                     NULL,                       /*  *display;          */
                                                     None,                       /*   selection_atom; initialized later */
                                                     None,                       /*   sending_to_window */
                                                     None,                       /*   sending_to_prop   */
                                                     None,                       /*   sending_to_format */
                                                     0,                          /*   sending_lasttime  */
                                                     False,                      /*   own_selection;    */
                                                     False,                      /*   Xfree_area;       */
                                                     False,                      /*   local             */
                                                     False,                      /*   cut_buffer;       */
                                                     False};                     /*   append_mode;      */


static PASTEBUF_LIST     *first_pastebuf  = &unnamed_paste_buffer;
static PASTEBUF_LIST     *last_pastebuf   = &X_secondary_paste_buffer;

/***************************************************************
*
*  Name of the property used to get paste buffer data.
*
***************************************************************/

#define  CE_PASTE_BUFF  "CE_paste_buff"

/***************************************************************
*
*  Macro to detect end of a paste buffer (not counting extensions).
*
*  END_OF_BUFF added 9/2/1998, paste buffers from XGetWindowProperty
*  may not end with a null character.
*
***************************************************************/

#define END_OF_BUFF(b) ((*(b->paste_cur_pos) == '\0') || (b->paste_cur_pos-b->paste_area > b->paste_str_len))

/***************************************************************
*
*  Trap variable to help with XIfEvent processing.  XIfEvent
*  is called when we are looking for a certain type of event to
*  be sent to us from another process.  If something goes wrong
*  on that end and the event never gets sent, we want to avoid
*  waiting forever.  The routine XIfEvent calls bumps the event
*  counter for each event.  If it reaches the limiting value,
*  it returns that event event if it the wrong one.  We need to
*  zero pastebuf_XIfEvent_event_counter and look at it in the
*  routine which calls XIfEvent.
*
***************************************************************/

int  pastebuf_XIfEvent_event_counter = 0;
#define MAX_XIFEVENT_COUNT 100

/***************************************************************
*
*  Atoms for communicating with aixterm, and other standard Atoms
*
***************************************************************/

#define  TEXT_ATOM_NAME       "TEXT"
#define  TARGETS_ATOM_NAME    "TARGETS"
#define  TIMESTAMP_ATOM_NAME  "TIMESTAMP"
#define  INCR_ATOM_NAME    "INCR"
#define  LENGTH_ATOM_NAME    "LENGTH"

static Time  aqcuire_selection_timestamp;
#ifndef LONG64
static long  max_transfer_increment = 65000;
#else
static int   max_transfer_increment = 65000;
#endif

#ifdef PGM_CEAPI
static Atom  paste_buff_property_atom = 0;
static Atom  text_atom = 0;
static Atom  targets_atom = 0;
static Atom  timestamp_atom = 0;
static Atom  incr_atom = 0;
static Atom  length_atom = 0;
#define CE_PROPERTY(a)   a
#else
#define CE_PROPERTY(a)   dspl_descr->properties->a
#endif

#ifdef WIN32
#define access _access
#else
#ifndef IBMRS6000
int    access(char *path, int mode);
#endif
#endif
#ifndef IBMRS6000
void   usleep(int usecs);
#endif

/***************************************************************
*
*  Prototypes for local routines
*
***************************************************************/

static FILE *open_X_paste_buffer(DMC               *dmc,
                                 char              *buffer_name,
                                 Display           *display,
                                 Window             main_x_window,
                                 int                to_write,
                                 int                append_mode,
                                 char              *cmd,
                                 Time               time,
                                 char               escape_char);

static Bool find_paste_buff_notify(Display        *display,
                                     XEvent         *event,
                                     char           *arg);

static Bool find_paste_buff_property(Display        *display,
                                     XEvent         *event,
                                     char           *arg);


static FILE *get_pastebuf_data(XEvent          *event,
                               char            *cmd,
                               Display           *display,
                               Window             main_x_window,
                               PASTEBUF_LIST   *pastebuf_entry);

static void free_paste_area_ext(PASTEBUF_LIST   *pastebuf_entry);


#ifndef PGM_CEAPI
static void  get_paste_data_from_X(FILE              *pastebuf_entry,
                                   DMC               *dmc,
                                   Display           *display,
                                   Window             main_x_window,
                                   Time               timestamp,
                                   char               escape_char);
#endif

#ifdef apollo
static FILE *read_cutbuff_data(PASTEBUF_LIST   *pastebuf_entry);

static void kill_cutbuff_data(PASTEBUF_LIST   *pastebuf_entry);
#endif

/************************************************************************

NAME:      open_paste_buffer  -  Open a paste buffer for input or output.

PURPOSE:    This routine takes care of opening a paste buffer.

PARAMETERS:

   1.  dmc        -  pointer to DMC (INPUT)
                     This is the DM command structure for the command
                     being be processed.  This is either an xc, xd, xp, or cmdf
                     command.

   2.  display     - pointer to Display (Xlib type) (INPUT)
                     This is the display pointer for the output device.

   3.  main_x_window - Window Display (Xlib type) (INPUT)
                     This is X window id of the main Ce window

   4.  timestamp   - Time (INPUT - X lib type)
                     This is the timestamp from the keyboard or mouse event which
                     caused the xc or xd.  It is needed by open_paste_buffer
                     when dealing with the X paste buffers.  The value is only needed
                     when doing a copy or cut (write to the paste buffer), so NULL is
                     allowed for paste (read) operations.


FUNCTIONS :

   1.   If determine whether we want r+ wor w+ i/o mode from the command name.
        xc is write, xp is read.

   2.   Get the text of the command name from the command id.

   3.   If the -f flag was not specified, determine the paste buffer file name.
        Otherwise use the passed name.

   4.   If we are going to read the file, make sure it exists.

   5.   Open the file.  On error generate a DM error message.

RETURNED VALUE:

   The stream pointer (FILE *) is returned.  On failure, it is NULL.


*************************************************************************/

FILE *open_paste_buffer(DMC               *dmc,
                        Display           *display,
                        Window              main_x_window,
                        Time               time,
                        char               escape_char)
{
char              paste_name[MAXPATHLEN];
char              msg[MAXPATHLEN];
char             *paste_basename;
FILE             *paste_stream;
char             *paste_buff_from_cmd;
char             *cmd;
int               to_write;
int               not_a_regular_file;
int               xc_append_mode = False;
PASTE_NAME_LIST  *current;

/***************************************************************
*
*  Look at the command id to determine whether we want to open
*  the paste buffer for read or write.  copy(xc) and cut(xd)
*  need write, paste(xp) and command file (cmdf) need read.
*
***************************************************************/

DEBUG17(max_transfer_increment = 128;)

cmd = dmsyms[dmc->any.cmd].name;
if (dmc->any.cmd == DM_xc || dmc->any.cmd == DM_xd || dmc->any.cmd == DM_xl)
   {
      to_write = True;
      not_a_regular_file = !(dmc->xc.dash_f);
      paste_buff_from_cmd = dmc->xc.path;
      xc_append_mode = dmc->xc.dash_a;
   }
else
   {
      to_write = False; /* a paste  or cmdf */
      if (dmc->any.cmd == DM_cmdf)
         {
            not_a_regular_file = dmc->cmdf.dash_p;
            paste_buff_from_cmd = dmc->cmdf.path;
         }
      else
         {
            not_a_regular_file  = !(dmc->xp.dash_f);
            paste_buff_from_cmd = dmc->xp.path;
         }
   }

/***************************************************************
*
*  Handle the setup and open of a normal or unnamed paste buffer.
*  not_a_regular_file is true if this is an normal (not -f) paste buffer.
*
***************************************************************/

if (not_a_regular_file)
   {
      /***************************************************************
      *
      *  Special case for name <stdout>
      *
      ***************************************************************/
      if (paste_buff_from_cmd && (strcmp(paste_buff_from_cmd, "<STDOUT>") == 0))
         return(stdout);

      /***************************************************************
      *
      *  If we have an X paste buffer, make sure the window is available.
      *
      ***************************************************************/
      if (main_x_window == None)
         {
            dm_error("X paste buffers not available during initialization", DM_ERROR_BEEP);
            return(NULL);
         }

      /***************************************************************
      *
      *  Use the X paste buffer if we can, Otherwise use the file
      *  in the paste buffer directory.  The special names used by the
      *  bang (!) command are always backed by files.
      *
      ***************************************************************/
      if (!paste_buff_from_cmd || (paste_buff_from_cmd[0] != BANG_PASTEBUF_FIRST_CHAR) ||
          !((strcmp(paste_buff_from_cmd, BANGIN_PASTEBUF) == 0) ||
            (strcmp(paste_buff_from_cmd, BANGIN_PASTEBUF) != 0) ||
            (strcmp(paste_buff_from_cmd, ERR_PASTEBUF) != 0)))
         {
            /***************************************************************
            *   Changed 8/17/94, translate paste buffer names here
            *   instead of in open_X_paste_buffer.
            *   Changed 5/23/95, get default pastebuf name from list.
            ***************************************************************/
            if ((paste_buff_from_cmd == NULL) || (paste_buff_from_cmd[0] == '\0'))
               {
                  for (current = paste_name_list_head;
                       current && (current->display != display);
                       current = current->next)
                     ; /* do nothing in loop but search */
            
                  if (current)
                     paste_buff_from_cmd = current->name;
                  else
                     paste_buff_from_cmd = UNNAMED_PASTE_BUFF;
               }
            else
               if (strcmp(paste_buff_from_cmd, "X") == 0)
                  paste_buff_from_cmd = "PRIMARY";
               else
                  if (strcmp(paste_buff_from_cmd, "X2") == 0)
                     paste_buff_from_cmd = "SECONDARY";

            paste_stream = open_X_paste_buffer(dmc, paste_buff_from_cmd, display, main_x_window, to_write, xc_append_mode, cmd, time, escape_char);
            if (paste_stream != NULL)
               return(paste_stream);
         }

      if (setup_paste_buff_dir(cmd) == NULL)
         return(NULL);

       /***************************************************************
       *
       *  Handle the unnamed paste buffer as a special case.  Otherwise
       *  search the named paste buffer stuff to see if we already have it.
       *
       ***************************************************************/

      if (paste_buff_from_cmd == NULL || paste_buff_from_cmd[0] == '\0')
         {
            snprintf(paste_name, sizeof(paste_name), "%s/%s", paste_buff_dir, UNNAMED_PASTE_BUFF);
            paste_basename = "";
         }
      else
         {
            snprintf(paste_name, sizeof(paste_name), "%s/%s", paste_buff_dir, paste_buff_from_cmd);
            paste_basename = paste_buff_from_cmd;
         }

   }  /* end of  normal paste buffer */
else
   {
      if (paste_buff_from_cmd)
         strcpy(paste_name, paste_buff_from_cmd);
      else
         paste_name[0] = '\0';
      paste_basename = paste_buff_from_cmd;
      substitute_env_vars(paste_name, MAXPATHLEN, escape_char);
      translate_tilde(paste_name);  /* process names starting with ~ if the name starts that way */
   }

/***************************************************************
*  
*  We need to have a name at this point
*  
***************************************************************/

if (paste_name[0] == '\0')
   {
      snprintf(msg, sizeof(msg), "(%s) Missing file or paste buffer name", cmd);
      dm_error(msg, DM_ERROR_BEEP);
      return(NULL);
   }

/***************************************************************
*
*  Open the paste buffer
*
***************************************************************/

if (to_write)
{
   DEBUG2(fprintf(stderr, "open_paste_buffer:  Opening file \"%s\" for %s\n", paste_name, (xc_append_mode ? "APPEND" : "WRITE"));)
   if (xc_append_mode)
      paste_stream = fopen(paste_name, "a");
   else
      paste_stream = fopen(paste_name, "w");
}
else
{
   DEBUG2(fprintf(stderr, "open_paste_buffer:  Opening file \"%s\" for READ\n", paste_name);)
   paste_stream = fopen(paste_name, "r");
}

if (paste_stream == NULL && to_write)
   {
      unlink(paste_name);
      paste_stream = fopen(paste_name, "w");
      chmod(paste_name, 0666);
   }


if (paste_stream == NULL)
   {
      if (not_a_regular_file)
         snprintf(msg, sizeof(msg), "(%s) Cannot open paste buffer %s (%s)", cmd, paste_basename, strerror(errno));
      else
         snprintf(msg, sizeof(msg), "(%s) Cannot open file %s (%s)", cmd, paste_basename, strerror(errno));
      dm_error(msg, DM_ERROR_BEEP);
   }

return(paste_stream);

} /* end of open_paste_buffer */


/************************************************************************

NAME:      open_X_paste_buffer  -  Open an paste buffer transfered via the
                                   X server for input or output.

PURPOSE:    This routine takes care of setting up a paste buffer for the X server.
            This routine sets up the PASTEBUF_LIST structure so it can be used
            by ce_fgets, ce_fputs, ce_fwrite, and ce_fclose.  Note that no data is
            ever transfered into the server unless a request comes in from another
            X client to transfer it.  So long as you are in the same window,
            everything is done in memory.

PARAMETERS:

   1.  dmc        -  pointer to DMC (INPUT)
                     This is the DM command structure for the command
                     being be processed.  This is either an xc, xd, xp, or cmdf
                     command.

   2.  buffer_name - pointer to char (INPUT)
                     This is the name of the paste buffer.  It can point to
                     a null string for the unnamed paste buffer.  This data is
                     taken from the dmc, but is passed as a parm for convieience.

   3.  display     - pointer to Display (Xlib type) (INPUT)
                     This is the display pointer for the output device.

   4.  main_x_window - Window Display (Xlib type) (INPUT)
                     This is X window id of the main Ce window

   5.  to_write    - int (INPUT)
                     This boolean flag specifies whether we want to open the
                     paste buffer for read (paste/cmdf) or write (xc/xd).
                     False - open for read
                     True  - open for write

   6.  append_mode    - int (INPUT)
                     This boolean flag specifies whether we want to open the
                     paste buffer for append mode or normal rewrite mode.
                     This flag is only used when to_write is true.
                     False - open for rewrite (normal)
                     True  - open in append mode

   7.  cmd         - pointer to char (INPUT)
                     This is the text command name being execute.  It is used in message
                     generation.

   8.  timestamp   - Time (INPUT - X lib type)
                     This is the timestamp from the keyboard or mouse event which
                     caused the xc or xd.  It is needed by open_paste_buffer
                     when dealing with the X paste buffers.  The value is only needed
                     when doing a copy or cut (write to the paste buffer), so NULL is
                     allowed for paste (read) operations.


FUNCTIONS :

   1.   If we already know that selections do not work, return NULL.
        This situation occurs on the Apollo when the DM owns the root window.

   2.   Find the paste buffer entry for the passed paste buffer.  If one is
        not found, create one.

   3.   If we already own the selection atom for the paste buffer, we are done.
        For write, this means we are ready to rock and roll.  For read, the
        data we want is already in memory.

   3.   If we are opening for write, acquire ownership of the selection for the
        atom we are using.

   4.   If we are opening for read, call convert selection to tell the other
        program to send us the data through the server.

   5.   If we are opening for read, get the data transfered from the other program
        into our memory space.

RETURNED VALUE:

   The stream pointer (FILE *) is returned.  On failure, it is NULL.
   For this routine, it is actually a pointer to the PASTEBUF_LIST entry
   we are using.


*************************************************************************/

static FILE *open_X_paste_buffer(DMC               *dmc,
                                 char              *buffer_name,
                                 Display           *display,
                                 Window              main_x_window,
                                 int                to_write,
                                 int                append_mode,
                                 char              *cmd,
                                 Time               timestamp,
                                 char               escape_char)
{

int               paste_buff_tries = 0;
Window            temp_window;
XEvent            event_union;
FILE             *fake_stream;
char              msg[80];
PASTEBUF_LIST    *pastebuf_entry;
PASTEBUF_LIST    *candidate_pastebuf_entry = NULL;
#ifndef PGM_CEAPI
PASTEBUF_LIST    *walk_pastebuf_entry;
#endif
char             *ifevent_parm[2];
static int        selection_fail_count = 0;

if (selection_fail_count > 3)
   return(NULL);

/***************************************************************
*  Scan the list for the paste buffer record for this buffer.
*  RES 3/27/1998 Test for candidate_pastebuf_entry added as
*  separate from main search to fix bug when closing primary
*  window when cc window owns paste buffer.
***************************************************************/
for (pastebuf_entry = first_pastebuf;
     pastebuf_entry != NULL;
     pastebuf_entry = pastebuf_entry->next)
{
   if ((strcmp(buffer_name, pastebuf_entry->name) == 0)
      && (pastebuf_entry->display == display)) 
      break;
   if ((strcmp(buffer_name, pastebuf_entry->name) == 0)
      && (pastebuf_entry->display == NULL) && (candidate_pastebuf_entry == NULL)) 
      candidate_pastebuf_entry = pastebuf_entry;
}

/***************************************************************
*  If we did not find one, see if there is an empty one to use.
***************************************************************/
if ((pastebuf_entry == NULL) && (candidate_pastebuf_entry != NULL))
   pastebuf_entry = candidate_pastebuf_entry;

/***************************************************************
*  If we did not find one, create one.
***************************************************************/
if (pastebuf_entry == NULL)
   {
      DEBUG2(fprintf(stderr, "open_X_paste_buffer:  New X paste buffer for \"%s\", display 0x%X\n", buffer_name, display);)
      pastebuf_entry = (PASTEBUF_LIST *)CE_MALLOC(sizeof(PASTEBUF_LIST));
      if (!pastebuf_entry)
         return(NULL);
      memset((char *)pastebuf_entry, 0, sizeof(PASTEBUF_LIST));
      pastebuf_entry->marker = X_PASTEBUF_MARKER;
      pastebuf_entry->display = display;
      last_pastebuf->next    = pastebuf_entry;
      last_pastebuf          = pastebuf_entry;
      pastebuf_entry->name   = malloc_copy(buffer_name);
      if (!pastebuf_entry->name)
         return(NULL);
   }
else
   DEBUG2(fprintf(stderr, "open_X_paste_buffer:  Found X paste buffer for \"%s\", display 0x%X\n", buffer_name, display);)

/***************************************************************
*  If the CE paste buffer atom does not already exist, create it.
*  This is the property used to paste data from other applications.
***************************************************************/
if (CE_PROPERTY(paste_buff_property_atom) == 0)
   {
      DEBUG9(XERRORPOS)
      CE_PROPERTY(paste_buff_property_atom) = XInternAtom(display, CE_PASTE_BUFF, False);
      DEBUG9(XERRORPOS)
      CE_PROPERTY(text_atom)      = XInternAtom(display, TEXT_ATOM_NAME, False);
      DEBUG9(XERRORPOS)
      CE_PROPERTY(targets_atom)   = XInternAtom(display, TARGETS_ATOM_NAME, False);
      DEBUG9(XERRORPOS)
      CE_PROPERTY(timestamp_atom) = XInternAtom(display, TIMESTAMP_ATOM_NAME, False);
      DEBUG9(XERRORPOS)
      CE_PROPERTY(incr_atom)      = XInternAtom(display, INCR_ATOM_NAME, False);
      DEBUG9(XERRORPOS)
      CE_PROPERTY(length_atom)    = XInternAtom(display, LENGTH_ATOM_NAME, False);

      DEBUG2(fprintf(stderr, "text_atom: %d, targets_atom: %d, timestamp_atom: %d, XA_STRING: %d, incr_atom: %d, length_atom: %d, paste_property_atom: %d\n",
                              CE_PROPERTY(text_atom), CE_PROPERTY(targets_atom), CE_PROPERTY(timestamp_atom),
                              XA_STRING, CE_PROPERTY(incr_atom), CE_PROPERTY(length_atom), CE_PROPERTY(paste_buff_property_atom));
      )
   }

/***************************************************************
*  Make sure the display is set.  It is faster to set it than
*  to check first.
***************************************************************/
pastebuf_entry->display = display;


/***************************************************************
* Check for a local copy or cut (xc -l)
***************************************************************/
if (to_write)
   if (dmc->xc.dash_l)
      {
         pastebuf_entry->local = True;
         pastebuf_entry->cut_buffer = False;
         if (pastebuf_entry->own_selection)
            {
               DEBUG9(XERRORPOS)
               XSetSelectionOwner(display, pastebuf_entry->selection_atom, None, CurrentTime);
               pastebuf_entry->own_selection = False;
            }
      }
   else
      pastebuf_entry->local = False;

#ifndef PGM_CEAPI
/***************************************************************
*  If we are opening for input, scan the list to see if we
*  own the selection in another window.  If so, we will use this.
***************************************************************/
if (!to_write && !pastebuf_entry->own_selection && !pastebuf_entry->local)
   for (walk_pastebuf_entry = first_pastebuf;
        walk_pastebuf_entry != NULL;
        walk_pastebuf_entry = walk_pastebuf_entry->next)
      if ((strcmp(buffer_name, walk_pastebuf_entry->name) == 0) && walk_pastebuf_entry->own_selection && same_display(walk_pastebuf_entry->display, display))
         {
            pastebuf_entry = walk_pastebuf_entry;
            break;
         }
#endif


/***************************************************************
*  If we do not own the selection we need, get ownership if we will write it.
*  Otherwise, ask to have the data sent to us.
***************************************************************/
#ifdef apollo
if ((!pastebuf_entry->own_selection && !(pastebuf_entry->local)) || pastebuf_entry->cut_buffer || to_write)
#else
if ((!pastebuf_entry->own_selection && !(pastebuf_entry->local)) || pastebuf_entry->cut_buffer)
#endif
   if (to_write)
      {
         /***************************************************************
         *  Get the atom id for the selection we are using.
         ***************************************************************/
         DEBUG9(XERRORPOS)
         if (pastebuf_entry->selection_atom == None)
            pastebuf_entry->selection_atom = XInternAtom(display, pastebuf_entry->name, False);
         if (pastebuf_entry->selection_atom == None)
            return(NULL);

         /***************************************************************
         *  If this is append mode.  Collect any data into the paste
         *  buffer which may exist elsewhere.
         ***************************************************************/
#ifndef PGM_CEAPI
         if (append_mode && !(pastebuf_entry->own_selection))
            get_paste_data_from_X((FILE *)pastebuf_entry, dmc, display, main_x_window, timestamp, escape_char);
#else
         if (append_mode)
            fprintf(stderr, "Append mode not supported from ceapi\n");
#endif

         /***************************************************************
         *  Open for output.
         *  Get ownership of the paste buffer selection. We will create
         *  it if neccesary.
         ***************************************************************/
         DEBUG9(XERRORPOS)
         XSetSelectionOwner(display, pastebuf_entry->selection_atom, main_x_window, timestamp);
         aqcuire_selection_timestamp = timestamp;
         DEBUG9(XERRORPOS)
         while(((temp_window = XGetSelectionOwner(display, pastebuf_entry->selection_atom)) != main_x_window) && (paste_buff_tries < MAX_PASTE_TRIES))
         {
            DEBUG2(fprintf(stderr, "Selection acquisition failed, owner = %d\n", temp_window);)
            usleep(100000); /* sleep .1 seconds */
            paste_buff_tries++;
            DEBUG9(XERRORPOS)
            XSetSelectionOwner(display, pastebuf_entry->selection_atom, main_x_window, CurrentTime); /* CurrentTime should be ok in this case */
         }
         if (temp_window != main_x_window)
            {
#ifdef apollo
               pastebuf_entry->cut_buffer    = True;
               pastebuf_entry->own_selection = True;
               fake_stream = (FILE *)pastebuf_entry;
               DEBUG2(fprintf(stderr, "(%s) Cannot access selection for X paste buffer %s (owner is 0x%X), using cut buffer technique\n", cmd, pastebuf_entry->name, temp_window);)
#else
               fake_stream = NULL;
               selection_fail_count++;
#ifdef DebuG
               snprintf(msg, sizeof(msg), "(%s) Cannot access selection for X paste buffer %s (owner is 0x%X)", cmd, pastebuf_entry->name, temp_window);
#else
               snprintf(msg, sizeof(msg), "(%s) Cannot access selection for X paste buffer %s", cmd, pastebuf_entry->name);
#endif
               dm_error(msg, DM_ERROR_LOG);
#endif
            }
         else
            {
#ifdef apollo
               kill_cutbuff_data(pastebuf_entry);
#endif
               pastebuf_entry->own_selection = True;
               pastebuf_entry->cut_buffer    = False;
               pastebuf_entry->append_mode   = append_mode;
               fake_stream = (FILE *)pastebuf_entry;
               DEBUG2(fprintf(stderr, "Got ownership of selection for %s\n", pastebuf_entry->name);)
            }
      }
   else
      {
         /***************************************************************
         *  Open for input.
         *  Get the atom id for the selection we are using.
         *  For input, the atom must already exist, thus the True parm in XInternAtom.
         ***************************************************************/
         pastebuf_entry->cut_buffer    = False; /* once we write the cut buffer, forget it */
         pastebuf_entry->append_mode   = False; /* we are reading, we cannot be in append mode */
         DEBUG9(XERRORPOS)
         if (pastebuf_entry->selection_atom == None)
            pastebuf_entry->selection_atom = XInternAtom(display, pastebuf_entry->name, True);
         DEBUG2(fprintf(stderr, "Looking for Pastebuff atom %d, %s\n", pastebuf_entry->selection_atom, pastebuf_entry->name);)
         if (pastebuf_entry->selection_atom == None)
            fake_stream = NULL;
         else
            {
               ifevent_parm[0] = (char *)&main_x_window;
               ifevent_parm[1] = (char *)pastebuf_entry;
               DEBUG9(XERRORPOS)
               /* Send the request to get the paste buffer from another X Program */
               XConvertSelection(display, pastebuf_entry->selection_atom, XA_STRING, CE_PROPERTY(paste_buff_property_atom), main_x_window, CurrentTime);
               pastebuf_XIfEvent_event_counter = 0;
               DEBUG9(XERRORPOS)
               XIfEvent(display, &event_union, find_paste_buff_notify, (char *)ifevent_parm);
               if (pastebuf_XIfEvent_event_counter > MAX_XIFEVENT_COUNT)
                  fake_stream = NULL;
               else
                  {
                     DEBUG2(fprintf(stderr, "Pastebuff atom %d, %s found after %d tries, state = %s\n", 
                                    pastebuf_entry->selection_atom,
                                    pastebuf_entry->name,
                                    pastebuf_XIfEvent_event_counter,
                                    ((event_union.xproperty.state == PropertyDelete) ? "Deleted" : "NewValue"));)
                     fake_stream = get_pastebuf_data(&event_union, cmd, display, main_x_window, pastebuf_entry);
                  }

#ifdef apollo
               if (fake_stream == NULL)
                  fake_stream = read_cutbuff_data(pastebuf_entry);
#endif
            }
      }
else
   {
      /***************************************************************
      *  Just work locally if we own the selection already.
      *  This is needed for read type operations (eg: paste).
      ***************************************************************/
      pastebuf_entry->cut_buffer    = False; /* once we write the cut buffer, forget it */
      pastebuf_entry->append_mode   = append_mode;
      pastebuf_entry->paste_cur_pos = pastebuf_entry->paste_area;
      fake_stream = (FILE *)pastebuf_entry;
      DEBUG2(
         if (pastebuf_entry->local)
            fprintf(stderr, "Local ownership of selection for %s\n", pastebuf_entry->name);
         else
            if (pastebuf_entry->display != display)
               fprintf(stderr, "Have ownership of selection in a CC window for %s\n", pastebuf_entry->name);
            else
               fprintf(stderr, "Already have ownership of selection for %s\n", pastebuf_entry->name);
      )
   }

return(fake_stream);

} /* end of open_X_paste_buffer */



/************************************************************************

NAME:      init_paste_buff  -  Initialize default paste buffer name.

PURPOSE:    This routine loads the default paste buffer name into an 
            element of the linked list of type PASTE_NAME_LIST.
            Use of the list allows different paste buffer defaults
            for different displays (cc windows).  When the list is
            searched it is by display.  Note that usually there is
            one or at most 2 elements in the list.

PARAMETERS:

   1.  paste_name  - pointer to char (INPUT)
                     This is the name to be used for the default paste
                     buffer.  If NULL or a null string is passed, the
                     default is PRIMARY.

   2.  display     - pointer to display (INPUT)
                     This is the current display.  It identifies the
                     default name to the display when doing a search.

FUNCTIONS :

   1.   Scan the list for the passed display.  If it is found, reuse it.

   2.   Create a new element if needed and put it at the front of the list.


*************************************************************************/

void  init_paste_buff(char             *paste_name,
                      Display          *display)
{
PASTE_NAME_LIST  *new;
int               created_new_one = False;

/***************************************************************
*
*  Linked list of default paste buffer names set up by
*  init_paste_buff.  This is used for the unnamed paste buffer.
*
***************************************************************/

/***************************************************************
*  See if there already is one for this display (rare case)
***************************************************************/
for (new = paste_name_list_head;
     new && (new->display != display);
     new = new->next)
   ; /* do nothing in loop */

/***************************************************************
*  If not, create one.
***************************************************************/
if (!new)
   {
      new = (PASTE_NAME_LIST *)CE_MALLOC(sizeof(PASTE_NAME_LIST));
      if (!new)
         return;
      memset((char *)new, 0, sizeof(PASTE_NAME_LIST));
      created_new_one = True;
   }
else
   if (new->name)
      free(new->name);

/**********************************************************************
*  Initialize the new entry and put it on the stack if we created it.
**********************************************************************/
new->display = display;
if (!paste_name || !(*paste_name))
   paste_name = UNNAMED_PASTE_BUFF;

new->name = malloc_copy(paste_name);
if (!new->name)
   {
      free((char *)new);
      return;
   }

if (created_new_one)
   {
      new->next = paste_name_list_head;
      paste_name_list_head = new;
   }


} /* end init_paste_buff */


/************************************************************************

NAME:      ce_fclose  -  Special ce fclose routine.

PURPOSE:    This routine takes care closing files opened by open_paste_buffer.
            It is needed because sometimes open_paste_buffer returns a pointer to
            a real FILE and sometimes it returns a pointer to the PASTEBUF_LIST
            entry for the paste buffer being used.

PARAMETERS:

   1.  paste_stream - pointer to FILE (INPUT)
                     This is the pointer to file returned by open_paste_buffer.

FUNCTIONS :

   1.   If this is a real file pointer, close it.  Otherwise  do nothing.


*************************************************************************/

void  ce_fclose(FILE             *paste_stream)
{
PASTEBUF_LIST *pastebuf_entry;

if (!X_PASTE_STREAM(paste_stream))
   {
      DEBUG2(fprintf(stderr, "Closing a real file\n\n");)
      fclose(paste_stream);
   }
else
   {
      pastebuf_entry = (PASTEBUF_LIST *)paste_stream;
      DEBUG2(fprintf(stderr, "ce_fclose:  Closing an X paste buffer for %s\n", pastebuf_entry->name);)
#ifdef apollo
      if (pastebuf_entry->cut_buffer)
         {
            DEBUG2(fprintf(stderr, "ce_fclose:  Writing out cut buffer data for %s atom %d, len = %d\n", pastebuf_entry->name, pastebuf_entry->selection_atom, pastebuf_entry->paste_str_len+1);)
            DEBUG9(XERRORPOS)
            XChangeProperty(pastebuf_entry->display,
                            RootWindow(pastebuf_entry->display, DefaultScreen(pastebuf_entry->display)),
                            pastebuf_entry->selection_atom,
                            XA_STRING,
                            8, /* we handle little endian ourselves */  /*32, /* 32 bit quantities, X server handles littleendin */
                            PropModeReplace,
                            (unsigned char *)pastebuf_entry->paste_area,
                            (int)(pastebuf_entry->paste_str_len+1));
         }
#endif
   }

} /* end ce_fclose */


/************************************************************************

NAME:      setup_x_paste_buffer  -  build the paste buffer area  to write into.

PURPOSE:    This routine is used by xc and xd to build the area it will copy data
            into.

PARAMETERS:

   1.  stream      - pointer to FILE (INPUT)
                     This is the pointer to file returned by open_paste_buffer.
                     It may be a real FILE * or a pointer to a PASTEBUF_LIST entry.

   2.  token       - pointer to DATA_TOKEN (INPUT)
                     This is the pointer to memdata area which is being copied from.
                     It is used to access the data which will tell us how much space
                     we need to malloc.
                     If rectangular width is passed. This parameter
                     is not used and may be NULL.

   3.  from_line   - int (INPUT)
                     This is the first line being copied from.
                     It is true that the actual copy may be some offset from the beginning
                     of this line, but we will ignore partial lines and work with whole lines.

   4.  to_line     - int (INPUT)
                     This is the last line being copied from (inclusive).
                     It is true that the actual copy may end before the end
                     of this line, but we will ignore partial lines and work with whole lines.

   5.  rectangular_width - int (INPUT)
                     If this flag/value is non-zero it is the the larger column minus the smaller
                     column for the rectangular copies.  It is used in estimating pastebuf sizes.
                     For non-rectangular copies, zero is passed.

FUNCTIONS :

   1.   If this is a real file pointer, return.

   2.   Get the size of the area we need.

   3.   Free the old area if it exists.

   4.   Malloc the new area.  If the malloc fails, the pointer
        will be null, the ce_fputs handles this as an I/O error.

RETURNED VALUE:
   rc - return code
        The return code shows whether this call worked.
        0  - paste buffer set up ok
        -1 - paste buffer set up failed.


*************************************************************************/

int   setup_x_paste_buffer(FILE           *stream,
                           DATA_TOKEN     *token,
                           int             from_line,
                           int             to_line,
                           int             rectangular_width)
{
int               buff_size_needed;
PASTEBUF_LIST    *pastebuf_entry;
char             *work;


/***************************************************************
*
*  If this is not an X paste buffer, do nothing.
*
***************************************************************/

if (!X_PASTE_STREAM(stream))
   return(-1);

pastebuf_entry = (PASTEBUF_LIST *)stream;

DEBUG2(fprintf(stderr, "setup_x_paste_buffer: from %d to %d (rectangular width %d)\n", from_line, to_line, rectangular_width);)

/***************************************************************
*  Get the size we need for the paste buffer.
*  This is the area alocated for the line plus the newline for
*  each line plus a null at the end of the line.
***************************************************************/

#ifndef PGM_CEAPI
if (!rectangular_width)
   buff_size_needed = sum_size(token, from_line, to_line) + ((to_line - from_line) + 2);
else
   buff_size_needed = (rectangular_width + 2) * ((to_line - from_line) + 2);
#else
/* no memdata in the ceapi */
buff_size_needed = (rectangular_width + 2) * ((to_line - from_line) + 2);
#endif

DEBUG2(fprintf(stderr, "setup_x_paste_buffer: paste area size %d\n", buff_size_needed);)


/***************************************************************
*  Allocate the new paste buffer and initialize the
*  work pointers and lengths for the area.
*  Free the old paste buffer if necessary.
***************************************************************/
if ((pastebuf_entry->paste_area == NULL) || !pastebuf_entry->append_mode)
   {
      /***************************************************************
      *  Normal situation.  Not append mode or there is no current buffer.
      ***************************************************************/

      if (pastebuf_entry->paste_area != NULL)
         if (pastebuf_entry->Xfree_area)
            XFree(pastebuf_entry->paste_area);
         else
            free(pastebuf_entry->paste_area);

      pastebuf_entry->paste_area = CE_MALLOC(buff_size_needed);
      if (!pastebuf_entry->paste_area)
         return(-1);

      pastebuf_entry->paste_area_len = buff_size_needed;
      pastebuf_entry->Xfree_area = False;

      pastebuf_entry->paste_str_len = 0;
      pastebuf_entry->paste_cur_pos = pastebuf_entry->paste_area;
   }
else
   {
      /***************************************************************
      *  Add the new area onto the existing area.
      ***************************************************************/
      work = CE_MALLOC(buff_size_needed + pastebuf_entry->paste_area_len);
      if (!work)
         return(-1);

      memcpy(work, pastebuf_entry->paste_area, pastebuf_entry->paste_str_len);
      if (pastebuf_entry->Xfree_area)
         XFree(pastebuf_entry->paste_area);
      else
         free(pastebuf_entry->paste_area);

      pastebuf_entry->paste_area = work;
      pastebuf_entry->paste_area_len += buff_size_needed;
      pastebuf_entry->Xfree_area = False;

      pastebuf_entry->paste_cur_pos = pastebuf_entry->paste_area + pastebuf_entry->paste_str_len;
   }

return(0);

} /* end of setup_x_paste_buffer */




/************************************************************************

NAME:      clear_pastebuf_owner  -  Deal with selection clear events.

PURPOSE:    This routine takes care of selection clear events which come in.
            They are recieved when some other program gets selection ownership
            of the selection atom.  What we do is find the paste buffer for
            this selection and clear the ownership flag.

PARAMETERS:

   1.  event       - pointer to XEvent (INPUT)
                     This is the pointer to xselectionclear event which
                     caused this routine to be called.

FUNCTIONS :

   1.   Scan the list of pastebuf entries looking for one which matches
        the passed selection atom.  If it is found, clear its ownership flag.


*************************************************************************/

void  clear_pastebuf_owner(XEvent    *event)
{

PASTEBUF_LIST    *pastebuf_entry;
char             *p;



for (pastebuf_entry = first_pastebuf;
     pastebuf_entry != NULL &&
     (pastebuf_entry->selection_atom != event->xselectionclear.selection || pastebuf_entry->display != event->xselectionclear.display);
     pastebuf_entry = pastebuf_entry->next)
   ; /* do nothing */

if (pastebuf_entry == NULL)
   {
      DEBUG(
            DEBUG9(XERRORPOS)
            if (event->xselectionclear.selection != None)
                p = XGetAtomName(event->xselectionclear.display, event->xselectionclear.selection);
            else
               p = "None";
            fprintf(stderr, "Got selection clear for unknown atom %d (%s)\n", event->xselectionclear.selection, p);
            if (event->xselectionclear.selection != None)
               XFree(p);
      )
      return;
   }
else
   {
      pastebuf_entry->own_selection = False;
      pastebuf_entry->cut_buffer = False;
      DEBUG2(fprintf(stderr, "Got selection clear for %s\n", pastebuf_entry->name);)
   }

} /* end of clear_pastebuf_owner */


/************************************************************************

NAME:      send_pastebuf_data     -   Sent the paste buffer data to the server via a property


PURPOSE:    This routine is called as a result of an xselectionrequest event.
            It means, some other program is pasting data we have copied.
            We need to send this data.  The requesting program can request
            the data directly or request other information.

PARAMETERS:

   1.  event       - pointer to XEvent (INPUT)
                     This is the pointer to xselectionrequest event which
                     caused this routine to be called.

FUNCTIONS :

   1.   Make sure the atoms we need are known about. If not, get them.

   2.   Scan the list of pastebuf entries looking for one which matches
        the passed selection atom.  If it is not found, modifiy the passed
        event to force failure.

   3.   Set the property with the requested data on the requested window.

   4.   Send the event back to the caller showing success for failure.


*************************************************************************/

void  send_pastebuf_data(XEvent    *event)
{
XSelectionEvent     send_event;
PASTEBUF_LIST      *pastebuf_entry;
char               *p;
char               *q;
char               *r;

#define TYPES_SUPPORTED_COUNT 5
#ifndef LONG64
Atom                types_supported[TYPES_SUPPORTED_COUNT];
#else
CARD32              types_supported[TYPES_SUPPORTED_COUNT];
#endif

DEBUG9(XERRORPOS)
DEBUG2(fprintf(stderr, "Request paste data for %d (%s) to window 0x%X, property =  %d (%s), format = %d (%s)\n",
               event->xselectionrequest.selection, (p = XGetAtomName(event->xselectionrequest.display, event->xselectionrequest.selection)),
               event->xselectionrequest.requestor,
               event->xselectionrequest.property, (q = XGetAtomName(event->xselectionrequest.display, event->xselectionrequest.property)),
               event->xselectionrequest.target, (r = XGetAtomName(event->xselectionrequest.display, event->xselectionrequest.target)));
       XFree(p);
       XFree(q);
       XFree(r);
       fprintf(stderr, "sizeof(Atom) = %d,  sizeof(types_supported) = %d, CARD32 = %d, sz_xInternAtomReply = %d\n",
                sizeof(Atom),  sizeof(types_supported), sizeof(CARD32), sz_xInternAtomReply);

)

send_event.type       = SelectionNotify;
send_event.serial     = event->xselectionrequest.serial;
send_event.send_event = True;
send_event.display    = event->xselectionrequest.display;
send_event.requestor  = event->xselectionrequest.requestor;
send_event.selection  = event->xselectionrequest.selection;
send_event.target     = event->xselectionrequest.target;
send_event.property   = event->xselectionrequest.property;
send_event.time       = event->xselectionrequest.time;

/***************************************************************
*
*  Scan the list of paste buffer entries to find the one which
*  matches this selection.
*
***************************************************************/

for (pastebuf_entry = first_pastebuf;
     pastebuf_entry != NULL &&
     (pastebuf_entry->selection_atom != event->xselectionrequest.selection || pastebuf_entry->display != event->xselectionrequest.display);
     pastebuf_entry = pastebuf_entry->next)
   ; /* do nothing */

if (pastebuf_entry == NULL)
   {
      DEBUG2(fprintf(stderr, "Cannot locate paste buffer entry for selection %d\n", event->xselectionrequest.selection);)
      event->xselectionrequest.target = None; /* force failure */
   }


/***************************************************************
*
*  We only know how to transfer strings, if the requestor does
*  not want strings, send failure.
*
***************************************************************/


if (event->xselectionrequest.target == XA_STRING || event->xselectionrequest.target == CE_PROPERTY(text_atom))
   {
      /***************************************************************
      *
      *  Show we are sending a string.  If we have nothing in the
      *  copy buffer, pass a zero length string.
      *
      ***************************************************************/

      if (pastebuf_entry->paste_str_len > max_transfer_increment)
         {
            DEBUG9(XERRORPOS)
            XChangeProperty(send_event.display,
                            send_event.requestor,
                            send_event.property,
                            CE_PROPERTY(incr_atom),
                            sizeof(max_transfer_increment)*8,
                            PropModeReplace,
                            (unsigned char *)&max_transfer_increment,
                            1);
            /***************************************************************
            *  Show transfer in progress.
            ***************************************************************/
            pastebuf_entry->sending_to_window = event->xselectionrequest.requestor;
            pastebuf_entry->sending_to_prop   = event->xselectionrequest.property;
            pastebuf_entry->sending_lasttime  = event->xselectionrequest.time;
            pastebuf_entry->sending_to_format = event->xselectionrequest.target;

            pastebuf_entry->paste_cur_pos     = pastebuf_entry->paste_area;

            DEBUG2(
               fprintf(stderr, "Paste buffer too long (%d), sending INCR for %d, ent: 0x%X(%s), curpos = 0x%X, start = 0x%x\n",
                       pastebuf_entry->paste_str_len, max_transfer_increment, pastebuf_entry, pastebuf_entry->name,
                       pastebuf_entry->paste_cur_pos, pastebuf_entry->paste_area);
            )

            XSelectInput(send_event.display,
                         send_event.requestor,
                         PropertyChangeMask);

         }
      else
         {
            send_event.target = XA_STRING;
            DEBUG2(fprintf(stderr, "Sending %d bytes of string data\n", pastebuf_entry->paste_str_len);)

            DEBUG9(XERRORPOS)
            if (pastebuf_entry->paste_area != NULL)
               XChangeProperty(send_event.display,
                               send_event.requestor,
                               send_event.property,
                               send_event.target,
                               8,  /* eight bit quantities */
                               PropModeReplace,
                               (unsigned char *)pastebuf_entry->paste_area,
                               pastebuf_entry->paste_str_len);
            else
               XChangeProperty(send_event.display,
                               send_event.requestor,
                               send_event.property,
                               send_event.target,
                               8,  /* eight bit quantities */
                               PropModeReplace,
                               (unsigned char *)"",
                               0);

            /***************************************************************
            *  Show no transfer in progress.
            ***************************************************************/
            pastebuf_entry->sending_to_window = None;
            pastebuf_entry->sending_to_prop   = None;
            pastebuf_entry->sending_lasttime  = 0;

         }
   }
else
   if (event->xselectionrequest.target == CE_PROPERTY(targets_atom))
      {
         /***************************************************************
         *
         *  Send a list of the target types (1) we support.
         *
         ***************************************************************/

         types_supported[0] = XA_STRING;
         types_supported[1] = CE_PROPERTY(targets_atom);
         types_supported[2] = CE_PROPERTY(timestamp_atom);
         types_supported[3] = CE_PROPERTY(incr_atom);
         types_supported[4] = CE_PROPERTY(length_atom);

         DEBUG9(XERRORPOS)
         XChangeProperty(send_event.display,
                         send_event.requestor,
                         send_event.property,
                         send_event.target,
                         (sizeof(types_supported)/TYPES_SUPPORTED_COUNT)*8,
                         PropModeReplace,
                         (unsigned char *)types_supported,
                         TYPES_SUPPORTED_COUNT);
      }
   else
      if (event->xselectionrequest.target == CE_PROPERTY(timestamp_atom))
         {
            /***************************************************************
            *
            *  Send a list of the target types (1) we support.
            *
            ***************************************************************/

            DEBUG9(XERRORPOS)
            XChangeProperty(send_event.display,
                            send_event.requestor,
                            send_event.property,
                            send_event.target,
                            sizeof(Time)*8,
                            PropModeReplace,
                            (unsigned char *)&aqcuire_selection_timestamp,
                            1);
         }
      else
         if (event->xselectionrequest.target == CE_PROPERTY(incr_atom))
            {
               /***************************************************************
               *
               *  Send the incr value.
               *
               ***************************************************************/

               DEBUG9(XERRORPOS)
               XChangeProperty(send_event.display,
                               send_event.requestor,
                               send_event.property,
                               send_event.target,
                               sizeof(max_transfer_increment)*8,
                               PropModeReplace,
                               (unsigned char *)&max_transfer_increment,
                               1);
                  DEBUG2(fprintf(stderr, "Sent incr %d\n", max_transfer_increment);)
            }
         else
            if (event->xselectionrequest.target == CE_PROPERTY(length_atom))
               {
                  /***************************************************************
                  *
                  *  Send the incr value.
                  *
                  ***************************************************************/

                  DEBUG9(XERRORPOS)
                  XChangeProperty(send_event.display,
                                  send_event.requestor,
                                  send_event.property,
                                  send_event.target,
                                  sizeof(int)*8,
                                  PropModeReplace,
                                  (unsigned char *)&pastebuf_entry->paste_str_len,
                                  1);
                  DEBUG2(fprintf(stderr, "Sent length %d\n", pastebuf_entry->paste_str_len);)
               }
            else
               {
                  /***************************************************************
                  *
                  *  Request for something we don't know how to send
                  *
                  ***************************************************************/

                  send_event.property = None;
                  DEBUG2(fprintf(stderr, "Cannot convert to target format %d\n", event->xselectionrequest.target);)
               }

/* need real XSendEvent, ce_XSendEvent will not do  RES 2/15/94 */
DEBUG9(XERRORPOS)
XSendEvent(send_event.display,
           send_event.requestor,
           True,  /* normal propagation ? */
           0,     /* zero event mask */
           (XEvent *)&send_event);

XFlush(send_event.display);

} /* end of send_pastebuf_data */



int more_pastebuf_data(XEvent    *event)
{
PASTEBUF_LIST      *pastebuf_entry;
int                 len_to_send;
char               *p;


DEBUG2(fprintf(stderr, "Request INCR paste data to window 0x%X, property =  %d (%s), state = %s\n",
               event->xproperty.window,
               event->xproperty.atom, (p = XGetAtomName(event->xproperty.display, event->xproperty.atom)),
               ((event->xproperty.state == PropertyDelete) ? "Deleted" : "NewValue"));
       XFree(p);
)

/***************************************************************
*
*  Make sure we have started some send processing of some type
*  (text_atom is no longer zero) and that this is the correct
*  type of property change.  If not, we are not interested.
*
***************************************************************/
if ((CE_PROPERTY(text_atom == 0)) || (event->xproperty.state != PropertyDelete))
   {
      DEBUG2(fprintf(stderr, "Not a request for more pastebuf data\n");)
      return(False);
   }

/***************************************************************
*
*  Scan the list of paste buffer entries to find the one which
*  matches this selection.
*
***************************************************************/

for (pastebuf_entry = first_pastebuf;
     (pastebuf_entry != NULL) &&
     ((pastebuf_entry->sending_to_window != event->xproperty.window) ||
      (pastebuf_entry->display != event->xproperty.display) ||
      (pastebuf_entry->sending_to_prop != event->xproperty.atom));
     pastebuf_entry = pastebuf_entry->next)
   ; /* do nothing */

if (pastebuf_entry == NULL)
   {
      DEBUG2(fprintf(stderr, "Cannot locate paste buffer entry for selection %d\n", event->xproperty.atom);)
      return(False);
   }


/***************************************************************
*
*  We now have the pastebuf entry to match this request.
*  First check to see if we transfer data or the zero length
*  end marker.  Since we are always sending data we own, we
*  do not have to worry about the paste_buff_extension field.
*  This is only used when we are recieveing data via INCR processing.
*
***************************************************************/

DEBUG2(
   fprintf(stderr, "ent: 0x%X(%s), curr_pos:  0x%X, end pos: 0x%X, start: 0x%X, len: %d\n",
           pastebuf_entry, pastebuf_entry->name, pastebuf_entry->paste_cur_pos,
           (pastebuf_entry->paste_area + pastebuf_entry->paste_str_len),  pastebuf_entry->paste_area,
            pastebuf_entry->paste_str_len);
)

if (pastebuf_entry->paste_cur_pos  < (pastebuf_entry->paste_area + pastebuf_entry->paste_str_len))
   {
      /***************************************************************
      *
      *  We are sending data.  Grab up to max_transfer_increment
      *  bytes.  Put it in the property and update the pointers.
      *
      ***************************************************************/

      len_to_send = pastebuf_entry->paste_str_len - (pastebuf_entry->paste_cur_pos - pastebuf_entry->paste_area);
      if (len_to_send > max_transfer_increment)
         len_to_send = max_transfer_increment;


      DEBUG2(fprintf(stderr, "Sending %d bytes of string data\n", len_to_send);)

      DEBUG9(XERRORPOS)
      XChangeProperty(event->xproperty.display,
                      event->xproperty.window,
                      event->xproperty.atom,
                      XA_STRING,
                      8,  /* eight bit quantities */
                      PropModeAppend,
                      (unsigned char *)pastebuf_entry->paste_cur_pos,
                      len_to_send);

      pastebuf_entry->paste_cur_pos += len_to_send;

      pastebuf_entry->sending_lasttime  = event->xproperty.time;

   }
else
   {
      /***************************************************************
      *
      *  We have sent all the data.  Send the zero length data to
      *  show we are done and turn off event watching for this window.
      *
      ***************************************************************/

      DEBUG2(fprintf(stderr, "Sending end marker\n");)

      DEBUG9(XERRORPOS)
      XChangeProperty(event->xproperty.display,
                      event->xproperty.window,
                      event->xproperty.atom,
                      XA_STRING,
                      8,  /* eight bit quantities */
                      PropModeAppend,
                      (unsigned char *)pastebuf_entry->paste_cur_pos,
                      0);

            DEBUG9(XERRORPOS)
            XSelectInput(event->xproperty.display,
                         event->xproperty.window,
                         None);

      pastebuf_entry->sending_to_window = None;
      pastebuf_entry->sending_to_prop   = None;
      pastebuf_entry->sending_lasttime  = 0;
      pastebuf_entry->sending_to_format = None;



   } /* end of else not sending any more data */

XFlush(event->xproperty.display);

return(True);

} /* end of more_pastebuf_data */

static Bool ReturnTrue(Display        *display, /* not used, but required for interface to X routines */
                       XEvent         *event,
                       char           *arg)
{
  return(True);
} /* end of 

/************************************************************************

NAME:      find_paste_buff_notify - Look for the notify that says we got
                                    a paste buffer we requested from another
                                    program.


PURPOSE:    This routine is the test routine used in a call to XIfEvent.
            It watches for the response to a XConvertSelection call.
            That is the call used by paste to ask for the paste buffer from
            another process.

PARAMETERS:

   1.  display     - pointer to Display (INPUT)
                     This is the pointer to the current display.

   2.  event       - pointer to XEvent (INPUT)
                     This is the pointer to event to be looked at.

   3.  arg         - pointer to char (INPUT)
                     This is the user argument specified in the call
                     to XIfEvent.  In this case it is the address of a
                     pair of pointers.  The first is to the main window id.
                     The second is to the paste buffer entry
                     for the selection atom we are looking for.
RETURNED VALUE:
   True - Found what we are looking for, or have given up
   False - Not found yet, keep looking

FUNCTIONS :

   1.   Unwrap the user parm.

   2.   If this event is a SelectionNotify on the main window for the
        correct atom, return True (we found it).  Otherwise return False.


*************************************************************************/

/*ARGSUSED*/
static Bool find_paste_buff_notify(Display        *display, /* not used, but required for interface to X routines */
                                   XEvent         *event,
                                   char           *arg)
{
char    **ifevent_arg;

Window            *main_x_window;
PASTEBUF_LIST     *pastebuf_entry;
Atom               selection_atom;
#ifdef DebuG
XEvent             BailEvent;
#endif

ifevent_arg    = (char **)arg;
main_x_window  = (Window *)ifevent_arg[0];
pastebuf_entry = (PASTEBUF_LIST  *)ifevent_arg[1];
selection_atom = pastebuf_entry->selection_atom;

pastebuf_XIfEvent_event_counter++;
if (pastebuf_XIfEvent_event_counter > MAX_XIFEVENT_COUNT)
   {
      DEBUG(fprintf(stderr, "find_paste_buff_notify: bailing out after searching %d events\n", MAX_XIFEVENT_COUNT);)

      /***************************************************************
      *  RES  04/13/2020
      *  Found issue with falling into this block of code.  The problem
      *  was that we were not reading key release events if there were
      *  no key release key definitions.  This caused them to stack up
      *  and eventually cause the XIfEvent to fail to find the property change.
      *  I added KeyReleaseMask to to the basic event mask in serverdef.c
      *  I will leave in this code which flushes all the events if this happens.
      ***************************************************************/
      while(XCheckIfEvent(display, &BailEvent, ReturnTrue, arg))
      {
         DEBUG2(dump_Xevent(stderr, &BailEvent, 3);)
      }

      return(True);
   }

if ((event->type                 == SelectionNotify)     &&
    (event->xselection.requestor == *main_x_window) &&
    (event->xselection.selection == selection_atom))
   {
#ifndef PGM_CEAPI
      DEBUG15(dump_Xevent(stderr, event, 3);)
#endif
      return(True);
   }
else
   {
      DEBUG(
      if ((event->type                 == SelectionNotify)     &&
          (event->xselection.selection == selection_atom))
         {
            fprintf(stderr, "find_paste_buff_notify: !!! Type is SelectionNotify and atom matches (%d), but not main window\n", selection_atom);
         }
      )
      return(False);
   }

} /* end of find_paste_buff_notify */


/************************************************************************

NAME:      get_pastebuf_data - Get X paste buffer data from another X client.


PURPOSE:    This routine gets the paste buffer data from another client.
            It starts with the response to a XConvertSelection call.
            That is the call used by paste to ask for the paste buffer from
            another server.  It hen either gets the data or handles INCR
            type data passing for large buffers.

PARAMETERS:

   1.  event       - pointer to XEvent (INPUT)
                     This is the response to the X selection event
                     we received in responce to the XConvertSelection call.
                     This event either says it failed or says the property
                     has been set on our window.

   2.  cmd         - pointer to char (INPUT)
                     This is the current Ce command name.  It is used in
                     message generation.

   3.  main_window - pointer to DRAWABLE_DESCR (INPUT)
                     This is the display pointer main window.

   4.  pastebuf_entry - pointer to PASTEBUF_LIST (INPUT)
                     This is the current paste buffer entry.
                     The data to paste is loaded into this area.

FUNCTIONS :

   1.   If the convert request failed, see if we have data to use.
        If not, fail.

   2.   If this event is a SelectionNotify on the main window for the
        correct atom, return True (we found it).  Otherwise return False.


*************************************************************************/

static FILE *get_pastebuf_data(XEvent          *event,
                               char            *cmd,
                               Display         *display,
                               Window           main_x_window,
                               PASTEBUF_LIST   *pastebuf_entry)
{

Atom              actual_type;
int               actual_format;
unsigned long     nitems;
unsigned long     bytes_after;
char             *p;
char             *q;
char             *ifevent_parm[2]; /* future, pass third parm which is counter, too many events - bail out */
int               done;
XEvent            event_union;
PASTE_DATA_LIST  *new_paste_area_ext;
PASTE_DATA_LIST  *last_paste_area_ext;
char             *property_data;
int               property_len;


/***************************************************************
*
*  If we could not get the property translated, see if we have
*  a paste buffer of our own in memory.  If so, use it.
*
***************************************************************/

if (event->xselection.property == None || event->xany.type != SelectionNotify)
   {
      DEBUG2(fprintf(stderr, "(%s) Conversion of X paste buffer entry for name %s failed\n", cmd, pastebuf_entry->name);)
      return(NULL);
   }


/***************************************************************
*
*  Free the existing paste buff area
*
***************************************************************/

if (pastebuf_entry->paste_area != NULL)
   if (pastebuf_entry->Xfree_area)
      XFree(pastebuf_entry->paste_area);
   else
      free(pastebuf_entry->paste_area);

pastebuf_entry->paste_area = NULL;
pastebuf_entry->paste_area_len = 0; /* RES 9/2/1998, Fixes problem with INCR processing */

if (pastebuf_entry->paste_area_ext != NULL)
   free_paste_area_ext(pastebuf_entry);

/***************************************************************
*
*  Set up to make sure the new paste window property is here.
*  This is unimportant except in INCR cases.  What we want to do
*  is pick up the property change (new value) for the selection.
*  At this point we know it is already sent, but we need to eat
*  the event so the future calls in the INCR processing will be
*  in sync with what is sent.
*
***************************************************************/

ifevent_parm[0] = (char *)&main_x_window;
ifevent_parm[1] = (char *)pastebuf_entry;

event_union.xproperty.state = PropertyDelete; /* init to the stay in loop value */
pastebuf_XIfEvent_event_counter = 0;

DEBUG9(XERRORPOS)
while((event_union.xproperty.state != PropertyNewValue) && (pastebuf_XIfEvent_event_counter <= MAX_XIFEVENT_COUNT))
   XIfEvent(display, &event_union, find_paste_buff_property, (char *)ifevent_parm);

if (pastebuf_XIfEvent_event_counter > MAX_XIFEVENT_COUNT)
   return(NULL);


/***************************************************************
*
*  Get the property.
*
***************************************************************/

DEBUG(if (display != event->xselection.display) fprintf(stderr, "Bad display value passed in xselection event, good 0x%X, bad 0x%X\n", display, event->xselection.display);)

DEBUG9(XERRORPOS)
XGetWindowProperty(display,
                   event->xselection.requestor,
                   event->xselection.property,
                   0, /* offset */
                   INT_MAX / 4,
                   True,  /* delete after retrieval */
                   AnyPropertyType,
                   &actual_type,
                   &actual_format,
                   &nitems,
                   &bytes_after,
                   (unsigned char **)&(pastebuf_entry->paste_area));

DEBUG2(
       DEBUG9(XERRORPOS)
       fprintf(stderr, "Selection %d (%s), Actual_type = %d (%s), actual_format = %d, nitems = %d, &bytes_after = %d, mylen = %d\n",
               event->xselection.selection, (p = XGetAtomName(event->xselection.display, event->xselection.selection)),
               actual_type, (q = XGetAtomName(event->xselection.display, actual_type)),
               actual_format, nitems, bytes_after, pastebuf_entry->paste_area_len);
       if (p)
          XFree(p);
       if (q)
          XFree(q);
)



/***************************************************************
*
*  Non-INCR processing is the norm.  This is a normal paste.
*  INCR processing only gets involved with large data transfers.
*
***************************************************************/

if (actual_type != CE_PROPERTY(incr_atom))
   {
      /***************************************************************
      *
      *  Initialize the paste buffer entry variables to allow fgets to work.
      *
      ***************************************************************/

      pastebuf_entry->paste_area_len  = nitems * (actual_format / 8);
      pastebuf_entry->paste_str_len   = pastebuf_entry->paste_area_len;
      pastebuf_entry->paste_cur_pos   = pastebuf_entry->paste_area;

   }
else
   {
      /***************************************************************
      *
      *  INCR processing.  The data is passed piecemeal.
      *  The data we just got was the INCR flag and the maximum data segment
      *  to be sent.  We are not really interested in the Max segment length.
      *  We have just deleted the incr property when we read it.  So,
      *  now we start getting pieces and chaining them together.  The
      *  first piece goes in paste area. Later ones go in paste_area_ext.
      *
      ***************************************************************/

      done = False;
      last_paste_area_ext = NULL;

      while(!done)
      {
         /***************************************************************
         *  Get the next newvalue for the property, throw away the
         *  delete events which will show up.
         ***************************************************************/
         event_union.xproperty.state = PropertyDelete; /* init to the stay in loop value */
         pastebuf_XIfEvent_event_counter = 0;

         DEBUG9(XERRORPOS)
         while((event_union.xproperty.state != PropertyNewValue) && (pastebuf_XIfEvent_event_counter <= MAX_XIFEVENT_COUNT))
            XIfEvent(display, &event_union, find_paste_buff_property, (char *)ifevent_parm);

         if (pastebuf_XIfEvent_event_counter > MAX_XIFEVENT_COUNT)
            return(NULL);

         /***************************************************************
         *  The property change event tells us the property is ready - get it.
         ***************************************************************/
         DEBUG9(XERRORPOS)
         XGetWindowProperty(event_union.xproperty.display,
                            event_union.xproperty.window,
                            event_union.xproperty.atom,
                            0, /* offset */
                            INT_MAX / 4,
                            True,  /* delete after retrieval */
                            AnyPropertyType,
                            &actual_type,
                            &actual_format,
                            &nitems,
                            &bytes_after,
                            (unsigned char **)&property_data);

         if (actual_type == None)
            {
               dm_error("Error getting paste buffer from server", DM_ERROR_BEEP);
               free_paste_area_ext(pastebuf_entry);
               break;
            }
         else
            {
               DEBUG2(
                DEBUG9(XERRORPOS)
                fprintf(stderr, "Property %d (%s), Actual_type = %d (%s), actual_format = %d, nitems = %d, &bytes_after = %d, mylen = %d\n",
                              event_union.xproperty.atom, (p = XGetAtomName(event_union.xproperty.display, event_union.xproperty.atom)),
                              actual_type, (q = XGetAtomName(event_union.xproperty.display, actual_type)),
                              actual_format, nitems, bytes_after, pastebuf_entry->paste_area_len);
                if (p)
                   XFree(p);
                if (q)
                   XFree(q);
               )
            }

         property_len = nitems * (actual_format / 8);
         DEBUG2(fprintf(stderr, "Got %d bytes of data\n", property_len);)

         /***************************************************************
         *
         *  The first piece goes in the pastebuf_entry proper just like a
         *  non-INCR transfer.
         *
         ***************************************************************/

         if (property_len != 0)
            if (pastebuf_entry->paste_area_len == 0)  /* first one */
               {
                  pastebuf_entry->paste_area      = property_data;
                  pastebuf_entry->paste_area_len  = property_len;
                  pastebuf_entry->paste_str_len   = pastebuf_entry->paste_area_len;
                  pastebuf_entry->paste_cur_pos   = pastebuf_entry->paste_area;
                  DEBUG2(fprintf(stderr, "Put in main paste area\n");)
               }
            else
               {
                  /***************************************************************
                  *
                  *  Additional paste buffers go on a linked list chained off
                  *  the pastebuf_entry.
                  *
                  ***************************************************************/

                  new_paste_area_ext = (PASTE_DATA_LIST *)CE_MALLOC(sizeof(PASTE_DATA_LIST));
                  if (!new_paste_area_ext)
                     {
                        free_paste_area_ext(pastebuf_entry);
                        return(NULL);
                     }
                  new_paste_area_ext->next = NULL;
                  new_paste_area_ext->data = property_data;
                  new_paste_area_ext->len  = property_len;

                  if (last_paste_area_ext == NULL)
                     pastebuf_entry->paste_area_ext = new_paste_area_ext;
                  else
                     last_paste_area_ext->next = new_paste_area_ext;

                  last_paste_area_ext = new_paste_area_ext;
                  DEBUG2(fprintf(stderr, "Put in paste area extension\n");)
               }
         else
            /***************************************************************
            *  The zero length property is the end marker.  The null on the
            *  end of the list, so to speak.
            ***************************************************************/
            {
               done = True;
               DEBUG2(fprintf(stderr, "Found eof marker\n");)
            }

      }  /* while not done */

      pastebuf_entry->Xfree_area = True;

   } /* else is the incr property */


pastebuf_entry->Xfree_area = True;
return((FILE *)pastebuf_entry);




} /* end of get_pastebuf_data */


/************************************************************************

NAME:      find_paste_buff_property - Look for the notify that says we got
                                      an incremental paste buffer piece we
                                      requested from another program.


PURPOSE:    This routine is the test routine used in a call to XIfEvent.
            It watches for the response to a PropertyNotify event.
            This is part of the incremental transfer of data handshake.
            It is used by paste to ask for the paste buffer from
            another process.

PARAMETERS:

   1.  display     - pointer to Display (INPUT)
                     This is the pointer to the current display.

   2.  event       - pointer to XEvent (INPUT)
                     This is the pointer to event to be looked at.

   3.  arg         - pointer to char (INPUT)
                     This is the user argument specified in the call
                     to XIfEvent.  In this case it is the address of a
                     pair of pointers.  The first is to the main window X id.
                     The second is to the paste buffer entry
                     for the selection atom we are looking for.

FUNCTIONS :

   1.   Unwrap the user parm.

   2.   If this event is a PropertyNotify on the main window for the
        correct atom, return True (we found it).  Otherwise return False.


*************************************************************************/

/*ARGSUSED*/
static Bool find_paste_buff_property(Display        *display, /* not used, but required for interface to X routines */
                                     XEvent         *event,
                                     char           *arg)
{
char    **ifevent_arg;
Window    *main_x_window;
#ifdef DebuG
XEvent             BailEvent;
#endif

ifevent_arg    = (char **)arg;
main_x_window  = (Window *)ifevent_arg[0];

pastebuf_XIfEvent_event_counter++;
if (pastebuf_XIfEvent_event_counter > MAX_XIFEVENT_COUNT)
   {
      DEBUG(fprintf(stderr, "find_paste_buff_property: bailing out after searching %d events\n", MAX_XIFEVENT_COUNT);)
      /***************************************************************
      *  RES  04/13/2020
      *  Found issue with falling into this block of code.  The problem
      *  was that we were not reading key release events if there were
      *  no key release key definitions.  This caused them to stack up
      *  and eventually cause the XIfEvent to fail to find the property change.
      *  I added KeyReleaseMask to to the basic event mask in serverdef.c
      *  I will leave in this code which flushes all the events if this happens.
      ***************************************************************/
      while(XCheckIfEvent(display, &BailEvent, ReturnTrue, arg))
      {
         DEBUG2(dump_Xevent(stderr, &BailEvent, 3);)
      }
      return(True);
   }

if ((event->type                 == PropertyNotify)     &&
    (event->xproperty.window     == *main_x_window) &&
    (event->xproperty.atom       == CE_PROPERTY(paste_buff_property_atom)))
   {
#ifndef PGM_CEAPI
      DEBUG15(dump_Xevent(stderr, event, 3);)
#endif
      return(True);
   }
else
   return(False);

} /* end of find_paste_buff_property */


/************************************************************************

NAME:      setup_paste_buff_dir - Verify that the paste buffer directory
                                  exists and is writable.


PURPOSE:    This routine is the test routine used in a call to XIfEvent.
            It watches for the response to a PropertyNotify event.
            This is part of the incremental transfer of data handshake.
            It is used by paste to ask for the paste buffer from
            another process.

PARAMETERS:

   1.  cmd         - pointer to char (INPUT)
                     This is a pointer to the name of the command which
                     caused this routine to be called.  It is used in
                     error message generation.

FUNCTIONS :

   1.   If the paste buffer name has not yet been determined, build it.

   2.   Create the directory if necessary.

   3.   Make sure the directory is writable.

RETURNED VALUE

   dir_name      - pointer to char
                   This is the name of the paste buffer directory or NULL
                   if the paste buffer could not be created.


*************************************************************************/

char *setup_paste_buff_dir(char              *cmd)
{
char      msg[512];

/***************************************************************
*
*  Make sure we can get to the paste buffer directory
*  If not, try to create it.
*
***************************************************************/
if (paste_buff_dir[0] == '\0')
   {
      strcpy(paste_buff_dir, "~/.CePaste");
      translate_tilde(paste_buff_dir);  /* process names starting with ~ if the name starts that way */
   }


if (access(paste_buff_dir, 07) != 0)
   {
#ifdef WIN32
      _mkdir(paste_buff_dir);
#else
      mkdir(paste_buff_dir, 0777);
#endif
      chmod(paste_buff_dir, 0755);
      if (access(paste_buff_dir, 07) != 0)
         {
            snprintf(msg, sizeof(msg), "(%s) No write access to paste buffer dir %s (%s)", cmd, paste_buff_dir, strerror(errno));
            dm_error(msg, DM_ERROR_LOG);
            return(NULL);
         }
   }

return(paste_buff_dir);

} /* end of setup_paste_buff_dir */


/************************************************************************

NAME:      closeup_paste_buffers - Release paste buffers during closeup


PURPOSE:    This routine is called during cleanup.  It releases the
            paste buffers we currently own.  It deals with the fact
            that there may be more than one set of windows open.

PARAMETERS:
   1.  display - pointer to Display (Xlib type) (INPUT)
                 This is the current display being processed.
                 It is needed in Xlib calls.

FUNCTIONS :

   1.   Get the name of the paste buffer directory.

   2.   Walk the list of X paste buffers looking for ones we own.

   3.   For ones we own, write the data to the paste buffer directory file.

   4.   Free up any data associated with pastebuffers for this display.


*************************************************************************/

void closeup_paste_buffers(Display    *display)
{
char              work[MAXPATHLEN];
PASTEBUF_LIST    *pastebuf_entry;
int               fd;
char             *paste_buff_dir;

/***************************************************************
*
*  Verify that the paste buff directory is set up correctly.
*
***************************************************************/

if ((paste_buff_dir = setup_paste_buff_dir("cleanup")) == NULL)
   return;

/***************************************************************
*  Walk the paste buffer list and look for ones we own the
*  selection on.  Save these in the real paste buffer files.
*  Also look for ones we have read in.  Get rid of these.
***************************************************************/

for (pastebuf_entry = first_pastebuf;
     pastebuf_entry != NULL;
     pastebuf_entry = pastebuf_entry->next)
{
   if (pastebuf_entry->own_selection
      && (pastebuf_entry->paste_area != NULL)
      && !pastebuf_entry->local
      && (pastebuf_entry->display == display))
      {
         DEBUG9(XERRORPOS)
         XSetSelectionOwner(display,  pastebuf_entry->selection_atom, None, CurrentTime);
         pastebuf_entry->own_selection = False;
         snprintf(work, sizeof(work), "%s/%s", paste_buff_dir, pastebuf_entry->name);
         fd = open(work, O_WRONLY | O_CREAT | O_TRUNC, 0666);
         if (fd == -1)
            {
               unlink(work);
               fd = open(work, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            }
         if (fd == -1)
            {
               DEBUG(fprintf(stderr, "Cannot open %s - %s\n", work, strerror(errno));)
            }
         else
            {
               write(fd, pastebuf_entry->paste_area, pastebuf_entry->paste_str_len);
               DEBUG2(fprintf(stderr, "Wrote %d chars to pastebuf %s\n", pastebuf_entry->paste_str_len, work);)
               close(fd);
            }
      }

   if (pastebuf_entry->display == display)
      {
         if (pastebuf_entry->paste_area != NULL)
            if (pastebuf_entry->Xfree_area)
               XFree(pastebuf_entry->paste_area);
            else
               free(pastebuf_entry->paste_area);

         pastebuf_entry->paste_area = NULL;

         if (pastebuf_entry->paste_area_ext != NULL)
            free_paste_area_ext(pastebuf_entry);

         pastebuf_entry->display = NULL;
      }

}

} /* end of closeup_paste_buffers */


/************************************************************************

NAME:      release_x_pastebufs - Clear ownership flags


PURPOSE:    This routine clears ownership flags of all paste buffers and
            releases storage associated with them.  It is used in the
            cv, ce, and cp commands after the fork.

PARAMETERS:
   None

FUNCTIONS :

   2.   Walk the list of X paste buffers.

   3.   Clear them all the ownership bits

   3.   Release data storage associated with the buffers.


*************************************************************************/

void release_x_pastebufs()
{
PASTEBUF_LIST      *pastebuf_entry;

for (pastebuf_entry =  first_pastebuf;
     pastebuf_entry != NULL;
     pastebuf_entry =  pastebuf_entry->next)
{
   pastebuf_entry->own_selection = False;
   pastebuf_entry->cut_buffer    = False;

   if (pastebuf_entry->paste_area != NULL)
      if (pastebuf_entry->Xfree_area)
         XFree(pastebuf_entry->paste_area);
      else
         free(pastebuf_entry->paste_area);

   pastebuf_entry->paste_area = NULL;

   if (pastebuf_entry->paste_area_ext != NULL)
      free_paste_area_ext(pastebuf_entry);

   pastebuf_entry->display = NULL;
}


} /* end of release_x_pastebufs */


/************************************************************************

NAME:      free_paste_area_ext - Free the paste buffe area extension.


PURPOSE:    This walks the linked list of paste buffer extensions
            and frees the data.  Paste buff extensions are a linked
            list of paste buffers used when a paste buffer is sent over
            using X Incr (incremental) processing.  The incremental pieces
            sent over are chained together in a linked list.

PARAMETERS:

   1.  pastebuf_entry - pointer to PASTEBUF_LIST (INPUT)
                        This is the pointer to the pastebuf entry to be processed.

FUNCTIONS :

   1.   Walk the list of paste buffer entries and look at free each one.
        Free both the paste buffer data and the entry itself.


*************************************************************************/

static void free_paste_area_ext(PASTEBUF_LIST   *pastebuf_entry)
{
PASTE_DATA_LIST   *curr_buff;
PASTE_DATA_LIST   *next_buff;

for (curr_buff = pastebuf_entry->paste_area_ext;
     curr_buff != NULL;
     curr_buff = next_buff)
{
   next_buff = curr_buff->next;
   XFree(curr_buff->data);
   free(curr_buff);
}

pastebuf_entry->paste_area_ext = NULL;

} /* end of free_paste_area_ext */


/************************************************************************

NAME:      load_next_paste_area_segment - Switch to the next  incremental paste buffer
                                          piece in the linked list of
                                          paste buffer extensions.


PURPOSE:    This routine is called when a paste buffer is exhausted by ce_fgets
            and there are more paste buffer extensions on the list.  It clears
            out the current main paste area and loads the next extension into
            the spot.  Paste buff extensions are a linked
            list of paste buffers used when a paste buffer is sent over
            using X Incr (incremental) processing.  The incremental pieces
            sent over are chained together in a linked list.

PARAMETERS:

   1.  pastebuf_entry - pointer to PASTEBUF_LIST (INPUT)
                        This is the pointer to the pastebuf entry to be processed.

FUNCTIONS :

   1.   Walk the list of paste buffer entries and look at free each one.
        Free both the paste buffer data and the entry itself.


*************************************************************************/

void load_next_paste_area_segment(PASTEBUF_LIST   *pastebuf_entry)
{
PASTE_DATA_LIST   *curr_buff;

XFree(pastebuf_entry->paste_area);

/* put the new buffer in place and set the current char pointer to the beginning */
pastebuf_entry->paste_area     = pastebuf_entry->paste_area_ext->data;
pastebuf_entry->paste_cur_pos  = pastebuf_entry->paste_area;

/* update the lengths with the data from the extention */
pastebuf_entry->paste_str_len  = pastebuf_entry->paste_area_ext->len;
pastebuf_entry->paste_area_len = pastebuf_entry->paste_area_ext->len;

/* free the paste buff extension */
curr_buff = pastebuf_entry->paste_area_ext;
pastebuf_entry->paste_area_ext = pastebuf_entry->paste_area_ext->next;

free(curr_buff);


} /* end of load_next_paste_area_segment */



/************************************************************************

NAME:      ce_fgets - Perform fgets function allowing for X paste buffers.


PURPOSE:    This routine either does an fgets if the passed stream is a real
            pointer to file, or gets one line from the X paste buffer whose
            address is passed as the stream pointer.

PARAMETERS:

   1.  str         - pointer to char (OUTPUT)
                     This is the target string to recieve the source.

   2.  max_len     - pointer to int (INPUT)
                     This is the maximum size of str.  At most max_len-1
                     chars are copied into str.

   3.  stream      - pointer to FILE (INPUT)
                     This is either a real stream or a pointer to
                     a pastebuf_entry.  The macro X_PASTE_STREAM
                     knows how to tell the difference.

FUNCTIONS :

   1.   If this is a real stream, do a real fgets and return.

   2.   If there is a problem with the pastebuf entry or we are at the
        end of the data, return NULL.

   3.   Copy one line (just line fgets) to the target or max_len-1 chars.
        put a '\0' at the end of the line and return str.

RETURNED VALUE:
   On eof or error NULL is returned.  Otherwise str (the parm) is returned.


*************************************************************************/

char *ce_fgets(char       *str,
               int         max_len,
               FILE       *stream)
{
char            *p;
char            *end;
PASTEBUF_LIST   *pastebuf_entry;


if (!X_PASTE_STREAM(stream))
   return(fgets(str, max_len, stream));
else
   {
      if (stream == NULL)
         return(NULL);

      pastebuf_entry = (PASTEBUF_LIST *)stream;
      if ((pastebuf_entry->paste_area == NULL)  || /* no buffer or */
          (pastebuf_entry->paste_str_len == 0)  || /* empty buffer or */
          (END_OF_BUFF(pastebuf_entry)) && (pastebuf_entry->paste_area_ext == NULL)) /* at end of last buffer */
         return(NULL);

      end = str + (max_len - 1);
      for (p = str; p < end; p++)
      {
         if ((END_OF_BUFF(pastebuf_entry)) && (pastebuf_entry->paste_area_ext != NULL))
            load_next_paste_area_segment(pastebuf_entry);

         if ((*(pastebuf_entry->paste_cur_pos) != '\n') && (!END_OF_BUFF(pastebuf_entry)))
            *p = *pastebuf_entry->paste_cur_pos++;
         else
            {
               *p = *(pastebuf_entry->paste_cur_pos);
               if (*(pastebuf_entry->paste_cur_pos) == '\n')
                  {
                     p++;
                     pastebuf_entry->paste_cur_pos++;
                  }
               break;
            }
      }

      *p = '\0';

      return(str);
   }

} /* end of ce_fgets */

/************************************************************************

NAME:      ce_fputs - Perform fputs function allowing for X paste buffers.


PURPOSE:    This routine either does an fputts if the passed stream is a real
            pointer to file, or puts one line of data into the X paste buffer whose
            address is passed as the stream pointer.

PARAMETERS:

   1.  str         - pointer to char (INPUT)
                     This is the string containing the data to write.

   2.  stream      - pointer to FILE (INPUT)
                     This is either a real stream or a pointer to
                     a pastebuf_entry.  The macro X_PASTE_STREAM
                     knows how to tell the difference.

FUNCTIONS :

   1.   If this is a real stream, do a real fputs and return.

   2.   If there is a problem with the pastebuf entry (such as overflow) return EOF.

   3.   Copy the data to the paste buffer area and put a newline at the end.


RETURNED VALUE:
   On error EOF (-1) is returned.  Otherwise the number of bytes copied is returned.


*************************************************************************/

int   ce_fputs(char      *str,
               FILE      *stream)
{
char            *end;
int              count = 0;
PASTEBUF_LIST   *pastebuf_entry;
char            *X_cur_pos;

/* RES 02/26/2003 Avoid memory fault if str is null */
if (!str)
   str = "";

if (!X_PASTE_STREAM(stream))
   return(fputs(str, stream));
else
   {
      if (stream == NULL)
         return(EOF);

      pastebuf_entry = (PASTEBUF_LIST *)stream;
      X_cur_pos = pastebuf_entry->paste_cur_pos;

      if ((pastebuf_entry->paste_area == NULL) || ((X_cur_pos - pastebuf_entry->paste_area) >= pastebuf_entry->paste_area_len))
         {
            dm_error("paste buffer overflow on fputs", DM_ERROR_LOG);
            return(EOF);
         }

      end = pastebuf_entry->paste_area + (pastebuf_entry->paste_area_len - 1);

      while((X_cur_pos < end) && (*str))
      {
         *X_cur_pos++ = *str++;
         count++;
      }

      *X_cur_pos = '\0';
      pastebuf_entry->paste_str_len += count;
      pastebuf_entry->paste_cur_pos = X_cur_pos;

      if (X_cur_pos == end)
         {
            dm_error("paste buffer overflow during fputs", DM_ERROR_LOG);
            count = EOF;
         }

      return(count);
   }
} /* end of ce_fputs */


/************************************************************************

NAME:      ce_fwrite - Perform fputs function allowing for X paste buffers.


PURPOSE:    This routine either does an fputts if the passed stream is a real
            pointer to file, or puts one line of data into the X paste buffer whose
            address is passed as the stream pointer.

PARAMETERS:

   1.  str         - pointer to char (INPUT)
                     This is the string containing the data to write.

   2.  stream      - pointer to FILE (INPUT)
                     This is either a real stream or a pointer to
                     a pastebuf_entry.  The macro X_PASTE_STREAM
                     knows how to tell the difference.

FUNCTIONS :

   1.   If this is a real stream, do a real fwrite and return.

   2.   If there is a problem with the pastebuf entry (such as overflow) return 0.

   3.   Copy the data to the paste buffer area and put a newline at the end.


RETURNED VALUE:
   The number of bytes successfully copied is returned.


*************************************************************************/

int   ce_fwrite(char     *str,
                int       size,
                int       nmemb,
                FILE     *stream)
{
char            *end;
int              count = 0;
int              num_bytes;
PASTEBUF_LIST   *pastebuf_entry;
char            *X_cur_pos;

if (!X_PASTE_STREAM(stream))
   return(fwrite(str, size, nmemb, stream));
else
   {
      if (stream == NULL)
         return(0);

      pastebuf_entry = (PASTEBUF_LIST *)stream;
      X_cur_pos = pastebuf_entry->paste_cur_pos;

      if ((pastebuf_entry->paste_area == NULL) || ((X_cur_pos - pastebuf_entry->paste_area) >= pastebuf_entry->paste_area_len))
         {
            dm_error("paste buffer overflow on fwrite", DM_ERROR_LOG);
            return(0);
         }

      end = pastebuf_entry->paste_area + (pastebuf_entry->paste_area_len - 1);
      num_bytes = size * nmemb;

      while((X_cur_pos < end) && (count < num_bytes))
      {
         *X_cur_pos++ = *str++;
         count++;
      }

      *X_cur_pos = '\0';
      pastebuf_entry->paste_str_len += count;
      pastebuf_entry->paste_cur_pos = X_cur_pos;

      return(count);
   }
} /* end of ce_fwrite */

#ifndef PGM_CEAPI
/************************************************************************

NAME:      get_paste_data_from_X - Pre load a paste buffer to make append
                                   mode processing work for X paste buffers.


PURPOSE:    This routine is called when an append mode copy is done to an
            X paste buffer.  The routine needs to load the buffer from whatever
            already exists in the paste buffer so it can be appended to.

PARAMETERS:

   1.  pastebuf_entry - pointer to FILE (fake) (INPUT/OUTPUT)
                     This is the paste buffer entry being processed.

   2.  dmc         - pointer to DMC (INPUT)
                     This is the DM command structure for the
                     xc command which caused this routine to be called.

   3.  main_window - pointer to DRAWABLE_DESCR (INPUT)
                     This is the description of the main window needed for
                     Xlib calls.

   4.  timestamp   - Time (INPUT)
                     This is the timestamp from the X event which caused
                     the Xcopy to be called.


FUNCTIONS :

   1.   Create a fake paste command to use to get to the paste buffer data.

   2.   Read the paste data and copy it to a temporary memdata structure.

   3.   Copy the data to the paste buffer.

   4.   Destroy the temporary memdata structure.


*************************************************************************/

static void  get_paste_data_from_X(FILE              *pastebuf_entry,
                                   DMC               *dmc,
                                   Display           *display,
                                   Window             main_x_window,
                                   Time               timestamp,
                                   char               escape_char)
{
DMC_xp            paste_dmc;
char             *p;
int               rc;
FILE             *paste_stream;
DATA_TOKEN       *temp_area;
int               line_count;
int               ending_newline;

/***************************************************************
*  Make a fake paste command an open the paste buffer for reading.
*  If this fails, there is nothing to do.
***************************************************************/
memcpy((char *)&paste_dmc, (char *)dmc, sizeof(DMC_xp));
paste_dmc.cmd   = DM_xp;
paste_dmc.temp  = False;

paste_stream = open_paste_buffer((DMC *)&paste_dmc, display, main_x_window, timestamp, escape_char);
if (!paste_stream)
   return;


/***************************************************************
*  Get a temporary memdata structure to hold the data and copy
*  it in.  Turn off undo processing to save time.
*  Put in one line with a '$".  After we read the
*  data, we can tell if there was a newline at the end of the
*  last line.  Then we get rid of it.
***************************************************************/
temp_area = mem_init(100, False);
undo_semafor = ON;
put_line_by_num(temp_area, 0, "$", INSERT);
put_block_by_num(temp_area, 0, 2, 0, paste_stream); /* 2 is an unused parameter */

/***************************************************************
*  check to see if the $ is still on a line by itself or is at
*  the end of a some other line.  Get rid of the $ and set the
*  newline flag accordingly.
***************************************************************/

p = get_line_by_num(temp_area, total_lines(temp_area)-1);
if ((p[0] == '$') && (p[1] == '\0'))
   {
      ending_newline = True;
      delete_line_by_num(temp_area, total_lines(temp_area)-1, 1);
   }
else
   {
      ending_newline = False;
      *(p+(strlen(p)-1)) = '\0'; /* kill off the $ */
   }
undo_semafor = OFF;


/***************************************************************
*  Get the size of the data to copy and allocate space for it.
***************************************************************/
rc = setup_x_paste_buffer(pastebuf_entry,
                          temp_area,
                          0,
                          total_lines(temp_area),
                          0 /* Not rectangular */);
if (rc != 0)
   return;

/***************************************************************
*  Copy the data to the paste buffer;
***************************************************************/
position_file_pointer(temp_area, 0);
line_count = total_lines(temp_area);
while((p = next_line(temp_area)) != NULL)
{
   ce_fputs(p, pastebuf_entry);
   line_count--;
   if (line_count || ending_newline)
      ce_fputs("\n", pastebuf_entry);
}

mem_kill(temp_area);

} /* end of get_paste_data_from_X */
#endif  /* ifndef PGM_CEAPI */


/************************************************************************

NAME:      get_pastebuf_size - return number of bytes in a paste buffer.


PURPOSE:    This routine returns the number of bytes in a one part or
            multi part paste buffer.  It does not include the null terminator.

PARAMETERS:

   1.  stream      - pointer to FILE (INPUT)
                     This is a pointer to the paste buffer.

FUNCTIONS :

   1.   If this is a real stream, get the size from an fstat

   2.   Get the size of the current entry.

   3.   If there are extensions, add them in.

RETURNED VALUE:
   bytes   -  int
              -1  -  Bad paste buffer address passed
            other -  count of bytes of paste buffer data.


*************************************************************************/

int   get_pastebuf_size(FILE   *stream)
{
PASTEBUF_LIST   *pastebuf_entry;
PASTE_DATA_LIST *more_data;
int              count;
struct stat      file_stats;


if (X_PASTE_STREAM(stream))
   {
      if (stream == NULL)
         return(-1);

      pastebuf_entry = (PASTEBUF_LIST *)stream;
      if ((pastebuf_entry->paste_area == NULL) || (pastebuf_entry->paste_str_len == 0)  || (END_OF_BUFF(pastebuf_entry)) && (pastebuf_entry->paste_area_ext == NULL))
         return(0);

      count = pastebuf_entry->paste_str_len;

      more_data = pastebuf_entry->paste_area_ext;

      while(more_data)
      {
         count += more_data->len;
         more_data = more_data->next;
      }
   }
else
   {
      count = fstat(fileno(stream), &file_stats);
      if (count == 0)
         count = file_stats.st_size;
   }

return(count);


} /* end of get_pastebuf_size */


#ifdef apollo
/************************************************************************

NAME:      read_cutbuff_data - Open a pastebuf entry for input by reading from
                               a property off the root window.


PURPOSE:    This attempts to get the named property off the root window as if
            it was a cut buffer.  If the property exists, it is used as the paste data.

PARAMETERS:

   1.  pastebuf_entry - pointer to PASTEBUF_LIST (INPUT)
                     This is the current paste buffer entry which is being
                     opened.  If cut data is read, it is loaded into this structure.


FUNCTIONS :

   1.   Attempt to read the passed window property from the root window.

   2.   If there is no property, return NULL, there is no data.

   3.   If there is data, attach it to the pastebuf entry and return this
        address type cast as a pointer to FILE.


RETURNED VALUE:
   file  -    pointer to FILE
              If data is gotten the pastebuf_entry is returned cast
              as a pointer to file.


*************************************************************************/

static FILE *read_cutbuff_data(PASTEBUF_LIST   *pastebuf_entry)
{
Atom              actual_type;
int               actual_format;
unsigned long     nitems;
unsigned long     bytes_after;
char             *p;
char             *property_data;
int               property_len;

/***************************************************************
*  Free the existing paste buff area
***************************************************************/
if (pastebuf_entry->paste_area != NULL)
   if (pastebuf_entry->Xfree_area)
      XFree(pastebuf_entry->paste_area);
   else
      free(pastebuf_entry->paste_area);

pastebuf_entry->paste_area = NULL;

if (pastebuf_entry->paste_area_ext != NULL)
   free_paste_area_ext(pastebuf_entry);

/***************************************************************
*  See if there is a property off the root window with the correct name.
**************************************************************/
DEBUG9(XERRORPOS)
XGetWindowProperty(pastebuf_entry->display,
                   RootWindow(pastebuf_entry->display, DefaultScreen(pastebuf_entry->display)),
                   pastebuf_entry->selection_atom,
                   0, /* offset */
                   INT_MAX / 4,
                   False,  /* Do NOT delete after retrieval */
                   XA_STRING,
                   &actual_type,
                   &actual_format,
                   &nitems,
                   &bytes_after,
                   (unsigned char **)&property_data);

property_len = nitems * (actual_format / 8);


DEBUG9(XERRORPOS)
DEBUG2(
  if (actual_type != None)
     p = XGetAtomName(pastebuf_entry->display, actual_type);
  else
     p = "None";
  fprintf(stderr, "read_cutbuff_data:  Size = %d, actual_type = %d (%s), actual_format = %d, nitems = %d, &bytes_after = %d\n",
                   property_len, actual_type, p,
                   actual_format, nitems, bytes_after);
  if (actual_type != None)
     XFree(p);
)

/***************************************************************
*  If there is no property, there is nothing to look at.
***************************************************************/

if (actual_type == None)
   return(NULL);

pastebuf_entry->paste_area      = property_data;
pastebuf_entry->paste_area_len  = property_len;
pastebuf_entry->paste_str_len   = strlen(property_data);
pastebuf_entry->paste_cur_pos   = pastebuf_entry->paste_area;
pastebuf_entry->Xfree_area = True;
return((FILE *)pastebuf_entry);

} /* end of read_cutbuff_data */


/************************************************************************

NAME:      kill_cutbuff_data - get rid of any residual pastebuf date held
                               in a cut buffer.


PURPOSE:    This routine reads the cut buffer with delete if it exists.

PARAMETERS:

   1.  pastebuf_entry - pointer to PASTEBUF_LIST (INPUT)
                     This is the current paste buffer entry which is being
                     opened.  It is used to clear the data.


FUNCTIONS :

   1.   Attempt to read the passed window property from the root window with delete.

   2.   We do not care if it succeeds or fails.


*************************************************************************/

static void kill_cutbuff_data(PASTEBUF_LIST   *pastebuf_entry)
{
Atom              actual_type;
int               actual_format;
unsigned long     nitems;
unsigned long     bytes_after;
char             *property_data;
int               property_len;

/***************************************************************
*  See if there is a property off the root window with the correct name.
**************************************************************/
DEBUG9(XERRORPOS)
XGetWindowProperty(pastebuf_entry->display,
                   RootWindow(pastebuf_entry->display, DefaultScreen(pastebuf_entry->display)),
                   pastebuf_entry->selection_atom,
                   0, /* offset */
                   INT_MAX / 4,
                   True,  /* Delete after retrieval */
                   XA_STRING,
                   &actual_type,
                   &actual_format,
                   &nitems,
                   &bytes_after,
                   (unsigned char **)&property_data);

} /* end of read_cutbuff_data */
#endif


#ifndef PGM_CEAPI
/************************************************************************

NAME:      same_display -   Determine if two displays are the same


PURPOSE:    This routine determines if two display pointers point to the
            same display.

PARAMETERS:

   1.  dpy1        - pointer to Display (Xlib type) (INPUT)
                     This is the first display to check.  Several fields in
                     this structure are looked at.

  21.  dpy2        - pointer to Display (Xlib type) (INPUT)
                     This is the second display to check.  Several fields in
                     this structure are looked at.


FUNCTIONS :

   1.   Do the stupid check first (same display passed twice)

   2.   Get the internet address of the remote end of the socket.
        If they are different, the displays are different.

   3.   Get the display number from the display.  If these are different,
        the displayes are different.

RETURNED VALUE:
   same     -   int (flag)
                True  -  The displays are the same
                False -  The displays are different


*************************************************************************/

int   same_display(Display  *dpy1, 
                   Display  *dpy2)
{
unsigned long     net_addr1;
unsigned long     net_addr2;
unsigned long     scrap_netmask;
char              display_no1[256];
char              display_no2[256];
char             *p;
int               dpy1_local;
int               dpy2_local;

static Display   *last_dpy1;
static Display   *last_dpy2;
static int        last_result;

if (dpy1 == dpy2)
   return(True);

if (((last_dpy1 == dpy1) && (last_dpy2 == dpy2)) || ((last_dpy1 == dpy2) && (last_dpy2 == dpy1)))
   return(last_result);

if (strcmp(DisplayString(dpy1), DisplayString(dpy2)) == 0)
   return(True);

/***************************************************************
*  Check each name for the local display.
***************************************************************/
dpy1_local = ((DisplayString(dpy1)[0] == ':') || (strncmp(DisplayString(dpy1), "unix:", 5) == 0) || (strncmp(DisplayString(dpy1), "local:", 6) == 0));
dpy2_local = ((DisplayString(dpy2)[0] == ':') || (strncmp(DisplayString(dpy2), "unix:", 5) == 0) || (strncmp(DisplayString(dpy2), "local:", 6) == 0));

if (dpy1_local && dpy2_local)
   return(True);

if (dpy1_local)
   netlist(&net_addr1, 1, &scrap_netmask);
else
   net_addr1 = socketfd2hostinet(ConnectionNumber(dpy1) ,DISPLAY_PORT_REMOTE); /* in netlist.c */
if (dpy2_local)
   netlist(&net_addr2, 1, &scrap_netmask);
else
   net_addr2 = socketfd2hostinet(ConnectionNumber(dpy2) ,DISPLAY_PORT_REMOTE);

if (net_addr1 != net_addr2)
   last_result = False;
else
   {
      p = strchr(DisplayString(dpy1), ':');
      if (p)
         strcpy(display_no1, p+1);
      else
         display_no1[0] = '\0';

      p = strchr(DisplayString(dpy2), ':');
      if (p)
         strcpy(display_no2, p+1);
      else
         display_no2[0] = '\0';

      p = strchr(display_no1, '.');
      if (p)
         *p = '\0';

      p = strchr(display_no2, '.');
      if (p)
         *p = '\0';

      if (strcmp(display_no1, display_no2) != 0)
         last_result = False;
      else
         last_result = True;
  }

last_dpy1 = dpy1;
last_dpy2 = dpy2;

DEBUG2(fprintf(stderr, "same_display: Displays 0x%X and 0x%X are %s\n", dpy1, dpy2, (last_result ? "the same" : "different"));)

return(last_result);

} /* end of same_display */
#endif

