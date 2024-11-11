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
*  module redraw.c
*
*  Routines:
*     process_redraw          - handle redrawing the screen after the action of DM commands
*     highlight_setup         - Perform the setup for text highlighting.
*     redo_cursor             - Generate a warp to reposition the cursor after a window size change.
*     fake_cursor_motion      - Fudge up a motion event
*     off_screen_warp_check   - Adjust warps to areas on the physical screen.
*
*  Internal:
*     redraw_pad              - Perform redraw processing on one pad.
*     redraw_impacted         - Redraw impacted CC screens.
*     draw_titlebar           - Redraw the titlebar
*     typing_impacts_this_window - Check for typing in other windows which impacts this window of data
*     fake_warp_pointer       - Substitute for XWarpPointer when mouse flow is off.
*     y_in_winline_check      - Verify that a warp is to the window line it is supposed to be.
*     really_moved            - Check to see if the cursor really moved
*     position_in_window      - Check if requested x,y position in the window
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h      */
#include <sys/types.h>      /* /usr/include/sys/types.h      */
#ifdef WIN32
#include <time.h>
#else
#include <sys/time.h>       /* /usr/include/sys/time.h       */
#endif
#include <signal.h>         /* /usr/include/signal.h         */

#include "debug.h"
#include "dmc.h"
#include "dmwin.h"
#include "dumpxevent.h"
#include "getevent.h"
#include "init.h"
#include "lineno.h"
#ifdef PAD
#include "pad.h"         /* needed for macros TTY_ECHO_MODE and TTY_DOT_MODE */
#endif
#include "parms.h"
#include "pd.h"
#include "redraw.h"
#include "scroll.h"
#include "sendevnt.h"
#include "sbwin.h"
#include "tab.h"
#include "titlebar.h"
#include "txcursor.h"
#include "typing.h"
#include "window.h"
#include "winsetup.h"
#include "windowdefs.h" /* needed for edit file in titlebar */
#ifdef PAD
#include "unixwin.h"
#endif
#include "xerrorpos.h"

/***************************************************************
*  
*  Prototypes for the internal routines.
*  
***************************************************************/

static int  redraw_pad(PAD_DESCR      *pad,
                       DISPLAY_DESCR  *dspl_descr,
                       int             redraw_needed);

static void redraw_impacted(DISPLAY_DESCR  *dspl_descr);

static void draw_titlebar(int             redraw_needed,
                          DISPLAY_DESCR  *dspl_descr);

static int typing_impacts_this_window(DISPLAY_DESCR  *impacting_dspl,
                                      DISPLAY_DESCR  *impacted_dspl);

static void fake_warp_pointer(BUFF_DESCR    *cursor_buff,
                              PAD_DESCR     *main_window,
                              int            x,
                              int            y);

static void y_in_winline_check(DISPLAY_DESCR      *dspl_descr,
                               Window              x_window,
                               DRAWABLE_DESCR     *window,
                               int                *win_line_no,
                               int                *y);

static int  really_moved(BUFF_DESCR *cursor_buff);

static int position_in_window(int              x,
                              int              y,
                              DRAWABLE_DESCR  *window);


/************************************************************************

NAME:      process_redraw

PURPOSE:    This routine processes redraw and warp cursor requests generated
            by commands which update the screen.  It is called from the bottom of
            the main command loop and from do_background_task.

PARAMETERS:

   1.  dspl_descr     - pointer to DISPLAY_DESCR (INPUT)
                        This is the current display description we are drawing to.

   2.  redraw_needed  - int (INPUT)
                        This is the bitmask of what types of redraws and what windows
                        need to be redrawn. This must always be a parm.

   3.  warp_needed    - input (INPUT)
                        If a warp is needed, this flag is set.  This must always be a parm.

FUNCTIONS :

   1.   Determine which window(s) need to be redrawn and what kind
        of redraw they need.

 

*************************************************************************/

