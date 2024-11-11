#ifndef _BUFDESCR_H_INCLUDED
#define _BUFDESCR_H_INCLUDED

/* "%Z% %M% %I% - %G% %U% " */

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
*  Include file:  bufdescr.h
*
*  PAD_DESCR
*  
*  Definition of the pad description.  This structure describes
*  each of the pads.  There are up to 4.  They describe the
*  main pad, dm_input, dm_output, and unix command windows.
*  This structure contains the anchor for the memdata database
*  which backs up each window and describes the portion of the
*  data currently viewable.  Each structure is referenced in the
*  display description (DISPLAY_DESCR).  The actual window shapes
*  and sizes are described by the drawable descriptions (drawable.h).
*
*  Description of fields:
*
*  which_window
*  This is one of the values MAIN_PAD, DMINPUT_WINDOW, etc.
*  It is initialized in display.c when the pad descriptions are
*  created and never changed.
*
*  file_line_no
*  This is the line number of in the file (zero based) of the current
*  line.  This is the most recent line under
*  the cursor when some sort of DM command was executed.  For the
*  DM input and output windows, this field is always zero.
*  This is the line number the buff_ptr points to.
*
*  first_line
*  This is the file line number of the first line currently displayed.
*
*  file_col_no
*  This is the zero based column number in the file where the cursor is,
*  This column number may be used as an offset into buff_ptr.
*  It is a character offset into the line of text without tabs being expaneded. 
*
*  first_char
*  This is the zero based character number of the left most character
*  on the screen.  This field is important in scrolling horizontally.
*  It is always in tab expanded characters.
*
*  lines_displayed
*  This is the number of lines currently displayed.  It is normally equal
*  to element window.lines_on_screen, but may be larger if lines are
*  excluded.  It is updated by textfill_drawable in xutil.c
*
*  buff_ptr
*  This field points to the buffer for the line described by file_line_no.
*  Note that when a modifying DM command is detected, the buff_ptr is pointed to
*  a copy of the line in the work buffer.
*
*  work_buff_ptr
*  This field is used in all but the current_cursor buff description.
*  It statically points to the work buffer for this window.  This is
*  the buffer used when text is being inserted and deleted.
*
*  buff_modified
*  This flag shows whether the work buffer has been modified and must be
*  replaced back in the file.
*
*  buff_modified
*  This flag shows whether the work buffer has been modified and must be
*  replaced back in the file.
*
*  redraw_mask
*  This field is permanently set in the buffer assigned to each window.
*  It is the redraw bit mask value which shows this window needs to be
*  redrawn.
*
*  display_data
*  This field is permanently set to the display description containing
*  this buffer description.
*
*  insert_mode
*  This field is modified by the ei command.  1(True) means the
*  buffer is in insert mode, 0(False) means overstrike mode.
*  It is a pointer to an insert mode flag used for the entire display description.
*
*  echo_mode
*  This one of the values: NO_HIGHLIGHT, REGULAR_HIGHLIGHT, RECTANGULAR_HIGHLIGHT
*  which are defined in txcursor.h.  It tells whether highlighting is active.
*  It is a pointer to an echo mode flag used for the entire display description.
*
*  hold_mode
*  This field is modified by the wh command.  1(True) means the
*  window is in hold mode, 0(False) means normal mode.
*  It is relevant only when pad mode is on in the unixcmd and main buffer descriptions.
*  It is a pointer to an hold mode flag used for the entire display description.
*
*  autohold_mode
*  This field is modified by the wa command.  1(True) means the
*  window is in autohold mode, 0(False) means non-autohold mode.
*  It is relevant only when pad mode is on in the the main buffer description.
*  It is a pointer to an autohold mode flag used for the entire display description.
*
*  vt100_mode
*  This field is modified by the vt100 command.  1(True) means the
*  window is in vt100 emulation mode to support vi and other such programs.
*   0(False) means normal mode.
*  It is relevant only when pad mode is on in the the main buffer description.
*  It is a pointer to an vt100 mode flag used for the entire display description.
*
*  x_window
*  This is the X11 window id for the window this pad refers to.  For the main
*  pad, this is the window used for events.  The dm_input, dm_output, and unix
*  windows are children of this window.
*
*  window
*  This points to the drawable description of the window associated
*  with this pad.  It is used by the routines in xutil.c and other
*  X drawing routines.  It tells the size and shape of the window
*  and contains data about colors and what fonts are in use.
*  It also describes the usable area within the window.  It is modified
*  when the window changes location.
*
*  pix_window
*  This points to the drawable description which describes the window as
*  overlaid on the pixmap used to back up the window.  It is the same
*  size as the window description, but the sub window (usable area) portion
*  of the window is offset into the pixmap by the correct amount.
*  
*  redraw_start_line
*  This is the window line number on the screen to redraw when a routine
*  returns a redraw type of PARTIAL_REDRAW or PARTIAL_LINE.
*
*  redraw_start_col
*  This is the column number on the window to start the redraw.
*  When a routines returns a redraw type of PARTIAL_LINE..
*
*  redraw_scroll_lines
*  If a scrolling redraw is requested, this is the number of lines to
*  scroll.  This may be a negative number is scrolling up.
*  scroll redraw is only used for vertical scrolling.
*
*  impacted_redraw_line
*  This field serves a similar purpose to redraw_start_line in cc mode
*  when the display containing this buffer description is impacted by a
*  change to another window.  It is set in cc_plbn in cc.c and examined 
*  process_redraw in redraw.c.  It is only used for the main window.
*  This number is a window line number.
*
*  impacted_redraw_mask
*  This field is used in cc mode along with impacted_redraw_line.  It is
*  the redraw mask for the display which is impacted by a change in
*  another window.  For example.  Suppose there are two cc windows up and
*  they both show the top page of data.  In one window a new line is entered.
*  This field is used to mark the redrawing instructions for the other window.
*  This field is only used for the main window which is the only one shared.
*
*  window_wrap_col
*  When the ww command is active, this is the column at which window 
*  wrapping is to take place.
*
*  formfeed_in_window
*  This the window line number (zero based) of the line
*  which contains the form feed.  Note that a form feed
*  on line zero and no form feed are the same thing.
*  This is only used in the Main window description when 
*  hex mode is off.
*
*  win_lines_size
*  This points is the number of elements in win_lines.
*
*  win_lines
*  This points to the window lines array buffer for this window.  For
*  the main window, this array has one element for each line.  Each
*  element contains a pointer to the line displayed on that line of the
*  window and its length in pixels.  The line is already adjusted for 
*  left shifting.  The pointer points to the first char displayed on
*  the line.  If no portion of the line is displayed because of shifting,
*  the pointer points to a null string.
*  
*  token
*  This is an opaque pointer used by the memdata routines. (memdata.h).
*  This value is associated with a window and is needed for all
*  the memdata calls which reference this window.
*
***************************************************************/
/**************************************************************
*
*  BUFF_DESCR
*  
*  Definition of the buffer description.  This structure
*  is used to tell where the cursor is.  It describes the cursor \
*  position and contains window based pointers.  It is updated
*  as motion events come in.  It is used to update the pad description
*  position information when keystrokes arrive.  It is updated 
*  from the pad description when the cursor needs to be moved.
*  
*  Description of fields:
*
*  current_win_buff
*  This field field points to the pad description where the cursor
*  currently is.
*
*  which_window
*  This is one of the values MAIN_PAD, DMINPUT_WINDOW, etc.
*  It allows for a quick check to see which buffer description
*  this is.  Certain commands such as en (enter) need this information.
*
*  up_to_snuff
*  This flag shows whether the data in the current cursor position
*  buff is up to date.  Cursor motion events only update a minimal
*  subset of the data in the structure.  Before it is used by
*  keypress events, it is brought up to snuff by having the other
*  fields filled in.  Also data is tranfered to the PAD descriptions
*  in the up to snuff process.
*
*  win_line_no
*  This is the line number of the current line in the window.  Line
*  zero is the first line displayed in the window.
*
*  win_col_no
*  This is the column number on the window of where the cursor is.
*  It is the column number in tab expaned columns.  That is,  it is
*  the position after the tabs are expanded to blanks.
*
*  x
*  This is the current x coordinate of the cursor in the window.
*
*  y
*  This is the current y coordinate of the cursor in the window.
*
*  start_x
*  This is the x coordinate of the cursor in the window at the start
*  of a keystroke or button press.  It is set in crpad.c and used in redraw.c
*  It is used to see if the cursor really moved.
*
*  start_y
*  This is the y coordinate of the cursor in the window at the start
*  of a keystroke or button press.  It compliments start_x.
*
*  start_which_window
*  This value is copied from which_window at the same time start_x
*  and start_y are copied.   With start_x and start_y it defines
*  a window position.
*
*
***************************************************************/

