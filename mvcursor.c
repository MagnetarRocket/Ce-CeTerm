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
*  module mvcursor.c
*
*  Routines:
*     dm_pv       -   Move pad vertically
*     dm_ph       -   Move Pad Horizontally
*     dm_pt       -   Scroll to top of pad
*     dm_tt       -   Put cursor on first line in window
*     dm_pb       -   Scroll to bottom of the pad
*     pad_to_bottom - Move a pad to the bottom
*     pb_ff_scan  -   Scan last lines in pad for a form feed.
*     ff_scan     -   Scan forward for a form feed
*     dm_tb       -   Put cursor on bottom line in window
*     dm_tr       -   Put cursor on right most char on line
*     dm_tl       -   Put cursor on left most char on line
*     dm_au       -   Move cursor up one line
*     dm_ad       -   Move cursor down one line
*     dm_ar       -   Move cursor right one character
*     dm_al       -   Move cursor left one character
*     dm_num      -   Move cursor to the requested file line number
*     dm_corner   -   Position the window upper left corner over the file
*     dm_twb      -   Move cursor to the requested window border
*     dm_sic      -   Set Insert Cursor for Mouse Off mode
*     dm_tn       -   tn (to next) and ti (to next input) window commands
*     dm_wh       -   Take window into and out of hold mode
*     dm_position -   Position the cursor to a given file row and col
*
*     After return from these routines. A screen redraw may
*     be required, and or a warp cursor.
*
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h      */

#include "cc.h"
#include "debug.h"
#include "dmc.h"
#include "dmsyms.h"
#include "dmfind.h"
#include "dmwin.h"
#include "drawable.h"
#include "getevent.h"
#include "mark.h"
#include "memdata.h"
#include "mvcursor.h"
#include "parsedm.h"
#include "tab.h"
#include "txcursor.h"
#include "typing.h"
#include "undo.h"
#ifdef PAD
#include "unixpad.h"
#include "unixwin.h"
#endif
#include "window.h"
#include "xerrorpos.h"


/************************************************************************

NAME:      dm_pv  -  do pv and pp commands

PURPOSE:    This routine will scroll the pad up or down.

PARAMETERS:

   1.  cursor_buff   - pointer to BUFF_DESCR (INPUT / OUTPUT)
               This is a pointer to the buffer / window description
               showing the current state of the window.

   2.  dmc   - pointer to DMC (INPUT)
               This is a pointer to the DM command structure
               for a DMC_pp or DMC_pv command.

   3.  warp_needed - pointer to int (INPUT/OUTPUT)
               This flag is turned on if a warp is needed.
               Otherwise it is left alone.


FUNCTIONS :

   1.   Calculate the number of lines to move.  For pv commands,
        this is the passed number.  For pp commands, this is the
        number of pages (float number) times the number of lines
        on the window.

   2.   If we are scrolling up and are at the top, do nothing.

   3.   Calculate the new first line on the screen and set it.

   4.   If the old character under the cursor is still on the
        screen, track it with the cursor.  Otherwise leave the
        cursor where it is.

RETURNED VALUE:
   redraw -  If the screen needs to be redrawn, the mask of which screen
             and what type of redrawing is returned.

NOTES:
   1.  This routine moves the pad in addition to moving the cursor, so it
       modifies the current window buffer first_line on screen.

*************************************************************************/

int   dm_pv(BUFF_DESCR   *cursor_buff,
            DMC          *dmc,
            int          *warp_needed)
{
PAD_DESCR         *target_buffer;
int                delta_lines;
char              *line;
int                i;
int                form_feed_processed = False;
int                redraw_needed = 0;

target_buffer = cursor_buff->current_win_buff;
if (total_lines(target_buffer->token) == 0)
   {
      DEBUG5(fprintf(stderr, "empty file, no change\n");)
      return(0);
   }

/***************************************************************
*  Scroll by pages or chars. Then figure out what goes on the
*  screen and where the cursor goes.
***************************************************************/
if (dmc->any.cmd == DM_pp)
   {
      delta_lines = (int)(dmc->pp.amount * target_buffer->window->lines_on_screen);
      if (delta_lines == 0)
         if (dmc->pp.amount > 0)
            delta_lines = 1;
         else
            if (dmc->pp.amount < 0)
               delta_lines = -1;
   }
else
   delta_lines = dmc->pv.chars;

DEBUG5(fprintf(stderr, "scroll request of %d lines\n", delta_lines);)

/***************************************************************
*  If we are scrolling up and are at the top, do nothing.
*  If we are at the bottom and are scrolling down, do nothing.
***************************************************************/
if (delta_lines == 0)
   {
      DEBUG5(fprintf(stderr, "scroll of zero lines, no change\n");)
      return(0);
   }
if (target_buffer->first_line == 0 && delta_lines < 0)
   {
      DEBUG5(fprintf(stderr, "pv: scroll up at top, no change\n");)
      ce_XBell(target_buffer->display_data, 0);
      return(0);
   }
else
   if ((target_buffer->first_line == total_lines(target_buffer->token)-1) && (delta_lines > 0))
      {
         DEBUG5(fprintf(stderr, "pv: scroll down at bottom, no change\n");)
         ce_XBell(target_buffer->display_data, 0);
         return(0);
      }

/***************************************************************
*  Make sure enough data has been loaded in case of noreadahead.
*  RES 6/23/95
***************************************************************/
if (load_enough_data(target_buffer->first_line + delta_lines) == False)
   redraw_needed |= target_buffer->redraw_mask & FULL_REDRAW;

/***************************************************************
*  Deal with the special case of form feeds.
***************************************************************/
if (!target_buffer->display_data->hex_mode)
   if ((delta_lines > 0) && (target_buffer->formfeed_in_window))
      {
         /***************************************************************
         *  scroll down 
         *  Make the form feed line the first line.
         ***************************************************************/
         delta_lines = target_buffer->formfeed_in_window;
         DEBUG5(fprintf(stderr, "%s: moving to form feed on window line %d\n", dmsyms[dmc->pv.cmd].name, delta_lines);)
      }
   else
      if ((delta_lines < 0) && (-delta_lines <= target_buffer->window->lines_on_screen))
         {
            if (target_buffer->first_line < (total_lines(target_buffer->token)-1))
               {
                  position_file_pointer(target_buffer->token, target_buffer->first_line+1);
                  line = prev_line(target_buffer->token);
                  if (strchr(line, '\f') != NULL)
                     {
                        /***************************************************************
                        *  scroll up
                        *  The first line contains a form feed, back up so the current
                        *  first line is 1 past the window.
                        ***************************************************************/
                        form_feed_processed = True;
                        delta_lines = 0 - target_buffer->window->lines_on_screen;
                        DEBUG5(fprintf(stderr, "%s: first line has form feed, changing scroll amount to %d\n", dmsyms[dmc->pv.cmd].name, delta_lines);)
                     }
               }
            else
               position_file_pointer(target_buffer->token, target_buffer->first_line);

            /***************************************************************
            *  scroll up
            *  Check the previous lines for form feeds.  If found,
            *  Position the first line there.
            ***************************************************************/
            for (i = -1; i >= delta_lines; i--)  /* remember delta_lines is a negative number */
            {
               line = prev_line(target_buffer->token);
               if (!line)
                  break;
               if (strchr(line, '\f') != NULL)
                  {
                     form_feed_processed = True;
                     delta_lines = i;
                     DEBUG5(fprintf(stderr, "%s: found form feed above window, changing scroll amount to %d\n", dmsyms[dmc->pv.cmd].name, delta_lines);)
                     break;
                  }
            }
         }  /* end of if delta_lines < 0 (scroll up ) */
      

/***************************************************************
*  Adjust the first line shown on the window and then sanity check it.
***************************************************************/
target_buffer->first_line += delta_lines;

if (target_buffer->first_line <= 0)
   {
      delta_lines -= target_buffer->first_line;  /* adjust delta lines to # actually scrolled */
      target_buffer->first_line = 0;
      /***************************************************************
      *  If we scrolled into line zero, check for formfeeds in the first screen.
      ***************************************************************/
      if (!target_buffer->display_data->hex_mode)
         {
            position_file_pointer(target_buffer->token, 1);
            for (i = 1; i < target_buffer->window->lines_on_screen; i++)
            {
               line = next_line(target_buffer->token);
               if ((line) && (strchr(line, '\f') != NULL))
                  {
                     DEBUG5(fprintf(stderr, "%s: Form feed found on first page of data, line %d\n", dmsyms[dmc->pv.cmd].name, i);)
                     target_buffer->formfeed_in_window = i;
                     break;
                  }
               if (!line)
                  break;
            }
         }
   }
else
   if (target_buffer->first_line >= total_lines(target_buffer->token))
       {
          delta_lines -= (target_buffer->first_line - (total_lines(target_buffer->token)-1));
          target_buffer->first_line = total_lines(target_buffer->token) - 1;
       }

/***************************************************************
*  Figure out where the cursor goes.  If the old position scrolled
*  off the top or bottom of the screen, Leave the cursor alone.
*  Otherwise put the cursor on the same char it was on.
***************************************************************/
if (target_buffer->file_line_no >= target_buffer->first_line &&
    target_buffer->file_line_no <= (target_buffer->first_line + (target_buffer->window->lines_on_screen - 1)) )
   {
      /* still on the screen */
      cursor_buff->win_line_no = target_buffer->file_line_no - target_buffer->first_line;
      cursor_buff->y = (cursor_buff->win_line_no * target_buffer->window->line_height) + target_buffer->window->sub_y;
      *warp_needed |= True;
   }
else
   {
      /***************************************************************
      *  If we do not move the cursor, the underlying line number changed.
      ***************************************************************/
      flush(target_buffer);
      target_buffer->file_line_no = target_buffer->first_line + cursor_buff->win_line_no;
      target_buffer->buff_ptr     = get_line_by_num(target_buffer->token, target_buffer->file_line_no);
   }


DEBUG5(fprintf(stderr, "%s: scrolled %d lines, top corner [%d,%d] file [%d,%d] win [%d,%d] %s\n",
       dmsyms[dmc->pv.cmd].name, delta_lines,
       target_buffer->first_line, target_buffer->first_char,
       target_buffer->file_line_no, target_buffer->file_col_no,
       cursor_buff->win_line_no, cursor_buff->win_col_no,
       which_window_names[cursor_buff->which_window]);
)

if ((target_buffer->which_window == MAIN_PAD)
    && !(target_buffer->display_data->echo_mode)
    && !form_feed_processed
    && (target_buffer->redraw_scroll_lines == 0)
    && (target_buffer->redraw_start_line == -1)
    && (*target_buffer->hold_mode == 0)
    && (ABSVALUE(delta_lines) < target_buffer->window->lines_on_screen))
   {
      target_buffer->redraw_scroll_lines = delta_lines;
      redraw_needed |= target_buffer->redraw_mask & SCROLL_REDRAW;
   }
else
   redraw_needed |= target_buffer->redraw_mask & FULL_REDRAW;

return(redraw_needed);

} /* end of dm_pv */


/************************************************************************

NAME:      dm_ph  -  do ph command

PURPOSE:    This routine will scroll the horizontally.

PARAMETERS:

   1.  cursor_buff   - pointer to BUFF_DESCR (INPUT / OUTPUT)
               This is a pointer to the buffer / window description
               showing the current state of the window.

   2.  dmc   - pointer to DMC (INPUT)
               This is a pointer to the DM command structure.

   3.  main_window   - pointer to DRAWABLE_DESCR (INPUT)
               This is a pointer to the drawable description of the
               main window.  This is needed because there are
               processing differences between the main and DM windows.


FUNCTIONS :

   1.   Make sure there is a scroll to do.  If we are already
        as far left as we can go, no more scroll lefts can be done.

   2.   Calculate the new left character from the scroll amount and
        the current position.  This is done in pixels because of
        the possiblity of variable width fonts.

   3.   Adjust the structures for the data.  On the main window, we
        schedule a redraw, on the DM windows, we just do the redraw.


RETURNED VALUE:
   redraw -  The window mask anded with the type of redraw needed is returned.

NOTES:
   1.  This routine moves the pad in addition to moving the cursor, so it
       modifies the current window buffer first_char on screen.

*************************************************************************/