void process_redraw(DISPLAY_DESCR  *dspl_descr,
                    int             redraw_needed,
                    int             warp_needed)
{

XEvent                event_union;
int                   need_real_warp = False;
#ifdef PAD
int                   normal_pad_mode = (dspl_descr->pad_mode && !(dspl_descr->vt100_mode));
int                   old_lines_on_main_text_area;
int                   delta_size;
#endif
DISPLAY_DESCR        *walk_dspl;
long                  event_mask;
int                   overlaid_cursor = False;
int                   warp_changed;
int                   warp_bad;


DEBUG12(fprintf(stderr, "process_redraw:   Redraw = 0x%02X, Warp = %d, display %d\n", redraw_needed, warp_needed, dspl_descr->display_no);)
if (dspl_descr->x_pixmap == None)
   return;

#ifdef PAD
/***************************************************************
*  If vt100 mode is on, we want to do warps if the cursor is
*  going from the dminput or output windows to the main pad.
*  check to see if it is in either of these windows.
***************************************************************/
if (warp_needed && dspl_descr->vt100_mode)
   {
     need_real_warp = cursor_in_area(dspl_descr->dminput_pad->x_window,
                                     0, 0, 
                                     dspl_descr->dminput_pad->window->width,
                                     dspl_descr->dminput_pad->window->height,
                                     dspl_descr)
                    | cursor_in_area(dspl_descr->dmoutput_pad->x_window,
                                     0, 0, 
                                     dspl_descr->dmoutput_pad->window->width,
                                     dspl_descr->dmoutput_pad->window->height,
                                     dspl_descr);
   }
#endif

need_real_warp |= (warp_needed == REDRAW_FORCE_REAL_WARP);


#ifdef PAD
/***************************************************************
*  If auto vt100 mode is turned on, check to see if we need to
*  switch.
***************************************************************/

if (AUTOVT && redraw_needed && dspl_descr->pad_mode)
   if (autovt_switch())
      return;

/***************************************************************
*  If check to see if the unix command window has to be resized.
*  If so, handle that now.
***************************************************************/
if (normal_pad_mode && need_unixwin_resize(dspl_descr->unix_pad))
   {
      resize_unix_window(dspl_descr, total_lines(dspl_descr->unix_pad->token));
      unix_subarea(dspl_descr->unix_pad->window,
                   dspl_descr->unix_pad->pix_window);
      DEBUG12(fprintf(stderr, ":  process_redraw:  need unixwin resize, new lines = %d\n", dspl_descr->unix_pad->window->lines_on_screen);)
      if (dspl_descr->show_lineno)
         setup_lineno_window(dspl_descr->main_pad->window,                         /* input  */
                             dspl_descr->title_subarea,                            /* input  */
                             dspl_descr->dminput_pad->window,                      /* input  */
                             dspl_descr->unix_pad->window,                         /* input  */
                             dspl_descr->lineno_subarea);                          /* output  */

      old_lines_on_main_text_area = dspl_descr->main_pad->window->lines_on_screen;

      if (main_subarea(dspl_descr))
         return;  /* skip back to event loop, returns true if it had to resize the window */

      /***************************************************************
      *  If scroll bars are on, resize them as well.
      ***************************************************************/
      if (dspl_descr->sb_data->vert_window_on || dspl_descr->sb_data->horiz_window_on)
         resize_sb_windows(dspl_descr->sb_data, dspl_descr->main_pad->window, dspl_descr->display);

      /***************************************************************
      *  delta_size is the change in size of the unixcmd window,
      *  ei, delta_size > 0 means unixcmd gets bigger, main gets smaller.
      ***************************************************************/
      delta_size = old_lines_on_main_text_area - dspl_descr->main_pad->window->lines_on_screen;
      redraw_needed |=  (MAIN_PAD_MASK | UNIXCMD_MASK) & FULL_REDRAW;

      /* make the bottom window line displayed stay the same if the screen is full */
      if (delta_size > 0)
         {
            if (((dspl_descr->main_pad->first_line + dspl_descr->main_pad->window->lines_on_screen) <= total_lines(dspl_descr->main_pad->token)) && (!dspl_descr->main_pad->formfeed_in_window))
               dspl_descr->main_pad->first_line += delta_size;
         }
      else
         if (delta_size < 0)
            {
               if (((dspl_descr->main_pad->first_line + old_lines_on_main_text_area) <= total_lines(dspl_descr->main_pad->token)) && (!dspl_descr->main_pad->formfeed_in_window))
                  dspl_descr->main_pad->first_line += delta_size;
            }

      if (dspl_descr->cursor_buff->which_window == UNIXCMD_WINDOW)
         {
            dm_position(dspl_descr->cursor_buff, dspl_descr->cursor_buff->current_win_buff, dspl_descr->cursor_buff->current_win_buff->file_line_no, dspl_descr->cursor_buff->current_win_buff->file_col_no);
            warp_needed = True;
         }
   }
#endif

#if defined(WIN32)
/***************************************************************
*  In Windows, lock the display description while we process
*  commands to sync with the shell2pad thread.  This is for
*  pad mode only.
***************************************************************/
if (dspl_descr->pad_mode || TRANSPAD || STDIN_CMDS)
   lock_display_descr(dspl_descr, "redraw");
#endif


/***************************************************************
*  If we need to redraw the main window, do it.
*  We redraw the pixmap and then send an expose event to copy
*  it to the window.
***************************************************************/
if (redraw_needed & MAIN_PAD_MASK)
   {
      overlaid_cursor |= redraw_pad(dspl_descr->main_pad, dspl_descr, redraw_needed);
      if (redraw_needed & (MAIN_PAD_MASK & (FULL_REDRAW | SCROLL_REDRAW | PARTIAL_REDRAW)))
         if (dspl_descr->sb_data->vert_window_on  ||
             dspl_descr->sb_data->horiz_window_on ||
             dspl_descr->sb_data->sb_mode != SCROLL_BAR_OFF)
            {
               if (dspl_descr->sb_data->vert_window == None)
                  get_sb_windows(dspl_descr->main_pad,
                                 dspl_descr->display,
                                 dspl_descr->sb_data,
                                 SBCOLORS,
                                 SBWIDTH);

               (void)draw_vert_sb(dspl_descr->sb_data, dspl_descr->main_pad, dspl_descr->display);
               (void)draw_horiz_sb(dspl_descr->sb_data, dspl_descr->main_pad, dspl_descr->display);
            }
   }

/***************************************************************
*  
*  If we were redrawing the titlebar or did something to change
*  the titlebar, other than a full redraw of the main, then
*  redraw the titlebar.  For a full redraw of the main the
*  call to draw titlebar is done from within redraw_pad.
*  
***************************************************************/

if (((redraw_needed & TITLEBAR_MASK) || (redraw_needed & (SCROLL_REDRAW & MAIN_PAD_MASK))) &&
   !(redraw_needed & (FULL_REDRAW & MAIN_PAD_MASK)))
   draw_titlebar(redraw_needed, dspl_descr);

/***************************************************************
*  
*  On a main window full redraw, redraw the menu bar if needed.
*  It does not change much.
*  
***************************************************************/

if (dspl_descr->pd_data->menubar_on && (redraw_needed & (FULL_REDRAW & MAIN_PAD_MASK)))
   {
      draw_menu_bar(dspl_descr->display,
                    dspl_descr->x_pixmap,
                    dspl_descr->hsearch_data,
                    dspl_descr->pd_data);  /* in pd.c */
      XCopyArea(dspl_descr->display, dspl_descr->x_pixmap, dspl_descr->pd_data->menubar_x_window,
                dspl_descr->pd_data->menu_bar.gc,
                dspl_descr->pd_data->menu_bar.x,     dspl_descr->pd_data->menu_bar.y,
                dspl_descr->pd_data->menu_bar.width, dspl_descr->pd_data->menu_bar.height,
                0, 0);  /*              dspl_descr->pd_data->menu_bar.x,     dspl_descr->pd_data->menu_bar.y);*/
   }

#ifdef PAD
/***************************************************************
*  
*  If we are in pad mode, see if the unix window needs redrawing.
*  This window is also backed up by the pixmap.
*  
***************************************************************/

 if (normal_pad_mode && (redraw_needed & UNIXCMD_MASK))
    overlaid_cursor |= redraw_pad(dspl_descr->unix_pad, dspl_descr, redraw_needed);

#endif

/***************************************************************
*
*  If we have a change to the DM input, reexpose it.
*  RES 8/18/94, Redraw dmoutput if dminput is wider than window.
*  
***************************************************************/

if (redraw_needed & DMINPUT_MASK)
   {
      overlaid_cursor |= redraw_pad(dspl_descr->dminput_pad, dspl_descr, redraw_needed);
      if (XTextWidth(dspl_descr->dminput_pad->window->font,
                     dspl_descr->dminput_pad->buff_ptr,
                     strlen(dspl_descr->dminput_pad->buff_ptr)) >
          (int)dspl_descr->dminput_pad->window->sub_width)
      redraw_needed |= DMOUTPUT_MASK & FULL_REDRAW;
   }

/***************************************************************
*  
*  If we have a change to the DM output, reexpose it.
*  
***************************************************************/

if (redraw_needed & DMOUTPUT_MASK)
   overlaid_cursor |= redraw_pad(dspl_descr->dmoutput_pad, dspl_descr, redraw_needed);

/***************************************************************
*  
*  If a warp was requested, do it unless we are already waiting
*  on a warp to that coordinate.  This can occur when the window
*  gets raised.
*  
***************************************************************/

if (warp_needed &&
    in_window(dspl_descr) &&
    !duplicate_warp(dspl_descr,
                    dspl_descr->cursor_buff->current_win_buff->x_window,
                    dspl_descr->cursor_buff->x, 
                    dspl_descr->cursor_buff->y))
   {
      if (expecting_warp(dspl_descr))
         {
            event_mask = EnterWindowMask | LeaveWindowMask | PointerMotionMask;
            DEBUG9(XERRORPOS)
            while (ce_XCheckIfEvent(dspl_descr->display,
                                    &event_union,
                                    find_valid_event,
                                    (char *)&event_mask))
               DEBUG10(fprintf(stderr, "IGNORING:  ");dump_Xevent(stderr, &event_union, debug);)
         }

      y_in_winline_check(dspl_descr,
                         dspl_descr->cursor_buff->current_win_buff->x_window,
                         dspl_descr->cursor_buff->current_win_buff->window,
                         &dspl_descr->cursor_buff->win_line_no,
                         &dspl_descr->cursor_buff->y);
      DEBUG10( fprintf(stderr, "Warping to file [%d,%d] in %s top corner [%d,%d] win [%d,%d] line \"%s\"\n",
                               dspl_descr->cursor_buff->current_win_buff->file_line_no, dspl_descr->cursor_buff->current_win_buff->file_col_no, which_window_names[dspl_descr->cursor_buff->which_window], 
                               dspl_descr->cursor_buff->current_win_buff->first_line, dspl_descr->cursor_buff->current_win_buff->first_char,
                               dspl_descr->cursor_buff->win_line_no, dspl_descr->cursor_buff->win_col_no,
                               dspl_descr->cursor_buff->current_win_buff->buff_ptr);
      )

      warp_changed = off_screen_warp_check(dspl_descr->display, dspl_descr->main_pad->window, dspl_descr->cursor_buff->current_win_buff->window, &dspl_descr->cursor_buff->x, &dspl_descr->cursor_buff->y);
      /*if (warp_changed) RES 4/4/2003 take off if, always check for off screen warp */
         if (position_in_window(dspl_descr->cursor_buff->x, dspl_descr->cursor_buff->y, dspl_descr->cursor_buff->current_win_buff->window))
            warp_bad = False;
         else
            warp_bad = True;
      /*else
         warp_bad = False; */

      DEBUG9(XERRORPOS)
      if ((really_moved(dspl_descr->cursor_buff) || need_real_warp) &&
          MOUSEON && !warp_bad &&
          (!((dspl_descr->vt100_mode) && (dspl_descr->cursor_buff->which_window == MAIN_PAD)) || need_real_warp))
         XWarpPointer(dspl_descr->display, None, dspl_descr->cursor_buff->current_win_buff->x_window,
                      0, 0, 0, 0,
                      dspl_descr->cursor_buff->x, dspl_descr->cursor_buff->y);
      else
         fake_warp_pointer(dspl_descr->cursor_buff, dspl_descr->main_pad, dspl_descr->cursor_buff->x, dspl_descr->cursor_buff->y); 

      expect_warp(dspl_descr->cursor_buff->x, dspl_descr->cursor_buff->y, dspl_descr->cursor_buff->current_win_buff->x_window, dspl_descr);
      DEBUG10( fprintf(stderr, "Warped to (%d,%d) in %s\n", dspl_descr->cursor_buff->x, dspl_descr->cursor_buff->y, which_window_names[dspl_descr->cursor_buff->which_window]);)
   }
else
   {
      if (overlaid_cursor)
         redo_cursor(dspl_descr, True);

      DEBUG10(
         if (warp_needed)
            if (!in_window(dspl_descr))
               fprintf(stderr, "Warp not done, mouse cursor is not in window\n");
            else
               if (duplicate_warp(dspl_descr,
                                  dspl_descr->cursor_buff->current_win_buff->x_window,
                                  dspl_descr->cursor_buff->x, 
                                  dspl_descr->cursor_buff->y))
                  fprintf(stderr, "Warp not done, already expecting warp\n");
      )
   }

XFlush(dspl_descr->display);

/***************************************************************
*  Redraw any main pads which have to be redrawn because of
*  the changes in this window.
***************************************************************/
for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
{
   walk_dspl->main_pad->impacted_redraw_mask |= (redraw_needed & TITLEBAR_MASK);
   if ((walk_dspl->main_pad->impacted_redraw_mask) && (walk_dspl->x_pixmap != None)) /* RES 8/16/94, don't redraw icon'ed windows */
      redraw_impacted(walk_dspl);
}

#if defined(WIN32)
/***************************************************************
*  In Windows, lock the display description while we process
*  commands to sync with the shell2pad thread.  This is for
*  pad mode only.
***************************************************************/
if (dspl_descr->pad_mode || TRANSPAD || STDIN_CMDS)
   unlock_display_descr(dspl_descr, "redraw");
#endif

} /* end of process_redraw */