#include <X11/Xproto.h>

#include   "memdata.h"    /* needed for DATA_TOKEN */
#include   "drawable.h"   /* needed for DRAWABLE_DESCR */

#ifdef WIN32
#ifdef WIN32_XNT
#include "xnt.h"
#else
#include "X11/X.h"         /* /usr/include/X11/X.h */
#include "X11/Xatom.h"     /* /usr/include/X11/Xatom.h */
#endif
#else
#include <X11/X.h>         /* /usr/include/X11/X.h */
#include <X11/Xatom.h>     /* /usr/include/X11/Xatom.h */
#endif

struct DISPLAY_DESCR  ;

typedef struct {
   int                   which_window;           /* One of MAIN_PAD, DMINPUT_WINDOW, OR DMOUTPUT_WINDOW */
   int                   file_line_no;           /* line in file                        */
   int                   first_line;             /* first line on screen                */
   int                   file_col_no;            /* col no on the line tabs count for 1 */
   int                   first_char;             /* offset to first char on screen (line based col no)      */
   int                   lines_displayed;        /* Lines currently displayed, includes excluded lines */
   char                 *buff_ptr;               /* Current buffer                      */
   char                 *work_buff_ptr;          /* Permanent maxlen buffer to modify   */
   int                   buff_modified;          /* shows buff was modified             */
   void                 *cdgc_data;              /* Color data work area                */
   int                   redraw_mask;            /* One of the REDRAW_* below           */
   struct DISPLAY_DESCR *display_data;           /* pointer to overall display structure*/
   int                  *insert_mode;            /* points toTrue for insert false for overwrite */
   int                  *hold_mode;              /* True for hold mode (after deref) - pad support    */
   int                  *autohold_mode;          /* True for autohold mode (after deref) - pad support    */
   int                  *vt100_mode;             /* True for vt100 mode emulation (after deref) - pad support    */
   Window                x_window;               /* X window id for this pads window    */
   DRAWABLE_DESCR       *window;                 /* Window + useable subwindow shape    */
   DRAWABLE_DESCR       *pix_window;             /* Window shape positioned on pixmap   */
   int                   redraw_start_line;      /* If partial redraw is needed, window line to start on */
   int                   redraw_start_col;       /* If partial line redraw is needed, window col to start redrawing */
   int                   redraw_scroll_lines;    /* If scroll redraw is needed, number of lines to scroll */
   int                   impacted_redraw_line;   /* If change in cc window impacts this window, window line affected */
   int                   impacted_redraw_mask;   /* If change in cc window impacts this window,redraw mask */
   int                   window_wrap_col;        /* wrap column when window wrap is turned on */
   int                   formfeed_in_window;     /* Window lineno of the line with form feed */
   int                   win_lines_size;         /* number of elements in win_lines     */
   WINDOW_LINE          *win_lines;              /* window_lines array for buffer       */
   DATA_TOKEN           *token;                  /* memdata token for memory data       */

} PAD_DESCR;