int   dm_ph(BUFF_DESCR       *cursor_buff,
            DMC              *dmc)
{
int                delta_x;
int                scroll_chars;

/***************************************************************
*  Don't move if we scroll left and are at the left side.
***************************************************************/
if (cursor_buff->current_win_buff->first_char == 0 && dmc->ph.chars <= 0)
   {
      if ((cursor_buff->which_window == DMINPUT_WINDOW) && (scroll_dminput_prompt(dmc->ph.chars)))
          {
             set_window_col_from_file_col(cursor_buff); /* input / output */
             return(DMINPUT_MASK & FULL_REDRAW);
          }
      else
#ifdef PAD
         if ((cursor_buff->which_window == UNIXCMD_WINDOW) && (scroll_unix_prompt(dmc->ph.chars)))
             {
                set_window_col_from_file_col(cursor_buff); /* input / output */
                return(UNIXCMD_MASK & FULL_REDRAW);
             }
         else
            return(0);
#else
         return(0);
#endif
   }

/***************************************************************
*  copy the chars to scroll to a local variable in case it gets
*  modified.
***************************************************************/
scroll_chars = dmc->ph.chars;

/***************************************************************
*  Check for the DM input window.  Scrolls to the right may 
*  get eaten up by command input window scrolling.
***************************************************************/
if ((cursor_buff->which_window == DMINPUT_WINDOW) && (scroll_chars > 0))
   {
      scroll_chars -= scroll_dminput_prompt(scroll_chars);
      if (scroll_chars <= 0)
          {
             set_window_col_from_file_col(cursor_buff); /* input / output */
             return(DMINPUT_MASK & FULL_REDRAW);
          }
   }

#ifdef PAD
/***************************************************************
*  Check for the Unix command window.  Scrolls to the right may 
*  get eaten up by command input window scrolling.
***************************************************************/
if ((cursor_buff->which_window == UNIXCMD_WINDOW) && (scroll_chars > 0))
   {
      scroll_chars -= scroll_unix_prompt(scroll_chars);
      if (scroll_chars <= 0)
          {
             set_window_col_from_file_col(cursor_buff); /* input / output */
             return(UNIXCMD_MASK & FULL_REDRAW);
          }
   }
#endif

/***************************************************************
*  Calculate the change in first character position displayed.
***************************************************************/
cursor_buff->current_win_buff->first_char += scroll_chars;
if (cursor_buff->current_win_buff->first_char > MAX_LINE)
   cursor_buff->current_win_buff->first_char = MAX_LINE;


/***************************************************************
*  Calculate the change in pixels
***************************************************************/
if (cursor_buff->current_win_buff->window->fixed_font)
   delta_x = cursor_buff->current_win_buff->window->fixed_font * scroll_chars;
else
   {
      delta_x = XTextWidth(cursor_buff->current_win_buff->window->font,
                           &cursor_buff->current_win_buff->buff_ptr[cursor_buff->current_win_buff->file_col_no],
                           ABSVALUE(scroll_chars));
      if (scroll_chars < 0)
         delta_x *= -1;
   }


/***************************************************************
*  Don't scroll too far to the left.
***************************************************************/
if (cursor_buff->current_win_buff->first_char < 0)
   {
      if (cursor_buff->which_window == DMINPUT_WINDOW)
         scroll_dminput_prompt(cursor_buff->current_win_buff->first_char);
#ifdef PAD
      else
         if (cursor_buff->which_window == UNIXCMD_WINDOW)
            scroll_unix_prompt(cursor_buff->current_win_buff->first_char);
#endif
      cursor_buff->current_win_buff->first_char = 0;
   }

   
/***************************************************************
*  Figure out where the cursor goes.  If the old position scrolled
*  off the left or right of the screen, leave the cursor alone.
*  Otherwise put the cursor on the same char it was on.
***************************************************************/

if ((cursor_buff->x - delta_x) >= 0 &&
    (cursor_buff->x - delta_x) < (int)(cursor_buff->current_win_buff->window->sub_width + cursor_buff->current_win_buff->window->sub_x))
   {
      /* still on screen */
      set_window_col_from_file_col(cursor_buff); /* input / output */
   }


DEBUG5(fprintf(stderr, "ph: top corner [%d,%d] file [%d,%d] win [%d,%d], %s\n",
       cursor_buff->current_win_buff->first_line, cursor_buff->current_win_buff->first_char,
       cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no,
       cursor_buff->win_line_no, cursor_buff->win_col_no,
       which_window_names[cursor_buff->which_window]);
)

return(cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);

} /* end of dm_ph */


/************************************************************************

NAME:      dm_pt  -  DM pt (Pad Top) command

PURPOSE:    This routine will move the first line of the screen
            to the top of the pad.

PARAMETERS:

   1.  cursor_buff   - pointer to BUFF_DESCR (INPUT / OUTPUT)
               This is a pointer to the buffer / window description
               showing the current state of the window.


FUNCTIONS :

   1.   If we are already at the top of the pad, do nothing.

   2.   Set the pointers to show we are at the first line.


RETURNED VALUE:
   redraw -  The window mask anded with the type of redraw needed is returned.

NOTES:
   1.  This routine moves the pad in addition to moving the cursor, so it
       modifies the current window buffer first_line on screen.

*************************************************************************/

int   dm_pt(BUFF_DESCR       *cursor_buff)
{

/***************************************************************
*  
*  Make sure we are on top.
*  
***************************************************************/

if (window_is_obscured(cursor_buff->current_win_buff->display_data)) /* in getevent.c */
   {
      DEBUG9(XERRORPOS)
      XRaiseWindow(cursor_buff->current_win_buff->display_data->display, cursor_buff->current_win_buff->x_window);
   }

/***************************************************************
*  If we are at the top already, do nothing
***************************************************************/
if (cursor_buff->current_win_buff->first_line == 0)
   return(0);

/***************************************************************
*  Adjust the first line on the pad to be the first line.
*  Also reflect this in the current window.
***************************************************************/
cursor_buff->current_win_buff->first_line = 0;

if (cursor_buff->current_win_buff->file_line_no >= cursor_buff->current_win_buff->window->lines_on_screen)
   {
      flush(cursor_buff->current_win_buff); /* added 1/16/93 */
      cursor_buff->current_win_buff->file_line_no = 0; /* added 11/16/93 */
      cursor_buff->win_line_no = 0;
      cursor_buff->y = cursor_buff->current_win_buff->window->sub_y + 1;
      cursor_buff->current_win_buff->buff_ptr  = get_line_by_num(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no);
   }

DEBUG5(fprintf(stderr, "pt: top corner [%d,%d] file [%d,%d] win [%d,%d], %s\n",
       cursor_buff->current_win_buff->first_line, cursor_buff->current_win_buff->first_char,
       cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no,
       cursor_buff->win_line_no, cursor_buff->win_col_no,
       which_window_names[cursor_buff->which_window]);
)

return(cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);

} /* end of dm_pt */


/************************************************************************

NAME:      dm_tt  -  DM tt (To Top) command

PURPOSE:    This routine will move the cursor to the first line
            on the window.

PARAMETERS:

   1.  cursor_buff   - pointer to BUFF_DESCR (INPUT / OUTPUT)
               This is a pointer to the buffer / window description
               showing the current state of the window.


FUNCTIONS :

   1.   If we are already at the top of the window, do nothing.

   2.   Set the pointers to show we are at the first line.


RETURNED VALUE:
   warp -  If the cursor needs to be redrawn, true is returned,
             Otherwise 0 is returned.
        

*************************************************************************/

int   dm_tt(BUFF_DESCR       *cursor_buff)
{


/***************************************************************
*  
*  Make sure we are on top.
*  
***************************************************************/

if (window_is_obscured(cursor_buff->current_win_buff->display_data)) /* in getevent.c */
   {
      DEBUG9(XERRORPOS)
      XRaiseWindow(cursor_buff->current_win_buff->display_data->display, cursor_buff->current_win_buff->x_window);
   }

/***************************************************************
*  If we are already in the right place, we are done.
***************************************************************/

if (cursor_buff->win_line_no == 0)
   return(False);

/***************************************************************
*  Adjust the cursor to be the first line in the window.
***************************************************************/
cursor_buff->current_win_buff->file_line_no = cursor_buff->current_win_buff->first_line;
cursor_buff->current_win_buff->buff_ptr     = get_line_by_num(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no);

cursor_buff->win_line_no  = 0;

cursor_buff->y            = cursor_buff->current_win_buff->window->sub_y + 1;

set_window_col_from_file_col(cursor_buff); /* input / output */

DEBUG5(fprintf(stderr, "tt: top corner [%d,%d] file [%d,%d] win [%d,%d], %s  \"%s\"\n",
       cursor_buff->current_win_buff->first_line, cursor_buff->current_win_buff->first_char,
       cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no,
       cursor_buff->win_line_no, cursor_buff->win_col_no,
       which_window_names[cursor_buff->which_window], cursor_buff->current_win_buff->buff_ptr);
)

return(True);

} /* end of dm_tt */



/************************************************************************

NAME:      dm_pb  -  DM pb (Pad Bottom) command

PURPOSE:    This routine will move the first line of the window
            so that the bottom line of the pad is the bottom
            line of the window.

PARAMETERS:

   1.  cursor_buff   - pointer to BUFF_DESCR (INPUT / OUTPUT)
               This is a pointer to the buffer / window description
               showing the current state of the window.


FUNCTIONS :

   1.   If we are already at the correct spot, do nothing.

   2.   Set the pointers to show we are at the bottom of the pad

   3.   If the cursor_buff line is no longer on the screen, set it to
        the last line of the file.


RETURNED VALUE:
   redraw_needed -  int
          The window mask anded with the type of redraw needed is returned.

NOTES:
   1.  This routine moves the pad in addition to moving the cursor, so it
       modifies the current window buffer first_line on screen.

   2.  Do not modify the window row and col in this routine.

*************************************************************************/

int   dm_pb(BUFF_DESCR       *cursor_buff)
{

int                redraw_needed;

/***************************************************************
*  
*  Make sure we are on top of the stack of windows.
*  
***************************************************************/

if (window_is_obscured(cursor_buff->current_win_buff->display_data)) /* in getevent.c */
   {
      DEBUG9(XERRORPOS)
      XRaiseWindow(cursor_buff->current_win_buff->display_data->display, cursor_buff->current_win_buff->x_window);
   }

/***************************************************************
*  Position the pad.
***************************************************************/
redraw_needed = pad_to_bottom(cursor_buff->current_win_buff);
if (!redraw_needed)
   return(redraw_needed);

/***************************************************************
*  If the current line is no longer on the screen, reset it.
***************************************************************/
if (cursor_buff->current_win_buff->file_line_no < cursor_buff->current_win_buff->first_line)
   {
      if (cursor_buff->current_win_buff->buff_modified)
         flush(cursor_buff->current_win_buff);
      cursor_buff->current_win_buff->file_line_no = total_lines(cursor_buff->current_win_buff->token) - 1;
      cursor_buff->current_win_buff->buff_ptr = get_line_by_num(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no);
      set_window_col_from_file_col(cursor_buff); /* input / output */
   }


DEBUG5(fprintf(stderr, "pb: top corner [%d,%d] file [%d,%d] win [%d,%d], (%d,%d), %s  \"%s\"\n",
       cursor_buff->current_win_buff->first_line, cursor_buff->current_win_buff->first_char,
       cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no,
       cursor_buff->win_line_no, cursor_buff->win_col_no,
       cursor_buff->x, cursor_buff->y,
       which_window_names[cursor_buff->which_window], cursor_buff->current_win_buff->buff_ptr);
)

return(cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);

} /* end of dm_pb */


/************************************************************************

NAME:      pad_to_bottom  -  move a pad to the bottom

PURPOSE:    This routine will move the first line of a pad
            so that the bottom line of the pad is the bottom
            line of the window.

PARAMETERS:

   1.  pad   - pointer to PAD_DESCR (INPUT / OUTPUT)
               This is a pointer to the pad description to be
               modified.


FUNCTIONS :

   1.   If we are already at the correct spot, do nothing.

   2.   Set the pointers to show we are at the correct place.


RETURNED VALUE:
   redraw -  The window mask anded with the type of redraw needed is returned.

NOTES:
   1.  This routine moves the pad in addition to moving the cursor, so it
       modifies the current window buffer first_line on screen.

*************************************************************************/