#ifdef DebuG
static char   *redraw_name(PAD_DESCR      *pad,
                           int             redraw_needed)
{
int          lcl_redraw;
static char  returned_text[40];

returned_text[0] = '\0';
lcl_redraw = redraw_needed & pad->redraw_mask;

if (lcl_redraw & FULL_REDRAW)
   strcpy(returned_text, "Full Redraw");
else
   {
      if (lcl_redraw & PARTIAL_REDRAW)
        strcpy(returned_text, "Partial Redraw");
      else
         if (lcl_redraw & PARTIAL_LINE)
            strcpy(returned_text, "Partial Line Redraw");


      if (lcl_redraw & SCROLL_REDRAW)
         if (returned_text[0])
            strcat(returned_text, " + Scroll Redraw");
         else
            strcpy(returned_text, "Scroll Redraw");
   }

return(returned_text);
} /* end of redraw_name */
#endif


/************************************************************************

NAME:      redraw_pad - Perform redraw processing on one pad.

PURPOSE:    This routine processes the redrawing of one pad.  This can be the
            pad for the main window, unix window, dminput window, or dmoutput window.
            Data is written to the pixmap and then copied to the window.


PARAMETERS:

   1.  pad            - pointer to PAD_DESCR (INPUT/OUTPUT)
                        This is the pad description to be drawn as directed by 
                        the redraw needed mask.  The redraw_start_line
                        value is reset to -1, so it is also an output parm.

   2.  redraw_needed  - int (INPUT)
                        This is the bitmask of what types of redraws and what windows
                        need to be redrawn. This must always be a parm.


GLOBAL DATA:

   1.  the unixcmd window and dminput window drawable descriptions are accessed
       when determining how to clear the main window without blasting the dminput or
       unixcmd area's in the pixmap.


FUNCTIONS :

   1.   If there is no pixmap, do nothing.

   2.   Calculate the full height and width and location in the 
        pixmap of the window being redrawn.

   3.   If we need a full redraw, do it.   If we redraw the whole window
        we do not care about scroll redraws, partial redraws, or partial lines.

   4.   For full redraws of the main window, redo the titlebar.

   5.   If we need a partial redraw, do it.  We could do a partial redraw
        followed by a scroll, so we process both.  If we redraw part
        of the screen, a partial line redraw is not relevant

   6.   For full and partial redraws, redraw the unix or dminput prompts
        if applicable.

   7.   If all we need is a partial line, do it.

   8.   xcopy the relevant data from the pixmap to the main screen.
   
RETURNED VALUE:
   overlaid_cursor  -  int
                       This flag is returned true if the cursor is overlaid
                       and needs to be redrawn.

*************************************************************************/