typedef struct {
   PAD_DESCR         *current_win_buff;       /* current buf for the new window or NULL */
   int                which_window;           /* One of MAIN_PAD, DMINPUT_WINDOW, UNIXCMD_WINDOW, OR DMOUTPUT_WINDOW */
   int                up_to_snuff;            /* Boolean flag                        */
   int                win_line_no;            /* line on screen                      */
   int                win_col_no;             /* cursor col on window tabs expaneded */
   int                x;                      /* Current X position                  */
   int                y;                      /* Current Y position                  */
   int                start_x;                /* X position at last keystroke        */
   int                start_y;                /* Y position at last keystroke        */
   int                start_which_window;     /* which window at last keystroke      */
   int                root_x;                 /* root X position at last keystroke   */
   int                root_y;                 /* root Y position at last keystroke   */

} BUFF_DESCR;


/***************************************************************
*  
*  Values for the which_window field of a BUFF_DESCR.
*  The names array is for the purpose of debugging messages.
*  
***************************************************************/

#define  MAIN_PAD          1
#define  DMINPUT_WINDOW    2
#define  DMOUTPUT_WINDOW   3
#define  UNIXCMD_WINDOW    4

/***************************************************************
*  constants used in message generation
***************************************************************/
#ifdef _MAIN_
char *which_window_names[] = {"Undefined", "MAIN_PAD", "DMINPUT_WINDOW", "DMOUTPUT_WINDOW", "UNIX_CMD"};
#else
extern char *which_window_names[];
#endif