int   pad_to_bottom(PAD_DESCR        *pad)
{

int                new_row;

/***************************************************************
*  If we are at the right spot already, do nothing
***************************************************************/
new_row = total_lines(pad->token) - pad->window->lines_on_screen;
if (new_row < 0)
   new_row = 0;

/***************************************************************
*  For non-hex mode, check for form feeds in the last screen
***************************************************************/
pad->formfeed_in_window = 0;
new_row = pb_ff_scan(pad, new_row);

if (pad->first_line == new_row)
   return(0);

#ifdef PAD
/***************************************************************
*  If this is the unix command window, we may need to delete lines and
*  make some commands to pass to the shell.
***************************************************************/
if (pad->which_window == UNIXCMD_WINDOW && (new_row != 0) && !(*pad->hold_mode))
   {
      /* RES 8/9/95 event_do added to allow undo after paste to unixwin followed by pb */
      event_do(pad->token, KS_EVENT, 0, 0, 0, 0);
      DEBUG22(fprintf(stderr, "pad_to_bottom: Want to send %d lines to the shell\n", new_row);)
      new_row -= send_to_shell(new_row, pad);
      DEBUG22(fprintf(stderr, "pad_to_bottom: %d left to send\n", new_row);)
   }
#endif

/***************************************************************
*  Adjust the last line on the pad to be the last line in the window.
*  Also reflect this in the current window.
***************************************************************/
pad->first_line = new_row;

DEBUG5(fprintf(stderr, "pad_to_bottom: top corner [%d,%d] %s\n",
       pad->first_line, pad->first_char,
       which_window_names[pad->which_window]);
)

return(pad->redraw_mask & FULL_REDRAW);

} /* end of pad_to_bottom */


/************************************************************************

NAME:       pb_ff_scan  -   Scan last lines in pad for a form feed.

PURPOSE:    This routine scans from the bottom of a pad up to the
            passed file line number to see if there are any form feeds.
            If there are, the line number of the first formfeed found 
            returned.

PARAMETERS:

   1.  pad   - pointer to PAD_DESCR (INPUT)
               This is a pointer to the pad to be scanned.

   2.  new_row   - pointer to int (INPUT)
               This is the file line number to scan up to.
               This is usually a new candidate for the pad's
               first_line field.

FUNCTIONS :

   1.   If we in hex mode, do no scanning.

   2.   Scan backwards looking for a line with a form feed.

   3.   If a form feed is found, assigne it to new_row

   4.   Return the new row if changed and the original row if
        not changed.


RETURNED VALUE:
   new_row -  The file line number with the form feed on it or the passed
              new_row parm is returned. 

*************************************************************************/

int   pb_ff_scan(PAD_DESCR        *pad,
                 int               new_row)
{
char              *line;
int                i;

/***************************************************************
*  For non-hex mode, check for form feeds in the last screen
***************************************************************/
if (!pad->display_data->hex_mode)
   {
      position_file_pointer(pad->token, total_lines(pad->token));

      /***************************************************************
      *  scroll up
      *  Check the previous lines for form feeds.  If found,
      *  Position the first line there.
      ***************************************************************/
      for (i = total_lines(pad->token)-1; i > new_row; i--)
      {
         line = prev_line(pad->token);
         if (!line)
            break;
         if (strchr(line, '\f') != NULL)
            {
               DEBUG5(fprintf(stderr, "(pb) found form feed window, changing first line from %d to %d\n", new_row, i);)
               new_row = i;
               break;
            }
      }
   }

return(new_row);

} /* end of pb_ff_scan */


/************************************************************************

NAME:       ff_scan  -  Scan forward for a form feed

PURPOSE:    This routine scans from the passed position in a pad forward
            to find a form feed.  It sets the formfeed_in_window
            in the pad.

PARAMETERS:

   1.  pad         - pointer to PAD_DESCR (INPUT)
                     This is a pointer to the pad to be scanned.

   2.  first_row   - pointer to int (INPUT)
                     This is the file line number to start the 
                     scan at.

FUNCTIONS :

   1.   Position to read starting at the line after the
        form feed.  Form feeds on the first line on the
        window are ignored.

   2.   Scan forwards looking for a line with a form feed on it.

   3.   If a form feed is found, assign it to the formfeed_in_window
        field int he pad and stop scanning.

   4.   Quit when the number of lines on the screen is reached.

*************************************************************************/

void ff_scan(PAD_DESCR        *pad,
             int               first_row)
{
char              *line;
int                i;

pad->formfeed_in_window = 0;
position_file_pointer(pad->token, first_row+1); /* ff on first row is ignored */
for (i = 1; i < pad->window->lines_on_screen; i++)
{
   line = next_line(pad->token);
   if ((line) && (strchr(line, '\f') != NULL))
      {
         DEBUG5(fprintf(stderr, "ff_scan: Form feed found on file line, line %d\n", first_row+i);)
         pad->formfeed_in_window = i;
         break;
      }
   if (!line)
      break;
}

} /* end of ff_scan */


/************************************************************************

NAME:      dm_tb  -  DM tb (To Bottom) command

PURPOSE:    This routine will move the cursor to the last line
            on the window.

PARAMETERS:

   1.  cursor_buff - pointer to BUFF_DESCR (INPUT / OUTPUT)
                     This is a pointer to the buffer / window description
                     showing the current state of the window.

   2.  main_window_cur_descr   - pointer to PAD_DESCR (INPUT)
               This is a pointer to the to the buffer description of the
               main window.  Needed if we arrow up from the DM window to
               the main.

FUNCTIONS :

   1.   Set the pointers to show we are at the last line.


RETURNED VALUE:
   warp -  If the cursor needs to be redrawn, true is returned,
             Otherwise 0 is returned.
        

*************************************************************************/

int   dm_tb(BUFF_DESCR       *cursor_buff)

{

int                new_row;
int                lines_past_first_line_on_screen;
int                i;
int                lines_sent;
char              *line;

/***************************************************************
*  
*  Make sure we are on top.
*  
***************************************************************/

if (window_is_obscured(cursor_buff->current_win_buff->display_data)) /* in getevent.c */
   {
      DEBUG9(XERRORPOS)
      XRaiseWindow(cursor_buff->current_win_buff->display_data->display, cursor_buff->current_win_buff->x_window);
   }

/***************************************************************
*  Figure out where the bottom of the window is.
***************************************************************/
new_row = cursor_buff->current_win_buff->window->lines_on_screen - 1;

/***************************************************************
*  For non-hex mode, check for form feeds in the screen
***************************************************************/
if (!cursor_buff->current_win_buff->display_data->hex_mode)
   {
      position_file_pointer(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->first_line+1);

      for (i = 1; i < cursor_buff->current_win_buff->window->lines_on_screen; i++)
      {
         line = next_line(cursor_buff->current_win_buff->token);
         if (!line)
            break;
         if (strchr(line, '\f') != NULL)
            {
               DEBUG5(fprintf(stderr, "(tb) found form feed window, changing last line from %d to %d\n", new_row, i);)
               new_row = i;
               break;
            }
      }
   }

/***************************************************************
*  If this is the last screen, partly filled, use the last line.
***************************************************************/
lines_past_first_line_on_screen = total_lines(cursor_buff->current_win_buff->token) - cursor_buff->current_win_buff->first_line;
if (new_row > lines_past_first_line_on_screen)
   new_row = lines_past_first_line_on_screen - 1;

/* check for empty screen */
if (new_row < 0)
   new_row = 0;
   
#ifdef PAD
/***************************************************************
*  If this is the unix command window, we may need to delete lines and
*  make some commands to pass to the shell.
***************************************************************/
if (cursor_buff->which_window == UNIXCMD_WINDOW && (new_row != 0) && !(*cursor_buff->current_win_buff->hold_mode))
   {
      flush(cursor_buff->current_win_buff);
      /* RES 8/9/95 event_do added to allow undo after paste to unixwin followed by tb */
      event_do(cursor_buff->current_win_buff->token, KS_EVENT, 0, 0, 0, 0);
      DEBUG22(fprintf(stderr, "dm_tb: Want to send %d lines to the shell\n", new_row+cursor_buff->current_win_buff->first_line);)
      lines_sent = send_to_shell(new_row+cursor_buff->current_win_buff->first_line, cursor_buff->current_win_buff);
      DEBUG22(fprintf(stderr, "dm_tb: sent %d line(s)\n", lines_sent);)
      cursor_buff->current_win_buff->first_line -= lines_sent;
      if (cursor_buff->current_win_buff->first_line < 0)
         {
            new_row += cursor_buff->current_win_buff->first_line; /* remember first line is currently negative */
            cursor_buff->current_win_buff->first_line = 0;
            if (new_row < 0)
               new_row = 0;
         }
      cursor_buff->current_win_buff->file_line_no -= lines_sent;
      if (cursor_buff->current_win_buff->file_line_no < 0)
         cursor_buff->current_win_buff->file_line_no = 0;
      DEBUG22(fprintf(stderr, "dm_tb: send %d line(s)\n", lines_sent);)
      cursor_buff->current_win_buff->buff_ptr = get_line_by_num(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no);
   }
#endif

/***************************************************************
*  If we are in the right spot, do nothing.
***************************************************************/

/*if (cursor_buff->win_line_no == new_row)
   return(False); */
   
/***************************************************************
*  Move the cursor to the last line
***************************************************************/
cursor_buff->win_line_no = new_row;
cursor_buff->current_win_buff->file_line_no = new_row + cursor_buff->current_win_buff->first_line;
cursor_buff->current_win_buff->buff_ptr  = get_line_by_num(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no);

cursor_buff->y = (cursor_buff->current_win_buff->window->line_height * new_row) + cursor_buff->current_win_buff->window->sub_y + 1;
set_window_col_from_file_col(cursor_buff); /* input / output */

DEBUG5(fprintf(stderr, "tb: top corner [%d,%d] file [%d,%d] win [%d,%d], (%d,%d), %s  \"%s\"\n",
       cursor_buff->current_win_buff->first_line, cursor_buff->current_win_buff->first_char,
       cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no,
       cursor_buff->win_line_no, cursor_buff->win_col_no,
       cursor_buff->x, cursor_buff->y,
       which_window_names[cursor_buff->which_window], cursor_buff->current_win_buff->buff_ptr);
)

return(True);

} /* end of dm_tb */



/************************************************************************

NAME:      dm_tr  -  DM tr (To Right) command

PURPOSE:    This routine will move the cursor to the last char
            on the current line.

PARAMETERS:

   1.  cursor_buff   - pointer to BUFF_DESCR (INPUT / OUTPUT)
               This is a pointer to the buffer / window description
               showing the current state of the window.

FUNCTIONS :

   1.   Scroll the screen sideways to make sure the end of the current
        line is on the screen.

   2.   Set the pointers to show we are at the column on the current line. 


RETURNED VALUE:
   redraw -  The window mask anded with the type of redraw needed is returned.

NOTES:
   1.  This routine moves the pad in addition to moving the cursor, so it
       modifies the current window buffer first_char on screen.

*************************************************************************/

#define CHARS_RIGHT_OF_END  2