static int  redraw_pad(PAD_DESCR      *pad,
                       DISPLAY_DESCR  *dspl_descr,
                       int             redraw_needed)
{

int                   expose_x;
XExposeEvent          expose;
int                   pixmap_x;         /* offset of window in pixmap, used in expose event processing                */
int                   pixmap_y;         /* main window is at 0,0, other windows are elsewhere on the pixmap           */
int                   dot_mode;
int                   full_width;
int                   full_height;
int                   highlight_mode;
int                   need_expose = False;
DISPLAY_DESCR        *walk_dspl;
int                   overlaid_cursor = False;

if (dspl_descr->x_pixmap == None)
   return(overlaid_cursor);

/***************************************************************
*  Calculate the full height and width and location in the 
*  pixmap of the window being redrawn.
*
*  Figure out whether dot mode applies.  Dot mode applies to the
*  Unix command window when we are entering a password.  We 
*  display dots instead of the data.
***************************************************************/

if (pad->which_window == MAIN_PAD)
   {
      pixmap_x = 0;
      pixmap_y = 0;
      dot_mode = False;
      full_width  = pad->window->width;
#ifdef PAD
      full_height = ((dspl_descr->pad_mode && !dspl_descr->vt100_mode ? dspl_descr->unix_pad->window->y : dspl_descr->dminput_pad->window->y));
#else
      full_height = dspl_descr->dminput_pad->window->y;
#endif
   }
else
   {
      pixmap_x = pad->window->x;
      pixmap_y = pad->window->y;

#ifdef PAD
      if ((pad->which_window == UNIXCMD_WINDOW) && !(TTY_DOT_MODE) && !setprompt_mode() && !NODOTS)
         dot_mode = True;
      else
         dot_mode = False;
#else
      dot_mode = False;
#endif

      full_width  = pad->window->width;
      full_height = pad->window->height;
   }

if (pixmap_x < 0)
   pixmap_x = 0;
if (pixmap_y < 0)
   pixmap_y = 0;


DEBUG12(
   fprintf(stderr, "redraw_pad:       Redrawing %s, redraw = 0x%02X(%s), display %d\n",
                    which_window_names[pad->which_window], redraw_needed,
                    redraw_name(pad, redraw_needed),
                    dspl_descr->display_no);
)

if (dspl_descr->echo_mode && dspl_descr->mark1.mark_set && (dspl_descr->mark1.buffer->which_window == pad->which_window))
   highlight_mode = True;
else
   highlight_mode = False;

if (highlight_mode)
   highlight_setup(&(dspl_descr->mark1), dspl_descr->echo_mode);

if (redraw_needed & (FULL_REDRAW & pad->redraw_mask))
   {
      /***************************************************************
      *  
      *  FULL REDRAW of WINDOW
      *  
      ***************************************************************/

      if (pad->buff_modified)
         flush(pad);
      DEBUG12( fprintf(stderr," Fully redrawing %s pixmap, first row = %d, first col = %d\n", which_window_names[pad->which_window], pad->first_line, pad->first_char); )

      DEBUG9(XERRORPOS)
      XFillRectangle(dspl_descr->display,
                     dspl_descr->x_pixmap,
                     pad->window->reverse_gc,
                     pixmap_x, pixmap_y,
                     full_width, full_height);

      /***************************************************************
      *  Make sure there is enough stuff loaded to fill the screen.
      *  Also, the erase just killed the title bar, redraw it.
      ***************************************************************/
      if (pad->which_window == MAIN_PAD)
         {
            if (!get_background_work(MAIN_WINDOW_EOF))
               load_enough_data(pad->first_line + pad->window->lines_on_screen);
            else
               if (pad->window->lines_on_screen > pad->win_lines_size)
                   setup_winlines(pad);
            draw_titlebar(redraw_needed, dspl_descr);
         }

      /***************************************************************
      *  If the prompt is needed, draw it,
      ***************************************************************/
#ifdef PAD
      if ((pad->which_window == UNIXCMD_WINDOW) && (pad->first_line == 0))
         draw_unix_prompt(dspl_descr->display, dspl_descr->x_pixmap, pad->pix_window);
      else
#endif
        if ((pad->which_window == DMINPUT_WINDOW) && (pad->first_line == 0))
            draw_dm_prompt(dspl_descr, pad->pix_window);


      DEBUG9(XERRORPOS)
      textfill_drawable(pad,
                        True,                /* overlay mode */
                        0,                   /* starting line on window */
                        dot_mode);           /* do dots  only in unixcmd window if ttyecho is off */
      if (dspl_descr->show_lineno && (pad->which_window == MAIN_PAD))
         write_lineno(dspl_descr->display,
                      dspl_descr->x_pixmap,
                      dspl_descr->lineno_subarea,
                      pad->first_line,
                      1,     /* overlay mode - already cleared */
                      0,     /* skip lines                     */
                      pad->token,
                      pad->formfeed_in_window,
                      pad->win_lines);

      expose.type       = Expose;
      expose.serial     = 0;
      expose.send_event = True;
      expose.display    = dspl_descr->display;
      expose.window     = pad->x_window;
      expose.x          = 0;
      expose.y          = 0;
      expose.width      = full_width;
      expose.height     = full_height;
      expose.count      = 0;
      need_expose       = True;

      if (cursor_in_area(pad->x_window, 0, 0, pad->window->width, pad->window->height, dspl_descr))
         overlaid_cursor = True;
   } /* end of full redraw */
else
   {
      if (redraw_needed & (PARTIAL_REDRAW & pad->redraw_mask))
         {
            /***************************************************************
            *  
            *  PARTIAL REDRAW of WINDOW
            *  
            ***************************************************************/

            if (pad->buff_modified)
               flush(pad);
            DEBUG12( fprintf(stderr," Partial redraw %s pixmap, from line %d, first row = %d, first col = %d\n", which_window_names[pad->which_window], pad->redraw_start_line,  pad->first_line, pad->first_char); )
            /***************************************************************
            *  If the prompt is needed, draw it,
            ***************************************************************/
#ifdef PAD
            if ((pad->which_window == UNIXCMD_WINDOW) && (pad->first_line == 0) && (pad->redraw_start_line == 0))
               draw_unix_prompt(dspl_descr->display, dspl_descr->x_pixmap, dspl_descr->unix_pad->pix_window);
            else
#endif
               if ((pad->which_window == DMINPUT_WINDOW) && (pad->first_line == 0) && (pad->redraw_start_line == 0))
                  draw_dm_prompt(dspl_descr, dspl_descr->dminput_pad->pix_window);

            textfill_drawable(pad,
                              False,                  /* not overlay mode */
                              pad->redraw_start_line, /* starting line on window */
                              dot_mode);              /* do dots  only in unixcmd window if ttyecho is off */

            if (dspl_descr->show_lineno && (pad->which_window == MAIN_PAD))
               write_lineno(dspl_descr->display,
                            dspl_descr->x_pixmap,
                            dspl_descr->lineno_subarea,
                            pad->first_line,
                            0,                        /* not overlay mode - not already cleared */
                            pad->redraw_start_line,   /* skip lines                     */
                            pad->token,
                            pad->formfeed_in_window,
                            pad->win_lines);

            expose.type       = Expose;
            expose.serial     = 0;
            expose.send_event = True;
            expose.display    = dspl_descr->display;
            expose.window     = pad->x_window;
            expose.x          = 0;
            expose.y          = 0;
            expose.width      = full_width;
            expose.height     = full_height;
            expose.count      = 0;
            need_expose       = True;

            if (cursor_in_area(pad->x_window, expose.x, expose.y, expose.width, expose.height, dspl_descr))
               overlaid_cursor = True;

         } /* end of partial screen redraw */
      else
         if (redraw_needed & (PARTIAL_LINE & pad->redraw_mask))
            {
               /***************************************************************
               *  
               *  PARTIAL REDRAW of 1 LINE
               *  
               ***************************************************************/

               if (pad->window->font->min_bounds.lbearing < 0)
                  pad->redraw_start_col = 0;
               draw_partial_line(pad,
                                 pad->redraw_start_line,
                                 pad->redraw_start_col,
                                 pad->file_line_no,
                                 pad->buff_ptr,
                                 dot_mode,
                                 &expose_x);

               expose.type       = Expose;
               expose.serial     = 0;
               expose.send_event = True;
               expose.display    = dspl_descr->display;
               expose.window     = pad->x_window;

               if ((highlight_mode) || (pad->window->font->min_bounds.lbearing < 0))
                  {
                      expose.x          = 0;
                      expose.y          = 0;
                      expose.width      = full_width;
                      expose.height     = full_height;
                  }
               else
                  {
                     expose.x          = expose_x;
                     expose.y          = (pad->redraw_start_line * pad->window->line_height) + pad->window->sub_y;
                     expose.width      = pad->window->width - expose_x;
                     expose.height     = pad->window->line_height;
                  }
               expose.count      = 0;
               need_expose       = True;

               if (cursor_in_area(pad->x_window, expose.x, expose.y, expose.width, expose.height, dspl_descr))
                  overlaid_cursor = True;

            }  /* end of partial line redraw */

      if (redraw_needed & (SCROLL_REDRAW & MAIN_PAD_MASK))
         {
            /***************************************************************
            *  
            *  SCROLL REDRAW
            *  
            ***************************************************************/

            if (pad->buff_modified)
               flush(pad);
            if (pad->redraw_scroll_lines)
               {
                  DEBUG12( fprintf(stderr," Scroll redraw %s pixmap, first row = %d, first col = %d, lines scrolled = %d\n", which_window_names[pad->which_window], pad->first_line, pad->first_char, pad->redraw_scroll_lines); )
                  scroll_redraw(pad,
                                ((dspl_descr->show_lineno && (pad->which_window == MAIN_PAD)) ? dspl_descr->lineno_subarea : NULL),
                                pad->redraw_scroll_lines);

                  if (cursor_in_area(pad->x_window, 0, 0, pad->window->width, pad->window->height, dspl_descr))
                     overlaid_cursor = True;

                  pad->redraw_scroll_lines = 0;
 
               }
            else
               if (dspl_descr->show_lineno && (pad->which_window == MAIN_PAD))
                  {
                     DEBUG12(fprintf(stderr," Lineno redraw %s pixmap, first row = %d, first col = %d\n", which_window_names[pad->which_window], pad->first_line, pad->first_char);)
                     /* if it wasn't a scroll redraw, must be a lineno redraw */
                     write_lineno(dspl_descr->display,
                                  dspl_descr->x_pixmap,
                                  dspl_descr->lineno_subarea,
                                  pad->first_line,
                                  0,     /* not overlay mode - clear lineno area */
                                  0,     /* skip lines                     */
                                  pad->token,
                                  pad->formfeed_in_window,
                                  pad->win_lines);

                     /***************************************************************
                     *  We need an expose, unless we already have one.
                     *  The previous exposes cover the lineno area.  This expose
                     *  is for just the lineno area.
                     ***************************************************************/
                     if (!need_expose)
                        {
                           expose.type       = Expose;
                           expose.serial     = 0;
                           expose.send_event = True;
                           expose.display    = dspl_descr->display;
                           expose.window     = pad->x_window;
                           expose.x          = 0;
                           expose.y          = 0;
                           expose.width      = dspl_descr->lineno_subarea->width+dspl_descr->lineno_subarea->x;
                           expose.height     = full_height;
                           expose.count      = 0;
                           need_expose       = True;
                        }
                  }
         }

   }  /* end of not main pad full redraw */

/***************************************************************
*  
*  Check if typing in some other window affects this window but
*  was not caught because of scrolling in this window.
*  
***************************************************************/
if (pad->which_window == MAIN_PAD)
   for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
      if (typing_impacts_this_window(walk_dspl, dspl_descr))
         {
               DEBUG12(fprintf(stderr, "Impact to display %d, from display %d, window line %d \"%s\"\n",
                       dspl_descr->display_no,  walk_dspl->display_no,
                       walk_dspl->main_pad->file_line_no - dspl_descr->main_pad->first_line,
                       walk_dspl->main_pad->buff_ptr);
                )
               draw_partial_line(pad,
                                 walk_dspl->main_pad->file_line_no - dspl_descr->main_pad->first_line,
                                 0, /* redraw_start_col */
                                 walk_dspl->main_pad->file_line_no,
                                 walk_dspl->main_pad->buff_ptr,
                                 False, /* not dot_mode */
                                 &expose_x);

               /* recopy the whole window, too hard to deal with partials at this point */
               expose.type       = Expose;
               expose.serial     = 0;
               expose.send_event = True;
               expose.display    = dspl_descr->display;
               expose.window     = pad->x_window;
               expose.x          = 0;
               expose.y          = 0;
               expose.width      = full_width;
               expose.height     = full_height;
               expose.count      = 0;
               need_expose       = True;

               if (cursor_in_area(pad->x_window, 0, 0, expose.width, expose.height, dspl_descr))
                  overlaid_cursor = True;
         }

if (overlaid_cursor)
   clear_text_cursor(pad->x_window, dspl_descr);


/***************************************************************
*  
*  The pixmap drawing is done.  If we need to copy data from the
*  pixmap to the window, do it.
*  
***************************************************************/

if (need_expose)
   {
      if (highlight_mode)
         clear_text_highlight(dspl_descr);
      DEBUG12(
      fprintf(stderr, "xcopy from %dx%d%+d%+d to %dx%d%+d%+d, %s\n",
                   expose.width, expose.height,
                   expose.x+pixmap_x, expose.y+pixmap_y,
                   expose.width, expose.height,
                   expose.x, expose.y, which_window_names[pad->which_window]);
      )
      XCopyArea(dspl_descr->display, dspl_descr->x_pixmap, pad->x_window,
                pad->window->gc,
                expose.x+pixmap_x, expose.y+pixmap_y,
                expose.width, expose.height,
                expose.x, expose.y);
      if (highlight_mode)
         text_rehighlight(dspl_descr->main_pad->x_window, dspl_descr->main_pad->window, dspl_descr);

   } /* end of need expose */

pad->redraw_start_line = -1;
return(overlaid_cursor);

} /* end of redraw_pad */