/***************************************************************
*  
*  Routines which return an int which is the redraw_needed
*  flag return one of these three values.  This value is used
*  as a bitmask telling whether the main window, DM input window,
*  or DM output window need to be redrawn.
*
*  There are 15 piece of information to carry.  5 possible windows
*  and three values for each window.  The _MASK defines tell which
*  window.  Each nibble is assigned to a window.  For example, the
*  MAIN_PAD has the low order nibble turned on.  The other three
*  defines have the same bit turned on in each nibble and represent
*  the activitly.  For eaxmple the low bit in each nibble says
*  that a full redraw is needed.  When you "and" the two defines
*  together, you get one bit on which tells which window to act
*  upon and which operation to do.
*
*  The reason for this strategy is that in a long string of
*  DM commands under a single keystroke, it is possible that
*  several windows may need updating, and one window may get
*  several types of redraw requests.  The greatest redraw
*  will take precedence.  That is, if a full redraw is needed
*  in a window, there is no point in doing a partial redraw or
*  a partial line redraw.  This layout allows the various redraw
*  bits to be or'ed together in the main command loop as DM 
*  commands are processed with the resulting accumulated information
*  being easy to process.
*
*  If processing a composite redraw, you can "and" with the 
*  window mask and tell if this window is affected.  You can
*  "and" with the redraw mask to tell what kind of action is
*   required.
*  
***************************************************************/

#define  MAIN_PAD_MASK     0x0000000F
#define  DMINPUT_MASK      0x000000F0
#define  DMOUTPUT_MASK     0x00000F00
#define  TITLEBAR_MASK     0x0000F000
#define  UNIXCMD_MASK      0x000F0000

#define  FULL_REDRAW       0x00011111
#define  PARTIAL_REDRAW    0x00022222
#define  PARTIAL_LINE      0x00044444
#define  SCROLL_REDRAW     0x00088888
/* note for impacted redraws, LINENO_REDRAW is used in place of SCROLL_REDRAW */
#define  LINENO_REDRAW     0x00088888

/***************************************************************
*  
*  Definition of the MARK_REGION structure.  It contains
*  information about the marks and regions over which commands
*  operate.  There are two of these marks in existence.  Both
*  are declared in the main procedure (crpad.c) and passed
*  as parms to routines.  mark1 is the mark set when a dr
*  command is executed.  tdm_mark remembers where the cursor
*  was on the main screen when the tdm command is executed.
*  
*  Description of fields:
*  mark_set
*  This flag specifies whether the mark is in the set or not
*  set condition.
*
*  file_line_no
*  This is the line number from the beginning of file where the
*  mark exists.
*
*  col_no
*  This is the column number of the mark.  This is with 
*  respect to the beginning of the line.  To find the
*  column on the window, the firstchar field for the 
*  window must be removed.
*
*  buffer
*  This contains the address of the buffer description for
*  the window which contains the mark.  This is ususually
*  main_window_cur_descr
*
*
***************************************************************/

typedef struct {
   int                mark_set;               /* true = has data in it        */
   int                file_line_no;           /* line in file                 */
   int                col_no;                 /* col in line (zero base)      */
   int                corner_row;             /* first row on screen, used by sl and rl */
   int                corner_col;             /* first col on screen, used by sl and rl */
   PAD_DESCR         *buffer;                 /* Window buffer affected       */
} MARK_REGION;