int   dm_tr(BUFF_DESCR       *cursor_buff)
{

int                new_x;
int                end_x;
int                len;
int                left_side_x;
int                pix_len;
int                i;
int                redraw_needed = 0;
int                end_col;
char               work[MAX_LINE];
char              *line;
int                tabs;
int                new_first_char;
int                subarea_width;

/***************************************************************
*  We need to work with tabs expanded
*  Everything is done with tabs expanded and then translated back.
***************************************************************/

if ((tabs = untab(cursor_buff->current_win_buff->buff_ptr,
                  work,
                  MAX_LINE+1,
                  cursor_buff->current_win_buff->display_data->hex_mode)) != 0)
   line = work;
else
   line = cursor_buff->current_win_buff->buff_ptr;

/***************************************************************
*  Determine the subwindow width based on the prompt.
*  This is relevant for the dminput window and the unixcmd window.
***************************************************************/
subarea_width = cursor_buff->current_win_buff->window->sub_width;

/***************************************************************
*  Take care of the case where the prompt is wider than the window.
***************************************************************/
if (subarea_width <= 0) /* real wide prompt */
   {
      if (cursor_buff->which_window == DMINPUT_WINDOW) 
         scroll_dminput_prompt(strlen(get_dm_prompt())-2);
#ifdef PAD
      if (cursor_buff->which_window == UNIXCMD_WINDOW) 
         scroll_unix_prompt(strlen(get_drawn_unix_prompt())-2);
#endif
      subarea_width = cursor_buff->current_win_buff->window->sub_width;
      redraw_needed = cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW;
   }




/***************************************************************
*  First get the pixlen of the string.
*  We add 2 because we want room for 2 characters at the end 
*  of the screen.
***************************************************************/
len = strlen(line);
end_col = len + CHARS_RIGHT_OF_END;

if (cursor_buff->current_win_buff->window->fixed_font)
   {
      new_x = len * cursor_buff->current_win_buff->window->fixed_font;
      end_x = end_col * cursor_buff->current_win_buff->window->fixed_font;
      left_side_x = cursor_buff->current_win_buff->first_char * cursor_buff->current_win_buff->window->fixed_font;
   }
else
   {
      new_x = XTextWidth(cursor_buff->current_win_buff->window->font, line, len);
      end_x = new_x + (cursor_buff->current_win_buff->window->font->max_bounds.width * CHARS_RIGHT_OF_END);
      left_side_x = XTextWidth(cursor_buff->current_win_buff->window->font, line, cursor_buff->current_win_buff->first_char);
   }

DEBUG5(fprintf(stderr, "tr: pixlen = %d, line = \"%s\", len = %d, left side pix = %d, width = %d\n",
       new_x, line, len, left_side_x, subarea_width);
)

/***************************************************************
*  Determine if we need a scroll.
***************************************************************/
if (new_x < left_side_x)  /* line to short, end if off left side of screen */
   {
      DEBUG5(fprintf(stderr, "tr: need scroll left\n");)
      /***************************************************************
      *  Line too short, end is off left side of screen.
      *  Move pointers to full left side and recalculate left side.
      ***************************************************************/
      cursor_buff->current_win_buff->first_char =  0;  /* moves the pad */
      left_side_x = 0;
      redraw_needed = cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW;
      if (cursor_buff->which_window == DMINPUT_WINDOW)
         scroll_dminput_prompt(-MAX_LINE);
#ifdef PAD
      if (cursor_buff->which_window == UNIXCMD_WINDOW)
         scroll_unix_prompt(-MAX_LINE);
#endif
   }

if (subarea_width < (end_x - left_side_x))
   {
      /***************************************************************
      *  Line off right edge of screen
      *  We need to scroll the line right.
      ***************************************************************/
      DEBUG5(fprintf(stderr, "tr: need scroll right\n");)
      if (cursor_buff->current_win_buff->window->fixed_font)
         new_first_char = end_col - (subarea_width / cursor_buff->current_win_buff->window->fixed_font);
      else
         {
            pix_len = 0;
            left_side_x = end_x - subarea_width;
            for (i = 0;
                 pix_len < left_side_x;
                 i++)
               pix_len += XTextWidth(cursor_buff->current_win_buff->window->font, &line[i], 1);
            new_first_char = i;
         }

      if (cursor_buff->which_window == DMINPUT_WINDOW)
         {
            new_first_char -= scroll_dminput_prompt(new_first_char - cursor_buff->current_win_buff->first_char);
            if (new_first_char < 0)
               new_first_char = 0;
         }
#ifdef PAD
      if (cursor_buff->which_window == UNIXCMD_WINDOW)
         {
            new_first_char -= scroll_unix_prompt(new_first_char - cursor_buff->current_win_buff->first_char);
            if (new_first_char < 0)
               new_first_char = 0;
         }
#endif

      cursor_buff->current_win_buff->first_char = new_first_char;
      redraw_needed = (cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);
   }

/***************************************************************
*  The end of the line is on the screen, go to it.
***************************************************************/
if (!tabs)
   cursor_buff->current_win_buff->file_col_no = len;
else
   cursor_buff->current_win_buff->file_col_no = strlen(cursor_buff->current_win_buff->buff_ptr);

set_window_col_from_file_col(cursor_buff); /* input / output */

DEBUG5(fprintf(stderr, "tr: file [%d,%d] win [%d,%d], (%d,%d), %s, str: \"%s\" redraw = 0x%02X\n",
       cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no, cursor_buff->win_line_no, cursor_buff->win_col_no,
       cursor_buff->x, cursor_buff->y,
       which_window_names[cursor_buff->which_window], cursor_buff->current_win_buff->buff_ptr, redraw_needed);)

return(redraw_needed);

} /* end of dm_tr */



/************************************************************************

NAME:      dm_tl  -  DM tl (To Left) command

PURPOSE:    This routine will move the cursor to the first char
            on the current line.

PARAMETERS:

   1.  cursor_buff   - pointer to BUFF_DESCR (INPUT / OUTPUT)
               This is a pointer to the buffer / window description
               showing the current state of the window.

FUNCTIONS :

   1.   If the screen is scrolled horizontally, scroll it all the
        way to the left.

   2.   If this is a DM or Unix command window, make sure the prompt
        is scrolled except that it does not eat the whole window (wide prompt).

   3.   Set the pointers to show we are at the first char on the line.


RETURNED VALUE:
   redraw -  The window mask anded with the type of redraw needed is returned.

NOTES:
   1.  This routine moves the pad in addition to moving the cursor, so it
       modifies the current window buffer first_char on screen.

*************************************************************************/

int   dm_tl(BUFF_DESCR       *cursor_buff)
{

int                redraw_needed = 0;

/***************************************************************
*  Determine if we need a scroll.
***************************************************************/
if (cursor_buff->current_win_buff->first_char)
   {
      /***************************************************************
      *  Scroll to the left
      ***************************************************************/
      cursor_buff->current_win_buff->first_char = 0;
      redraw_needed = (cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);
   }

/***************************************************************
*  For the DM input window, make sure the prompt is fully scrolled.
***************************************************************/
if (cursor_buff->which_window == DMINPUT_WINDOW)
   if (scroll_dminput_prompt(-MAX_LINE))
      redraw_needed = (cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);

#ifdef PAD
/***************************************************************
*  For the Unix cmd window, make sure the prompt is fully scrolled.
***************************************************************/
if (cursor_buff->which_window == UNIXCMD_WINDOW)
   if (scroll_unix_prompt(-MAX_LINE))
      redraw_needed = (cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);
#endif


DEBUG5(fprintf(stderr, "tl: redraw = 0x%02X, line = \"%s\"\n", redraw_needed, cursor_buff->current_win_buff->buff_ptr);)


/***************************************************************
*  The front of the line is on the screen, go to it.
*  Take care that the prompt is not wider than the window.
***************************************************************/
cursor_buff->x = cursor_buff->current_win_buff->window->sub_x;

if (cursor_buff->x >= (int)(cursor_buff->current_win_buff->window->width)) /* real wide prompt */
   {
      if (cursor_buff->which_window == DMINPUT_WINDOW) 
         scroll_dminput_prompt(strlen(get_dm_prompt())-2);
#ifdef PAD
      if (cursor_buff->which_window == UNIXCMD_WINDOW) 
         scroll_unix_prompt(strlen(get_drawn_unix_prompt())-2);
#endif
      cursor_buff->x = cursor_buff->current_win_buff->window->sub_x;
      redraw_needed = (cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);
   }

cursor_buff->win_col_no = 0;
cursor_buff->current_win_buff->file_col_no = 0;  /* added 9/29/93 RES, to fix ^t bug on sun when window is partly off screen */

DEBUG5(fprintf(stderr, "tl: file [%d,%d] win [%d,%d], %s, redraw = 0x%02X\n",
       cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no, cursor_buff->win_line_no, cursor_buff->win_col_no,
       which_window_names[cursor_buff->which_window], redraw_needed);
)


return(redraw_needed);

} /* end of dm_tl */



/************************************************************************

NAME:      dm_au  -  DM au (Arrow Up) command

PURPOSE:    This routine will move the cursor up one character

PARAMETERS:

   1.  cursor_buff   - pointer to BUFF_DESCR (INPUT / OUTPUT)
               This is a pointer to the buffer / window description
               showing the current state of the window.

   2.  main_window_cur_descr   - pointer to PAD_DESCR (INPUT)
               This is a pointer to the to the buffer description of the
               main window.  Needed if we arrow up from the DM window to
               the main.

   3.  dmoutput_cur_descr   - pointer to PAD_DESCR (INPUT)
               This is a pointer to the to the buffer description of the
               DM output window.  Needed if we arrow up from the DM window to
               the main.

   4.  unixcmd_cur_descr   - pointer to PAD_DESCR (INPUT)
               This is a pointer to the to the buffer description of the
               Unix command window.  Needed if we arrow up from the DM window to
               the main.


FUNCTIONS :


   1.   If we are in a DM window, go to the last line of the main window or.
        the unix command window, if it exists.

   2.   Move up one line.


        

*************************************************************************/

void  dm_au(BUFF_DESCR       *cursor_buff,
            PAD_DESCR        *main_window_cur_descr,
            PAD_DESCR        *dmoutput_cur_descr,
            PAD_DESCR        *unixcmd_cur_descr)
{
int       window_changed = False;

/***************************************************************
*  If we are on a line with tabs, we will need to recalculate positions.
***************************************************************/
if (strchr(cursor_buff->current_win_buff->buff_ptr, '\t') != NULL)
   cursor_buff->up_to_snuff = False;

switch(cursor_buff->which_window)
{
case  MAIN_PAD:
   if (cursor_buff->win_line_no > 0)
      {
         cursor_buff->win_line_no--;
         cursor_buff->current_win_buff->file_line_no--;
         cursor_buff->current_win_buff->buff_ptr  = get_line_by_num(cursor_buff->current_win_buff->token ,cursor_buff->current_win_buff->file_line_no);
         cursor_buff->y -= cursor_buff->current_win_buff->window->line_height;
      }
   else
      if (cursor_buff->y > (cursor_buff->current_win_buff->window->sub_y + cursor_buff->current_win_buff->window->line_height))
         cursor_buff->y -= cursor_buff->current_win_buff->window->line_height;
   break;

#ifdef PAD
case  UNIXCMD_WINDOW:
   if (cursor_buff->win_line_no > 0)
      {
         cursor_buff->win_line_no--;
         cursor_buff->current_win_buff->file_line_no--;
         cursor_buff->current_win_buff->buff_ptr  = get_line_by_num(cursor_buff->current_win_buff->token ,cursor_buff->current_win_buff->file_line_no);
         cursor_buff->y -= cursor_buff->current_win_buff->window->line_height;
      }
   else
      {
         cursor_buff->current_win_buff = main_window_cur_descr;
         window_changed = True;
      }
   break;
#endif

case  DMINPUT_WINDOW:
#ifdef PAD
   if (unixcmd_cur_descr)
      cursor_buff->current_win_buff = unixcmd_cur_descr;
   else
      cursor_buff->current_win_buff = main_window_cur_descr;
#else
   cursor_buff->current_win_buff = main_window_cur_descr;
#endif
   window_changed = True;
   break;

case  DMOUTPUT_WINDOW:
   cursor_buff->x += dmoutput_cur_descr->window->x;  /* adjust for position of dm_output window */
#ifdef PAD
   if (unixcmd_cur_descr)
      cursor_buff->current_win_buff = unixcmd_cur_descr;
   else
      cursor_buff->current_win_buff = main_window_cur_descr;
#else
   cursor_buff->current_win_buff = main_window_cur_descr;
#endif
   window_changed = True;
   break;

default:
   DEBUG5(fprintf(stderr, "Unknown which_window number (%d) passed to dm_au\n", cursor_buff->which_window);)

} /* end of switch */

/***************************************************************
*  
*  If we moved up into the unixcmd or main window, do the extra
*  setup.
*  
***************************************************************/

if (window_changed)
   {
      cursor_buff->which_window = cursor_buff->current_win_buff->which_window;
      cursor_buff->win_line_no = cursor_buff->current_win_buff->window->lines_on_screen - 1;
      cursor_buff->y = cursor_buff->current_win_buff->window->sub_y + (cursor_buff->win_line_no * cursor_buff->current_win_buff->window->line_height) +1;
      cursor_buff->current_win_buff->file_line_no = (main_window_cur_descr->first_line + (cursor_buff->current_win_buff->window->lines_on_screen -1));
      cursor_buff->current_win_buff->buff_ptr  = get_line_by_num(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no);
      cursor_buff->up_to_snuff = False;
   }

/***************************************************************
*  If we are on a line with tabs, we will need to recalculate positions.
***************************************************************/
if ((!cursor_buff->current_win_buff->window->fixed_font) || (strchr(cursor_buff->current_win_buff->buff_ptr, '\t') != NULL))
   cursor_buff->up_to_snuff = False;

DEBUG5(fprintf(stderr, "au: file [%d,%d] win [%d,%d] (%d,%d), %s\n",
       cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no, cursor_buff->win_line_no, cursor_buff->win_col_no,
       cursor_buff->x, cursor_buff->y,
       which_window_names[cursor_buff->which_window]);
)

} /* end of dm_au */