/************************************************************************

NAME:      redraw_impacted - Redraw impacted CC screens.

PURPOSE:    This routine is used when CC windows are up.  It does the redrawing
            needed on the other windows.


PARAMETERS:

   1.  dspl_descr     - pointer to DISPLAY_DESCR (INPUT)
                        This is the display description for the window set to
                        be redrawn.  It is known when this routine is called
                        that the impacted redraw mask is already non-zero.

FUNCTIONS :

   1.   If the main pad has to be redrawn, redraw it.

   2.   If the titlebar has to be redrawn, redraw it also.
        pixmap of the window being redrawn.
   
NOTES
   1.   Only the main pad is ever redrawn in CC mode.  This includes the
        titlebar window and the lineno window.

*************************************************************************/

static void redraw_impacted(DISPLAY_DESCR  *dspl_descr)
{
int     save_restart_line;
int     save_restart_col;
int     save_scroll_lines;
int     redraw_needed;

 DEBUG12(fprintf(stderr,"redraw_impacted:  display %d, redraw 0x%X\n", dspl_descr->display_no, dspl_descr->main_pad->impacted_redraw_mask);)

/***************************************************************
*  
*  Only main pad data is passed 
*  
***************************************************************/

redraw_needed = dspl_descr->main_pad->impacted_redraw_mask;

/***************************************************************
*  If we need to redraw the main window, do it.
*  We redraw the pixmap and then send an expose event to copy
*  it to the window.
***************************************************************/
if (redraw_needed & MAIN_PAD_MASK)
   {
      save_restart_line = dspl_descr->main_pad->redraw_start_line;
      save_restart_col  = dspl_descr->main_pad->redraw_start_col;
      save_scroll_lines = dspl_descr->main_pad->redraw_scroll_lines;
      DEBUG0(if ((save_restart_line != -1) || save_scroll_lines) fprintf(stderr, "redraw_impacted:  Suprise, save_restart_line = %d, save_scroll_lines = %d\n", save_restart_line, save_restart_line);)
                    
      dspl_descr->main_pad->redraw_start_line   = dspl_descr->main_pad->impacted_redraw_line;
      dspl_descr->main_pad->redraw_start_col    = 0;
      dspl_descr->main_pad->redraw_scroll_lines = 0;

      redraw_needed &=  ~PARTIAL_LINE; /* get rid of partial line redraws, impacted code in redraw pad handles this */
      redraw_pad(dspl_descr->main_pad, dspl_descr, redraw_needed);
      if (in_window(dspl_descr)) /* 8/15/94 RES, fix cursor appearing in wrong window */
         fake_cursor_motion(dspl_descr);

      dspl_descr->main_pad->redraw_start_line   = save_restart_line;
      dspl_descr->main_pad->redraw_start_col    = save_restart_col;
      dspl_descr->main_pad->redraw_scroll_lines = save_scroll_lines;
   }


/***************************************************************
*  
*  If we were redrawing the titlebar or did something to change
*  the titlebar, other than a full redraw of the main, then
*  redraw the titlebar.  For a full redraw of the main the
*  call to draw titlebar is done from within redraw_pad.
*  
***************************************************************/

if (((redraw_needed & TITLEBAR_MASK) || (redraw_needed & (SCROLL_REDRAW & MAIN_PAD_MASK)))
    && !(redraw_needed & (FULL_REDRAW & MAIN_PAD_MASK)))
   draw_titlebar(redraw_needed, dspl_descr);

/***************************************************************
*  
*  Reset the values for next time
*  
***************************************************************/

dspl_descr->main_pad->impacted_redraw_line   = -1;
dspl_descr->main_pad->impacted_redraw_mask   = 0;

} /* end of redraw_impacted */


/************************************************************************

NAME:      draw_titlebar - Redraw the titlebar

PURPOSE:    This routine redraws the titlebar in the main window.


PARAMETERS:

   1.  redraw_needed  - int (INPUT)
                        This is the mask which tells which windows need to
                        be redrawn and what kind of redrawing is done in each.

   1.  dspl_descr     - pointer to DISPLAY_DESCR (INPUT)
                        This is the display description for the window set to
                        be redrawn.  It is known when this routine is called
                        that the impacted redraw mask is already non-zero.

FUNCTIONS :

   1.   Redraw the titlebar into the pixmap

   2.   If the main pad was not also redrawn, copy the data to the
        window.

*************************************************************************/