/***************************************************************
*  
*  Data used by find/replace routines, included in the display description.
*  
***************************************************************/

typedef struct {
   void              *private;                /* private static data in dmfind        */
   void              *search_private;         /* private static data in search.c      */
   int                in_progress[2];         /* flags used withing getevent.c to flag find and substitute in progress, invisible outside */ 
} FIND_DATA;


/***************************************************************
*  
*  Public part of the pulldown bar drawing data.
*  
***************************************************************/

typedef struct {
   int                menubar_on;          /* set by pdm command                       */
   Window             menubar_x_window;    /* the X window id for the menubar (top)    */
   DRAWABLE_DESCR     menu_bar;            /* description of the menubar in both pixmap and main window */

   int                pulldown_active;     /* true if in menubar window or a pd window */
   int                pd_microseconds;     /* non-zero if sb button is down            */
   XEvent             button_push;         /* Event to pass when button is down        */

   void              *pd_private;
} PD_DATA;



/***************************************************************
*  
* Property atoms used by Ce
*  
*  
*  Module cc.c
*
*  CE_CC_LINE  is the property used to pass off line updates to 
*              other windows.  Properties are hung off CE windows.
*  CE_CC       is the property used to pass a cc request to
*              another window.  This is done during initialization.
*              This property is humg off the CE window of the 
*              session we are requesting to do a CC
*  CE_CCLIST   is used as a selection and a property to keep track
*              of the CE edit sessions currently in use.  This
*              property is hung off eachmain  window.
*
*  Module pastebuf.c
*              These atoms are used internally byt pastebuf.c.  They 
*              represent the different types of data which may be
*              copied via paste buffers.
*
*  Module init.c
*
*  WM_SAVE_YOURSELF  is the property used to tell the window manager
*                    we are interested in being notified if we are being
*                    terminated so we can save ourselves and set up
*                    to be restarted by the session manager.
*  WM_DELETE_WINDOW  is the property used to tell the window manager that
*                    we want to be sent a client message to close a window
*                    instead of being sent a kill.
*  WM_STATE          This is the property which is set with the restart 
*                    information (command line data).  It is used by
*                    the session manager to restart the program.
*  WM_PROTOCOLS      is the property used to pass the save yourself and
*                    delete window properties to the window manager.
*
*  Module serverdef.c
*
*  CeKeys            is the property where the parsed key definitions
*                    are saved.  It is set on the root window.
*
*  kb_event_mask       This mask is maintained as key defintions are processed.
*                      There are 4 keyboard type events which CE is interested in.
*                      KeyPress, KeyRelease, ButtonPress, or ButtonRelease.
*                      We always start out assuming we will need to process KeyPress
*                      and ButtonPress events.  However, if there are no button up
*                      (eg. m2u) definitions or no keypress up (eg. F1u) definitions,
*                      there is no need to request these type events from the X server.
*                      This mask keeps track of what is required for the event processing
*                      routines.
*
*  last_kb_event_mask  This field is used in getevent.c to detect that kb_event_mask
*                      has changed.
*
*  main_event_mask     This is the current event mask for the main window for this display.
*
*  dm_event_mask       This is the current event mask for the dminput
*                       and dmoutput windows for this display.
*
*  keydef_property_size  Current size of key definitions in the X server that
*                       we know about.
*
*  expecting_change    Counter used internally.  Count of pending expected
*                      key definition property changes against the main window.
*
*  Module wdf.c
*
*  CE_WDF_LIST is the property used to contain the geometry strings
*              it is set by dm_wdf and read by wdf_get_geometry.
*  CE_WDF_Q    is the property used by wdf_get_geometry to 
*              determine who gets which window geometry.
*  CE_WDF_LASTWIN is the property used by to save the geometry
*
*              from the last window closed.
*  CE_WDC_LIST is the property used to contain the colorset strings
*              it is set by dm_wdc and read by wdc_get_colors
*  CE_WDF_Q    is the property used by wdc_get_colors to 
*              determine who gets which window geometry.
*  CE_WDC_LASTWIN is the property used by to save last color set
*              from the last window closed.
*  
***************************************************************/