/************************************************************************

NAME:      dm_ad  -  DM ad (Arrow Down) command

PURPOSE:    This routine will move the cursor down one character

PARAMETERS:

   1.  cursor_buff   - pointer to BUFF_DESCR (INPUT / OUTPUT)
               This is a pointer to the buffer / window description
               showing the current state of the window.

   2.  main_window_cur_descr   - pointer to PAD_DESCR (INPUT)
               This is a pointer to the to the buffer description of the
               main window.  Needed if we arrow down from the main window to
               the DM.

   3.  dmoutput_cur_descr   - pointer to PAD_DESCR (INPUT)
               This is a pointer to the to the buffer description of the
               DM output window.  Needed if we arrow down from the main window to
               the DM.

   4.  dminput_cur_descr   - pointer to PAD_DESCR (INPUT)
               This is a pointer to the to the buffer description of the
               DM input window.  Needed if we arrow down from the main window to
               the DM.

   5.  unixcmd_cur_descr   - pointer to PAD_DESCR (INPUT)
               This is a pointer to the to the buffer description of the
               Unix command window.  Needed if we arrow down from the main window to
               the Unix window.  In non-pad mode, this parm is NULL.

FUNCTIONS :

   1.   If we are in a the last line of the main window, move into one of
        the DM windows.

   2.   Move down one line.


        

*************************************************************************/

void  dm_ad(BUFF_DESCR       *cursor_buff,
            PAD_DESCR        *dmoutput_cur_descr,
            PAD_DESCR        *dminput_cur_descr,
            PAD_DESCR        *unixcmd_cur_descr)
{
int   to_next_window;
int   lines_sent;

/***************************************************************
*  If we are on a line with tabs, we will need to recalculate positions.
***************************************************************/
if (strchr(cursor_buff->current_win_buff->buff_ptr, '\t') != NULL)
   cursor_buff->up_to_snuff = False;

#ifdef PAD
if ((cursor_buff->which_window == MAIN_PAD) &&
    ((cursor_buff->win_line_no == (cursor_buff->current_win_buff->window->lines_on_screen - 1)) ||
     ((cursor_buff->win_line_no == -1) &&
      (cursor_buff->y > (cursor_buff->current_win_buff->window->sub_y + (cursor_buff->current_win_buff->window->line_height * cursor_buff->current_win_buff->window->lines_on_screen))))))
   {
      /* move down into unixcmd or dm window */
      if (unixcmd_cur_descr != NULL)
         {
            cursor_buff->which_window = unixcmd_cur_descr->which_window;
            cursor_buff->current_win_buff = unixcmd_cur_descr;
            cursor_buff->win_line_no = 0;
            cursor_buff->y = (cursor_buff->current_win_buff->window->sub_y + cursor_buff->current_win_buff->window->line_height) - 2;
         }
      else
         {
            if (cursor_buff->x >= dmoutput_cur_descr->window->x)
               {
                  cursor_buff->which_window = dmoutput_cur_descr->which_window;
                  cursor_buff->current_win_buff = dmoutput_cur_descr;
                  cursor_buff->x -= dmoutput_cur_descr->window->x;
               }
            else
               {
                  cursor_buff->which_window = dminput_cur_descr->which_window;
                  cursor_buff->current_win_buff = dminput_cur_descr;
               }
            cursor_buff->win_line_no = 0;
            cursor_buff->current_win_buff->file_line_no = 0;
            cursor_buff->y = (cursor_buff->current_win_buff->window->sub_y + cursor_buff->current_win_buff->window->height) - 2;
         }
      cursor_buff->up_to_snuff = False;
   }
else
   if (cursor_buff->which_window == MAIN_PAD)
      {
         /* in the main window above the last line */
         cursor_buff->y += cursor_buff->current_win_buff->window->line_height;
         cursor_buff->win_line_no++;
         cursor_buff->current_win_buff->file_line_no++;
      }
   else
      if (cursor_buff->which_window == UNIXCMD_WINDOW)
         {
            /***************************************************************
            *  See if we fall out of this window into the next.
            ***************************************************************/
            if ((cursor_buff->win_line_no == (cursor_buff->current_win_buff->window->lines_on_screen - 1)) ||
                ((cursor_buff->win_line_no == -1) &&
                 (cursor_buff->y > (cursor_buff->current_win_buff->window->sub_y + (cursor_buff->current_win_buff->window->line_height * cursor_buff->current_win_buff->window->lines_on_screen)))))
               to_next_window = True;
            else
               to_next_window = False;
 
            /***************************************************************
            *  Position the cursor in the new window or the same window
            ***************************************************************/
            if (to_next_window)
               {
                  if (cursor_buff->x >= dmoutput_cur_descr->window->x)
                     {
                        cursor_buff->which_window = dmoutput_cur_descr->which_window;
                        cursor_buff->current_win_buff = dmoutput_cur_descr;
                        cursor_buff->x -= dmoutput_cur_descr->window->x;
                     }
                  else
                     {
                        cursor_buff->which_window = dminput_cur_descr->which_window;
                        cursor_buff->current_win_buff = dminput_cur_descr;
                     }
                  cursor_buff->win_line_no = 0;
                  cursor_buff->current_win_buff->file_line_no = 0;
                  cursor_buff->y = cursor_buff->current_win_buff->window->sub_y + 2;
                  cursor_buff->up_to_snuff = False;
               }
            else
               {
                  /* in the Unixcmd window above the last line */
                  cursor_buff->y += cursor_buff->current_win_buff->window->line_height;
                  cursor_buff->win_line_no++;
                  cursor_buff->current_win_buff->file_line_no++;
               }

            /***************************************************************
            *  If we are not in hold mode, we may have to send lines to the shell
            ***************************************************************/
            if ((!(*unixcmd_cur_descr->hold_mode)) && (total_lines(unixcmd_cur_descr->token) > 1))
               {
                  /* RES 8/9/95 event_do added to allow undo after paste to unixwin followed by ad */
                  event_do(unixcmd_cur_descr->token, KS_EVENT, 0, 0, 0, 0);
                  if (to_next_window)
                     {
                        DEBUG22(fprintf(stderr, "dm_ad: Want to send %d lines to the shell\n", total_lines(unixcmd_cur_descr->token));)
                        lines_sent = send_to_shell(total_lines(unixcmd_cur_descr->token), 
                                                   unixcmd_cur_descr);
                        DEBUG22(fprintf(stderr, "dm_ad: %d left to send\n", total_lines(unixcmd_cur_descr->token));)
                     }
                  else
                     {
                        DEBUG22(fprintf(stderr, "dm_ad: Want to send %d lines to the shell\n", unixcmd_cur_descr->file_line_no+1);)
                        /* this needs to be fixed somehow when we are in the socket overflow case RES 3/2/94 */
                        lines_sent = send_to_shell(unixcmd_cur_descr->file_line_no, unixcmd_cur_descr);
                     }
                  DEBUG22(fprintf(stderr, "dm_ad: sent %d line(s)\n", lines_sent);)
                  unixcmd_cur_descr->first_line -= lines_sent;
                  if (unixcmd_cur_descr->first_line < 0)
                     {
                        if (!to_next_window)
                           {
                              cursor_buff->win_line_no += unixcmd_cur_descr->first_line; /* remember first line is currently negative */
                              if (cursor_buff->win_line_no < 0)
                                 {
                                    cursor_buff->win_line_no = 0;
                                    cursor_buff->y = cursor_buff->current_win_buff->window->sub_y + 2;
                                 }
                              else
                                 cursor_buff->y += unixcmd_cur_descr->window->line_height * unixcmd_cur_descr->first_line;
                           }
                        unixcmd_cur_descr->first_line = 0;
                     }

                  unixcmd_cur_descr->file_line_no -= lines_sent;
                  if (unixcmd_cur_descr->file_line_no < 0)
                     unixcmd_cur_descr->file_line_no = 0;

                  cursor_buff->current_win_buff->buff_ptr = get_line_by_num(cursor_buff->current_win_buff->token, 0);
               }

         }
#else
if ((cursor_buff->which_window == MAIN_PAD) &&
    ((cursor_buff->win_line_no == (cursor_buff->current_win_buff->window->lines_on_screen - 1)) ||
     ((cursor_buff->win_line_no == -1) &&
      (cursor_buff->y > (cursor_buff->current_win_buff->window->sub_y + (cursor_buff->current_win_buff->window->line_height * cursor_buff->current_win_buff->window->lines_on_screen))))))
   {
      /* move down into dm window */
      if (cursor_buff->x >= dmoutput_cur_descr->window->x)
         {
             cursor_buff->which_window = dmoutput_cur_descr->which_window;
             cursor_buff->current_win_buff = dmoutput_cur_descr;
             cursor_buff->x -= dmoutput_cur_descr->window->x;
         }
      else
         {
             cursor_buff->which_window = dminput_cur_descr->which_window;
             cursor_buff->current_win_buff = dminput_cur_descr;
         }
      cursor_buff->win_line_no = 0;
      cursor_buff->current_win_buff->file_line_no = 0;
      cursor_buff->y = (cursor_buff->current_win_buff->window->sub_y + cursor_buff->current_win_buff->window->sub_height) - 2;
      cursor_buff->up_to_snuff = False;
   }
else
   if (cursor_buff->which_window == MAIN_PAD)
      {
         /* in the main window above the last line */
         cursor_buff->y += cursor_buff->current_win_buff->window->line_height;
         cursor_buff->win_line_no++;
         cursor_buff->current_win_buff->file_line_no++;
      }
#endif

cursor_buff->current_win_buff->buff_ptr  = get_line_by_num(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no);
/***************************************************************
*  If we are on a line with tabs, we will need to recalculate positions.
*  RES 8/10/95 Recalucate for variable fonts too.
***************************************************************/
if ((!cursor_buff->current_win_buff->window->fixed_font) || (strchr(cursor_buff->current_win_buff->buff_ptr, '\t') != NULL))
   cursor_buff->up_to_snuff = False;


DEBUG5(fprintf(stderr, "ad: file [%d,%d] win [%d,%d], %s\n",
       cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no, cursor_buff->win_line_no, cursor_buff->win_col_no,
       which_window_names[cursor_buff->which_window]);
)

} /* end of dm_ad */


/************************************************************************

NAME:      dm_ar  -  DM ar (Arrow Right) command

PURPOSE:    This routine will move the cursor right one character

PARAMETERS:
   1.  cursor_buff   - pointer to BUFF_DESCR (INPUT / OUTPUT)
               This is a pointer to the buffer / window description
               showing the current state of the window.

   2.  dmoutput_cur_descr   - pointer to PAD_DESCR (INPUT)
               This is a pointer to the output message window.  It is
               needed to allow the cursor to move from the dminput to
               dm_output windows.

   3.  wrap_mode            - int (INPUT)
               This flag indicates the ar command was executed with the
               -w option.  This causes the cursor to wrap to col 0 in the
               next line when we arrow right past the end of the line.

FUNCTIONS :
   1.   Move right one char.

   2.   If we are in the dm input window and we hit the right edge,
        move to the dm output window.

   3.   If we are in wrap mode and we are not on the last line of the
        file, go to the next line.
        

*************************************************************************/

int   dm_ar(BUFF_DESCR       *cursor_buff,
            PAD_DESCR        *dmoutput_cur_descr,
            int               wrap_mode)
{

int                adjusted;
int                redraw_needed = 0;

cursor_buff->current_win_buff->file_col_no++;
adjusted = set_window_col_from_file_col(cursor_buff); /* input / output */
/* if adjusted, file_col_no is set back.  Probably to original value */

if (adjusted && (cursor_buff->which_window == DMINPUT_WINDOW))
   dm_position(cursor_buff, dmoutput_cur_descr, dmoutput_cur_descr->file_line_no, 0);
else
   if (wrap_mode)
      if (((cursor_buff->current_win_buff->file_col_no + adjusted) > (int)strlen(cursor_buff->current_win_buff->buff_ptr)) &&
          (cursor_buff->current_win_buff->file_line_no < total_lines(cursor_buff->current_win_buff->token)-1))
         redraw_needed = dm_position(cursor_buff, cursor_buff->current_win_buff, cursor_buff->current_win_buff->file_line_no+1, 0);
      else
         if (adjusted)
            redraw_needed = dm_position(cursor_buff,
                                        cursor_buff->current_win_buff,
                                        cursor_buff->current_win_buff->file_line_no,
                                        cursor_buff->current_win_buff->file_col_no+1);
   

DEBUG5(fprintf(stderr, "ar: file [%d,%d] win [%d,%d], %s\n",
       cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no, cursor_buff->win_line_no, cursor_buff->win_col_no,
       which_window_names[cursor_buff->which_window]);
)

return(redraw_needed);

} /* end of dm_ar */