static void draw_titlebar(int             redraw_needed,
                          DISPLAY_DESCR  *dspl_descr)
{
int                   modified;
DISPLAY_DESCR        *walk_dspl;

modified = dirty_bit(dspl_descr->main_pad->token) | dspl_descr->main_pad->buff_modified;
for (walk_dspl = dspl_descr->next; walk_dspl != dspl_descr; walk_dspl = walk_dspl->next)
    modified |= walk_dspl->main_pad->buff_modified;

#ifdef PAD
if (dspl_descr->pad_mode)
   write_titlebar(dspl_descr->display,
                  dspl_descr->x_pixmap,
                  dspl_descr->title_subarea,
                  edit_file,
                  dspl_descr->titlebar_host_name,
                  -1, /* line numbers off */
                  0,  /* column numbers off */
                  *dspl_descr->unix_pad->insert_mode,
                  WRITABLE(dspl_descr->unix_pad->token),
                  dspl_descr->hold_mode,
                  dspl_descr->scroll_mode,
                  dspl_descr->autohold_mode,
                  dspl_descr->vt100_mode,
                  dspl_descr->unix_pad->window_wrap_col,
#ifdef CMD_RECORD_BOX
                  (dspl_descr->cmd_record_data != NULL),
#endif
                  False); /* modified flag off */
else
#endif
   write_titlebar(dspl_descr->display,
                  dspl_descr->x_pixmap,
                  dspl_descr->title_subarea,
                  edit_file,
                  dspl_descr->titlebar_host_name,
                  dspl_descr->main_pad->first_line,
                  dspl_descr->main_pad->first_char,
                  *dspl_descr->main_pad->insert_mode,
                  WRITABLE(dspl_descr->main_pad->token),
                  False, False, False, False,
                  dspl_descr->main_pad->window_wrap_col,
#ifdef CMD_RECORD_BOX
                  (dspl_descr->cmd_record_data != NULL),
#endif
                  modified);

DEBUG9(XERRORPOS)
if (!(redraw_needed & (FULL_REDRAW & MAIN_PAD_MASK)))  /* on full main redraw, we don't need the extra expose event */
   {
      XCopyArea(dspl_descr->display, dspl_descr->x_pixmap, dspl_descr->main_pad->x_window,
                dspl_descr->main_pad->window->gc,
                dspl_descr->title_subarea->x,
                dspl_descr->title_subarea->y,
                dspl_descr->title_subarea->width, dspl_descr->title_subarea->height,
                dspl_descr->title_subarea->x,
                dspl_descr->title_subarea->y);
   }
} /* end of draw_titlebar */


static int typing_impacts_this_window(DISPLAY_DESCR  *impacting_dspl,
                                      DISPLAY_DESCR  *impacted_dspl)
{
int                   window_line;

/***************************************************************
*  If there is no typing currently going on the in the impacting
*  window, we do not have any impacts.
***************************************************************/
if (!impacting_dspl->main_pad->buff_modified)
   return(False);

/***************************************************************
*  Find out some stuff about this display.
*  What window line is the typing line on the impacted display.
***************************************************************/
window_line = impacting_dspl->main_pad->file_line_no - impacted_dspl->main_pad->first_line;

/***************************************************************
*  If this is an insert on the screen, everything from that point
*  on down has to be redrawn.  If this is an overwrite, we can
*  replace just the line unless we are already impacted.  In that
*  case we also do a partial screen redraw.
***************************************************************/
if ((window_line >= 0) && (window_line < impacted_dspl->main_pad->window->lines_on_screen))
   return(True);
else
   return(False);
      
} /* end of  typing_impacts_this_window */


/************************************************************************

NAME:      off_screen_warp_check  - Adjust warps to areas on the physical screen.


PURPOSE:    This routine checks to make sure the that a warp cursor is within
            the bounds of the physcical screen.  The Xserver will ignore attempts
            to move the cursor off the physical screen.

PARAMETERS:

   1.  main_window - pointer to DRAWABLE_DESCR (INPUT)
                     This is the drawable description for the main window.
                     It is used to determine the root coordinates of the warp since,
                     the x and y values fo the main window are with relation to the
                     root window.

   2.  target_window - pointer to DRAWABLE_DESCR (INPUT)
                     This is the drawable description for the window (not sub_window)
                     within ce we are warping to.  

   3.  x           - pointer to int (INPUT / OUTPUT)
                     This is the proposed X coordinate we are to warp to.
                     It may be modified.

   4.  y           - pointer to int (INPUT / OUTPUT)
                     This is the proposed Y coordinate we are to warp to.
                     It may be modified.

FUNCTIONS :

   1.   Calculate root position of the upper left corner of the target window.

   2.   Add in the proposed warp position and make sure it is on screen.
        If not, adjust it.

RETURNED VALUE:

   bad   -  int
            This flag specifies whether the original x y coordinate was bad and
            had to be changed.
            Values:  True  -  X or Y or both values had to be changed
                     False -  Values were ok
 

*************************************************************************/

int off_screen_warp_check(Display            *display,
                          DRAWABLE_DESCR     *main_window,
                          DRAWABLE_DESCR     *target_window,
                          int                *x,
                          int                *y)
{
int                   root_x;
int                   root_y;
int                   display_width;
int                   display_height;
int                   bad = False;

display_width  = DisplayWidth(display, DefaultScreen(display)) ;
display_height = DisplayHeight(display, DefaultScreen(display)) ;

if (target_window == main_window)
   {
      root_x = main_window->x;
      root_y = main_window->y;
   }
else
   {
      root_x = main_window->x + target_window->x;
      root_y = main_window->y + target_window->y;
   }

DEBUG10(
fprintf(stderr, "root corner (%d,%d) root target (%d,%d) display (%d,%d) window (%d,%d)\n", root_x, root_y, root_x + *x, root_y + *y, display_width, display_height, *x, *y);
)

if (root_x + *x >= display_width)
   {
      *x = (display_width - root_x) - 3;
      bad = True;
   }
else
   if (root_x + *x < 0)
      {
         *x = (root_x * -1) + 3;
         bad = True;
      }

if (root_y + *y >= display_height)
   {
      *y = (display_height - root_y) - 3;
      bad = True;
   }
else
   if (root_y + *y < 0)
      {
         *y = (root_y * -1) + 3;
         bad = True;
      }

DEBUG10(if (bad) fprintf(stderr, "Off screen warp adjusted new root (%d,%d) warp ot (%d,%d)\n", root_x + *x, root_y + *y, *x, *y);)

return(bad);

} /* end of off_screen_warp */


/************************************************************************

NAME:      y_in_winline_check      - Verify that a warp is to the window line it is supposed to be.


PURPOSE:    This routine checks to make sure the y value passed is in the
            window line it is supposed to be.

PARAMETERS:

   1.  dspl_descr  - pointer to DISPLAY_DESCR  (INPUT)
                     This is the current display description

   2.  x_window    - Window (Xlib type) (INPUT)
                     This is the x window to be warped into.

   3.  sub_window  - pointer to DRAWABLE_DESCR (INPUT)
                     This is the subwindow to be warped into.

   4.  win_line_no - pointer to int (INPUT / OUTPUT)
                     This is the window line number (zero based) to which we
                     want to warp.  If it is off screen, it is adjusted.

   5.  y           - pointer to int (INPUT / OUTPUT)
                     This is where we are currently planning to warp.  This
                     value is checked and possibly modified.

FUNCTIONS :

   1.   Calculate the top and bottom of the line.

   2.   Make sure the y value is in the correct area.
        If not, adjust it.
 

*************************************************************************/

static void y_in_winline_check(DISPLAY_DESCR      *dspl_descr,
                               Window              x_window,
                               DRAWABLE_DESCR     *window,
                               int                *win_line_no,
                               int                *y)
{
int                   top;
int                   bottom;
int                   new_y;

if (*win_line_no >= window->lines_on_screen)
   {
      DEBUG10(fprintf(stderr, "Adjusting line %d to %d in %s\n", *win_line_no, window->lines_on_screen-1, which_window_names[window_to_buff_descr(dspl_descr, x_window)->which_window]);)
      *win_line_no = window->lines_on_screen - 1;
   }
else
   if (*win_line_no < 0)
      {
         DEBUG10(fprintf(stderr, "Adjusting line %d to 0 in %s\n", *win_line_no, which_window_names[window_to_buff_descr(dspl_descr, x_window)->which_window]);)
         *win_line_no = 0;
      }

top    = (window->line_height * (*win_line_no)) + window->sub_y;
bottom = (window->line_height * ((*win_line_no) + 1)) + window->sub_y;

if (*y < top || *y >= bottom)
   {
      new_y = top + (window->line_height >> 1);
      DEBUG10(
         fprintf(stderr, "Adjusting line %d, Y from %d to %d in %s  (top %d, bot %d)\n", *win_line_no, *y, new_y,
                 which_window_names[window_to_buff_descr(dspl_descr, x_window)->which_window], top, bottom);
      )
      *y = new_y;
   }

} /* end of y_in_winline_check */


