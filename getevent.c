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
*  module getevent.c
*
*  Routines:
*     get_next_X_event        - Get the next event, control background work.
*     expect_warp             - Mark warp data to show we are waiting for a warp
*     duplicate_warp          - Check if a potential warp is a duplicate
*     expecting_warp          - Check if a we are currenly waiting on a warp
*     find_valid_event        - CheckMaskEvent compare routine to find an event given a mask
*     find_event              - CheckMaskEvent compare routine to find a single event type
*     change_background_work  - Turn on or off background tasks.
*     get_background_work     - Retrieve the current state of background task variables
*     finish_keydefs          - Force the Keydefs to finish before continuing.
*     autovt_switch           - Perform switching for AUTOVT mode
*     set_event_masks         - Set the input event masks, for the windows.
*     window_to_buff_descr    - Find the pad description given the X window id.
*     find_padmode_dspl       - Find the pad mode display description
*     set_edit_file_changed   - Set the edit_file_changed in all dspl descriptions
*     load_enough_data        - Make sure we are at eof or at least enough data is read in.
*     kill_unixcmd_window     - set flag to kill off the unixcmd window when it is no longer needed.
*     fake_keystroke          - Generate a fake keystroke event to trigger other things.
*     store_cmds              - Store commands to be triggered with a fake keystroke
*     set_global_dspl         - Set the current global display to a particular display.
*
*  Internal:
*     do_background_task      - Handle the processing of background work.
*     ce_XIfEvent_pad         - Scan all displays for a typed event.
*     check_background_work   - calculate something_to_do_in_background
*     add_commas              - Make an integer a printable character string with commas inserted.
*     any_find_or_sub         - Scan display list for finds or subs in progress
*     any_scroll              - Scan display list for background scrolling in progress
*     adjust_motion_event     - Fixup motion events when the mouse button is held down
*     reset_child             - Reset the child state signal handler in pad.c
*     close_unixcmd_window    - Get rid of the unixcmd window after the shell has gone away.
*     check_pad_eof_char      - Fix output of eof char when pad changes to edit window
*     get_private_data        - Get the private warp data from the display description.
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h      */
#include <sys/types.h>      /* /usr/include/sys/types.h  */
#ifndef WIN32
#include <sys/time.h>       /* /usr/include/sys/time.h   */
#include <signal.h>         /* /usr/include/signal.h     */
#else
#include <time.h>
#include <winsock.h>
#endif

#include "borders.h"
#include "cc.h"
#include "debug.h"
#include "dmfind.h"
#include "dmwin.h"
#include "dumpxevent.h"
#include "emalloc.h"
#include "execute.h"     /* needef for reap_child_process    */
#include "getevent.h"
#include "ind.h"
#include "init.h"
#include "kd.h"
#include "lineno.h"
#include "lock.h"
#include "mouse.h"
#ifdef PAD
#include "pad.h"         /* needed for macros TTY_ECHO_MODE and TTY_DOT_MODE */
#include "pw.h"          /* needed for stdin string in close_unixcmd_window */
#endif
#include "parms.h"
#include "redraw.h"
#include "sbwin.h"
#include "scroll.h"
#include "search.h"
#include "sendevnt.h"
#include "tab.h"
#include "timeout.h"
#include "titlebar.h"
#include "txcursor.h"
#include "typing.h"
#include "vt100.h"
#include "window.h"
#include "windowdefs.h"
#ifdef PAD
#include "unixwin.h"
#endif
#include "unixpad.h"
#include "wc.h"
#include "xerrorpos.h"

/***************************************************************
*  
*  display head,  this is the head of the circlular linked
*  list of display pointers.  The head is the first one to be
*  looked at in the case of input being ready on multiple descriptors.
*  The head is rotated except in the case of warps, where the head
*  pointer is nailed to the display doing the warp.
*  
***************************************************************/

static DISPLAY_DESCR        *dspl_descr_head = NULL;

/***************************************************************
*  
*  Flags and variables for controlling background work.  
*  Background works is done when there are no events pending on
*  the X event queue.  Each flag is for a different kind of
*  background work.  The flags are held static global to this module.
*  They can be set from the outside via calls to change_background_work.
*  find_in_progress and substitute in progress are macros which look
*  at data attached to the display description.  To avoid the appearance
*  that these variables could be manipulated outsize this module,
*  while maintaining the ability to free the storage allocated for other
*  pieces of the find_data structure we make them macros.
*
*  something_to_do_in_background is a generic flag set by change_background_work
*  to show that something is up.  Routine do_background_task sorts out
*  what actually needs to be done.
*  
***************************************************************/

static int            something_to_do_in_background = 0;

#define  FIND_IN_PROGRESS         find_data->in_progress[0]
#define  SUBSTITUTE_IN_PROGRESS   find_data->in_progress[1]

static int            keydefs_in_progress = 0;

static int            main_window_eof = 0;
static int            read_ahead = 0;

/***************************************************************
*  Mask for mouse buttons.
***************************************************************/
#define ANY_BUTTON (Button1Mask | Button2Mask | Button3Mask | Button4Mask | Button5Mask)

/***************************************************************
*  
*  expecting_warp
*
*  This variable is global withing the getevent.c module.
*  It is critical in the processing of the cursor motion
*  and is used to perform certain event serialization functions.
*  expecting_warp is set by the process redraw and redo_cursor
*  routines when a warp_pointer or fake_warp call is made.
*  Actual updating of the variables is handled by routine expect warp.
*  When this flag gets set, event processing is modified to delay
*  PropertyChange events, button press (and release) events,
*  and keypress (and release) events.  This prevents the user
*  from entering position dependent data until the cursor is
*  positioned.  Configure notify events cause the most grief.
*  If the window changes size, the configure notify code attempts
*  to reposition the cursor to the correct place in the window.
*  If a warp is in progress, the process_redraw code does not
*  do the warp.  The warp is also skipped if the cursor has
*  moved outside the main window.  This is determined by a call
*  to in_window.  In window returns a flag which is maintained
*  by routines window_crossing and force_in_window.  Window_crossing
*  looks at enter and leave events and keeps track of whether the
*  cursor is in the window.  Force_in_window forces the flag to lie
*  and say the cursor is in the window when it may not be.  This
*  is used by the "ti" command to get the cursor from another window.
*  
*  The code that looks at motion and enter events in get_next_X_event,
*  is the receiving end of the Xwarp pointer.  When an X warp pointer
*  is issued, the event that is received does not have the sent
*  event flag turned on.  To identify this event agaist the mess of
*  other pointer motion events which may show up, the motion event
*  coordinates are compared against the ones used to send the event.
*  If they match, we got it.  On some machines, an X warp pointer 
*  which crosses a window boundary generates an enter window event.
*  Sometimes it generates a motion event too.  If we are looking for
*  a motion event, we make sure the X and Y coordinates match.  If
*  we get an enter event we just make sure the window matches.
*
*  When in mouse off mode or vt100 mode, we send enternotify events
*  instead of doing actuall warp cursors.  This has the effect of
*  disconnecting the mouse cursor from the text cursor.
*  
*  
***************************************************************/

typedef struct {
   int            expecting_warp;
   int            warped_to_x;
   int            warped_to_y;
   Window         warped_to_window;
   int            warped_motion_count;
} WARP_DATA;

static WARP_DATA dummy_warp_data;

#define WARPED_MOTION_LIMIT 5

#ifdef PAD
/***************************************************************
*  
*  This flag is used for serialization in pad mode when the
*  shell terminates.  The shell terimination is flagged by a
*  asynchronous signal SIGCLD.  As it is not good to have
*  asynchronous tasks write on the xserver socket, we set a
*  flag which is checked in the event loop.
*  
***************************************************************/

static int            shell_has_terminated = False;
static int            reset_child_signal = False;
#endif

/***************************************************************
*  
*  Prototypes for the internal routines.
*  
***************************************************************/

static Bool ce_XIfEvent_pad(DISPLAY_DESCR  *dspl_descr,
                            XEvent         *event_return,
                            Bool          (*chk_rtn)( Display   *    /* display */,
                                                      XEvent    *    /* event */,
                                                      char      *    /* arg */
                                                    ) 
                            );

static void  do_background_task(DISPLAY_DESCR *dspl_descr);

static char *add_commas(int value);

static void check_background_work(void);

static DISPLAY_DESCR *any_find_or_sub(DISPLAY_DESCR *dspl_descr);

static DISPLAY_DESCR *any_scroll(DISPLAY_DESCR *dspl_descr);

static void  adjust_motion_event(XEvent *event);

#ifdef PAD
static void reset_child(int *reset_child_signal);

static void close_unixcmd_window(void);
#endif

void check_pad_eof_char(void);

static WARP_DATA *get_private_data(DISPLAY_DESCR  *dspl_descr);


/************************************************************************

NAME:      get_next_X_event

PURPOSE:    This routine get the next x event which is needed.  It also
            handles the performing of background tasks.

PARAMETERS:

   1.  event_union     - pointer to XEvent (OUTPUT)
                         The event is returned in this parm.

   3.  shell_fd        - pointer int  (INPUT/OUTPUT)
                         In pad_mode, this is the file descriptor for the 
                         2 way socket to the shell.  In non-pad mode, this is
                         not used and is set to -1;
                            
   3.  cmd_fd          - pointer int  (INPUT/OUTPUT)
                         If the -input parm was specified, this is the file descriptor
                         for the alternate source of DM commands.  If it is used, it
                         is always zero, (stdin).
                         On end of file, it is set to -1.
                         Values:  -1    -  cmd_fd not in use
                                  other - fd to get commands from.
                            


GLOBAL INPUT DATA:

   1.  something_to_do_in_background -  int (INPUT)
               This flag shows that there is something to do in background.

FUNCTIONS :

   1.   If there is nothing to do in background, do a blocking read for
        the next appropriate type of event.  This is either whatever is
        there or a motion event.

   2.   If there is something do do in background, do a non-blocking read
        for the next appropriate event.  If there are none, do some background
        work.

   3.   If we complete all the background work, do a blocking read for the
        event.


*************************************************************************/