typedef struct {
      /* cc.c */
      Atom            ce_cc_line_atom;
      Atom            ce_cc_atom;
      Atom            ce_cclist_atom;

      /* pastebuf.c */
      Atom            paste_buff_property_atom; /* pastebuf.c use only */
      Atom            text_atom;                /* pastebuf.c use only */
      Atom            targets_atom;             /* pastebuf.c use only */
      Atom            timestamp_atom;           /* pastebuf.c use only */
      Atom            incr_atom;                /* pastebuf.c use only */
      Atom            length_atom;              /* pastebuf.c use only */

      /* init.c */
      Atom             save_yourself_atom;
      Atom             delete_window_atom;
      Atom             wm_protocols_atom;
      Atom             wm_state_atom;

      /* serverdef.c */
      Atom               cekeys_atom;
      unsigned int       kb_event_mask;          /* set when setting up key definitions, used in getevent.c */
      unsigned int       last_kb_event_mask;     /* used by getevent.c                   */
      unsigned int       main_event_mask;        /* used by getevent.c                   */
      unsigned int       dm_event_mask;          /* used by getevent.c                   */
      int                keydef_property_size;   /* used in serverdef.c                  */
      int                expecting_change;       /* used in serverdef.c                  */

      /* wdf.c */
      Atom  ce_wdf_list_atom;                    /* wdf.c use only */
      Atom  ce_wdf_wq_atom;                      /* wdf.c use only */
      Atom  ce_wdf_lastwin_atom;                 /* wdf.c use only */
      Atom  ce_wdc_list_atom;                    /* wdf.c use only */
      Atom  ce_wdc_wq_atom;                      /* wdf.c use only */
      Atom  ce_wdc_lastwin_atom;                 /* wdf.c use only */

} PROPERTIES;


/***************************************************************
*  
*  Display description structure
*  
***************************************************************/