/************************************************************************

NAME:      really_moved - Check to see if the cursor really moved

PURPOSE:    This routine compares the start location and end location
            values in the cursor buff to determine if the cursor really
            moved.  Sometimes we want to redraw the cursor, but the cursor
            did not really move.  This gets around problems in some X  
            servers, where if you warp the cursor to where it is already,
            it either does not send a motion event or it moves to a 
            location other than the one specfied.
            This routine added by RES 3/30/95 - resolving problem from Boeing.
            start locations are set in crpad.c at start of each keystroke.

PARAMETERS:

   1.  cursor_buff    - pointer to BUFF_DESCR (INPUT)
                        This is the buffer position buffer.

FUNCTIONS :

   1.   If the window changed, or the X or Y coordinates changed,
        return True.  Otherwise return False.
 
RETURNED VALUE:
   moved    -   int
                True  -  The cursor really moved
                False -  The cursor did not move

*************************************************************************/

static int  really_moved(BUFF_DESCR *cursor_buff)
{
DEBUG10(
   fprintf(stderr, "start: (%s,%d,%d), current (%s,%d,%d)\n",
                   which_window_names[cursor_buff->start_which_window], cursor_buff->start_x, cursor_buff->start_y,
                   which_window_names[cursor_buff->which_window], cursor_buff->x, cursor_buff->y);
)

if ((cursor_buff->which_window == cursor_buff->start_which_window) &&
    (cursor_buff->x            == cursor_buff->start_x) &&
    (cursor_buff->y            == cursor_buff->start_y))
   return(False);
else
   return(True);

} /* end of really_moved */


/************************************************************************

NAME:      highlight_setup

PURPOSE:    This routine interfaces between the actual start_text_highlight
            routine in txcursor.c and the rest of Ce.  start_text_highlight 
            works with window coordinates.  The MARK_REGION set by the dr
            command works with file coordinates.  This routine performs this
            translation.

PARAMETERS:

   1.  redraw_needed  - pointer to MARK_REGION (INPUT)
                        This is the mark which is set by dr.

   2.  echo_mode      - input (INPUT)
                        If the program is in echo (highlight text) mode,
                        this flag is non-zero.  The flag actually has 3 values.
                        NO_HIGHLIGHT, REGULAR_HIGHLIGHT, and RECTANGULAR_HIGHLIGHT
                        defined in txcursor.h

FUNCTIONS :

   1.   Get the for the mark and convert the file row col to the window row col.

   2.   Adjust the window row and column to compensate for the mark being
        currently off window.  (set a mark and then scroll).

   3.   Call start_text_highlight to get the highlight going.
 

*************************************************************************/

void  highlight_setup(MARK_REGION    *mark1,
                      int             echo_mode)
{
int                   window_line;
int                   window_col;
char                 *line;
char                 *p;
int                   mark_off_top_of_screen;
int                   mark_off_bottom_of_screen;

/***************************************************************
*  
*  Translate the file line number where the dr (mark) was placed
*  to a window line number.  It is possible that this line
*  has been scrolled off the top of the screen creating a negative
*  window line number.  
*  
***************************************************************/

line = get_line_by_num(mark1->buffer->token, mark1->file_line_no);

window_line = mark1->file_line_no  - mark1->buffer->first_line;
if (window_line < 0)
   {
      window_line = 0;
      mark_off_top_of_screen = True;
      mark_off_bottom_of_screen = False;
   }
else
   {
      mark_off_top_of_screen = False;
      if (window_line >= mark1->buffer->window->lines_on_screen)
         {
            window_line = mark1->buffer->window->lines_on_screen - 1;
            mark_off_bottom_of_screen = True;
         }
      else
         mark_off_bottom_of_screen = False;
   }

/***************************************************************
*  
*  Convert the file column number from the mark to a window column number.
*  If the line is off screen, we will not see it, so we can ignore
*  tabs on that line.
*  
***************************************************************/

window_col = tab_cursor_adjust(line,
                               tab_pos(line, mark1->col_no, TAB_EXPAND, mark1->buffer->display_data->hex_mode)-mark1->buffer->first_char,
                               mark1->buffer->first_char, 
                               TAB_WINDOW_COL,
                               mark1->buffer->display_data->hex_mode);

DEBUG10(fprintf(stderr, "@highlight_setup file :[%d,%d], win: [%d,%d] in %s\n", mark1->file_line_no, mark1->col_no, window_line, window_col, which_window_names[mark1->buffer->which_window]);)

/***************************************************************
*  
*  The main pad is the only window with multiple lines, so it
*   is the only one we need to worry about.
*
*  If the marked line is off the top of the screen, we can assume
*  window line 0 col 0 unless this is a rectangular highlight.
*  In that case we have to worry about the mark.  The window column
*  number may go negative for rectangular highlights.  This situation
*  is handled in start_text_highlight because null lines are highlighted
*  differently depending upon where the mark is and where the 
*  window has been scrolled to with respect to right and left.
*  The negative column number is a flag that says a rectangular 
*  highlight mark has been scrolled off the left side of the screen.
*  
***************************************************************/

if (mark1->buffer->which_window == MAIN_PAD)
   {
      if (mark_off_top_of_screen)
         {
            if (window_col > 0 && echo_mode != RECTANGULAR_HIGHLIGHT) /* negative value needed for rectangular setup */
               window_col  = 0;
         }
      else
         if (mark_off_bottom_of_screen)
            {
               if (echo_mode != RECTANGULAR_HIGHLIGHT) /* possible negative value needed for rectangular setup */
                  {
                     p = mark1->buffer->win_lines[window_line].line;
                     if (p)
                        window_col  = (strlen(p) - 1) - mark1->buffer->first_char;
                     else
                        window_col  = 0;
                  }
            }

   }

start_text_highlight(mark1->buffer->x_window,
                     mark1->buffer->window,
                     window_line,
                     window_col,
                     echo_mode,
                     mark1->buffer->win_lines,
                     line,
                     mark1->buffer->first_char,
                     mark1->buffer->display_data);

} /* end of highlight_setup */



/************************************************************************

NAME:      redo_cursor - Generate a warp to reposition the cursor after a window size change.

PURPOSE:    This routine is used to rebuild the cursor when expose events occur.,

PARAMETERS:

   1.  dspl_descr      - pointer to DISPLAY_DESCR (INPUT)
                         This is the current display description we are drawing to.

   2.  force_fake_warp - int (INPUT)
                         This flag, if true forces a fake warp and avoids a
                         real warp.

FUNCTIONS :

   1.   Get rid of the current cursor.

   2.   If we are not already expecting a warp, generate one.


*************************************************************************/