void get_next_X_event(XEvent      *event_union,    /* output */
                      int         *shell_fd,       /* input  */
                      int         *cmd_fd)         /* input  */
{

static long           movement_event_mask = EnterWindowMask
                                          | ExposureMask
                                          | StructureNotifyMask
                                          | LeaveWindowMask
                                          | VisibilityChangeMask
                                          | PropertyChangeMask
                                          | FocusChangeMask     /* 11/2/93 RES add focus change permanently on */
                                          | PointerMotionMask;
int                  *temp_cmd_fd;
Bool                  got_event = False;
#define               MAX_EVENT_LOOP_COUNT 200
int                   event_loop_count = 0;
WARP_DATA            *warp_data;

if (!dspl_descr_head)
   dspl_descr_head = dspl_descr; /* initialization for the first event */
else
   dspl_descr = dspl_descr_head; /* set_global_dspl() */

warp_data = get_private_data(dspl_descr);


/***************************************************************
*  
*  If we have moved out of the window. Forget about getting the
*  warp we wanted.  ADDED 11/2/93  RES
*  
***************************************************************/

if (warp_data->expecting_warp && !in_window(dspl_descr))
   {
      DEBUG10(fprintf(stderr, "Canceling Expecting a warp (%d,%d) in 0x%X\n", warp_data->warped_to_x, warp_data->warped_to_y, warp_data->warped_to_window);)
      warp_data->expecting_warp = False;
   }

/***************************************************************
*  
*  If there is not something to do in background, get the next
*  appropriate event.  If we are expecting a warp, wait for it.
*  Otherwise take whatever comes along.  This logic is for the
*  odd case where a second keystroke gets between the issue of
*  the warp from typing a letter and the arrival of the warp here.
*  The result is that the cursor jumps backwards.
*  
***************************************************************/
DEBUG10(if (warp_data->expecting_warp) fprintf(stderr, "Expecting a warp (%d,%d) in 0x%X\n", warp_data->warped_to_x, warp_data->warped_to_y, warp_data->warped_to_window);)

if (warp_data->expecting_warp)
   ce_XIfEvent(dspl_descr->display, event_union, find_valid_event, (char *)&movement_event_mask);
else
   {
      /***************************************************************
      *  
      *  There is something to do in background mode, we will take
      *  events if they are there since they take priority, however,
      *  if there are no events in the queue, do some background work.
      *  Thus we use the non-blocking x calls.
      *  
      ***************************************************************/

      DEBUG9(XERRORPOS)
      while (something_to_do_in_background && (!(got_event = ce_XIfEvent_pad(dspl_descr, event_union, find_valid_event))))
         {
            do_background_task(dspl_descr);
            timeout_set(dspl_descr);  /* if scroll bars are on and button is pressed, do the timing */
         }

      /***************************************************************
      *  
      *  For pad mode or when reading dm commands from stdin, we need
      *  to watch multiple file descriptors via the wait_for_input
      *  routine.  We also have to watch for certain signals to show
      *  up such as the shell ending or the need to reset the signal
      *  handler which is required on some machines.
      *  
      ***************************************************************/

      if (dspl_descr->pad_mode
          || (dspl_descr->next != dspl_descr)
          || dspl_descr->sb_data->sb_microseconds
          || dspl_descr->pd_data->pd_microseconds
          || dspl_descr->xsmp_active
          || (*cmd_fd != -1))
         {
            temp_cmd_fd = cmd_fd;

            while(!got_event)
            {
#ifdef PAD
               if (reset_child_signal)
                  reset_child(&reset_child_signal);

               if (shell_has_terminated)
                  {
                     close_unixcmd_window();
#ifdef WIN32
                     closesocket(*shell_fd);
#endif
                     *shell_fd = -1;
                  }
#endif

               DEBUG9(XERRORPOS)
               got_event = ce_XIfEvent_pad(dspl_descr, event_union, find_valid_event);
               if (!got_event)
                  {
                     dspl_descr = wait_for_input(dspl_descr, /* set_global_dspl() */
                                                *shell_fd,
                                                temp_cmd_fd);
                     warp_data = get_private_data(dspl_descr);
                  }

#ifdef PAD
               if (shell_has_terminated)
                  {
                     close_unixcmd_window();
#ifdef WIN32
                     closesocket(*shell_fd);
#endif
                     *shell_fd = -1;
                  }
#endif

               event_loop_count++;
               if (!got_event && (event_loop_count > MAX_EVENT_LOOP_COUNT) && in_window(dspl_descr))
                  {
                     DEBUG15(fprintf(stderr, "Too many loops looking for an event, taking next event\n");)
                     DEBUG9(XERRORPOS)
                     XNextEvent(dspl_descr->display, event_union);
                     got_event = True;
                  }
            }  /* while not got event */
         }
      else
         {
            DEBUG9(XERRORPOS)
            if (!got_event)
               ce_XNextEvent(dspl_descr->display, event_union);
         }
   } /* end of not expecting warp */


/***************************************************************
*  
*  If we are expecting a warp, this is one of the special cases.
*  Leave the cursor alone.  Otherwise, on a motion type event
*  set the cursor to normal and on a keypress type event set the
*  cursor to blank.
*
*  which_cursor is defined in mouse.h and controlled by the routines
*  in mouse.c (normal_mcursor and blank_mcursor).
*  
***************************************************************/

if (MOUSEON && !dspl_descr->vt100_mode && (event_union->xany.window != dspl_descr->sb_data->vert_window) && (event_union->xany.window != dspl_descr->sb_data->horiz_window))
   {
      if (!warp_data->expecting_warp)
         if ((dspl_descr->which_cursor == BLANK_CURSOR) && ((event_union->type == MotionNotify) || (event_union->type == EnterNotify) || (event_union->type == LeaveNotify)))
            normal_mcursor(dspl_descr);
         else
            if ((dspl_descr->which_cursor == NORMAL_CURSOR) && ((event_union->type == KeyPress) || (event_union->type == ButtonPress) || (event_union->type == KeyRelease) || (event_union->type == ButtonRelease)))
               blank_mcursor(dspl_descr);
   }


/***************************************************************
*  
*  If we were expecting a warp, we got some non-keyboard event.
*  If we got the real warp, clear the flag and mark the
*  event as sent.  We sent the event via a XWarpPointer which 
*  considers the movement the same as if the user moved it, however
*  we wish to distinguish between the two.
*  This is extremely sensitive code.  When an expose/event combination
*  is issued from process_redraw, we need to receive the 
*  expose and motion in the order they were sent, however we must
*  not get any keystrokes in between.  Otherwise the keystrokes get
*  put in the wrong place in the buffer.
*  
***************************************************************/

if (warp_data->expecting_warp)
   {
      if ((event_union->type == MotionNotify && event_union->xmotion.x == warp_data->warped_to_x && event_union->xmotion.y == warp_data->warped_to_y) ||
          (event_union->type == EnterNotify && event_union->xcrossing.window == warp_data->warped_to_window))
         {
            event_union->xany.send_event = True;
            DEBUG10(fprintf(stderr, "Got the warp, (%d,%d) in window 0x%X\n", event_union->xmotion.x, event_union->xmotion.y, event_union->xmotion.window);)
            /* next if added 9/29/93 RES, to fix sun problem where np9 window position followed by np8 put cursor in wrong place */
            if (event_union->type == EnterNotify)
               {
                  DEBUG10(
                     if ((event_union->xcrossing.x != warp_data->warped_to_x) || (event_union->xcrossing.y != warp_data->warped_to_y))
                        fprintf(stderr, "Changing crossing to (%d,%d)\n", warp_data->warped_to_x, warp_data->warped_to_y);
                  )
                  event_union->xcrossing.x = warp_data->warped_to_x;
                  event_union->xcrossing.y = warp_data->warped_to_y;
               }
            warp_data->expecting_warp = False;
         }
      else
         if ((event_union->type == MotionNotify) || (event_union->type == EnterNotify))
            {
               warp_data->warped_motion_count++;
               if (warp_data->warped_motion_count > WARPED_MOTION_LIMIT)
                  {
                     DEBUG10(fprintf(stderr, "Too long a wait, clearing warp with event, (%d,%d) in window 0x%X\n", event_union->xmotion.x, event_union->xmotion.y, event_union->xmotion.window);)
                     warp_data->expecting_warp = False;
                  }
            }
   }

/***************************************************************
*  
*  If the right debug stuff is on, dump the event info.
*  cursor motion takes a special debug since there is so much of it.
*  This needs to go after the possible setting of the send_event field.
*  
***************************************************************/

DEBUG0(
if (event_union->type != MotionNotify)
   DEBUG15(dump_Xevent(stderr, event_union, 3);)
else
   DEBUG14(dump_Xevent(stderr, event_union, 3);)
)



/***************************************************************
*  
*  In the case of motion events with a button held down, the
*  X server will continue to send events relative to the window
*  the cursor was in when the button was pressed down.  We need
*  to adjust these when going between the dm_input, dm_output,
*  and main windows.
*  
***************************************************************/

if (event_union->xany.type == MotionNotify && (event_union->xmotion.state & ANY_BUTTON))
   adjust_motion_event(event_union);

if (!warp_data->expecting_warp)
   dspl_descr_head = dspl_descr->next; /* rotate the head of the list */

/*escape_char = dspl_descr->escape_char;*/

} /* end of get_next_X_event */


/************************************************************************

NAME:      expect_warp - Mark that a warp pointer has been done.

PURPOSE:    This routine marks fields in the private warp data structure.

PARAMETERS:
   1.  dspl_descr  - pointer to DISPLAY_DESCR (INPUT/OUTPUT)
                     This is the display description to be looked at.

   2.  window      - Window (Xlib type) (INPUT)
                     This is the window we are warping to.

   3.  x           - int (INPUT)
                     This is the x pixel location we are warping to

   4.  y           - int (INPUT)
                     This is the y pixel location we are warping to

*************************************************************************/

void expect_warp(int             x,
                 int             y,
                 Window          window,
                 DISPLAY_DESCR  *dspl_descr)
{
WARP_DATA            *warp_data;

warp_data = get_private_data(dspl_descr);

warp_data->expecting_warp      = True;
warp_data->warped_to_x         = x;
warp_data->warped_to_y         = y;
warp_data->warped_to_window    = window;
warp_data->warped_motion_count = 0;
dspl_descr_head = dspl_descr; /* force the warp to be processed next */
} /* end of expect_warp */


/************************************************************************

NAME:      duplicate_warp - Test for warp to place we already want to warp

PURPOSE:    This routine tests fields in the private warp data structure
            and returns true if we are expecting a warp to the place
            referenced in the parameters.

PARAMETERS:
   1.  dspl_descr  - pointer to DISPLAY_DESCR (INPUT)
                     This is the display description to be looked at.

   2.  window      - Window (Xlib type) (INPUT)
                     This is the window we wish to test against

   3.  x           - int (INPUT)
                     This is the x pixel location relative to the passed window.

   4.  y           - int (INPUT)
                     This is the y pixel location relative to the passed window.

RETURNED VALUE
   duplicate  -  int
                 True   -  Already warped to that location
                 False  -  Have not already warped to that location

*************************************************************************/

int duplicate_warp(DISPLAY_DESCR    *dspl_descr, 
                   Window            window,
                   int               x,
                   int               y)
{
WARP_DATA            *warp_data;

warp_data = get_private_data(dspl_descr);

if (!warp_data ||
    !warp_data->expecting_warp  ||
    (warp_data->warped_to_window != window)  ||
    (warp_data->warped_to_x != x)  ||
    (warp_data->warped_to_y != y))
   return(False);
else
   return(True);

} /* end of duplicate_warp */


/************************************************************************

NAME:      expecting_warp - Test for expecting a warp on the passed display.

PURPOSE:    This routine tests fields in the private warp data structure
            and returns true if we are expecting a warp.

PARAMETERS:
   1.  dspl_descr  - pointer to DISPLAY_DESCR (INPUT/OUTPUT)
                     This is the display description to be looked at.

RETURNED VALUE
   expecting_warp  -  int
                      True   -  expecting a warp.
                      False  -  not expecting a warp.

*************************************************************************/

int expecting_warp(DISPLAY_DESCR    *dspl_descr)
{
WARP_DATA            *warp_data;

warp_data = get_private_data(dspl_descr);

if (!warp_data || !warp_data->expecting_warp)
   return(False);
else
   return(True);

} /* end of expecting_warp */


/************************************************************************

NAME:      ce_XIfEvent_pad - Check all displays for X events


PURPOSE:    This routine calls ce_XIfEvent first for the current display and
            if no events are ready, for all additional displays.  If an event
            is found, the display pointer is set to the display with the ready
            event.

PARAMETERS:

   1.  dspl_descr  - pointer to DISPLAY_DESCR (INPUT/OUTPUT)
                     This is the pointer to the current display.  It is
                     modified to point to the display which has the ready event.

   2.... The rest of the parameters match those of Xlib routine XIfEvent.
                     dspl_descr replaces the display pointer.

FUNCTIONS :

   1.   Check if there are events ready on the current display.

   2.   If the current display has no events, check the other displays.

RETURNED VALUE
   found      -  Bool (X type)
                 True is returned if we find an event.
                 False otherwise.


*************************************************************************/