/************************************************************************

NAME:      dm_al  -  DM al (Arrow Left) command

PURPOSE:    This routine will move the cursor left one character

PARAMETERS:

   1.  cursor_buff   - pointer to BUFF_DESCR (INPUT / OUTPUT)
               This is a pointer to the buffer / window description
               showing the current state of the window.

   2.  dmoutput_cur_descr   - pointer to PAD_DESCR (INPUT)
               This is a pointer to the output message window.  It is
               needed to allow the cursor to move from the dminput to
               dm_output windows.

   3.  wrap_mode            - int (INPUT)
               This flag indicates the ar command was executed with the
               -w option.  This causes the cursor to wrap to the previous line
               when we arrow left past column 0.

FUNCTIONS :
   1.   Move left one char unless we are at the left edge.

   2.   If we are at the edge of the DM_OUTPUT window, move
        into the dm input window.
        

*************************************************************************/

int   dm_al(BUFF_DESCR       *cursor_buff,
            PAD_DESCR        *dminput_cur_descr,
            int               wrap_mode)
{

int                i;
int                junk;
int                junk2;
char              *line;
int                redraw_needed = 0;

/* do not arrow off the left side of the window */
if (cursor_buff->win_col_no > 0)
   {
      cursor_buff->current_win_buff->file_col_no--;
      set_window_col_from_file_col(cursor_buff); /* input / output */
   }
else
   if (cursor_buff->which_window == DMOUTPUT_WINDOW)
      {
         junk =  (dminput_cur_descr->window->sub_width - dminput_cur_descr->window->x)-2;
         DEBUG5(fprintf(stderr, "x coord %d\n", junk);)

         i = which_char_on_line(cursor_buff->current_win_buff->window,
                                dminput_cur_descr->window->font,
                                junk,
                                dminput_cur_descr->buff_ptr,
                                &junk,
                                &junk2);
         dm_position(cursor_buff, dminput_cur_descr, dminput_cur_descr->file_line_no, i);
      }
   else
      if (wrap_mode)
         if (cursor_buff->current_win_buff->first_char > 0)
            redraw_needed = dm_position(cursor_buff,
                                        cursor_buff->current_win_buff,
                                        cursor_buff->current_win_buff->file_line_no,
                                        cursor_buff->current_win_buff->first_char-1);
         else
            if (cursor_buff->current_win_buff->file_line_no > 0)
              {
                 line = get_line_by_num(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no-1);
                 i = strlen(line);
                 redraw_needed = dm_position(cursor_buff, cursor_buff->current_win_buff, cursor_buff->current_win_buff->file_line_no-1, i);
              }
           else
              set_window_col_from_file_col(cursor_buff); /* input / output */
     else
        set_window_col_from_file_col(cursor_buff); /* input / output */

DEBUG5(fprintf(stderr, "al: file [%d,%d] win [%d,%d], %s\n",
       cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no, cursor_buff->win_line_no, cursor_buff->win_col_no,
       which_window_names[cursor_buff->which_window]);
)

return(redraw_needed);

} /* end of dm_al */


/************************************************************************

NAME:      dm_num  -  Move to a specified row and column in the file

PURPOSE:    This routine is  used to process commands [row,col] and just a number

PARAMETERS:

   1.  dmc          - pointer to DMC (INPUT)
                      This mark is command structure for the num command being executed.

   2.  cursor_buff  - pointer to BUFF_DESCR (INPUT / OUTPUT)
                      This is the buffer description which shows the current
                      location of the cursor.

   3.  find_border - int (INPUT)
                     This is the current find border value from the
                     fbdr command or the -findbrdr command line option.


FUNCTIONS :

   1.   Determine if this is a relative or absolute num command and set the
        current file line number accordingly.  This is done separately for
        mark_c and num commands.

   2.   Make sure all the data or enough data has been read in.

   3.   Calculate the column position.  For the num command, this is always zero.

   4.   Apply the find_border_adjustment if necessary.
   

RETURNED VALUE:
   redraw -  The window mask anded with the type of redraw needed is returned.


*************************************************************************/

int dm_num(DMC               *dmc,
           BUFF_DESCR        *cursor_buff,
           int                find_border)
{
int      i;
int      j;
int      redraw_needed;

if (dmc->any.cmd == DM_num)
   {
      if (dmc->num.relative != 0)
         i = cursor_buff->current_win_buff->file_line_no + dmc->num.row;
      else
         i = dmc->num.row;
   }
else
   {
      if (dmc->markc.row_relative != 0)
         i = cursor_buff->current_win_buff->file_line_no + dmc->markc.row;
      else
         i = dmc->markc.row;
   }

load_enough_data(i);

if (dmc->any.cmd == DM_num)
   j = 0;
else
   {
      if (dmc->markc.col_relative != 0)
         j = cursor_buff->current_win_buff->file_col_no + dmc->markc.col;
      else
         j = dmc->markc.col;
   }

redraw_needed = dm_position(cursor_buff, cursor_buff->current_win_buff, i, j);

if ((cursor_buff->current_win_buff->which_window == MAIN_PAD) &&
    (find_border > 0) &&
    (dmc->next == NULL) && /* unencumbered positioning only */
    find_border_adjust(cursor_buff->current_win_buff, find_border))
   {
      redraw_needed |= dm_position(cursor_buff, cursor_buff->current_win_buff, i ,j);
      redraw_needed |= (cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);/* 7/18/96 really need this! RES */
   }                   

DEBUG5(
   fprintf(stderr, "%s: File [%d,%d] win [%d,%d] in %s\n",
                   dmsyms[dmc->any.cmd].name,
                   cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no,
                   cursor_buff->win_line_no,  cursor_buff->win_col_no,
                   which_window_names[cursor_buff->which_window]);
)

return(redraw_needed);

} /* end of dm_num */


/************************************************************************

NAME:      dm_corner   -   Position the window upper left corner over the file

PURPOSE:    This routine is  used to process command {row,col}

PARAMETERS:

   1.  dmc                   - pointer to DMC (INPUT)
                               This mark is command structure for the command being executed.

   2.  cursor_buff           - pointer to BUFF_DESCR (INPUT / OUTPUT)
                               This is the buffer description which shows the current
                               location of the cursor.


FUNCTIONS :

   1.   If we are in the correct place, do nothing.

   2.   Determine if this is a relative or absolute num command and set the
        current first line number accordingly.

   3.   Make sure all the data or enough data has been read in.

   4.   Calculate the column position for the left corner

   5.   If the cursor is still on screen, keep it on the same line, otherwise
        leave it on the same window line.


RETURNED VALUE:
   redraw -  The window mask anded with the type of redraw needed is returned.


*************************************************************************/

int dm_corner(DMC               *dmc,
              BUFF_DESCR        *cursor_buff)
{
int      window_width;
int      redraw_needed;
int      end_line;


/***************************************************************
*  If we are in the right place, do nothing.
***************************************************************/
if ((dmc->corner.row_relative == 0) &&
    (dmc->corner.col_relative == 0) &&
    (dmc->corner.row == cursor_buff->current_win_buff->first_line) &&
    (dmc->corner.col == cursor_buff->current_win_buff->first_char))
   return(0);

/***************************************************************
*  We will need a full redraw, set the new row.
***************************************************************/
redraw_needed = (cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);

if (dmc->num.relative != 0)
   cursor_buff->current_win_buff->first_line += dmc->corner.row;
else
   cursor_buff->current_win_buff->first_line  = dmc->corner.row;


/***************************************************************
*  Watch for end of file or out of bounds data.
***************************************************************/
if (cursor_buff->current_win_buff->first_line >= total_lines(cursor_buff->current_win_buff->token))
   cursor_buff->current_win_buff->first_line = total_lines(cursor_buff->current_win_buff->token) - 2; /* show 1 line */
if (cursor_buff->current_win_buff->first_line < 0)
   cursor_buff->current_win_buff->first_line = 0;

end_line = cursor_buff->current_win_buff->first_line+cursor_buff->current_win_buff->window->lines_on_screen;
load_enough_data(end_line);

/***************************************************************
*  Adjust the vertical cursor position if necessary.
*  If the file row is no longer visible, keep the cursor
*  in the same window row number.
***************************************************************/
if ((cursor_buff->current_win_buff->file_line_no <  cursor_buff->current_win_buff->first_line) ||
    (cursor_buff->current_win_buff->file_line_no >= end_line))
   cursor_buff->current_win_buff->file_line_no = cursor_buff->current_win_buff->first_line+cursor_buff->win_line_no;

/***************************************************************
*  Set the new corner column.  Omitting the second argument
*  results in a relative change of zero.
***************************************************************/
if (dmc->corner.col_relative != 0)
   cursor_buff->current_win_buff->file_col_no += dmc->corner.col;
else
   cursor_buff->current_win_buff->file_col_no =  dmc->corner.col;

/***************************************************************
*  Watch for out of bounds data.
***************************************************************/
window_width = min_chars_on_window(cursor_buff->current_win_buff->window);
if ((cursor_buff->current_win_buff->first_char < 0) ||  (cursor_buff->current_win_buff->first_char >= MAX_LINE))
   cursor_buff->current_win_buff->first_char = 0;

/***************************************************************
*  Adjust the horizontal cursor position if necessary.
*  If the file row is no longer visible, keep the cursor
*  in the same window col number.
***************************************************************/
if ((cursor_buff->current_win_buff->file_col_no <  cursor_buff->current_win_buff->first_char) ||
    (cursor_buff->current_win_buff->file_col_no >= cursor_buff->current_win_buff->first_char+window_width))
   cursor_buff->current_win_buff->file_col_no = cursor_buff->current_win_buff->first_char+cursor_buff->win_col_no;
   
dm_position(cursor_buff,
            cursor_buff->current_win_buff,
            cursor_buff->current_win_buff->file_line_no,
            cursor_buff->current_win_buff->file_col_no);

DEBUG5(
   fprintf(stderr, "%s: File [%d,%d] win [%d,%d] in %s\n",
                   dmsyms[dmc->any.cmd].name,
                   cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no,
                   cursor_buff->win_line_no,  cursor_buff->win_col_no,
                   which_window_names[cursor_buff->which_window]);
)

return(redraw_needed);

} /* end of dm_corner */


/************************************************************************

NAME:      dm_twb  -  DM To Window Border command

PURPOSE:    This routine will move the cursor to the boarder, t, r, l, or b
            on the main window.

PARAMETERS:

   1.  cursor_buff   - pointer to BUFF_DESCR (INPUT / OUTPUT)
               This is a pointer to the buffer / window description
               showing the current state of the window.  It is modified
               to show the state in the correct place.  A warp_pointer
               is then requested.

   2.  dmc   - pointer to DMC (INPUT)
               This is a pointer to the DM command structure for the twb command.


FUNCTIONS :

   1.   Set the X or Y position in the window appropriately.  


NOTES:
   1.   A warp is always required after this call.

   2.   Use the window rather than the subwindow to avoid the effects of line
        numbers.


*************************************************************************/