void redo_cursor(DISPLAY_DESCR  *dspl_descr,
                 int             force_fake_warp)
{

int                   x;
int                   y;
int                   warp_changed;
int                   warp_bad;

clear_text_cursor(None, dspl_descr);

/* snuff check added for hold mode cursor jumps RES 9/27/95 */
if (!dspl_descr->cursor_buff->up_to_snuff)
   bring_up_to_snuff(dspl_descr);

if (!expecting_warp(dspl_descr)) /* in getevent.c */
   {
      if (dspl_descr->cursor_buff->x >= (int)dspl_descr->cursor_buff->current_win_buff->window->sub_width+dspl_descr->cursor_buff->current_win_buff->window->sub_x)
         x = (dspl_descr->cursor_buff->current_win_buff->window->sub_width+dspl_descr->cursor_buff->current_win_buff->window->sub_x) - 2;
      else
         x = dspl_descr->cursor_buff->x;

      if (dspl_descr->cursor_buff->y >= (int)dspl_descr->cursor_buff->current_win_buff->window->height)
         y = dspl_descr->cursor_buff->current_win_buff->window->height - 2;
      else
         y = dspl_descr->cursor_buff->y;

      y_in_winline_check(dspl_descr,
                         dspl_descr->cursor_buff->current_win_buff->x_window,
                         dspl_descr->cursor_buff->current_win_buff->window,
                         &dspl_descr->cursor_buff->win_line_no,
                         &y);

      DEBUG10(fprintf(stderr, "@redo_cursor: warping to: [%d,%d] (%d,%d) in %s\n",
                    dspl_descr->cursor_buff->win_line_no, dspl_descr->cursor_buff->win_col_no, x, y, which_window_names[dspl_descr->cursor_buff->which_window]);
      )
      warp_changed = off_screen_warp_check(dspl_descr->display, dspl_descr->main_pad->window, dspl_descr->cursor_buff->current_win_buff->window, &x, &y);
      if (warp_changed)
         if (position_in_window(dspl_descr->cursor_buff->x, dspl_descr->cursor_buff->y, dspl_descr->cursor_buff->current_win_buff->window))
            warp_bad = False;
         else
            warp_bad = True;
      else
         warp_bad = False;
      DEBUG9(XERRORPOS)
      if (MOUSEON && !warp_bad && !force_fake_warp && !((dspl_descr->vt100_mode) && (dspl_descr->cursor_buff->which_window == MAIN_PAD)))
         XWarpPointer(dspl_descr->display, None, dspl_descr->cursor_buff->current_win_buff->x_window,
                      0, 0, 0, 0,
                      x, y);
      else
         fake_warp_pointer(dspl_descr->cursor_buff, dspl_descr->main_pad, x, y); 


      /***************************************************************
      *  
      *  Set the global data within getevent.c to show we are expecting
      *  a warp.
      *  
      ***************************************************************/

      expect_warp(x, y, dspl_descr->cursor_buff->current_win_buff->x_window, dspl_descr);
   }
else
   {
      DEBUG10(fprintf(stderr, "@redo_cursor: already expecting warp\n");)
   }

} /* end of redo_cursor */




/************************************************************************

NAME:      fake_warp_pointer - Substitute for XWarpPointer when mouse flow is off.

PURPOSE:    This routine is used to send an enter notifiy in place of the warp pointer
            when mouse tracking is off.

PARAMETERS:

   1.  cursor_buff    - pointer to BUFF_DESCR (INPUT)
                        This is the current cursor description.

   2.  main_window    - pointer to PAD_DESCR (INPUT)
                        This is the PAD description for the main window.
                        It is needed to determine root coordinates.

FUNCTIONS :

   1.   Fabricate an EnterNotify event.

   2.   Send it to ourselves.


*************************************************************************/

static void fake_warp_pointer(BUFF_DESCR    *cursor_buff,
                              PAD_DESCR     *main_window,
                              int            x,
                              int            y)
{

XEvent                event;

memset((char *)&event, 0, sizeof(XEvent));

event.xcrossing.type  = EnterNotify;
event.xcrossing.serial  = 2;
event.xcrossing.send_event = True;
event.xcrossing.display    = cursor_buff->current_win_buff->display_data->display;
event.xcrossing.window     = cursor_buff->current_win_buff->x_window;
event.xcrossing.root       = RootWindow(cursor_buff->current_win_buff->display_data->display, DefaultScreen(cursor_buff->current_win_buff->display_data->display));
event.xcrossing.subwindow  = None;
event.xcrossing.time       = 0;
event.xcrossing.x          = x;
event.xcrossing.y          = y;
event.xcrossing.x_root     = x + main_window->window->x + ((cursor_buff->which_window != MAIN_PAD) ? cursor_buff->current_win_buff->window->x : 0);
event.xcrossing.y_root     = y + main_window->window->y + ((cursor_buff->which_window != MAIN_PAD) ? cursor_buff->current_win_buff->window->y : 0);
event.xcrossing.mode       = NotifyNormal;
event.xcrossing.detail     = 0;
event.xcrossing.focus      = True;
event.xcrossing.state      = 0;

DEBUG10(fprintf(stderr, "Fake warp via sent enternotify\n");)
DEBUG9(XERRORPOS)
ce_XSendEvent(event.xcrossing.display, event.xcrossing.window, True, EnterWindowMask, &event);

} /* end of fake_warp_pointer */


/************************************************************************

NAME:      fake_cursor_motion - fudge up a motion event

PURPOSE:    This routine is used to show the cursor is active when
            a focusin event occurs.

PARAMETERS:

   1.  dspl_descr     - pointer to DISPLAY_DESCR (INPUT)
                        This is the current display description we are drawing to.
                        It is needed for the cursor buffer information and the
                        call to text_cursor_motion.

FUNCTIONS :

   1.   Build a fake enter event

   2.   Give the event to text_cursor_motion


*************************************************************************/

void fake_cursor_motion(DISPLAY_DESCR   *dspl_descr)
{
XEvent                  fake_enter;

memset((char *)&fake_enter, 0, sizeof(fake_enter));
fake_enter.xcrossing.type       = EnterNotify;
fake_enter.xcrossing.send_event = True;
fake_enter.xcrossing.display    = dspl_descr->display;
fake_enter.xcrossing.window     = dspl_descr->cursor_buff->current_win_buff->x_window;
fake_enter.xcrossing.x          = dspl_descr->cursor_buff->x;
fake_enter.xcrossing.y          = dspl_descr->cursor_buff->y;
fake_enter.xcrossing.mode       = NotifyNormal;
fake_enter.xcrossing.focus      = True;

text_cursor_motion(dspl_descr->cursor_buff->current_win_buff->window,     /* input  */
                   dspl_descr->echo_mode,                                 /* input  */
                   &fake_enter,                                           /* input  */
                   &dspl_descr->cursor_buff->win_line_no,                 /* output */
                   &dspl_descr->cursor_buff->win_col_no,                  /* output */
                   dspl_descr->cursor_buff->current_win_buff->win_lines,  /* input  */
                   dspl_descr);                                           /* input  */

} /* end of fake_cursor_motion */


/************************************************************************

NAME:      position_in_window - Check if requested x,y position in the window

PURPOSE:    This routine is used to test if the coordinates returned by
            off_screen_warp_check are within the window.  Routine off_screen_warp_check
            modifies a target warp position if it is off the physical 
            screen.  In a virtual window environment, all our window
            may be off screen, in which case it moves the cursor out
            of our window.  In that case, we do not want to do a real warp.

PARAMETERS:

   1.  x             -  int (INPUT)
                        This is the target x coordinate to be tested.
                        It is relative to the passed window.

   2.  y             -  int (INPUT)
                        This is the target y coordinate to be tested.
                        It is relative to the passed window.

   3.  window        -  int (INPUT)
                        This is description of the window the coordinates
                        are issued against.  It contains the dimensions of
                        the window.

FUNCTIONS :

   1.   Test the passed coordinates against the boundaries of the window.


RETURNED VALUE:
   in_window    -  int flag
                   True   -  The passed coordinates are within the window
                   False  -  The passed coordinates are NOT within the window


*************************************************************************/

static int position_in_window(int              x,
                              int              y,
                              DRAWABLE_DESCR  *window)
{
int       in_window;

if ((x < 0) || (x >= (int)window->width))
   in_window = False;
else
   if ((y < 0) || (y >= (int)window->height))
      in_window = False;
   else
      in_window = True;

DEBUG10(if (!in_window) fprintf(stderr, "position_in_window: target position (%d,%d) is NOT in window\n", x, y);)

return(in_window);
      
} /* end of position_in_window */

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