struct DISPLAY_DESCR {
   struct DISPLAY_DESCR *next;              /* next descr in the list, currently not used             */
   int                   display_no;        /* Sequential number assigned to display, starts at 1     */
   Display              *display;           /* X display pointer used as a key                        */
#ifdef WIN32
   unsigned long         term_mainpad_mutex; /* mutex for threads updating dspl_decsr */
   unsigned long         main_thread_id;     /* ID of thread which can make X calls   */
#endif
   Pixmap                x_pixmap;          /* X pixmap backing all the windows                       */
   BUFF_DESCR           *cursor_buff;       /* pointer to cursor buff for this display                */
   PAD_DESCR            *main_pad;          /* pointer to pad buff for this display                   */
   PAD_DESCR            *dminput_pad;       /* pointer to pad buff for this display                   */
   PAD_DESCR            *dmoutput_pad;      /* pointer to pad buff for this display                   */
   PAD_DESCR            *unix_pad;          /* pointer to pad buff for this display                   */
   DRAWABLE_DESCR       *title_subarea;     /* description of titlebar area, both window & pixmap     */
   DRAWABLE_DESCR       *lineno_subarea;    /* decripttion of line area, both window & pixmap         */
   Window                wm_window;         /* outermost window for properties, the window manager's containing window */
   int                   insert_mode;       /* True for insert false for overwrite                    */
   int                   caps_mode;         /* True for convert typing to case caps                   */
   int                   echo_mode;         /* One of NO_HIGHLIGHT, REGULAR_HIGHLIGHT, RECTANGULAR_HIGHLIGHT */
   int                   hold_mode;         /* True for hold mode  - pad support                      */
   int                   scroll_mode;       /* True for scroll mode  - pad support                    */
   int                   autohold_mode;     /* True for autohold mode - pad support                   */
   int                   autoclose_mode;    /* True for close window on eef  - pad support            */
   int                   vt100_mode;        /* True for vt100 mode emulation  - pad support           */
   int                   show_lineno;       /* True if lineno is on                                   */
   int                   pad_mode;          /* True if this display is doing terminal emulation       */
   int                   edit_file_changed; /* True if a pad has been given a name via pn, set in all dspl descrs  */
   int                   expecting_focus_mouseclick;/* Set in explicet focus mode when user clicks in the window */
   int                   background_scroll_lines; /* In pad mode, number of lines that need to be scrolled */
   int                   autoindent;        /* True if autoindent is on                               */
   int                   which_cursor;      /* One of NORMAL_CURSOR or BLANK_CURSOR                   */
   int                   auto_save_limit;   /* non-zero for autosave keystroke count                  */
   int                   case_sensitive;    /* case sensitiveity flag                                 */
   int                   hex_mode;          /* True if hex mode is on                                 */
   int                   redo_cursor_in_configure;   /* flag to show we just issued a geo or vt cmd, used in Configure Notify event processing */
   int                   first_expose;      /* Flag to mark the first expose so we can put the cursor in the upper left corner */
   int                   dmoutput_last;     /* Marker in dm output window to detect redraw needed     */
   int                   dm_kk_flag;        /* Flag set by dm_kk command to make keynames be output   */
   int                   XFree86_flag;      /* Flag set from ServerVendor, XFree86 at this display    */
   int                   Hummingbird_flag;  /* Flag set from ServerVendor, Hummingbird Exceed at this display    */
   int                   find_border;       /* Offset from top of screen for unencumberd find         */
   int                   padding_between_line;/* Percent of char height for padding during drawing    */
   int                   button_down;       /* Flags showing which button down at any time            */
   int                   drag_scrolling;    /* Flag for when button down drag out of window causes scrolling */
   int                   linemax;           /* Max Lines for a transcript pad                         */
   int                   reload_off;        /* Set to true after reload -n                            */
   void                 *txcursor_area;     /* Static work area pointer used by txcursor.c            */
   void                 *cmd_record_data;   /* Static data area used by dm_rec, non-null if recording */
   char                **option_values;     /* hanger for the parm options per display                */
   char                 *option_from_parm;  /* 1 byte flags match option_values true if from argv     */
   FIND_DATA            *find_data;         /* Static data used by dmfind.  malloced by dmfind.c      */
   PROPERTIES           *properties;        /* property atoms used by ce                              */
   void                 *indent_data;       /* Static data used by dm_ind.  malloced by indent.c      */
   void                 *warp_data;         /* Static data used with warping cursor - getvent.c       */
   void                 *dmwin_data;        /* Static data used by dmwin.c                            */
   void                 *hsearch_data;      /* hsearch table                                          */
   void                 *prompt_data;       /* Static data used by prompt.c                           */
   void                 *cursor_data;       /* Static data used by mouse.c                            */
   void                 *focus_data;        /* Static data used by window.c                           */
   void                 *gc_private_data;   /* Static data used by cdgc.c                             */
   void                 *xsmp_private_data; /* Single data area used by xsmp.c */
   int                   xsmp_active;       /* flag, True means xsmp is active */
   void                 *ind_data;          /* data allocated and used by ind.c                       */
   char                 *current_font_name; /* current font name, no wild cards                       */
   char                 *wm_title;          /* Title set by the title commmand                        */
   SCROLLBAR_DESCR      *sb_data;           /* Public data from sbwin.c                               */
   PD_DATA              *pd_data;           /* Public data from pd.c                                  */
   char                 *VT_colors[8];      /* set from vtcolors option, colors for ceterm normal mode color processing */
   char                  titlebar_host_name[256]; /* host name saved for the titlebar                 */
   char                  display_host_name[256];  /* name of host we are displayed on or null string if on current host */
   MARK_REGION           mark1;
   MARK_REGION           mark2;             /* used by sl and gl */
   MARK_REGION           comma_mark;
   MARK_REGION           tdm_mark;
   MARK_REGION           root_mark;
   char                  escape_char;
   char                  currently_mapped;  /* True if window is currently mapped */ 
   char                  padding[2];
  };

typedef struct DISPLAY_DESCR DISPLAY_DESCR;


#define IS_HUMMINGBIRD_EXCEED(dpy) (ServerVendor(dpy) && (strstr(ServerVendor(dpy), "Hummingbird Communications") != NULL))


#endif