void  dm_twb(BUFF_DESCR       *cursor_buff,
             DMC              *dmc)
{
WINDOW_LINE       *win_line;

/***************************************************************
*  
*  Make sure we are on top.
*  
***************************************************************/

if (window_is_obscured(cursor_buff->current_win_buff->display_data)) /* in getevent.c */
   {
      DEBUG9(XERRORPOS)
      XRaiseWindow(cursor_buff->current_win_buff->display_data->display, cursor_buff->current_win_buff->x_window);
   }

win_line = &cursor_buff->current_win_buff->win_lines[cursor_buff->win_line_no];

switch(dmc->twb.border)
{
case 'l':
case 'L':
   cursor_buff->x = 0; /*  want zero relative to the main window.    cursor_buff->window->sub_x; */
   cursor_buff->current_win_buff->file_col_no = cursor_buff->current_win_buff->first_char;
   cursor_buff->win_col_no = 0;
   break;

case 'r':
case 'R':
   cursor_buff->x = (cursor_buff->current_win_buff->window->sub_width + cursor_buff->current_win_buff->window->sub_x) - 1;
   if (cursor_buff->current_win_buff->window->fixed_font)
      cursor_buff->win_col_no = (cursor_buff->current_win_buff->window->sub_width / cursor_buff->current_win_buff->window->fixed_font);
   else
      cursor_buff->win_col_no = (cursor_buff->current_win_buff->window->sub_width / cursor_buff->current_win_buff->window->font->max_bounds.width);
   cursor_buff->current_win_buff->file_col_no = tab_cursor_adjust(win_line->line,
                                                                  cursor_buff->win_col_no,
                                                                  win_line->w_first_char,
                                                                  TAB_LINE_OFFSET,
                                                                  cursor_buff->current_win_buff->display_data->hex_mode);
   break;

case 't':
case 'T':
default:
   cursor_buff->y = cursor_buff->current_win_buff->window->sub_y;
   cursor_buff->win_line_no = 0;
   cursor_buff->current_win_buff->file_line_no = cursor_buff->current_win_buff->first_line;
   cursor_buff->current_win_buff->buff_ptr  = get_line_by_num(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no);
   break;

case 'b':
case 'B':
   cursor_buff->y = cursor_buff->current_win_buff->window->sub_height + cursor_buff->current_win_buff->window->sub_y - 1;
   cursor_buff->win_line_no = cursor_buff->current_win_buff->window->sub_height / cursor_buff->current_win_buff->window->line_height;
   cursor_buff->current_win_buff->file_line_no = cursor_buff->current_win_buff->first_line + cursor_buff->win_line_no;
   cursor_buff->current_win_buff->buff_ptr  = get_line_by_num(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no);
   break;

} /* end of switch */

DEBUG5(fprintf(stderr, "twb: move to x = %d, y = %d, window: %s\n",
                        cursor_buff->x, cursor_buff->y,
                        which_window_names[cursor_buff->which_window]);
)
return;

} /* end of dm_twb */


/************************************************************************

NAME:      dm_sic  -  Set Insert Cursor for Mouse Off mode

PURPOSE:    This routine is used in -mouse off mode to move the text 
            cursor to the current mouse cursor position.

PARAMETERS:

   1.  event   - pointer to XEvent (INPUT)
               This is a pointer to the event which triggered this call.
               We need the X and Y coordinates and the window id.

   2.  cursor_buff   - pointer to BUFF_DESCR (INPUT / OUTPUT)
               This is a pointer to the buffer / window description
               showing the current state of the window.  It is modified
               to show the state in the correct place.  A fake warp_pointer
               is then requested.

   3.  main_window_cur_descr   - pointer to PAD_DESCR (INPUT)
               This is a pointer to the pad description for the main window.
               It is needed by routine fake_warp_pointer and is passed through.

   4.  wrap_mode            - int (INPUT)
               This flag indicates the sic command was executed with the
               -w option.  This causes the cursor to move to just past
               the end of line when the cursor is past the end of the data
               on the line.

FUNCTIONS :
   1.   If we are in mouse on mode, this is a noop, just return.

   2.   Get the X and Y positions and the window id from the event record.

   3.   Convert the passed window id to the correct pad.


RETURNED VALUE:
   warp -  boolean flag indicating that a warp pointer is needed.


*************************************************************************/

int dm_sic(XEvent       *event,
           BUFF_DESCR   *cursor_buff,
           PAD_DESCR    *main_window_cur_descr,
           int           wrap_mode)
{
int    target_x;
int    target_y;
Window window;
int    junk;
int    junk2;
int    i;

flush(cursor_buff->current_win_buff);

if ((event->type == KeyPress)|| (event->type == KeyRelease))
   {
      target_x = event->xkey.x;
      target_y = event->xkey.y;
      window   = event->xkey.window;
   }
else
   if ((event->type == ButtonPress) || (event->type == ButtonRelease))
      {
         target_x = event->xbutton.x;
         target_y = event->xbutton.y;
         window   = event->xbutton.window;
      }
   else
      {
         char msg[128];
         snprintf(msg, sizeof(msg), "Unexpected event type (%d) event passed to dm_sic", event->type);
         dm_error(msg, DM_ERROR_LOG);
      }

cursor_buff->current_win_buff = window_to_buff_descr(main_window_cur_descr->display_data, window);
cursor_buff->which_window     = cursor_buff->current_win_buff->which_window;
cursor_buff->x                = target_x;
cursor_buff->y                = target_y;
cursor_buff->win_line_no      = which_line_on_screen(cursor_buff->current_win_buff->window,
                                                     target_y,
                                                     &junk);
cursor_buff->current_win_buff->file_line_no = cursor_buff->win_line_no + cursor_buff->current_win_buff->first_line;

flush(cursor_buff->current_win_buff);
cursor_buff->current_win_buff->buff_ptr = get_line_by_num(cursor_buff->current_win_buff->token, cursor_buff->current_win_buff->file_line_no);


cursor_buff->win_col_no       = which_char_on_line(cursor_buff->current_win_buff->window,           /* input  */
                                                   cursor_buff->current_win_buff->window->font,     /* input  */
                                                   target_x,                                        /* input  */
                                                   cursor_buff->current_win_buff->buff_ptr,         /* input  */
                                                   &junk,                                           /* output  */
                                                   &junk2);                                         /* output  */

cursor_buff->current_win_buff->file_col_no = tab_cursor_adjust(cursor_buff->current_win_buff->buff_ptr,
                                                               cursor_buff->win_col_no,
                                                               cursor_buff->current_win_buff->first_char,
                                                               TAB_LINE_OFFSET,
                                                               cursor_buff->current_win_buff->display_data->hex_mode);

if (wrap_mode && (cursor_buff->current_win_buff->file_col_no > (i = strlen(cursor_buff->current_win_buff->buff_ptr))))
   dm_position(cursor_buff, cursor_buff->current_win_buff, cursor_buff->current_win_buff->file_line_no, i);


                                      
DEBUG5(fprintf(stderr, "sic: file [%d,%d] win [%d,%d], (%d,%d) %s\n",
       cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no,
       cursor_buff->win_line_no, cursor_buff->win_col_no,
       cursor_buff->x, cursor_buff->y,
       which_window_names[cursor_buff->which_window]);
)

return(True);

} /* end of dm_sic */


/************************************************************************

NAME:      dm_tn  -   tn (to next) and ti (to next input) window commands

PURPOSE:    This routine is used in -mouse off mode to move the text 
            cursor to the current mouse cursor position.

PARAMETERS:

   1.  dmc   - pointer to DMC (INPUT)
               This is a pointer to the command element which caused this
               routine to be called.  It is used to determine whether this
               is a tn or a ti command.

   2.  cursor_buff   - pointer to BUFF_DESCR (INPUT / OUTPUT)
               This is a pointer to the buffer / window description
               showing the current state of the window.  It is modified
               to show the state in the correct place.

   3.  main_window_cur_descr   - pointer to PAD_DESCR (INPUT)
               This is a pointer to the pad description for the main window.

   4.  dminput_cur_descr   - pointer to PAD_DESCR (INPUT)
               This is a pointer to the pad description for the dm input window.

   5.  unixcmd_cur_descr   - pointer to PAD_DESCR (INPUT)
               This is a pointer to the pad description for the Unix command window.


FUNCTIONS :

   1.   Determine whether we are going to a window on this ce session or to some
        other window.

   2.   For other windows, send a request to the the next window to grab the pointer.


RETURNED VALUE:
   warp_needed  -  int
                   A flag is returned based on whether we need a warp.
                   True - do a warp,
                   False - do NOT do a warp, we passed off control to some
                           other window.

TARGET TABLES:

tn and ti
if pad mode,
main    ->  unixcmd
unixcmd ->  another window
dm      ->  unixcmd

if Not pad mode
main     ->  dminput
dminput  ->  another window
dmoutput ->  dminput


*************************************************************************/

int  dm_tn(DMC          *dmc,
           BUFF_DESCR   *cursor_buff,
           PAD_DESCR    *dminput_cur_descr,
           PAD_DESCR    *unixcmd_cur_descr)
{
PAD_DESCR   *target_pad;

if (unixcmd_cur_descr)
   if (cursor_buff->which_window == UNIXCMD_WINDOW)
      target_pad = NULL;
   else
      target_pad = unixcmd_cur_descr;
else
   if (cursor_buff->which_window == DMINPUT_WINDOW)
      target_pad = NULL;
   else
      target_pad = dminput_cur_descr;
   
if (target_pad == NULL)
   cc_to_next(dmc->tn.cmd == DM_ti);
else
   dm_position(cursor_buff, target_pad, target_pad->file_line_no, target_pad->file_col_no);

return(target_pad != NULL);  /* true if dm_position was called */

} /* end of dm_tn */

#ifdef PAD
/************************************************************************

NAME:      dm_wh  -  Take window into and out of hold mode

PURPOSE:    This routine is used in pad mode to take the window into
            and out of hold mode.

PARAMETERS:

   1.  dmc   - pointer to DMC (INPUT)
               This is a pointer to the wh command element which caused
               routine to be called.  It is needed for the toggle switch
               value.

   2.  dspl_descr   - pointer to DISPLAY_DESCR (INPUT / OUTPUT)
               This is a pointer to the description of the current display.
               The main window, unix window, and misc flags are needed.



FUNCTIONS :

   1.   Toggle the window hold flag using the command.  If it did not change, 
        return.

   2.   If we are going out of hold mode, make sure the main window is positioned
        at the bottom of the screen.

   3.   If we are going out of hold mode, send lines to the shell from the Unix
        window.  


RETURNED VALUE:
   redraw -  The window mask and'ed with the type of redraw needed is returned.


*************************************************************************/

int  dm_wh(DMC            *dmc,
           DISPLAY_DESCR  *dspl_descr)
{
int          changed;
int          lines_copied;
int          redraw_needed;

/***************************************************************
*  
*  dm_wh is a no-op in non-pad mode
*  
***************************************************************/

if (!dspl_descr->pad_mode)
   return(0);

/***************************************************************
*  
*  Toggle the switch.  If it did not change, return.
*  
***************************************************************/

dspl_descr->hold_mode = toggle_switch(dmc->ws.mode, 
                                      dspl_descr->hold_mode,
                                      &changed);
if (!changed)
   return(0);

/***************************************************************
*  
*  We definitely have to redraw the title bar and reset autohold
*  if we are using it.
*  
***************************************************************/

redraw_needed = (FULL_REDRAW & TITLEBAR_MASK);
if (dspl_descr->autohold_mode)
   dspl_descr->autohold_mode = 1;

/***************************************************************
*
*  Scroll to the bottom of the transcript pad when hold mode is turned off.
*
***************************************************************/
if (!(dspl_descr->hold_mode))
   {
      redraw_needed |= pad_to_bottom(dspl_descr->main_pad);
      if (dspl_descr->main_pad->file_line_no < dspl_descr->main_pad->first_line)
         {
            dspl_descr->main_pad->file_line_no = dspl_descr->main_pad->first_line;
            dspl_descr->main_pad->buff_ptr = get_line_by_num(dspl_descr->main_pad->token, dspl_descr->main_pad->file_line_no);
         }
      change_background_work(dspl_descr, BACKGROUND_SCROLL, 0);
      scroll_some(NULL, dspl_descr, 0);  /* clear scrolling variables */

   }
/***************************************************************
*
*  If the cursor is not at the top line of the unix cmd window,
*  send the lines above the cursor to the shell.
*
***************************************************************/
if (!(dspl_descr->hold_mode) && (dspl_descr->unix_pad->file_line_no > 0))
   {
      /* RES 8/9/95 event_do added to allow undo after paste to unixwin followed by ad */
      event_do(dspl_descr->unix_pad->token, KS_EVENT, 0, 0, 0, 0);
      DEBUG22(fprintf(stderr, "dm_wh: Want to send %d lines to the shell\n", dspl_descr->unix_pad->file_line_no);)
      lines_copied = send_to_shell(dspl_descr->unix_pad->file_line_no, dspl_descr->unix_pad);
      DEBUG22(fprintf(stderr, "dm_wh: sent %d line(s)\n", lines_copied);)
      dspl_descr->unix_pad->file_line_no -= lines_copied;
      dspl_descr->unix_pad->first_line   -= lines_copied;
      if (dspl_descr->unix_pad->first_line < 0)
         dspl_descr->unix_pad->first_line = 0;
      dspl_descr->unix_pad->buff_ptr = get_line_by_num(dspl_descr->unix_pad->token, dspl_descr->unix_pad->file_line_no);
      if (dspl_descr->cursor_buff->which_window == UNIXCMD_WINDOW)
         {
            dm_position(dspl_descr->cursor_buff,
                        dspl_descr->unix_pad,
                        dspl_descr->unix_pad->file_line_no,
                        dspl_descr->unix_pad->file_col_no);
         }
      redraw_needed |= (FULL_REDRAW & UNIXCMD_MASK);
   }

return(redraw_needed);

} /* end of dm_wh */
#endif