static Bool ce_XIfEvent_pad(DISPLAY_DESCR  *dspl_descr,
                            XEvent         *event_return,
                            Bool          (*chk_rtn)( Display   *    /* display */,
                                                      XEvent    *    /* event */,
                                                      char      *    /* arg */
                                                    ) 
                            )
{
Bool                  got_event;
DISPLAY_DESCR        *walk_dspl;
long                  temp_event_mask;

temp_event_mask = dspl_descr->properties->main_event_mask | dspl_descr->properties->dm_event_mask; /* covers vt100 mode when main window omits pointer motion */
got_event = ce_XCheckIfEvent(dspl_descr->display, event_return, chk_rtn, (char *)&temp_event_mask);

for (walk_dspl = dspl_descr->next; !got_event && walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
{
   temp_event_mask = walk_dspl->properties->main_event_mask | walk_dspl->properties->dm_event_mask;
   got_event = ce_XCheckIfEvent(walk_dspl->display, event_return, chk_rtn, (char *)&temp_event_mask);
   if (got_event)
      set_global_dspl(walk_dspl);
}

return(got_event);

} /* end of ce_XIfEvent_pad */


#ifdef DebuG
/***************************************************************
*  
*  macro to translate event to its name
*  The unsigned in the macro takes care of invalid negative values.
*  
***************************************************************/

#define EVENT_NAME(event) (((unsigned)event < LASTEvent) ? event_names[event] : "Out of Range value")

char *event_names[] = {
"0",
"1",
"KeyPress",
"KeyRelease",
"ButtonPress",
"ButtonRelease",
"MotionNotify",
"EnterNotify",
"LeaveNotify",
"FocusIn",
"FocusOut",
"KeymapNotify",
"Expose",
"GraphicsExpose",
"NoExpose",
"VisibilityNotify",
"CreateNotify",
"DestroyNotify",
"UnmapNotify",
"MapNotify",
"MapRequest",
"ReparentNotify",
"ConfigureNotify",
"ConfigureRequest",
"GravityNotify",
"ResizeRequest",
"CirculateNotify",
"CirculateRequest",
"PropertyNotify",
"SelectionClear",
"SelectionRequest",
"SelectionNotify",
"ColormapNotify",
"ClientMessage",
"MappingNotify"};
#endif



/************************************************************************

NAME:      find_valid_event - CheckMaskEvent compare routine to find an event given a mask


PURPOSE:    This routine fills a hole in X between XNextEvent and XCheckMaskEvent.
            It does what XCheckMaskEvent does but also returns selectionrequest,
            and other interclient communication events which cannot be selected
            via XCheckMaskEvent.

PARAMETERS:

   1.  display     - pointer to Display (INPUT)
                     This is the pointer to the current display.

   2.  event       - pointer to XEvent (INPUT)
                     This is the pointer to event to be looked at.

   3.  mask_ptr    - pointer to char (INPUT)
                     This is the user argument specified in the call
                     to XIfEvent.  In this case it is the address of a
                     a long int which is the mask.

FUNCTIONS :

   1.   Convert the event type to the appropriate mask.

   2.   Check the mask against the passed mask.

RETURNED VALUE
   found      -  Bool (X type)
                 True is returned if we find the event satisfactory,
                 False otherwise.


*************************************************************************/

Bool find_valid_event(Display        *display,
                      XEvent         *event,
                      char           *mask_ptr)
{

long               passed_mask;
long               event_mask;

passed_mask = *((long *)mask_ptr);

switch(event->type)
{
case KeyPress:          event_mask = KeyPressMask;         break;
case KeyRelease:        event_mask = KeyReleaseMask;       break;
case ButtonPress:       event_mask = ButtonPressMask;      break;
case ButtonRelease:     event_mask = ButtonReleaseMask;    break;
case MotionNotify:      event_mask = PointerMotionMask | PointerMotionHintMask | Button1MotionMask | Button2MotionMask | Button3MotionMask | Button4MotionMask | Button5MotionMask | ButtonMotionMask; break;
case EnterNotify:       event_mask = EnterWindowMask;      break;
case LeaveNotify:       event_mask = LeaveWindowMask;      break;
case FocusIn:           event_mask = FocusChangeMask;      break;
case FocusOut:          event_mask = FocusChangeMask;      break;
case KeymapNotify:      event_mask = KeymapStateMask;      break;
case Expose:            event_mask = ExposureMask;         break;
case GraphicsExpose:    event_mask = -1;                   break;
case NoExpose:          event_mask = -1;                   break;
case VisibilityNotify:  event_mask = VisibilityChangeMask; break;
case CreateNotify:      event_mask = StructureNotifyMask;  break;
case DestroyNotify:     event_mask = StructureNotifyMask;  break;
case UnmapNotify:       event_mask = StructureNotifyMask | SubstructureNotifyMask;  break;
case MapNotify:         event_mask = StructureNotifyMask | SubstructureNotifyMask;  break;
case MapRequest:        event_mask = SubstructureRedirectMask;  break;
case ReparentNotify:    event_mask = StructureNotifyMask | SubstructureNotifyMask;  break;
case ConfigureNotify:   event_mask = StructureNotifyMask | SubstructureNotifyMask;  break;
case ConfigureRequest:  event_mask = SubstructureNotifyMask;  break;
case GravityNotify:     event_mask = StructureNotifyMask | SubstructureNotifyMask;  break;
case ResizeRequest:     event_mask = ResizeRedirectMask;   break;
case CirculateNotify:   event_mask = StructureNotifyMask | SubstructureNotifyMask;  break;
case CirculateRequest:  event_mask = SubstructureNotifyMask;  break;
case PropertyNotify:    event_mask = PropertyChangeMask;   break;
case SelectionClear:    event_mask = -1;                   break;
case SelectionRequest:  event_mask = -1;                   break;
case SelectionNotify:   event_mask = -1;                   break;
case ColormapNotify:    event_mask = ColormapChangeMask;   break;
case ClientMessage:     event_mask = -1;                   break;
case MappingNotify:     event_mask = -1;                   break;
default:                event_mask = -1;                   break;
}

if (event_mask & passed_mask)
   {
      DEBUG0(
      if (event->type != MotionNotify)
         DEBUG15(fprintf(stderr, "-------find_valid_event:  Returning True for event type %s (%d), mask 0x%08X, display 0x%X\n", EVENT_NAME(event->type), event->type, passed_mask, display);)
      else
         DEBUG14(fprintf(stderr, "-------find_valid_event:  Returning True for event type %s (%d), mask 0x%08X, display 0x%X\n", EVENT_NAME(event->type), event->type, passed_mask, display);)
      )
      return(True);
   }
else
   {
      DEBUG15(fprintf(stderr, "       find_valid_event:  Returning False for event type %s (%d), mask 0x%08X, display 0x%X\n", EVENT_NAME(event->type), event->type, passed_mask, display);)
      return(False);
   }

} /* end of find_valid_event */


/************************************************************************

NAME:      find_event - CheckMaskEvent compare routine to find a single event type


PURPOSE:    This routine looks for events of a particular type which is
            passed in the type parm.  It is invoked through the XCheckIfEvent
            call.  Currently this routine is used by the main event
            loop to eat all but the last configurenotify request
            on the queue.  If a group of them comes in, as does on the Sun,
            we only want the last one.  XCheckTyped event cannot be used because
            it selects too many different types of events.

PARAMETERS:

   1.  display     - pointer to Display (INPUT)
                     This is the pointer to the current display.

   2.  event       - pointer to XEvent (INPUT)
                     This is the pointer to event to be looked at.

   3.  type        - pointer to int passed as pointer to char (INPUT)
                     This is the type of the event to be looked for.
                     Pointer to char is required, we are passing
                     a the address of an int.

FUNCTIONS :

   1.   Compare the event type to the passed type.  If they match
        return true.

RETURNED VALUE
   found      -  Bool (X type)
                 True is returned if we find the event satisfactory,
                 False otherwise.


*************************************************************************/
/* ARGSUSED */
Bool find_event(Display        *display,
                XEvent         *event,
                char           *type)  /* really an int */
{
int   search_event_type;

search_event_type = *((int *)type);

if (event->type == search_event_type)
   {
      DEBUG15(fprintf(stderr, "          find_event:  Returning True for event type %s (%d)\n", EVENT_NAME(event->type), event->type);)
      return(True);
   }
else
   return(False);

} /* end of find_event */


/************************************************************************

NAME:      set_event_masks

PURPOSE:    This routine initializes and updates the event mask for the
            main window, the dm_input, and dm_output windows.

PARAMETERS:

   1.  dspl_descr  -  pointer to DISPLAY_DESCR (INPUT)
                 This is the display to act upon.

   2.  force  -  int (INPUT)
                 When true, (non-zero) this flag forces the select input events
                 calls to be executed.  It is True during initialization.  It is
                 needed because the CV and CE commands redo initialization.


GLOBAL INPUT DATA:

   2.  kb_event_mask    -   int (OUTPUT)
               This is the current event mask for keyboard events.
               It is maintained by the dm_kd function in kd.c and
               shows the keyboard (and mouse) type events we are
               interested in receiving.  It is intialized to key press
               and button press events.  If key defintions are 
               added for key up or mouse button up events, it is
               updated to include these.

   3.  last_kb_event_mask  -   int (INPUT / OUTPUT)
               This is the event mask for keyboard events which is
               currently installed.  It is input for comparison purposes
               and output when the new event mask is installed.
               It is maintained by this module.
               It is static to to this module.

FUNCTIONS :

   1.   If there is data from the file to read in, read it
 
   2.   If there is a find in progress, continue it for a while.
        continue_file() will do some number of lines and return.

   3.   If there is a substitute in progress, continue it as with the
        find.  Note that there is never a find and substitute in
        progress at the same time.


*************************************************************************/

void  set_event_masks(DISPLAY_DESCR    *dspl_descr,
                      int               force)
{
unsigned long         base_event_mask; /* RES 8/18/94 Make local */
unsigned long         base_dm_event_mask;

if (!dspl_descr_head || force)   /* force is used to reset after a ce or cp command */
   dspl_descr_head = dspl_descr; /* initialization for the first event */

/***************************************************************
*
*  Determine what sort of events we want to process
*  in the windows.
*
*  The keyboard event mask can change dynically, so we keep track
*  of its last value. kb_event_mask and last_kb_event_mask are
*  in kd.h
*
*  The property changes in the root window are interesting to us
*  because the server key definitions are held in a property hung
*  off the root window.
*
***************************************************************/

if (force || (dspl_descr->properties->last_kb_event_mask != dspl_descr->properties->kb_event_mask))
   {
      dspl_descr->properties->last_kb_event_mask = dspl_descr->properties->kb_event_mask;

      base_event_mask    = ExposureMask
                         | StructureNotifyMask
                         | PropertyChangeMask
                         | EnterWindowMask
                         | LeaveWindowMask
                         | VisibilityChangeMask
                         | FocusChangeMask
                         | PointerMotionMask 
                         | Button1MotionMask
                         | Button2MotionMask
                         | Button3MotionMask
                         | Button4MotionMask
                         | Button5MotionMask;

      base_dm_event_mask = ExposureMask
                         | EnterWindowMask
                         | LeaveWindowMask
                         | PointerMotionMask 
                         | Button1MotionMask
                         | Button2MotionMask
                         | Button3MotionMask
                         | Button4MotionMask
                         | Button5MotionMask;

      if (!MOUSEON)
         {
            base_event_mask    &= ~PointerMotionMask;  /* res 10/18/93, deal with mouse off explicet focus */
            base_dm_event_mask &= ~PointerMotionMask;
         }
      else
         {

            if (dspl_descr->vt100_mode)
               base_event_mask    &= ~PointerMotionMask;
            else
               base_event_mask    |= PointerMotionMask;
            base_dm_event_mask |= PointerMotionMask;
         }

      dspl_descr->properties->main_event_mask = (base_event_mask    | dspl_descr->properties->kb_event_mask);
      dspl_descr->properties->dm_event_mask   = (base_dm_event_mask | dspl_descr->properties->kb_event_mask);

      DEBUG9(XERRORPOS)
      XSelectInput(dspl_descr->display,
                   dspl_descr->main_pad->x_window,
                   dspl_descr->properties->main_event_mask);

      DEBUG9(XERRORPOS)
      XSelectInput(dspl_descr->display,
                   dspl_descr->dminput_pad->x_window,
                   dspl_descr->properties->dm_event_mask);

      DEBUG9(XERRORPOS)
      XSelectInput(dspl_descr->display,
                   dspl_descr->dmoutput_pad->x_window,
                   dspl_descr->properties->dm_event_mask);

      DEBUG9(XERRORPOS)
      XSelectInput(dspl_descr->display,
                   RootWindow(dspl_descr->display,DefaultScreen(dspl_descr->display)),
                   PropertyChangeMask);

#ifdef PAD
      if (dspl_descr->pad_mode)
         {
            DEBUG9(XERRORPOS)
            XSelectInput(dspl_descr->display,
                         dspl_descr->unix_pad->x_window,
                         dspl_descr->properties->dm_event_mask);
         }
#endif

   }
DEBUG1(
   fprintf(stderr, "set_event_masks:  dspl %d, kb mask = 0x%08X,  main mask = 0x%08X,  base mask = 0x%08X, dm mask = 0x%08X\n",
                   dspl_descr->display_no, dspl_descr->properties->kb_event_mask,
                   dspl_descr->properties->main_event_mask, base_event_mask, dspl_descr->properties->dm_event_mask);
)

} /* end of set_event_masks */


/************************************************************************

NAME:      set_global_dspl  - Set the current global display to a particular display.

PURPOSE:    This routine localizes changes to the current global display.
            Note that all reassignments to this variable go through this routine
            except 2 in get_next_X_event.  Both of those have a comment with the
            name of this routine on the line with the assignment.  Thus we can find
            any place this variable gets changed.

PARAMETERS:

   1.  passed_dspl_descr  -  pointer to DISPLAY_DESCR (INPUT)
                             This is the new display value.

GLOBAL DATA:

   1.  dspl_descr  - pointer to DISPLAY_DESCR (OUTPUT)
                    This is the global access display.

FUNCTIONS :

   1.   Assign the global value from the passed value.
 

*************************************************************************/

void  set_global_dspl(DISPLAY_DESCR    *passed_dspl_descr)
{

if (passed_dspl_descr)
   dspl_descr = passed_dspl_descr; /* use the global one if one is not passed */

dspl_descr_head = dspl_descr; /* force this to be the next one read from */

} /* end of set_global_dspl */


/************************************************************************

NAME:      do_background_task

PURPOSE:    This routine performs background task between incoming events.
            Currently only reading blocks of data is done in background.

PARAMETERS:

   1.  passed_dspl_descr  -  pointer to DISPLAY_DESCR (INPUT)
                             This is the current display value.

GLOBAL INPUT DATA:

   1.  main_window_eof -   int (INPUT / OUTPUT)
               This flag is set when all the data is read in.

   2.  FIND_IN_PROGRESS -  int (INPUT / OUTPUT)
               This flag shows that a find is in progress.

   3.  SUBSTITUTE_IN_PROGRESS -  int (INPUT / OUTPUT)
               This flag shows that a substitute is in progress.


FUNCTIONS :

   1.   If there is data from the file to read in, read it
 
   2.   If there is a find in progress, continue it for a while.
        continue_file() will do some number of lines and return.

   3.   If there is a substitute in progress, continue it as with the
        find.  Note that there is never a find and substitute in
        progress at the same time.


*************************************************************************/

static void  do_background_task(DISPLAY_DESCR *dspl_descr)
{

int                   redraw_needed = 0;
int                   warp_needed;
int                   found_line;
int                   found_col;
PAD_DESCR            *found_pad;
DISPLAY_DESCR        *walk_dspl;
#ifdef PAD
int                   lines_displayed;
#endif
#ifdef DebuG
static int  block_count = 0;
#endif

DEBUG2(fprintf(stderr, "@do_background_task: eof = %d, lines = %d find = %d, substitute = %d keydefs = %d scroll = %d\n",
                        main_window_eof, total_lines(dspl_descr->main_pad->token),
                        dspl_descr->FIND_IN_PROGRESS, dspl_descr->SUBSTITUTE_IN_PROGRESS,
                        keydefs_in_progress, dspl_descr->background_scroll_lines);
)


if (keydefs_in_progress)
   keydefs_in_progress = process_some_keydefs(dspl_descr); /* in kd.c */
else
   if ((walk_dspl = any_find_or_sub(dspl_descr)) != NULL)
      {
         set_global_dspl(walk_dspl);  /* switch displays globally to the one we need to process */
         if (walk_dspl->SUBSTITUTE_IN_PROGRESS)  /* is never both */
            dm_continue_sub(walk_dspl->find_data, &found_line, &found_col, &found_pad, walk_dspl->escape_char);
         else
            dm_continue_find(walk_dspl->find_data, &found_line, &found_col, &found_pad, walk_dspl->escape_char);

         if (found_line != DM_FIND_IN_PROGRESS)
            {
               walk_dspl->FIND_IN_PROGRESS = 0;
               walk_dspl->SUBSTITUTE_IN_PROGRESS = 0;
               if (found_line == DM_FIND_NOT_FOUND)
                  {
                     redraw_needed |= dm_position(walk_dspl->cursor_buff, walk_dspl->main_pad, walk_dspl->cursor_buff->current_win_buff->file_line_no, walk_dspl->cursor_buff->current_win_buff->file_col_no);
                     warp_needed = 1;
                  }
               else
                  {
                     if (found_line != DM_FIND_ERROR)
                        {
                           /***************************************************************
                           *  We found it.
                           ***************************************************************/
                           flush(walk_dspl->main_pad);
                           walk_dspl->main_pad->file_line_no = found_line;
                           walk_dspl->main_pad->file_col_no  = found_col;
                           walk_dspl->main_pad->buff_ptr = get_line_by_num(walk_dspl->main_pad->token, walk_dspl->main_pad->file_line_no);
                           redraw_needed |= dm_position(walk_dspl->cursor_buff, walk_dspl->main_pad, found_line, found_col);
                           if ((walk_dspl->find_border > 0) && find_border_adjust(walk_dspl->main_pad, walk_dspl->find_border))
                              redraw_needed |= dm_position(walk_dspl->cursor_buff, walk_dspl->main_pad, found_line, found_col);
                        }
                     redraw_needed |= (MAIN_PAD_MASK & FULL_REDRAW);
                     warp_needed = 1;
                  }
               /***************************************************************
               *  If we need to redraw, do it.
               *  We redraw the pixmap and then send an expose event to copy
               *  it to the window.
               ***************************************************************/
               if (redraw_needed || warp_needed)
                  process_redraw(walk_dspl, redraw_needed, warp_needed);
            }   /* end of find is not still in progress */
      } /* end of find_in_progress  or substitute_in_progress */
   else
      if (!main_window_eof)
         {
            load_enough_data(total_lines(dspl_descr->main_pad->token)+1);
            DEBUG32(
            block_count++;
            if (block_count >= 50)
               {
                  fprintf(stderr, "lines read %7d  (%d)\n", total_lines(dspl_descr->main_pad->token), time(0));
                  block_count = 0;
               }
            )

         }
#ifdef PAD
      else
         if ((walk_dspl = any_scroll(dspl_descr)) != NULL)
            {
               set_global_dspl(walk_dspl);  /* switch displays globally to the one we need to process */
               lines_displayed = total_lines(walk_dspl->main_pad->token) - walk_dspl->main_pad->first_line;
               if (lines_displayed > walk_dspl->main_pad->window->lines_on_screen)
                  lines_displayed = walk_dspl->main_pad->window->lines_on_screen;

               DEBUG19(fprintf(stderr, "Background scroll, %d lines\n", walk_dspl->background_scroll_lines);)
               scroll_some(&walk_dspl->background_scroll_lines,
                           walk_dspl,
                           lines_displayed);
            }
#endif

check_background_work();

} /* end of do_background_task  */


/************************************************************************

NAME:      any_find_or_sub - Scan display list for finds or subs in progress

PURPOSE:    This routine checks to see if there are any finds or subsitutes
            in progress and returns the first one it finds.

PARAMETERS:

   1.  dspl_descr      -  pointer to DISPLAY_DESCR (INPUT)
                          This is the display description to start the
                          search with.


FUNCTIONS :

   1.   Check the passed dspl_descr to see if it has a find or sub in progress.
 
   2.   If not, check any extra display descriptions.

RETURNED VALUE:

   dspl_descr   -   pointer to DISPLAY_DESCR
                    If found, this is the display description with a find or
                    substitute in progress.
                    NULL  -  no finds in progress
                    !NULL -  display description 

*************************************************************************/

static DISPLAY_DESCR *any_find_or_sub(DISPLAY_DESCR *dspl_descr)
{
DISPLAY_DESCR        *walk_dspl = dspl_descr;
int                   in_progress;

in_progress = dspl_descr->FIND_IN_PROGRESS | dspl_descr->SUBSTITUTE_IN_PROGRESS;
if (!in_progress)
   for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
   {
      in_progress = walk_dspl->FIND_IN_PROGRESS | walk_dspl->SUBSTITUTE_IN_PROGRESS;
      if (in_progress)
         break;
   }

if (in_progress)
   return(walk_dspl);
else
   return(NULL);

} /* end of any_find_or_sub */


/************************************************************************

NAME:      any_scroll - Scan display list for background scrolls

PURPOSE:    This routine checks to see if there are any background scrolls
            in progress and returns the first one found;

PARAMETERS:

   1.  dspl_descr      -  pointer to DISPLAY_DESCR (INPUT)
                          This is the display description to start the
                          search with.


FUNCTIONS :

   1.   Check the passed dspl_descr to see if it has a background scroll in progress.
 
   2.   If not, check any extra display descriptions.

RETURNED VALUE:

   dspl_descr   -   pointer to DISPLAY_DESCR
                    If found, this is the display description with a find or
                    substitute in progress.
                    NULL  -  no scrolls in progress
                    !NULL -  display description 

*************************************************************************/

static DISPLAY_DESCR *any_scroll(DISPLAY_DESCR *dspl_descr)
{
DISPLAY_DESCR        *walk_dspl;
DISPLAY_DESCR        *found_dspl = NULL;

if (dspl_descr->background_scroll_lines)
   found_dspl = dspl_descr;

if (!found_dspl)
   for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
   {
      if (walk_dspl->background_scroll_lines)
         {
            found_dspl = walk_dspl;
            break;
         }
   }


DEBUG19(if (found_dspl) fprintf(stderr, "any_scroll:  Background scrolling in display %d\n", found_dspl->display_no);)

return(found_dspl);

} /* end of any_scroll */


/************************************************************************

NAME:      find_padmode_dspl - Find the pad mode display description

PURPOSE:    This routine scans the list of display discriptions and returns
            the pad mode description if there is one.

PARAMETERS:

   passed_dspl_descr  - pointer to DISPLAY_DESCR (INPUT)
                        This is one of the display descriptions.  Usually
                        the current one.
FUNCTIONS :

   1.   If passed display description has the pad mode flag set,
        return it.

   2.   Scan the list for a display description with the pad mode flag
        set.  There will only be one.

RETURNED VALUE:
   padmode_dspl   -  pointer to DISPLAY_DESCR
                     NULL  -  No pad mode display exists, we are not in pad mode.
                     !NULL -  The address of the pad mode display.

*************************************************************************/

DISPLAY_DESCR *find_padmode_dspl(DISPLAY_DESCR   *passed_dspl_descr)
{
DISPLAY_DESCR        *walk_dspl;

if (passed_dspl_descr->pad_mode)
   return(passed_dspl_descr);

for (walk_dspl = dspl_descr->next; walk_dspl != passed_dspl_descr; walk_dspl = walk_dspl->next)
{
   if (walk_dspl->pad_mode)
      return(walk_dspl);
}

return(NULL);
} /* end of find_padmode_dspl */

/************************************************************************

NAME:      set_edit_file_changed - Find the pad mode display description

PURPOSE:    This routine sets the edit_file_changed flag in all display
            descriptions in the list.

PARAMETERS:
   passed_dspl_descr  - pointer to DISPLAY_DESCR (INPUT)
                        This is one of the display descriptions.  Usually
                        the current one.

   true_or_false      - int
                        This is the value to be inserted.  Use either
                        True or False.

FUNCTIONS :
   1.   Set the current display value.

   2.   Walk the circular list and set any additional display descriptions.


*************************************************************************/

void set_edit_file_changed(DISPLAY_DESCR   *passed_dspl_descr, int true_or_false)
{
DISPLAY_DESCR        *walk_dspl;

passed_dspl_descr->edit_file_changed = true_or_false;

for (walk_dspl = dspl_descr->next; walk_dspl != passed_dspl_descr; walk_dspl = walk_dspl->next)
{
   walk_dspl->edit_file_changed = true_or_false;
}

} /* end of set_edit_file_changed */


/************************************************************************

NAME:      load_enough_data

PURPOSE:    This routine loads data from the main task until enough for
            the current needs are met or eof is reached.

PARAMETERS:

   1.  total_lines_needed - int (INPUT)
               This is the total number of lines needed.


GLOBAL DATA:

   1.  main_window_eof -   int (INPUT / OUTPUT)
               This flag is set when all the data is read in.


FUNCTIONS :

   1.   Read until enough data is read from the file or eof is reached.
 
RETURNED VALUE:
   eof    -   int
              eof on the main file is returned;
              True  - all data has been read in
              False - data is still being read in

*************************************************************************/

int load_enough_data(int          total_lines_needed)  /* input  */
{
XEvent      event_union;
int                             start_line;
int                   tlines;
char                  msg[80];
#define               INIT_MESSAGE_TRIGGER  10000
#define               MESSAGE_TRIGGER       50000
static int            message_output_limit = INIT_MESSAGE_TRIGGER;
DISPLAY_DESCR        *walk_dspl;

DEBUG2(fprintf(stderr, "load_enough_data to line %d, current total lines is %d, eof is %s\n", total_lines_needed, total_lines(dspl_descr->main_pad->token), (main_window_eof ? "True" : "False"));)

if (total_lines(dspl_descr->main_pad->token) == 0)
   message_output_limit = INIT_MESSAGE_TRIGGER; /* reset in case of a fork */

while(!main_window_eof && total_lines(dspl_descr->main_pad->token) < total_lines_needed)
{
   /* RES 8/29/95, don't eat events if we are waiting on one */
   if (!expecting_warp(dspl_descr))
      while (XCheckTypedEvent(dspl_descr->display,
                              Expose,
                              &event_union))
      DEBUG14(fprintf(stderr, "load_enough_data: IGNORING:  ");dump_Xevent(stderr, &event_union, debug);)

   start_line = total_lines(dspl_descr->main_pad->token) % message_output_limit;
   load_a_block(dspl_descr->main_pad->token, /* opaque */
                instream,                    /* input  */
                MANFORMAT,                   /* input  */
                &main_window_eof);           /* input / output */

   DEBUG32(fprintf(stderr, "test %d <? %d   total lines = %d\n", (total_lines(dspl_descr->main_pad->token) % message_output_limit), start_line, total_lines(dspl_descr->main_pad->token));)
   if ((total_lines(dspl_descr->main_pad->token) % message_output_limit) < start_line)
      {
         tlines = (total_lines(dspl_descr->main_pad->token) / message_output_limit) * message_output_limit; 
         snprintf(msg, sizeof(msg), "Loaded %s lines.", add_commas(tlines));
         /* Following test taked from dmwin.c, sees if the window is up */
         if ((dspl_descr->dmoutput_pad->x_window != None) && (!dspl_descr->first_expose))
            dm_error(msg, DM_ERROR_MSG);
         walk_dspl = dspl_descr;
         do
         {
            dm_error_dspl(msg, DM_ERROR_MSG, walk_dspl);
            walk_dspl = walk_dspl->next;
         } while(walk_dspl != dspl_descr);

         message_output_limit = MESSAGE_TRIGGER;
      }
   if (main_window_eof && (instream != NULL)) /* RES 9/6/95 added close on eof */
      {
         if (LSF)
            lsf_close(instream, dspl_descr->ind_data);
         else
            fclose(instream);
         instream = NULL;
         /* close releases file lock, relock it RES 9/27/95 */
         if (LOCKF && WRITABLE(dspl_descr->main_pad->token) &&
          (!LSF || !is_virtual(&(dspl_descr->ind_data), dspl_descr->hsearch_data, edit_file)))
            {
               unlock_file();
               if (lock_file(edit_file) == False)
                  WRITABLE(dspl_descr->main_pad->token) = False;
            }
         instream = NULL;
      }
   if (main_window_eof && (message_output_limit == MESSAGE_TRIGGER))
      {
         message_output_limit = INIT_MESSAGE_TRIGGER; /* reset in case of a fork */
         strcpy(msg, "File loaded.");
         dm_error(msg, DM_ERROR_MSG);
         walk_dspl = dspl_descr;
         do
         {
            dm_error_dspl(msg, DM_ERROR_MSG, walk_dspl);
            walk_dspl = walk_dspl->next;
         } while(walk_dspl != dspl_descr);
      }
   if (dspl_descr->sb_data->vert_window_on)
      draw_vert_sb(dspl_descr->sb_data, dspl_descr->main_pad, dspl_descr->display);
}

check_background_work();

return(main_window_eof);

} /* end of load_enough_data */


/************************************************************************

NAME:      add_commas  - Make an integer a printable character string with commas inserted.

PURPOSE:    This routine formats an integer for printing.

PARAMETERS:
   1.  value  - int (INPUT)
                This is the value to convert.

FUNCTIONS :

   1.   For value zero, return the string "0";

   2.   If the value is less than zero, remember this so
        we can prefix the minus sign.

   3.   build a string from the back end forward three digits
        at a time.

RETURNED VALUE:
   ret   -   static character string
             The returned value is the printable number.  The number is
             held in a static character string which is reused for each call.
 

*************************************************************************/

static char *add_commas(int value)
{
static char     ret[32];
char            scrap[6];
int             rem;
int             minus = 0;
char            *p = &ret[sizeof(ret)-4];

if (value == 0)
   return("0");

p[3] = '\0';  /* end of the string */

/***************************************************************
*  If the value is negative, remember this and make it positive.
***************************************************************/
if (value < 0)
   {
      value = -value;
      minus = 1;
   }

/***************************************************************
*  Chop off three decimal digits at a time and build the string
*  backwards.
***************************************************************/
while(value)
{
   rem = value % 1000;
   value = value / 1000;
   if (value)
      {
         snprintf(scrap, sizeof(scrap), "%03d", rem);
         p[0] = scrap[0];
         p[1] = scrap[1];
         p[2] = scrap[2];
         p -= 4;
         p[3] = ',';
      }
   else
      {
         snprintf(scrap, sizeof(scrap), "%3d", rem);
         p[0] = scrap[0];
         p[1] = scrap[1];
         p[2] = scrap[2];
      }
}

while(*p == ' ') p++;
if (minus)
   *(--p) = '-';

return(p);


}  

/************************************************************************

NAME:      change_background_work  - Turn on or off background tasks.

PURPOSE:    This routine sets the find in progress and subsititute in progress
            flags as directed by start find.

PARAMETERS:

   1.  type  - int (INPUT)
               This is one of the #defined values for background work.
               Values: BACKGROUND_FIND
                       BACKGROUND_SUB
                       BACKGROUND_KEYDEFS
                       MAIN_WINDOW_EOF
                       BACKGROUND_SCROLL

   2.  value  - int (INPUT)
               This is the value to insert into the background value.


GLOBAL DATA:

   1.  FIND_IN_PROGRESS -  static global int (OUTPUT)
                           This flag is set to show a background find is in progress.

   2.  SUBSTITUTE_IN_PROGRESS -  static global int (OUTPUT)
                           This flag is set to show a background substitute is in progress.

   3.  keydefs_in_progress -  static global int (OUTPUT)
                           This flag is set to show a  that the keydefs are being read in.
                           This call is done from init keydefs.

   4.  main_window_eof  -  static global int (OUTPUT)
                           This flag is set to eof on the main window file descriptor.
                           It gets turned on when the cv or ce command starts a new session.
                           It gets forced off in pad mode.

   5.  background_scroll_lines  -  static global int (OUTPUT)
                           Used in pad mode, this is the number of lines to be scrolled on
                           on the screen in background mode.


FUNCTIONS :

   1.   Set the required variable.

   2.   Set the something_to_do_in_background variable which is static global
        to this module.

NOTES:
   FIND_IN_PROGRESS and SUBSTITUTE_IN_PROGRESS are never set at the same time.
 

*************************************************************************/

void change_background_work(DISPLAY_DESCR    *passed_dspl_descr,
                            int               type,
                            int               value)
{
if (!passed_dspl_descr)
   passed_dspl_descr = dspl_descr; /* use the global one if one is not passed */

switch(type)
{
case BACKGROUND_FIND:
   DEBUG2(fprintf(stderr, "FIND_IN_PROGRESS set to %d\n", value);)
   passed_dspl_descr->FIND_IN_PROGRESS = value;
   break;

case BACKGROUND_SUB:
   DEBUG2(fprintf(stderr, "SUBSTITUTE_IN_PROGRESS set to %d\n", value);)
   passed_dspl_descr->SUBSTITUTE_IN_PROGRESS = value;
   break;

case BACKGROUND_KEYDEFS:
   DEBUG4(fprintf(stderr, "keydefs_in_progress set to %d\n", value);)
   keydefs_in_progress = value;
   break;

case MAIN_WINDOW_EOF:
   DEBUG3(fprintf(stderr, "main_window_eof set to %d\n", value);)
   main_window_eof = value;
   break;

case BACKGROUND_SCROLL:
   DEBUG0(if (!passed_dspl_descr->pad_mode) fprintf(stderr, "Setting background scroll in non-pad mode display %d\n", passed_dspl_descr->display_no);)
   DEBUG19(fprintf(stderr, "Background scroll lines set to %d in display %d\n", value, passed_dspl_descr->display_no);)
   passed_dspl_descr->background_scroll_lines = value;
   break;

case BACKGROUND_READ_AHEAD:
   DEBUG1(fprintf(stderr, "Read ahead set to %d\n", value);)
   read_ahead = value;
   break;

default:
   DEBUG(fprintf(stderr, "change_background_work: Bad value \"&d\" passed\n", type);)
   break;

} /* end of switch on type */

check_background_work();

} /* end of change_background_work */


/************************************************************************

NAME:      check_background_work  - calculate something_to_do_in_background

PURPOSE:    This routine checks all the relevant variables to determine if
            there is something to do in background.

PARAMETERS:

   none

GLOBAL DATA:

   1.  FIND_IN_PROGRESS -  static global int (OUTPUT)
                           This flag is set to show a background find is in progress.

   2.  SUBSTITUTE_IN_PROGRESS -  static global int (OUTPUT)
                           This flag is set to show a background substitute is in progress.

   3.  keydefs_in_progress -  static global int (OUTPUT)
                           This flag is set to show a  that the keydefs are being read in.
                           This call is done from init keydefs.

   4.  main_window_eof  -  static global int (OUTPUT)
                           This flag is set to eof on the main window file descriptor.
                           It gets turned on when the cv or ce command starts a new session.
                           It gets forced off in pad mode.

   5.  background_scroll_lines  -  static global int (OUTPUT)
                           Used in pad mode, this is the number of lines to be scrolled on
                           on the screen in background mode.

   6.  something_to_do_in_background  -  static global int (OUTPUT)
                           This is a summary of whether there is anything to do in background.
                           It is true if any of the other flags are true.

FUNCTIONS :

   1.   Check the static variables and the variables in the current display.

   2.   Check any additional displays which may be present.


*************************************************************************/

static void check_background_work(void)
{

DISPLAY_DESCR        *walk_dspl;

/***************************************************************
*  
*  Set something_to_do_in_background from the static variables
*  and the current display.  If there are additional displays
*  check them too.
*  
***************************************************************/

something_to_do_in_background = (!main_window_eof && read_ahead) ||
                                dspl_descr->FIND_IN_PROGRESS || dspl_descr->SUBSTITUTE_IN_PROGRESS ||
                                keydefs_in_progress || 
                                (dspl_descr->background_scroll_lines && !dspl_descr->hold_mode);
if (!something_to_do_in_background)
   for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
      something_to_do_in_background |= ((walk_dspl->FIND_IN_PROGRESS) || (walk_dspl->SUBSTITUTE_IN_PROGRESS) ||
                                       (walk_dspl->background_scroll_lines && !(walk_dspl->hold_mode)));


} /* end of check_background_work */


/************************************************************************

NAME:      get_background_work  - Retrieve the current state of background task variables

PURPOSE:    This routine returns the current value of a background flag

PARAMETERS:

   1.  type  - int (INPUT)
               This is one of the #defined values for background work.
               Values: BACKGROUND_FIND
                       BACKGROUND_SUB
                       BACKGROUND_KEYDEFS
                       MAIN_WINDOW_EOF
                       BACKGROUND_SCROLL


GLOBAL DATA:
   see change_background_work


FUNCTIONS :

   1.   Return the value of the required variable.


*************************************************************************/

int  get_background_work(int      type)
{
int  value;

switch(type)
{
case BACKGROUND_FIND:
   value = dspl_descr->FIND_IN_PROGRESS;
   break;

case BACKGROUND_SUB:
   value = dspl_descr->SUBSTITUTE_IN_PROGRESS;
   break;

case BACKGROUND_KEYDEFS:
   value = keydefs_in_progress;
   break;

case MAIN_WINDOW_EOF:
   value = main_window_eof;
   break;

case BACKGROUND_SCROLL:
   value = dspl_descr->background_scroll_lines;
   break;

case BACKGROUND_READ_AHEAD:
   value = read_ahead;
   break;

case BACKGROUND_FIND_OR_SUB:
   value = dspl_descr->FIND_IN_PROGRESS || dspl_descr->SUBSTITUTE_IN_PROGRESS;
   break;

default:
   DEBUG(fprintf(stderr, "get_background_work: Bad value \"&d\" passed\n", type);)
   break;

} /* end of switch on type */

return(value);

} /* end of get_background_work */


/************************************************************************

NAME:      finish_keydefs - make sure all the key definitions have been read in.

PURPOSE:    This routine is called once when the first keypress or mouse press
            event is encountered.  It makes sure all the key defintions have
            been read in.  Otherwise it is hard to process the keydef.

GLOBAL DATA:

   1.  keydefs_in_progress  - int static global within this module

   2.  last_kb_event_mask   - unsigned int defined in kd.h
                              As key definitions are processed, the      
                              kb_event_mask is updated.  Initially, only 
                              key press and mouse button press events are
                              interesting.  if any key definitions for   
                              mouse up or key up events are encountered, 

   3.  kb_event_mask        - unsigned int defined in kd.h
                              The main event loop, after processing      
                              a kd command and the finish_keydefs routine     
                              in getevent.c look at this variable.  If it     
                              is different than kb_event_mask, the input      
                              selection mask is changed to match kb_event_mask
                              and this variable is set equal to kb_event_mask.

FUNCTIONS :

   1.   Repeatedly call process_some_keydefs till they are all done.

   2.   If the keyboard mask has changed as a result of the key definitions,
        modify the input selection mask.

*************************************************************************/

void finish_keydefs(void)
{

while(keydefs_in_progress)
   keydefs_in_progress = process_some_keydefs(dspl_descr); /* in kd.c */

if (dspl_descr->properties->last_kb_event_mask != dspl_descr->properties->kb_event_mask)
   set_event_masks(dspl_descr, False);

} /* end of finish_keydefs */


#ifdef PAD
/************************************************************************

NAME:      autovt_switch - Perform switching for AUTOVT mode

PURPOSE:    This routine flips into and out of vt100 mode to
            support the autovt parameter.

PARAMETERS:

   none

GLOBAL DATA:

   1.  AUTOVT         -  The autovt paramaeter is read

   2.  dspl_descr     - The current display description is read

   3.  TTY_ECHO_MODE  - This macro interfaces with pad.c to
                        get the echo mode.


FUNCTIONS :

   1.   If AUTOVT is enabled, check if the echo mode on the shell socket
        and the current vt100 mode are in sync.  If not call vt100 to
        correct the situation.

RETURNED VALUE:

   changed -   mode changed
               True   -  vt100 was turned on or off
               False  -  vt100 was not changed turned on or off
 

*************************************************************************/

int autovt_switch(void)
{
static DMC_vt100  vt100_dmc = {NULL, DM_vt, False, DM_TOGGLE};


/***************************************************************
*  If auto vt100 mode is turned on, check the vt100 flag and and
*  the echo flag.  If echo is off, vt100 should be on and vice
*  versa.  If this is not the case correct it.  dm_vt100 reconfigures
*  the window which causes a full redraw.  Thus we return.
*  We do not look at the case where this is a warp only call to
*  avoid recursion since dm_vt100 call process_redraw to move 
*  the cursor.
***************************************************************/

if (AUTOVT)
   if (TTY_ECHO_MODE && dspl_descr->vt100_mode)
      {
         /*vt100_dmc.mode = DM_OFF; RES 5/10/94 make vt -auto work */
         dm_vt100((DMC *)&vt100_dmc, dspl_descr);
         return(True);
      }
   else
      if (!(TTY_ECHO_MODE) && !(dspl_descr->vt100_mode))
         {
            /*vt100_dmc.mode = DM_ON; RES 5/10/94 make vt -auto work */
            dm_vt100((DMC *)&vt100_dmc, dspl_descr);
            return(True);
         }

return(False);

} /* end of autovt_switch */
#endif




/************************************************************************

NAME:      window_to_buff_descr - Find the pad description given the X window id.

PURPOSE:    This routine finds the pad description for a passed X window id.
            It is used when an X event gives a window id and we need to
            know what pad goes with it.

PARAMETERS:

   1.  event_window   - Window (INPUT)
                        This is the X window id whose PAD_DESCR is to be found.

GLOBAL DATA:

   1.  the cursor and window buffers.


FUNCTIONS :

   1.   Check the window id in each pad to find a match.

   2.   In the case of an error, assume the main window.


RETURNED VALUE:
   pad  - pointer to PAD_DESCR
          The address of the pad description for the passed
          window is returned.

*************************************************************************/

PAD_DESCR *window_to_buff_descr(DISPLAY_DESCR    *dspl_descr, 
                                Window            event_window)
{
   if (event_window == dspl_descr->main_pad->x_window)
      return(dspl_descr->main_pad);
   else
      if (event_window == dspl_descr->dminput_pad->x_window)
         return(dspl_descr->dminput_pad);
      else
         if (event_window == dspl_descr->dmoutput_pad->x_window)
            return(dspl_descr->dmoutput_pad);
         else
#ifdef PAD
         if (event_window == dspl_descr->unix_pad->x_window)
            return(dspl_descr->unix_pad);
         else
#endif
            {
               DEBUG(
                  if ((event_window != dspl_descr->sb_data->vert_window) && (event_window != dspl_descr->sb_data->horiz_window))
                     fprintf(stderr, "window_to_buff_descr:  Can't identify window 0x%X\n", event_window);
               )
               return(dspl_descr->main_pad);
            }

} /* end of window_to_buff_descr */



/************************************************************************

NAME:      adjust_motion_event - fixup motion events when the mouse button is held down

PURPOSE:    This routine performs surgery on X motion events when a mouse button
            is held down.  If a mouse button is held down and the cursor 
            moves between windows, the motion events are recorded relative
            to the window the cursor was in when the button was pressed.
            This routine will translate the motion event to be relative to
            the window it is in.  
             

PARAMETERS:

   1.  event          - pointer to XEvent (INPUT / OUTPUT)
                        This is the motion event to be examined and
                        potentially modified.


GLOBAL DATA:

   1.  The window descriptions, window, dminput_window, and dmoutput_windot.


FUNCTIONS :

   1.   Determine which window the X server is reporting the motion event for.

   2.   Determine whether the coordinates in the event are consistent for the
        window X is reporting on.

   3.   If the cursor is really in some other window, translate the event
        so it is in the same location as if the button was not held down.

NOTES:
   1.   The check for this being a motion event is done before this routine
        is called.

 

*************************************************************************/

static void  adjust_motion_event(XEvent *event)
{

if (event->xmotion.window == dspl_descr->main_pad->x_window)
   {
      /***************************************************************
      *  If we are really in the dminput or dm output windows, 
      *  translate the event to be in these windows.
      ***************************************************************/
      if (event->xmotion.subwindow == dspl_descr->dminput_pad->x_window)
         {
            DEBUG14(fprintf(stderr, "translating (%d,%d) in main to (%d,%d) in cmd input\n",
                                     event->xmotion.x, event->xmotion.y, event->xmotion.x, event->xmotion.y-dspl_descr->dminput_pad->window->y);
            )
            event->xmotion.window = event->xmotion.subwindow;
            event->xmotion.subwindow = 0;
            event->xmotion.y -= dspl_descr->dminput_pad->window->y;
         }
      else
         if (event->xmotion.subwindow == dspl_descr->dmoutput_pad->x_window)
            {
               DEBUG14(fprintf(stderr, "translating (%d,%d) in main to (%d,%d) in cmd output\n",
                                        event->xmotion.x, event->xmotion.y, event->xmotion.x-dspl_descr->dmoutput_pad->window->x, event->xmotion.y-dspl_descr->dmoutput_pad->window->y);
               )
               event->xmotion.window = event->xmotion.subwindow;
               event->xmotion.subwindow = 0;
               event->xmotion.y -= dspl_descr->dmoutput_pad->window->y;
               event->xmotion.x -= dspl_descr->dmoutput_pad->window->x;
            }
#ifdef PAD
         else
            if (dspl_descr->pad_mode && (event->xmotion.subwindow == dspl_descr->unix_pad->x_window))
               {
                  DEBUG14(fprintf(stderr, "translating (%d,%d) in main to (%d,%d) in unix cmd\n",
                                           event->xmotion.x, event->xmotion.y, event->xmotion.x, event->xmotion.y-dspl_descr->unix_pad->window->y);
                  )
                  event->xmotion.window = event->xmotion.subwindow;
                  event->xmotion.subwindow = 0;
                  event->xmotion.y -= dspl_descr->unix_pad->window->y;
               }
#endif
   }
else
   if (event->xmotion.window == dspl_descr->dminput_pad->x_window)
      {
         /***************************************************************
         *  If y < 0, we are really in the main window or the unix window.
         ***************************************************************/
         if (event->xmotion.y < 0)
            {
               DEBUG14(fprintf(stderr, "translating (%d,%d) in cmd input to (%d,%d) in main\n",
                                        event->xmotion.x, event->xmotion.y, event->xmotion.x, event->xmotion.y+dspl_descr->dminput_pad->window->y);
               )
               event->xmotion.window = dspl_descr->main_pad->x_window;
               event->xmotion.y += dspl_descr->dminput_pad->window->y;
#ifdef PAD
            if (dspl_descr->pad_mode && (event->xmotion.y >= dspl_descr->unix_pad->window->y))
               {
                  DEBUG14(fprintf(stderr, "translating (%d,%d) in main to (%d,%d) in unix cmd\n",
                                           event->xmotion.x, event->xmotion.y, event->xmotion.x - dspl_descr->unix_pad->window->x, event->xmotion.y - dspl_descr->unix_pad->window->y);
                  )
                  event->xmotion.window = dspl_descr->unix_pad->x_window;
                  event->xmotion.subwindow = 0;
                  event->xmotion.y -= dspl_descr->unix_pad->window->y;
               }
#endif
            }
         else
            /***************************************************************
            *  If x > the dminput window width, we are really in the dmoutput window
            ***************************************************************/
            if (event->xmotion.x > dspl_descr->dmoutput_pad->window->x)
               {
                  DEBUG14(fprintf(stderr, "translating (%d,%d) in cmd input to (%d,%d) in cmd output\n",
                                           event->xmotion.x, event->xmotion.y, event->xmotion.x-dspl_descr->dmoutput_pad->window->x, event->xmotion.y);
                  )
                  event->xmotion.window = dspl_descr->dmoutput_pad->x_window;
                  event->xmotion.x -= dspl_descr->dmoutput_pad->window->x;
               }
      }
   else
      if (event->xmotion.window == dspl_descr->dmoutput_pad->x_window)
         {
            /***************************************************************
            *  If y < 0, we are really in the main window or the unix window.
            ***************************************************************/
            if (event->xmotion.y < 0)
               {
                  DEBUG14(fprintf(stderr, "translating (%d,%d) in cmd output to (%d,%d) in main\n",
                                           event->xmotion.x, event->xmotion.y, event->xmotion.x+dspl_descr->dmoutput_pad->window->x, event->xmotion.y+dspl_descr->dmoutput_pad->window->y);
                  )
                  event->xmotion.window = dspl_descr->main_pad->x_window;
                  event->xmotion.y += dspl_descr->dmoutput_pad->window->y;
                  event->xmotion.x += dspl_descr->dmoutput_pad->window->x;
#ifdef PAD
                  if (dspl_descr->pad_mode && (event->xmotion.y >= dspl_descr->unix_pad->window->y))
                     {
                        DEBUG14(fprintf(stderr, "translating (%d,%d) in main to (%d,%d) in unix cmd\n",
                                                 event->xmotion.x, event->xmotion.y, event->xmotion.x - dspl_descr->unix_pad->window->x, event->xmotion.y - dspl_descr->unix_pad->window->y);
                        )
                        event->xmotion.window = dspl_descr->unix_pad->x_window;
                        event->xmotion.subwindow = 0;
                        event->xmotion.y -= dspl_descr->unix_pad->window->y;
                     }
#endif
               }
            else
               /***************************************************************
               *  If x < 0, we are really in the dminput window
               ***************************************************************/
               if (event->xmotion.x < 0)
                  {
                     DEBUG14(fprintf(stderr, "translating (%d,%d) in cmd output to (%d,%d) in cmd input\n",
                                              event->xmotion.x, event->xmotion.y, event->xmotion.x+dspl_descr->dmoutput_pad->window->x, event->xmotion.y);
                     )
                     event->xmotion.window = dspl_descr->dminput_pad->x_window;
                     event->xmotion.x += dspl_descr->dmoutput_pad->window->x;
                  }
         }
#ifdef PAD
      else
         if (dspl_descr->pad_mode && (event->xmotion.window == dspl_descr->unix_pad->x_window))
            {
               /***************************************************************
               *  If y < 0, we are really in the main window.
               ***************************************************************/
               if (event->xmotion.y < 0)
                  {
                     DEBUG14(fprintf(stderr, "translating (%d,%d) in unixcmd output to (%d,%d) in main\n",
                                              event->xmotion.x, event->xmotion.y, event->xmotion.x+dspl_descr->unix_pad->window->y, event->xmotion.y+dspl_descr->unix_pad->window->y);
                     )
                     event->xmotion.window = dspl_descr->main_pad->x_window;
                     event->xmotion.y += dspl_descr->unix_pad->window->y;
                     event->xmotion.x += dspl_descr->unix_pad->window->x;
                  }
               else
                  /***************************************************************
                  *  If x >= height, we are really in the dminput or dmoutput window
                  ***************************************************************/
                  if (event->xmotion.y >= (int)dspl_descr->unix_pad->window->height)
                  /***************************************************************
                  *  If x > the dminput window width, we are really in the dmoutput window
                  ***************************************************************/
                     if (event->xmotion.x > dspl_descr->dmoutput_pad->window->x)
                        {
                           DEBUG14(fprintf(stderr, "translating (%d,%d) in unixcmd to (%d,%d) in cmd output\n",
                                                    event->xmotion.x, event->xmotion.y, event->xmotion.x-dspl_descr->dmoutput_pad->window->x, event->xmotion.y-dspl_descr->unix_pad->window->height);
                           )
                           event->xmotion.window = dspl_descr->dmoutput_pad->x_window;
                           event->xmotion.x -= dspl_descr->dmoutput_pad->window->x;
                           event->xmotion.y -= dspl_descr->unix_pad->window->height;
                        }
                     else
                        {
                           DEBUG14(fprintf(stderr, "translating (%d,%d) in unixcmd to (%d,%d) in cmd input\n",
                                                    event->xmotion.x, event->xmotion.y, event->xmotion.x, event->xmotion.y-dspl_descr->unix_pad->window->height);
                           )
                           event->xmotion.window = dspl_descr->dminput_pad->x_window;
                           event->xmotion.y -= dspl_descr->unix_pad->window->height;
                        }
         }
#endif
} /* end of adjust_motion_event */



/************************************************************************

NAME:      fake_keystroke - Generate a fake keystroke event to trigger other things.

PARAMETERS:

   1.  passed_dspl_descr - pointer to DISPLAY_DESCR (INPUT)
                           This is the display description to
                           use to build the fake keystroke.  If
                           NULL is passed, use the current 
                           display description passed globally.

GLOBAL DATA:

   1.  The main window drawable description.


FUNCTIONS :

   1.   Build a fake keystroke event.

   2.   send it to the main window.

*************************************************************************/

void fake_keystroke(DISPLAY_DESCR    *passed_dspl_descr)
{
XEvent                event_union;      /* The event structure filled in by X as events occur            */

DEBUG15(fprintf(stderr, "Generating fake keystroke\n");)

if (passed_dspl_descr == NULL)
   passed_dspl_descr = dspl_descr;

event_union.xkey.type        = KeyPress;
event_union.xkey.serial      = 0;
event_union.xkey.send_event  = True;
event_union.xkey.display     = passed_dspl_descr->display;
event_union.xkey.window      = passed_dspl_descr->main_pad->x_window;
event_union.xkey.root        = RootWindow(passed_dspl_descr->display, DefaultScreen(passed_dspl_descr->display));
event_union.xkey.subwindow   = None;
event_union.xkey.time        = 0;
event_union.xkey.x           = 0;
event_union.xkey.y           = 0;
event_union.xkey.x_root      = passed_dspl_descr->main_pad->window->x;
event_union.xkey.y_root      = passed_dspl_descr->main_pad->window->y;
event_union.xkey.state       = None;
event_union.xkey.keycode     = 0; /* an invalid keycode, can never translate to anything */
event_union.xkey.same_screen = True;

DEBUG9(XERRORPOS)
ce_XSendEvent(passed_dspl_descr->display,
              passed_dspl_descr->main_pad->x_window,
              True,
              KeyPressMask,
              &event_union);

} /* end of fake_keystroke */


/************************************************************************

NAME:      store_cmds - Store commands to be triggered with a fake keystroke

PARAMETERS:

   1.  dspl_descr   - pointer to DISPLAY_DESCR  (input)
                      This is the display description the commands are
                      being attached to.

   2.  cmds         - pointer to char  (input)
                      This is the list of commands to store.

   3.  prefix_cmds  - int (input)
                      If true, this flag says to put the commands on the front
                      of the list.

FUNCTIONS :

   1.   Add the passed list to the current list of commands.
        stored in the option values.

*************************************************************************/

void store_cmds(DISPLAY_DESCR    *dspl_descr,
                char             *cmds,
                int               prefix_cmds)
{
int                   len;
char                 *p;

DEBUG1(fprintf(stderr, "store_cmds:  \"%s\"\n", cmds);)
len = strlen(cmds);
if (len > 0)
   {
      if (cmds[len-1] == '\n')
          cmds[len--] = '\0';  /* trim trailing newline */

      for (p = &cmds[len-1]; (p >= cmds) && ((*p == ' ') || (*p == '\t')); p--) /* trim trailing white space */
         *p = '\0';

      if (p >= cmds) /* not an empty line */
         {
            if (!CMDS_FROM_ARG)  /* nothing in the buffer currently */
               {
                  CMDS_FROM_ARG = CE_MALLOC(strlen(cmds)+2);
                  if (CMDS_FROM_ARG)
                     {
                        strcpy(CMDS_FROM_ARG, cmds);
                     }
               }
            else
                {
                   p = CE_MALLOC(strlen(cmds)+strlen(CMDS_FROM_ARG)+4);
                   if (p)
                      {
                         if (prefix_cmds)
                            {
                               strcpy(p, cmds);
                               strcat(p, ";");
                               strcat(p, CMDS_FROM_ARG);
                            }
                         else
                            {
                               strcpy(p, CMDS_FROM_ARG);
                               strcat(p, ";");
                               strcat(p, cmds);
                            }
                         free(CMDS_FROM_ARG);
                         CMDS_FROM_ARG = p;
                      }
                }
         } /* end of cmds is not an empty line */
      else
         DEBUG1(fprintf(stderr, "store_cmds:  empty command list\n");)

   } /* len(cmds) > 0 */

DEBUG1(
   fprintf(stderr, "store_cmds:  saved list:  \"%s\"\n",
           (CMDS_FROM_ARG ? CMDS_FROM_ARG : "<NULL>"));
)

} /* end of store_cmds */


#ifdef PAD
/************************************************************************

NAME:      kill_unixcmd_window - kill off the unixcmd window when it is no longer needed.

PURPOSE:    This routine is called in pad mode when the shell goes away.
            It is called by the asynchronous signal catcher which detects
            the termination of the shell.  This routine sets a flag which is
            examined in the event loop.

PARAMETERS:

   real_kill  - int (INPUT)
                This flag indicates whether the shell really died or there
                was some other reason for this call.
                VALUES:
                1  -  really kill the UNIX pad
                0  -  Set signal handler flag

GLOBAL DATA:

   1.  shell_has_terminated  - int flag
            This flag is set to show the shell has terminated

   2.  pad_mode  - int flag
            This flag shows we are in pad mode.  It gets cleared by 
            close_unixcmd_window

FUNCTIONS :

   1.   If pad mode is already off, do nothing

   2.   Set the global shell_has_terminated flag.

NOTE:

   This routine is called by the async exit which traps interupts.

*************************************************************************/

void kill_unixcmd_window(int real_kill)
{
DISPLAY_DESCR        *padmode_dspl;

DEBUG(fprintf(stderr, "@kill_unixcmd_window, real_kill = %d\n", real_kill);)

padmode_dspl = find_padmode_dspl(dspl_descr);

DEBUG19(
   if (padmode_dspl)
      fprintf(stderr, "kill_unixcmd_window:  Pad mode display is display %d\n", padmode_dspl->display_no);
   else
      fprintf(stderr, "kill_unixcmd_window:  Cannot find pad mode display\n");
)

if (padmode_dspl)
   if (real_kill)
      {
         /* RES 8/29/95 Generate fake keystroke in case cursor is not in window */
         fake_keystroke(padmode_dspl);
         shell_has_terminated = True;
#ifndef WIN32
         kill(0, SIGCONT);
#endif
      }
   else
      reset_child_signal   = True;
} /* end of kill_unixcmd_window */


/************************************************************************

NAME:      reset_child - Reset the child state signal handler in pad.c

PURPOSE:    This routine is called when the reset_child_signal gets set
            in kill_dspl_descr->unix_pad->window->  It resets the SIG_CHLD signal which
            is cleared on some machines when the signal handler is called.

PARAMETERS:

   reset_child_signal - pointer to int (INPUT/OUTPUT)
            This flag is cleared by this routine.

FUNCTIONS :

   1.   Reset the signal handler

*************************************************************************/

static void reset_child(int *reset_child_signal)
{

DEBUG(fprintf(stderr, "@reset_child\n");)

#ifndef WIN32
signal(SIGCHLD, dead_child);                              

#ifdef SIGCLD
#if (SIGCLD != SIGCHLD)
signal(SIGCLD, dead_child);                               
#endif
#endif
#endif
*reset_child_signal = False;

} /* end of reset_child */


/************************************************************************

NAME:      close_unixcmd_window - Get rid of the unixcmd window after the shell has gone away.

PURPOSE:    This routine is called in pad mode when the shell goes away.
            If gets rid of the unixpad window and leaves the file in browse
            mode agaist the transcript pad.  If autoclose is activated, the
            whole process is shut down and this routine does not return
            because dm_wc does not return.

PARAMETERS:

   none

GLOBAL DATA:

   1.  The window descriptions, window, dminput_window, and dspl_descr->dmoutput_pad->window->


FUNCTIONS :

   1.   If autoclose is on, use dm_wc to end this program.

   2.   Release the storage for the unixcmd window.

   3.   Destroy the unixcmd X window.

   4.   Set the dirty bit for the main pad to False, this is the intial
        state for the file we are now browsing.

*************************************************************************/

static void close_unixcmd_window(void)
{
static DMC_wc         autoclose_dmc = {NULL, DM_wc, False, 'f'};
static DMC_vt100      vt100_off_dmc = {NULL, DM_vt, False, DM_OFF};
XEvent                configure_event;
DISPLAY_DESCR        *padmode_dspl;


DEBUG(fprintf(stderr, "@close_unixcmd_window\n");)

padmode_dspl = find_padmode_dspl(dspl_descr);

DEBUG19(
   if (padmode_dspl)
      fprintf(stderr, "close_unixcmd_window:  Pad mode display is display %d\n", padmode_dspl->display_no);
   else
      fprintf(stderr, "close_unixcmd_window:  Cannot find pad mode display\n");
)

if (!padmode_dspl)
   return;
else
   set_global_dspl(padmode_dspl);


/***************************************************************
*  In vt100 mode, shut down vt100 mode first.
***************************************************************/
if (dspl_descr->vt100_mode)
   dm_vt100((DMC *)&vt100_off_dmc, dspl_descr);

if (dspl_descr->autoclose_mode)
   {
      if (padmode_dspl->edit_file_changed)
         dm_pw(dspl_descr, edit_file, False);

      dm_wc((DMC *)&autoclose_dmc, /* input */
            dspl_descr,            /* input  / output */
            edit_file,             /* input  */
            NULL,
            True,                  /* no_prompt */
            MAKE_UTMP);            /* input  */
   }
else
   {
      /***************************************************************
      *  Copy the prompt to the main window.
      ***************************************************************/
      put_line_by_num(dspl_descr->main_pad->token, total_lines(dspl_descr->main_pad->token)-1, get_unix_prompt(), INSERT);
#ifndef WIN32
      /***************************************************************
      *  We want normal ce child signal handling now.
      ***************************************************************/
      signal(SIGCHLD, reap_child_process);                               
#ifdef SIGCLD
#if (SIGCLD != SIGCHLD)
      signal(SIGCLD, reap_child_process);                               
#endif
#endif
#endif
      shell_has_terminated = False;

      /***************************************************************
      *  Shut down pad mode
      ***************************************************************/
      close_shell();
#ifndef WIN32
      if (MAKE_UTMP)
         kill_utmp();
      tty_reset(0); /* in pad.c */
#endif
      /***************************************************************
      *  We are no longer in pad mode.  Get rid of the
      *  pad mode flag and the window, but leave the window id in
      *  the drawable description.  This is for the leave and focusout
      *  events we may get when the window goes away.
      ***************************************************************/
      dspl_descr->pad_mode = False;

      mem_kill(dspl_descr->unix_pad->token);
      dspl_descr->unix_pad->token = NULL;
      clear_text_cursor(dspl_descr->unix_pad->x_window, dspl_descr);
      DEBUG9(XERRORPOS)
      XDestroyWindow(dspl_descr->display, dspl_descr->unix_pad->x_window);
      dspl_descr->unix_pad->x_window = None;
      
      /***************************************************************
      *  If we were in the Unix window, flip out to the main window.
      *  1/28/94, RES. 
      ***************************************************************/
      if ((dspl_descr->cursor_buff->which_window == UNIXCMD_WINDOW) || (dspl_descr->cursor_buff->current_win_buff->which_window == UNIXCMD_WINDOW))
         {
            DEBUG16(fprintf(stderr, "close_unixcmd_window: flipping cursor out of Unix cmd window\n");)
            dspl_descr->cursor_buff->which_window = MAIN_PAD;
            dspl_descr->cursor_buff->current_win_buff = dspl_descr->main_pad;
            dspl_descr->cursor_buff->up_to_snuff = False;
            dspl_descr->cursor_buff->win_line_no = dspl_descr->cursor_buff->current_win_buff->file_line_no - dspl_descr->cursor_buff->current_win_buff->first_line;
            dspl_descr->cursor_buff->y = dspl_descr->cursor_buff->current_win_buff->window->sub_y + (dspl_descr->cursor_buff->win_line_no * dspl_descr->cursor_buff->current_win_buff->window->line_height) +1;
            dspl_descr->cursor_buff->current_win_buff->buff_ptr  = get_line_by_num(dspl_descr->cursor_buff->current_win_buff->token, dspl_descr->cursor_buff->current_win_buff->file_line_no);
         }
      else
         DEBUG(fprintf(stderr, "close_unixcmd_window:  Cursor in %s, buff for %s\n", which_window_names[dspl_descr->cursor_buff->which_window],which_window_names[dspl_descr->cursor_buff->current_win_buff->which_window]);)

      /***************************************************************
      *  Switch out of pad mode.
      ***************************************************************/
      if (!padmode_dspl->edit_file_changed)
         strcpy(edit_file, STDIN_FILE_STRING);
      else
         dm_pw(dspl_descr, edit_file, False);
      cc_padname(dspl_descr, edit_file);
      change_icon(dspl_descr->display,
                  dspl_descr->main_pad->x_window,
                  edit_file,
                  False);

      /***************************************************************
      *  Force the screen to be reconfigured.
      ***************************************************************/
      memset((char *)&configure_event, 0, sizeof(configure_event));
      configure_event.xconfigure.type               = ConfigureNotify;
      configure_event.xconfigure.serial             = 0;
      configure_event.xconfigure.send_event         = True;
      configure_event.xconfigure.display            = dspl_descr->display;
      configure_event.xconfigure.event              = dspl_descr->main_pad->x_window;
      configure_event.xconfigure.window             = dspl_descr->main_pad->x_window;
      configure_event.xconfigure.x                  = dspl_descr->main_pad->window->x;
      configure_event.xconfigure.y                  = dspl_descr->main_pad->window->y;
      configure_event.xconfigure.width              = dspl_descr->main_pad->window->width;
      configure_event.xconfigure.height             = dspl_descr->main_pad->window->height;
      configure_event.xconfigure.border_width       = 1;
      configure_event.xconfigure.above              = None;
      configure_event.xconfigure.override_redirect  = False;

      DEBUG9(XERRORPOS)
      ce_XSendEvent(dspl_descr->display, dspl_descr->main_pad->x_window, True, StructureNotifyMask, (XEvent *)&configure_event);

      /***************************************************************
      *  If the last thing in the output pad is a ^d, replace it with the
      *  *** eof *** 
      ***************************************************************/
      check_pad_eof_char();
      /***************************************************************
      *  RES 3/4/2003
      *  A pad name occur, in pad mode flag this.  This causes wc
      *  to make sure the file is saved.
      ***************************************************************/
      dirty_bit(dspl_descr->main_pad->token) = dspl_descr->edit_file_changed;
      set_edit_file_changed(dspl_descr, False);

      /***************************************************************
      *  Disengage the unixwin from the current buffer if that is where it is.
      ***************************************************************/
      if (dspl_descr->cursor_buff->which_window == UNIXCMD_WINDOW)
         dm_position(dspl_descr->cursor_buff, dspl_descr->main_pad, dspl_descr->main_pad->first_line, 0);
   }

} /* end of close_unixcmd_window */

/************************************************************************

NAME:      check_pad_eof_char - check if the last character in the main pad
                                is the EOF (0x04) char and convert it to 
                                the *** eof *** string.

PARAMETERS:

   none

GLOBAL DATA:

   1.  The main window token.


FUNCTIONS :

   1.   Get the last line in the main pad.

   2.   If the last char on this line is the eof 0x04 char, 
        and this is the only char on the line, replace the line with
        the eof line.

   3.   If the last char on the line is the eof char and there are other
        things on the line, remove the eof char from the line and add 
        the eof line at the bottom.

*************************************************************************/

void check_pad_eof_char(void)
{
char             *last_line;
int               len;
static char      *eof_marker = "*** EOF ***";

last_line = get_line_by_num(dspl_descr->main_pad->token, total_lines(dspl_descr->main_pad->token)-1);
if (last_line != NULL)
   {
      len = strlen(last_line) - 1;
      if (len >= 0 && last_line[len] == 0x04 /* eof */)
         if (len == 0)
            put_line_by_num(dspl_descr->main_pad->token, total_lines(dspl_descr->main_pad->token)-1, eof_marker, OVERWRITE);
         else
            {
               split_line(dspl_descr->main_pad->token, total_lines(dspl_descr->main_pad->token)-1, len, False /* truncate, no new line */);
               put_line_by_num(dspl_descr->main_pad->token, total_lines(dspl_descr->main_pad->token)-1, eof_marker, INSERT);
            }
   }

} /* end of check_pad_eof_char */


#endif

#ifdef  SPECIAL_DEBUG
/***************************************************************
*  
*  This routine puts a copy of the unix command window prompt
*  in the titlebar area.  Used for tracking down problems
*  where the pixmap is getting changed unexpectedly.
*  
***************************************************************/

debug_prompt()
{
XCopyArea(dspl_descr->display, dspl_descr->x_pixmap, dspl_descr->main_pad->x_window,
          dspl_descr->main_pad->window->gc,
          0, dspl_descr->unix_pad->window->y,
          100, dspl_descr->unix_pad->window->height,
          2,2);
XFlush(dspl_descr->display);

} /* end of debug_prompt */
#endif

/************************************************************************

NAME:      get_private_data - Get the private warp data from the display description.

PURPOSE:    This routine returns the existing txcursor private area
            from the display description or initializes a new one.

PARAMETERS:

   none

GLOBAL DATA:

   dspl_descr  -  pointer to DISPLAY_DESCR (in windowdefs.h)
                  This is where the private data for this window set
                  is hung.


FUNCTIONS :

   1.   If a text cursor private area has already been allocated,
        return it.

   2.   Malloc a new area.  If the malloc fails, return NULL

   3.   Initialize the new private area and return it.

RETURNED VALUE
   warp_data  -  pointer to WARP_DATA
               This is the private static area used by the
               routines in this module.
               On malloc failure, a pointer to a static piece of data is returned.

*************************************************************************/

static WARP_DATA *get_private_data(DISPLAY_DESCR  *dspl_descr)
{
WARP_DATA    *warp_data;

warp_data = (WARP_DATA *)dspl_descr->warp_data;
if (!warp_data)
   {
      warp_data = (WARP_DATA *)CE_MALLOC(sizeof(WARP_DATA));
      if (warp_data)
         {
            dspl_descr->warp_data = (void *)warp_data;
            memset((char *)warp_data, 0, sizeof(WARP_DATA));
         }
      else
         warp_data = &dummy_warp_data; /* avoid memory fault */
   }

return(warp_data);

} /* end of get_private_data */