/************************************************************************

NAME:      dm_position  -  Move cursor to a row and col in the file.

PURPOSE:    This routine will move the cursor to a designated location in the file.
            This will put the cursor in the main window.

PARAMETERS:

   1.  cursor_buff   - pointer to BUFF_DESCR (INPUT / OUTPUT)
               This is a pointer to the buffer / window description
               showing the current state of the window.

   2.  target_win_buff   - pointer to PAD_DESCR (INPUT)
               This is a pointer to the to the buffer description of the
               window we will move into.

   3.  to_line   - int (INPUT)
               This is the line number in the file to move to.

   4.  to_col    - int (INPUT)
               This is the column number in the file to move to.
               This is the column on the line rather than the one of the window.



FUNCTIONS :

   1.   Set up the easy field in the new cursor buff by copying them from the]
        main window description.

   2.   Determine whether the line we want is currently displayed.  If not
        make it the first line in the window.  If so, position the description
        of the cursor over it.

   3.   Determine the horizontal position of the cursor from the column number.

OUTPUTS:
   redraw -  The mask anded with the type of redraw needed is returned.

NOTES:
   1.  Any line changes must have been saved back to the memdata structure before
       this call as get_line_by_num is used.
        
   2.  This routine moves the pad in addition to moving the cursor, so it
       modifies the current window buffer first_line and first_char.


*************************************************************************/

int dm_position(BUFF_DESCR   *cursor_buff,
                PAD_DESCR    *target_win_buff,
                int           target_line,
                int           target_col)
{
int                pix_len;
int                left_side_pixlen;
int                redraw_needed = 0;
char               work[MAX_LINE];
char              *untab_line;
int                untab_first_char;
int                untab_target_col;
int                i;
int                temp_pix;
int                need_vertical_scroll;


DEBUG5(fprintf(stderr, "@dm_position: file [%d,%d] in %s\n", target_line, target_col, which_window_names[target_win_buff->which_window]);)

/***************************************************************
*  Make sure the row / col are in bounds.
***************************************************************/
if (target_line < 0)
   target_line = 0;

if (target_line >=( total_lines(target_win_buff->token) + target_win_buff->window->lines_on_screen))
   {
      target_line = total_lines(target_win_buff->token) - 1;
      target_win_buff->buff_ptr = get_line_by_num(target_win_buff->token, target_line);
      target_col = strlen(target_win_buff->buff_ptr);
   }

if (target_col < 0)
   target_col = 0;


/***************************************************************
*  Easy stuff first - just copy data to get us in the right window.
***************************************************************/
if (cursor_buff->which_window != target_win_buff->which_window)
   cursor_buff->up_to_snuff = 0;
cursor_buff->which_window     = target_win_buff->which_window;
cursor_buff->current_win_buff = target_win_buff;

if (cursor_buff->current_win_buff->buff_modified)
   flush(cursor_buff->current_win_buff);

cursor_buff->current_win_buff->file_line_no     = target_line;
cursor_buff->current_win_buff->buff_ptr         = get_line_by_num(target_win_buff->token, target_line);

/***************************************************************
*  We need to work with tabs expanded
*  All the horizontal work is done with tabs expanded and then
*  converted back.
***************************************************************/

if (untab(cursor_buff->current_win_buff->buff_ptr, work, MAX_LINE+1, target_win_buff->display_data->hex_mode) != 0)
   {
      untab_line       = work;
      untab_first_char = tab_pos(cursor_buff->current_win_buff->buff_ptr, cursor_buff->current_win_buff->first_char, TAB_EXPAND, cursor_buff->current_win_buff->display_data->hex_mode);
      untab_target_col = tab_pos(cursor_buff->current_win_buff->buff_ptr, target_col, TAB_EXPAND, cursor_buff->current_win_buff->display_data->hex_mode);
   }
else
   {
      untab_line       = cursor_buff->current_win_buff->buff_ptr;
      untab_first_char = cursor_buff->current_win_buff->first_char;
      untab_target_col = target_col;
   }

/***************************************************************
*
*  Set up the vertical position of the window over the file.
*
***************************************************************/

DEBUG5(fprintf(stderr, "@dm_position: formfeed window line %d\n", cursor_buff->current_win_buff->formfeed_in_window);)
if (cursor_buff->current_win_buff->formfeed_in_window)
   if (target_line >= target_win_buff->first_line &&
       target_line <  (target_win_buff->first_line + cursor_buff->current_win_buff->formfeed_in_window))
      need_vertical_scroll = False;
   else
      need_vertical_scroll = True;
else
   if (target_line >= target_win_buff->first_line &&
       target_line <  (target_win_buff->first_line + cursor_buff->current_win_buff->window->lines_on_screen))
      need_vertical_scroll = False;
   else
      need_vertical_scroll = True;


/* following block changed 3/1/94 to correct find problem */
if (need_vertical_scroll)
   {
      /***************************************************************
      *  Need to move the screen vertically.
      *  Make sure we do not have a blank screen with one line on it.
      ***************************************************************/
      if ((target_line + target_win_buff->window->lines_on_screen) < total_lines(target_win_buff->token)) 
         {
            if ((target_line == target_win_buff->first_line + target_win_buff->window->lines_on_screen) &&
                 !cursor_buff->current_win_buff->formfeed_in_window)
               {
                  /***************************************************************
                  *  If the target line is one past the bottom of the screen,
                  *  put target line as second to the last line on the screen.
                  ***************************************************************/
                  target_win_buff->first_line += 2;
                  cursor_buff->win_line_no     = target_win_buff->window->lines_on_screen - 2;
               }
            else
               {
                  /***************************************************************
                  *  Put target line as first line on screen.
                  ***************************************************************/
                  target_win_buff->first_line = target_line;
                  cursor_buff->win_line_no = 0;
               }
         }
      else
         {
            target_win_buff->first_line = total_lines(target_win_buff->token) - target_win_buff->window->lines_on_screen;
            if (target_win_buff->first_line < 0)
               target_win_buff->first_line = 0;
            target_win_buff->first_line = pb_ff_scan(target_win_buff, target_win_buff->first_line);
            /* handle the case of a form feed near the bottom of the window.  res 1/13/94 */
            if (target_win_buff->first_line > target_line)
               target_win_buff->first_line = target_line;

            cursor_buff->win_line_no = target_line - target_win_buff->first_line;
         }

      /* cursor_buff->current_win_buff->formfeed_in_window = 0; RES 7/12/95, changed to do call to ff_scan */
      ff_scan(target_win_buff, target_win_buff->first_line);
      redraw_needed |= (cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);
   } /* need vertical scroll */
else
   {
      /***************************************************************
      *  Same screen, just move the cursor
      ***************************************************************/
      cursor_buff->win_line_no     = target_line - target_win_buff->first_line;
   }


/***************************************************************
*  Put the cursor on the correct line.  The hot point goes
*  near the bottom of the character.
***************************************************************/
cursor_buff->y = cursor_buff->current_win_buff->window->sub_y + (cursor_buff->current_win_buff->window->line_height * (cursor_buff->win_line_no + 1)) - 2;


/***************************************************************
*
*  Set up the horizontal position of the window over the file.
*
***************************************************************/

/***************************************************************
*  RES 4/4/2003
*  Add test to make sure sub_x is not past the right edge of the
*  window.  This can happen in the UNIX window when there is a
*  wide prompt.  Do not do the short cut in this case.
***************************************************************/
if ((target_col == 0) && (cursor_buff->current_win_buff->window->sub_x < cursor_buff->current_win_buff->window->width))
   {
      /***************************************************************
      *  Special fast case for going to column zero
      ***************************************************************/
      if (target_win_buff->first_char != 0)
         redraw_needed = (cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);

      target_win_buff->first_char  = 0;
      cursor_buff->x               = cursor_buff->current_win_buff->window->sub_x + 1;
      if (cursor_buff->x > (int)(cursor_buff->current_win_buff->window->sub_width+cursor_buff->current_win_buff->window->sub_x))
         cursor_buff->x = (cursor_buff->current_win_buff->window->sub_width+cursor_buff->current_win_buff->window->sub_width) - 3; /* three is an arbitrary amount */
      cursor_buff->win_col_no      = 0;
      target_win_buff->file_col_no = 0;

   }
else
   {
      /***************************************************************
      *  Figure out where the character we want is and where the
      *  left size of the window is in pixels.  pix_len is the
      *  pixel position of the target column relative to the beginning
      *  of the line (independent of the window).  left_side_pixlen
      *  is the pixel position of the start of the window relative to
      *  the beginning of the line.
      ***************************************************************/
      if (cursor_buff->current_win_buff->window->fixed_font)
         {
            pix_len = cursor_buff->current_win_buff->window->fixed_font * untab_target_col;
            left_side_pixlen = cursor_buff->current_win_buff->window->fixed_font * untab_first_char;
         }
      else
         {
            pix_len = window_col_to_x_pixel(untab_line, untab_target_col, 0, 0,
                                            cursor_buff->current_win_buff->window->font,
                                            cursor_buff->current_win_buff->display_data->hex_mode);
            left_side_pixlen = window_col_to_x_pixel(untab_line, untab_first_char, 0, 0,
                                                     cursor_buff->current_win_buff->window->font,
                                                     cursor_buff->current_win_buff->display_data->hex_mode);
         }

      /***************************************************************
      *  If we are off the left or right sides of the screen we must redraw.
      *  Otherwise position the cursor on the existing screen.
      ***************************************************************/
      if (pix_len < left_side_pixlen)  /* need to scroll left */
         {
            DEBUG5(fprintf(stderr, "dm_position: need scroll left, old first char = %d\n", cursor_buff->current_win_buff->first_char);)
            /***************************************************************
            *  Line to short, end if off left side of screen.
            *  Move pointers to full left side and recalculate left side.
            *  Then we only have to worry about scrolling right.
            ***************************************************************/
            target_win_buff->first_char = 0;
            cursor_buff->x              = cursor_buff->current_win_buff->window->sub_x + 1;
            redraw_needed              |= cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW;
            left_side_pixlen = 0;
         }

      if ((pix_len - left_side_pixlen) >= (int)(cursor_buff->current_win_buff->window->sub_width -
          XTextWidth(cursor_buff->current_win_buff->window->font, "x", 1))) /* keep 1 character on screen RES 8/1/95, 1 is arbitrary (looks good) */
         {      
            /***************************************************************
            *  Need to scroll right.  
            ***************************************************************/
            DEBUG5(fprintf(stderr, "dm_position: need scroll right, old first char %d, ", cursor_buff->current_win_buff->first_char);)
            if (cursor_buff->current_win_buff->window->fixed_font)
               cursor_buff->current_win_buff->first_char = untab_target_col - ((cursor_buff->current_win_buff->window->sub_width / cursor_buff->current_win_buff->window->fixed_font)/2);
            else
               {
                  /***************************************************************
                  *  Figure out what the pixlen must be for the left side of
                  *  the screen and add characters till we get to it.
                  ***************************************************************/
                  temp_pix = 0;
                  left_side_pixlen = pix_len - (cursor_buff->current_win_buff->window->sub_width / 2);
                  for (i = 0;
                       temp_pix < left_side_pixlen;
                       i++)
                     temp_pix += XTextWidth(cursor_buff->current_win_buff->window->font, &untab_line[i], 1);
                  cursor_buff->current_win_buff->first_char = i;
               }

            redraw_needed = (cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW);
            DEBUG5(fprintf(stderr, "new first char %d\n", cursor_buff->current_win_buff->first_char);)
         }

      /***************************************************************
      *  Character is on screen, 
      ***************************************************************/
      target_win_buff->file_col_no = target_col;
      set_window_col_from_file_col(cursor_buff); /* input / output */

   } /* end of not special case of target column zero */

DEBUG5(fprintf(stderr, "dm_position: top corner [%d,%d] file [%d,%d] win [%d,%d], (%d,%d)  %s, redraw = 0x%02X\n",
       cursor_buff->current_win_buff->first_line, cursor_buff->current_win_buff->first_char,
       cursor_buff->current_win_buff->file_line_no, cursor_buff->current_win_buff->file_col_no,
       cursor_buff->win_line_no, cursor_buff->win_col_no,
       cursor_buff->x, cursor_buff->y, which_window_names[cursor_buff->which_window], redraw_needed);
)

return(redraw_needed);

} /* end of dm_position */


