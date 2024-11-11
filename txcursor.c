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
*  module txcursor.c
*
*  Routines:
*
*         text_cursor_motion         - Make the text cursor track the mouse cursor.
*         which_line_on_screen       - Convert a Y coordinate to a line number
*         which_char_on_line         - Convert an X coordinate to a character offset
*         which_char_on_fixed_line   - Convert an X coordinate to a character offset for fixed width fonts
*         erase_text_cursor          - Erase the text cursor
*         clear_text_cursor          - Clear the marker that shows where the text cursor is - for when cursor was blasted by screen refresh
*         cursor_in_area             - Check if the text cursor is in an area
*         text_highlight             - Make the highlight area track the text cursor.
*         text_rehighlight           - Redo a cleared text highlight
*         start_text_highlight       - Initialize the highlight environment variables
*         stop_text_highlight        - Erase all text highlight
*         clear_text_highlight       - Erase current record of highlight (screen refresh) retain base for highlight
*         min_chars_on_window        - Get mimimum number of chars to fill a window horizontally
*
*  Internal:
*         highlight_area             - Do the text highlight video processing.
*         highlight_area_r           - Do the rectangular text highlight video processing.
*         check_private_data         - Get the txcursor private data from the display description.
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h      */

#ifndef WIN32
#include <X11/keysym.h>     /* /usr/include/X11/keysym.h */
#endif

#include "buffer.h"
#include "debug.h"
#include "dumpxevent.h"
#include "emalloc.h"
#include "gc.h"
#include "memdata.h"   /* needed for MAX_LINE */
#include "mouse.h"
#include "tab.h"
#include "txcursor.h"
#include "xerrorpos.h"

#define NO_VALUE -500

/***************************************************************
*  
*  Static data hung off the display description.
*
*  The following structure is hung off the display_data field
*  in the display description.
*  
***************************************************************/

typedef struct {
     int               last_x;
     int               last_y;
     unsigned int      last_height;
     unsigned int      last_width;
     int               last_echo;
     Display          *last_display;
     Window            last_window;
     GC                last_xor_gc;

     int               last_win_row;

     int               base_highlight_x;
     int               base_highlight_y ;
     int               last_highlight_x;
     int               last_highlight_y;
     int               highlight_echo_mode;
     int               highlight_mark_off_screen;
     int               rect_highlight_exists;
     Window            highlight_x_window;
     DRAWABLE_DESCR   *highlight_window;
     WINDOW_LINE      *highlight_winlines;
     int               save_highlight_x;
     int               save_highlight_y;

     /* used in which_line_on_screen only  */
     int          last_top_pix;
     int          last_line_height;
     int          last_line_no;
     DRAWABLE_DESCR *last_win;

     /* used in which_char_on_line only */
     int          last_left_pix;
     int          last_char_width;
     int          last_char_pos;
     char        *last_line;
     XFontStruct *last_font;

} TXCURSOR_PRIVATE;


/***************************************************************
*  
*  Prototypes for local routines
*  
***************************************************************/

static void   highlight_area(Display          *display,      /* input  */
                             Window            x_window,     /* input  */
                             DRAWABLE_DESCR   *win,          /* input  */
                             int               low_x,        /* input  */
                             int               low_y,        /* input  */
                             int               high_x,       /* input  */
                             int               high_y,       /* input  */
                             int               r_call,       /* input  */
                             WINDOW_LINE      *win_lines);   /* input  */

static void   highlight_area_r(Display          *display,      /* input  */
                               Window            x_window,     /* input  */
                               DRAWABLE_DESCR   *win,          /* input  */
                               int               base_x,       /* input  */
                               int               base_y,       /* input  */
                               int               old_x,        /* input  */
                               int               old_y,        /* input  */
                               int               new_x,        /* input  */
                               int               new_y,        /* input  */
                               int               off_screen,   /* input  */
                               int               stopping,     /* input  */
                               TXCURSOR_PRIVATE *private);     /* input  */

static TXCURSOR_PRIVATE *check_private_data(DISPLAY_DESCR  *dspl_descr);


/************************************************************************

NAME:      text_cursor_motion

PURPOSE:    This routine will move the text cursor to the character pointed
            to by the passed pointer.

PARAMETERS:

   1.  win   - pointer to DRAWABLE_DESCR (INPUT)
               This is a pointer to the DRAWABLE_DESCR which
               which defines the window being operated on.

   2.  subwindow -  pointer to DRAWABLE_DESCR (INPUT / output)
               This is the pointer to the subwindow.  This is the
               valid part win which will accept character input.
               If NULL is specified, the whole window will
               accept input.

   3.  echo_mode -  int (INPUT)
               This flag indicates whether text highlighting is in effect.
               It is needed to detect change in the highlighting mode.  The
               cursor changes from solid to outline when we go to highlight
               mode.  Thus we need to know which 

   4.  event - pointer to XEvent (INPUT)
               This is the event to be processed.
               Three flavors are actually processed:
               MotionNotify, EnterNotify, and LeaveNotify

   5.  win_row - pointer to int (OUTPUT)
               The line number relative to the top of the window
               of the line under the cursor is returned.
               This is a zero based line number.  That
               is, the first visible line on the screen is line zero.

   6.  win_col - pointer to int (OUTPUT)
               The character offset relative to the left size of the
               window is returned.  This is the character under the
               cursor.  If data has been scrolled off the left side of
               the screen, this is not known.

   7.  window_lines - text data on the current window (INPUT)
               The data on the window is contained in this array of structures.
               The data is the pixel length of each line on the screen.

   8.  dspl_descr - pointer to DISPLAY_DESCR (INPUT)
                This is the current display description used to access the static
                text cursor data.

FUNCTIONS :

   1.   If this is a motion or enter notify, note where the cursor is
        so we can draw it.  Otherwise, show that we will not draw a
        cursor on window.

   3.   If will be drawing a new text cursor, determine its boundaries
        by finding what character the cursor is over and determining
        its origin,width, and height.

   4.   If there is already a cursor out there, get rid of it.

   6.   Xor in the new text cursor and save its location.


*************************************************************************/

void  text_cursor_motion(DRAWABLE_DESCR   *win,          /* input  */
                         int               echo_mode,    /* input  */
                         XEvent           *event,        /* input  */
                         int              *win_row,      /* output */
                         int              *win_col,      /* output */
                         WINDOW_LINE      *window_lines, /* input  */
                         DISPLAY_DESCR    *dspl_descr)   /* input  */
{
int               char_height;
int               char_top_pixel;
int               char_width;
int               char_left_pixel;
char             *line_text;
int               cursor_x;
int               cursor_y;
Display          *current_display;
Window            current_window;
XFontStruct      *font;
int               fixed_font;
unsigned long     temp_pix;
int               tab_win_col;
char              work[MAX_LINE+1];
int               this_line_x;
int               subwindow_bottom = win->sub_y + (win->lines_on_screen * win->line_height);
TXCURSOR_PRIVATE *private;

if ((private = check_private_data(dspl_descr)) == NULL)
   return;

/***************************************************************
*  Determine the type of event and get the x and y pointer
*  location relative to the window we are working in.  For
*  leave events show we don't want a cursor any more.
***************************************************************/

if (event->type == MotionNotify)
   {
      cursor_x = event->xmotion.x;
      cursor_y = event->xmotion.y;
      current_display = event->xmotion.display;
      current_window  = event->xmotion.window;
      DEBUG8(  fprintf(stderr, "text_cursor_motion:  Got MotionNotify for point %d x %d in window 0x%X\n", cursor_x, cursor_y, current_window);)
   }
else
   if (event->type == EnterNotify)
      {
         cursor_x = event->xcrossing.x;
         cursor_y = event->xcrossing.y;
         current_display = event->xcrossing.display;
         current_window  = event->xcrossing.window;
         DEBUG8(   fprintf(stderr, "text_cursor_motion:  Got EnterNotify for point %d x %d in window 0x%X\n", cursor_x, cursor_y, current_window);)
      }
   else
      if (event->type == LeaveNotify)
         {
            cursor_x = NO_VALUE;
            cursor_y = NO_VALUE;
            current_display = event->xcrossing.display;
            current_window  = event->xcrossing.window;
            DEBUG8(   fprintf(stderr, "text_cursor_motion:  Got LeaveNotify for point %d x %d in window 0x%X\n", event->xcrossing.x, event->xcrossing.y, current_window);)
         }
      else
         {
            DEBUG8(   fprintf(stderr, "?  text_cursor_motion:  Got UNEXPECTED event %d\n", event->type);)
            return;
         }

/***************************************************************
*  TEST:  If we are in the top or bottom border, adjust to
*  The first or last row.
***************************************************************/
if ((cursor_y < win->sub_y) && (cursor_y >= 0))
   {
      cursor_y = win->sub_y;
      DEBUG8(fprintf(stderr, "Adjusting y coordinate down to %d\n", cursor_y);)
   }
else
   if ((cursor_y >= (int)subwindow_bottom) && (cursor_y <= (int)win->height))
      {
         cursor_y = subwindow_bottom - 2;
         DEBUG8(fprintf(stderr, "Adjusting y coordinate up to %d\n", cursor_y);)
      }

/***************************************************************
*  If we are not in the text subwindow, there will be no cursor.
*  If we are out in the border, there will be not cursor.
***************************************************************/
if ((cursor_y < win->sub_y) || (cursor_y >= subwindow_bottom) || /* subwindow check */
    (cursor_y < 0)) /* border check */
   {
      cursor_y = NO_VALUE;
   }

font = win->font;
fixed_font = win->fixed_font;


/***************************************************************
*  If the cursor is ok vertically, get the row number
***************************************************************/
if (cursor_y != NO_VALUE)
   {
      *win_row = which_line_on_screen(win,
                                      cursor_y,
                                      &char_top_pixel);
   }
else
   *win_row = -1;

/***************************************************************
*  If we are not in the text subwindow, there will be no cursor.
*  If we are out in the border, there will be not cursor.
***************************************************************/
this_line_x = win->sub_x;
if ((cursor_x < this_line_x) ||
    (cursor_x >= (int)(this_line_x + win->sub_width)) || /* subwindow check */
    (cursor_x < 0) || (cursor_y == NO_VALUE))       /* border check */
#ifdef EXCLUDEP
    (window_lines[*win_row].w_file_lines_here > 1))    /* excluded_lines RS 12/4/95 test added for excluded lines */
#endif
   {
      cursor_x = NO_VALUE;
   }


/***************************************************************
*  If we will put a new cursor on the screen, find it.
***************************************************************/
if (cursor_x != NO_VALUE)
   {
      /***************************************************************
      *  Find the character we need to put the text cursor on.
      ***************************************************************/
      if (fixed_font)
         {
            char_width = fixed_font;

            *win_col = which_char_on_fixed_line(win,             /* input  */
                                                cursor_x,        /* input  */
                                                char_width,      /* input  */
                                               &char_left_pixel);/* output */
            
         }
      else 
         {
            if (window_lines[*win_row].tabs)
               {
                  (void) untab(window_lines[*win_row].line, work, MAX_LINE+1, dspl_descr->hex_mode);
                  line_text = work + window_lines[*win_row].w_first_char;
               }
            else
               line_text = window_lines[*win_row].line + window_lines[*win_row].w_first_char;
            DEBUG8(fprintf(stderr, "cur line = \"%s\" winline first char=%d\n", (line_text ? line_text : "<NULL>"), window_lines[*win_row].w_first_char);)

            *win_col = which_char_on_line(win,             /* input  */
                                          font,            /* input  */
                                          cursor_x,        /* input  */
                                          line_text,       /* input  */
                                          &char_left_pixel,/* output  */
                                          &char_width);    /* output  */
            /***************************************************************
            *  Added 11/2/93 RES
            *  If the line was null, assume a single blank.
            ***************************************************************/
            if (*win_col < 0)
               {
                  *win_col = 0;
                  char_width = font->min_bounds.width;
                  char_left_pixel = win->sub_x;
               }
         }

      /***************************************************************
      *  Deal with tabs.  If the tab moves the cursor recalculate the
      *  left pixel value.
      ***************************************************************/
      if (window_lines[*win_row].tabs)
         {
           tab_win_col = tab_cursor_adjust(window_lines[*win_row].line, *win_col, window_lines[*win_row].w_first_char, TAB_WINDOW_COL, dspl_descr->hex_mode);
           if (tab_win_col != *win_col)
              {
                 *win_col = tab_win_col;
                 if (fixed_font)
                    char_left_pixel = (fixed_font * tab_win_col) + win->sub_x;
                 else
                    char_left_pixel = XTextWidth(font, line_text, tab_win_col) + win->sub_x;
              }
         }

      char_height = font->ascent + font->descent;

      DEBUG8(   fprintf(stderr, "text_cursor_motion:  New cursor %dx%d+%d+%d [%d,%d] \n",
                char_width, char_height, char_left_pixel, char_top_pixel, *win_row, *win_col);
      )

      /***************************************************************
      *  If this is the same character, do nothing
      ***************************************************************/
      if (private->last_x == char_left_pixel && private->last_y == char_top_pixel && private->last_echo == echo_mode)
         {
            DEBUG8(   fprintf(stderr, "text_cursor_motion:  Same cursor as last time\n");)
            return;
         }
   } /* cursor_x != No value */
else
   {
      *win_col = -1;
      DEBUG8(   fprintf(stderr, "text_cursor_motion:  No new cursor to be generated\n");)
   }

/***************************************************************
*  If there is an old box cursor, get rid of it.
***************************************************************/

if (private->last_x != NO_VALUE)
   {
      DEBUG8(   fprintf(stderr, "text_cursor_motion:  Getting rid of old cursor %dx%d+%d+%d  \n", private->last_width, private->last_height, private->last_x, private->last_y);)

      DEBUG9(XERRORPOS)
      if (private->last_echo)
         XDrawRectangle(private->last_display,
                        private->last_window,
                        private->last_xor_gc,
                        private->last_x,     private->last_y,
                        private->last_width, private->last_height);
      else
         XFillRectangle(private->last_display,
                        private->last_window,
                        private->last_xor_gc,
                        private->last_x,     private->last_y,
                        private->last_width, private->last_height);

   }
else
   DEBUG8(   fprintf(stderr, "text_cursor_motion:  No old cursor to get rid of\n");)

private->last_echo = echo_mode;

/***************************************************************
*  Setup the new box text cursor.
***************************************************************/

if (cursor_x != NO_VALUE)
   {

      /***************************************************************
      *  Draw the new box cursor, if we are not leaving the window.
      ***************************************************************/

      DEBUG14(
         temp_pix = pix_value(dspl_descr->display, current_window, cursor_x, cursor_y);
         fprintf(stderr, "Before pixel value = %d (0x%X)\n", temp_pix, temp_pix);
      )
      DEBUG9(XERRORPOS)
      if (private->last_echo)
         XDrawRectangle(current_display,
                        current_window,
                        win->xor_gc,
                        char_left_pixel, char_top_pixel,
                        char_width,      char_height);
      else
         XFillRectangle(current_display,
                        current_window,
                        win->xor_gc,
                        char_left_pixel, char_top_pixel,
                        char_width,      char_height);
      DEBUG8(fprintf(stderr, "text_cursor_motion:  Built new cursor.\n");)
      DEBUG14(
         temp_pix = pix_value(dspl_descr->display, current_window, cursor_x, cursor_y);
         fprintf(stderr, "After pixel value = %d (0x%X)\n", temp_pix, temp_pix);
      )

      /***************************************************************
      *  Remember where we put the cursor, so we don't loose it.
      ***************************************************************/
      private->last_x = char_left_pixel;
      private->last_y = char_top_pixel;
      private->last_win_row = *win_row;
      private->last_height = char_height;
      private->last_width = char_width;
      private->last_display = current_display;
      private->last_window  = current_window;
      private->last_xor_gc  = win->xor_gc;
   }  /* cursor_x is not NO_VALUE (leaving window) */
else
   {
      private->last_x = NO_VALUE;
      private->last_y = NO_VALUE;
      private->last_win_row = -1;
      private->last_height = NO_VALUE;
      private->last_width = -500;
      if (dspl_descr->which_cursor != NORMAL_CURSOR)
         normal_mcursor(dspl_descr);
      DEBUG8(   fprintf(stderr, "text_cursor_motion:  No new cursor to build.\n");)
   }

} /* end of text_cursor_motion  */


/************************************************************************

NAME:      clear_text_cursor

PURPOSE:    This routine will move the text cursor to the character pointed
            to by the passed pointer.

PARAMETERS:

   1.  window - Window (Xlib type) (INPUT)
               This is the window that is being redrawn or otherwise modfied.
               The cursor clear only occurs if this is the window the cursor
               is in.  The clear can be forced by passing None as the
               value of window.  None is an Xlib defined constant.

   2.  dspl_descr - pointer to DISPLAY_DESCR (INPUT)
                This is the current display description used to access the static
                text cursor data.



FUNCTIONS :

   1.   If the passed window matches the window that the cursor is
        currently in, clear the values which show that the cursor exists.


*************************************************************************/

void clear_text_cursor(Window         window,
                       DISPLAY_DESCR *dspl_descr)
{
TXCURSOR_PRIVATE *private;

if ((private = check_private_data(dspl_descr)) == NULL)
   return;


DEBUG10(fprintf(stderr, "@clear_text_cursor:  "); )
if ((window == private->last_window) || (window == None)) /* added None 9/29/93 RES */
   {
      DEBUG10(fprintf(stderr, "  doing clear in window 0x%X\n", private->last_window);)
      DEBUG8(   fprintf(stderr, "Clearing old text cursor data in window 0x%X\n", window);)
      private->last_x = NO_VALUE;
      private->last_y = NO_VALUE;
      private->last_height = NO_VALUE;
      private->last_width = NO_VALUE;
      private->last_win_row = -1;
   }
else
   {
      DEBUG10(fprintf(stderr, "  Different window, not doing clear current window: 0x%X, cursor window 0x%X\n", window, private->last_window);)
   }



} /* end of clear_text_cursor  */



/************************************************************************

NAME:      erase_text_cursor

PURPOSE:    This routine will erase the cursor and clear the variables
            which show where it is.

PARAMETERS:

   none

GLOBAL DATA:

   The variables last_* are static variables global to this module
   which remember where the cursor is and what state text
   highlighting is in.  

FUNCTIONS :

   1.   
        currently in, clear the values which show that the cursor exists.


*************************************************************************/

void erase_text_cursor(DISPLAY_DESCR *dspl_descr)
{
TXCURSOR_PRIVATE *private;

if ((private = check_private_data(dspl_descr)) == NULL)
   return;

/***************************************************************
*  If there is an old box cursor, get rid of it.
***************************************************************/
DEBUG10(fprintf(stderr, "@erase_text_cursor\n"); )

if (private->last_x != NO_VALUE)
   {
      DEBUG8(   fprintf(stderr, "erase_text_cursor: Getting rid of old cursor %dx%d+%d+%d  \n", private->last_width, private->last_height, private->last_x, private->last_y);)

      DEBUG9(XERRORPOS)
      if (private->last_echo)
         XDrawRectangle(private->last_display,
                        private->last_window,
                        private->last_xor_gc,
                        private->last_x,     private->last_y,
                        private->last_width, private->last_height);
      else
         XFillRectangle(private->last_display,
                        private->last_window,
                        private->last_xor_gc,
                        private->last_x,     private->last_y,
                        private->last_width, private->last_height);

      private->last_x = NO_VALUE;
      private->last_y = NO_VALUE;
      private->last_height = NO_VALUE;
      private->last_width = NO_VALUE;
      private->last_win_row = -1;

   }
else
   DEBUG8(   fprintf(stderr, "erase_text_cursor: No old cursor to get rid of\n");)


} /* end of erase_text_cursor  */



/************************************************************************

NAME:      which_line_on_screen

PURPOSE:    This routine will translate a y coordinate to a line number
            on a text filled window or subarea of a window.

PARAMETERS:

   1.  win  -  pointer to DRAWABLE_DESCR (input)
               This is the DRAWABLE_DESCR for the window the cursor is in.

   2.  subwindow -  pointer to DRAWABLE_DESCR (input)
               This is the pointer to the subwindow.  This is the
               valid part win which will accept character input.
               If NULL is specified, the whole window will
               accept input.  For the purposes of this routine,
               sub_window is an area inside the real window.
               The y value is relative to the real window and
               must be adjusted to deal with the sub window.
               The main window is not passed, because it is not
               needed in this case.

   3.  y     - int (input)
               This is the y value relative to the base window which
               received the event .  This is the value returned in
               the event structure.

   4.  line_top_pix - int (output)
               This is the y pixel value to the top of the line
               which is in.


FUNCTIONS :

   1.   If the subwindow was specified, adjust they value for the
        subwindow.

   2.   Determine the height in pixels of one line.

   3.   Calculate the line number from this value.

OUTPUTS:
   returned_value - int
        The zero based text line number which contains the passed coordinate
        y coordinate is returned.

*************************************************************************/

int   which_line_on_screen(DRAWABLE_DESCR   *win,          /* input  */
                           int               y,            /* input  */
                           int              *line_top_pix) /* output  */
{
int                 line_height;
int                 line_no;

static int          last_top_pix = NO_VALUE;
static int          last_line_height = NO_VALUE;
static int          last_line_no = NO_VALUE;
static DRAWABLE_DESCR *last_win = (DRAWABLE_DESCR *)NO_VALUE;

/***************************************************************
*  Check for the same line
*  This optimization is a little shakey in the case that
*  the cursor moves to the same y coordinate on a different
*  window which uses the same font description.  (unlikely)
***************************************************************/

if (win == last_win && y >= last_top_pix &&  y < (last_top_pix+last_line_height))
   {
      *line_top_pix = last_top_pix;
      return(last_line_no);
   }


/***************************************************************
*  The passed y is relative to the window passed.  If a sub-window
*  is specified, adjust by the y offset of the sub-sindow.
*  Then grab the height of a line in pixels.
***************************************************************/

y -= win->sub_y; /* y is now relative to sub window */
line_height = win->line_height;

line_no = y / line_height;

*line_top_pix = line_no * line_height;
*line_top_pix += win->sub_y; /* line_top_pix is now relative to main window */

last_line_no     =  line_no;
last_top_pix     = *line_top_pix;
last_win         =  win;
last_line_height =  line_height;

return(line_no);

} /* end of which_line_on_screen  */



/************************************************************************

NAME:      which_char_on_line

PURPOSE:    This routine will translate an x coordinate to a char string
            offset.

PARAMETERS:

   1.  window -  pointer to DRAWABLE_DESCR (input / output)
               This is the pointer to the subwindow.  This is the
               valid part win which will accept character input.
               If NULL is specified, the whole window will
               accept input.  For the purposes of this routine,
               sub_window is an area inside the real window.
               The x value is relative to the real window and
               must be adjusted to deal with the sub window.
               The main window is not passed, because it is not
               needed in this case.

   2.  font -  pointer to XFontStruct (input)
               This is the font data for the font being used in the
               window.

   3.  x     - int (input)
               This is the x value relative to the base window which
               received the event .  This is the value returned in
               the event structure.

   4.  line  - pointer to char (input)
               This is the line which is displayed on the window.

   5.  char_left_pix - pointer to int (output)
               This is the x pixel value to the left side of the char.

   6.  char_width - pointer to int (output)
               This is the width in pixels of the char under the pointer.




FUNCTIONS :

   1.   If the bwindow was specified, adjust the x value for the
        subwindow.

   2.   Determine if any of the string is actually displayed, (it may
        all be scrolled off the left of the screen) and determine the
        length in pixels of the displayed portion of the string.

   3.   If x value is within the string, calculate which character
        it is in.

   4.   If the x value is past the end of the string, calculate how
        many blanks have to be added to the string to get to the
        requested location.  Add these in to get the char offset.


OUTPUTS:
   returned_value - int
        The zero based character postition which contains the passed
        x coordinate is returned.


*************************************************************************/

int   which_char_on_line(DRAWABLE_DESCR   *window,         /* input  */
                         XFontStruct      *font,           /* input  */
                         int               x,              /* input  */
                         char             *line,           /* input  */
                         int              *char_left_pix,  /* output  */
                         int              *char_width)     /* output  */
{
int             char_pos;
int             len;
int             pix_len;
int             cumm_pix_len = 0;
int             prev_cumm_pix_len;
int             blank_len;
int             blank_char_count;

static int          last_left_pix = NO_VALUE;
static int          last_char_width = NO_VALUE;
static int          last_char_pos = NO_VALUE;
static char        *last_line = NULL;
static XFontStruct *last_font = (XFontStruct *)NO_VALUE;

/***************************************************************
*  Check for the same line
*  This optimization is a little shakey in the case that
*  the cursor moves to the same y coordinate on a different
*  window which uses the same font description.  (unlikely)
***************************************************************/

if ((last_line == line) && (font == last_font) && (x >= last_left_pix) &&  (x < (last_left_pix+last_char_width)))
   {
      *char_left_pix = last_left_pix;
      *char_width    = last_char_width;
      return(last_char_pos);
   }


/***************************************************************
*  The passed x is relative to the main window.  If a sub-window
*  is specified, adjust by the x offset of the sub-sindow.
***************************************************************/

if (window != NULL)
   x -= window->sub_x; /* x is now relative to sub window */

/***************************************************************
*
*  Check the string
*  First check for the pointer past the end of the string or
*  the string scrolled off the left side of the screen.
*
***************************************************************/

if (line != NULL)
   len = strlen(line);
else
   {
      len = 0;
      line = "";
   }

if (len > 0)
   {
      pix_len = XTextWidth(font, line, len);
   }
else
   {
      pix_len = 0;
   }

/***************************************************************
*
*  Compare the displayed string length in pixels to the x
*  coordinate.  If it is greater than the x value.  The pointer
*  is somewhere in the string.  If it is less than the string,
*  there is some number of blanks we would need to add to the
*  string to get to this spot.
*
***************************************************************/

if (pix_len > x)
   {
      for (char_pos = 0;
           (cumm_pix_len <= x) && (char_pos < len);
           char_pos++)
      {
         prev_cumm_pix_len = cumm_pix_len;
         cumm_pix_len += XTextWidth(font, &line[char_pos], 1);
      }
      char_pos--;
      *char_width = cumm_pix_len - prev_cumm_pix_len;
      if (debug && char_pos == len)
         fprintf(stderr, "?  unexpected condition, ##12\n");
      *char_left_pix = prev_cumm_pix_len;
   }
else
   {
      blank_len = XTextWidth(font, " ", 1);
      if (blank_len == 0)
         {
            if (debug)
               fprintf(stderr, "?  unexpected condition, ##13\n");
            blank_len = 1;
         }
      blank_char_count = (x - pix_len) / blank_len;
      char_pos = len + blank_char_count;
      *char_width = blank_len;
      *char_left_pix = (blank_char_count * blank_len) + pix_len;
   }

if (window != NULL)
   *char_left_pix +=  window->sub_x; /* Put fixed offset back */



last_char_pos    =  char_pos;
last_left_pix    = *char_left_pix;
last_font        =  font;
last_char_width  =  *char_width;
last_line        = line;

return(char_pos);

} /* end of which_char_on_line  */




/************************************************************************

NAME:      which_char_on_fixed_line

PURPOSE:    This routine will translate an x coordinate to a char string
            offset.

PARAMETERS:

   1.  window -  pointer to DRAWABLE_DESCR (input / output)
               This is the pointer to the window.  This is the
               valid part win which will accept character input.
               If NULL is specified, the whole window will
               accept input.  For the purposes of this routine,
               sub_window is an area inside the real window.
               The x value is relative to the real window and
               must be adjusted to deal with the sub window.
               The main window is not passed, because it is not
               needed in this case.

   2.  x     - int (input)
               This is the x value relative to the base window which
               received the event .  This is the value returned in
               the event structure.

   3.  width - int (input)
               This is the fixed width of each char.

   4.  char_left_pix - pointer to int (output)
               This is the x pixel value to the left side of the char.




FUNCTIONS :

   1.   If the window was specified, adjust the x value for the
        subwindow.

   2.   calculate the character number (zero based).

   3.   Calculate the pixel value of the start of that character.
        Adjust for the subwindow if necessary.


OUTPUTS:
   returned_value - int
        The zero based character postition which contains the passed
        x coordinate is returned.


*************************************************************************/

int   which_char_on_fixed_line(DRAWABLE_DESCR   *window,      /* input  */
                               int               x,              /* input  */
                               int               width,          /* input  */
                               int              *char_left_pix)  /* output  */
{
int             char_pos;

/***************************************************************
*  The passed x is relative to the main passed.  If a sub-window
*  is specified, adjust by the x offset of the sub-sindow.
***************************************************************/

if (window != NULL)
   x -= window->sub_x; /* x is now relative to sub window */

char_pos = x / width;
*char_left_pix = char_pos * width;

if (window != NULL)
   *char_left_pix += window->sub_x; /* char_left_pix is now relative to main window */

return(char_pos);

} /* end of which_char_on_fixed_line  */


/************************************************************************

NAME:      highlight_area

PURPOSE:    This performs non-rectangular text highlighting over a region
            of text.  It turnes out that highlighting and un-highlighting
            are done the exact same way (XOR), and thus this routine
            is called for both.

PARAMETERS:

   1.  display       - Display (Xlib type) (INPUT)
                       This is the display token needed for Xlib calls.

   2.  x_window - Window (Xlib type) (INPUT)
                       This is the X window id of the window being highlighted.

   3.  subwindow -  pointer to DRAWABLE_DESCR (INPUT)
                This is the pointer to the subwindow.  This is the
                valid part win which will accept character input.
                If NULL is specified, the whole window will
                accept input, so we are always in the subwindow.

   4.  low_x  - int (INPUT)
                This is the x coordinate of the start of a character position.
                It goes with low_y and defines the start of the area to be higlighted.
                It is the point closer to the upper left corner of the screen.
                Vertical (y) takes precedence over horizontal (x) in determining
                closer to the top.

   5.  low_y  - int (INPUT)
                This is the y coordinate of the top of a character position.
                It goes with low_x and defines the start of the area to be higlighted.
                It is the point closer to the upper left corner of the screen.
                Vertical (y) takes precedence over horizontal (x) in determining
                closer to the top.

   6.  high_x - int (INPUT)
                This is the x coordinate of the start of a character position.
                 It goes with high_y and defines the end of the area to be higlighted.
                It is the point farther from the upper left corner of the screen.
         
   7.  high_y - int (INPUT)
                This is the y coordinate of the top of a character position.
                It goes with high_x and defines the end of the area to be higlighted.
                It is the point farther from the upper left corner of the screen.

   8.  r_call - int (INPUT)
                This flag indicates that highlight_area is being called from 
                highlight_area_r.  In this case, the low_x value is the lowest
                x value that is displayed.

   9.  window_lines - text data on the current window (input)
               The data on the window is contained in this array of structures.
               The data is the pixel length of each line on the screen.





FUNCTIONS :

   1.   Get the relevant data from the drawable descriptions into local variables.

   2.   If the r_call flag is set, set the starting X point on each line to the start of
        the low_x value passed.

   3.   Detemine which window line number the y value corresponds to.  This is gives us
        the initial index into the win_lines array.

   4.   Highlight (or unhighlight) each line in the range.  Note that highlighting and
        un-highlighting are done the same way so it is not important to know which it is
        we are doing.


*************************************************************************/

static void   highlight_area(Display          *display,      /* input  */
                             Window            x_window,     /* input  */
                             DRAWABLE_DESCR   *win,          /* input  */
                             int               low_x,        /* input  */
                             int               low_y,        /* input  */
                             int               high_x,       /* input  */
                             int               high_y,       /* input  */
                             int               r_call,       /* input  */
                             WINDOW_LINE      *win_lines)    /* input  */
{
int                      low_line_no;
XFontStruct             *font;
int                      fixed_font;
int                      x;
int                      y;
int                      y_prime;
int                      width;
int                      first_pixlen;

/***************************************************************
*  
*  Find the line number for the low y value.
*  This is needed for indexes into the win_lines array.
*  
***************************************************************/
low_line_no = which_line_on_screen(win,
                                   low_y,
                                   &y_prime);  /* junk parm */


font       = win->font;
fixed_font = win->fixed_font;


x          = win->sub_x;

/***************************************************************
*  
*  If this is a call from the rectangular highlight, low X
*  is the start for all the lines.
*  
***************************************************************/

if (r_call)
   x = low_x;

/***************************************************************
*  
*  Get the first pixlen up front into a local variable.  This
*  handles the case of the dminput and output windows which
*  do not have the pixlens stored.  (value is -1).
*  
***************************************************************/

if (win_lines[low_line_no].pix_len == -1)  /* special value for dminput and output windows */
   {
      fprintf(stderr, "Har dee Har Har\n");
      if (fixed_font)
         first_pixlen = fixed_font * strlen(win_lines[low_line_no].line+win_lines[low_line_no].w_first_char);
      else
         first_pixlen = XTextWidth(font, win_lines[low_line_no].line+win_lines[low_line_no].w_first_char, strlen(win_lines[low_line_no].line+win_lines[low_line_no].w_first_char));
   }
else
   first_pixlen = win_lines[low_line_no].pix_len;

/***************************************************************
*  
*  Examine the low and high y values to see if we are on the same
*  line.  Since these are calculated values they are always
*  the same for a given line.  That is, while the character is
*  several pixels high, the y value always points to the top pixel
*  on for the line.
*  
***************************************************************/

if (low_y == high_y)
   {
      /***************************************************************
      *  Special case, same line.
      ***************************************************************/
      if (high_x > first_pixlen + x)
         width = first_pixlen - (low_x - x);
      else
         width = high_x - low_x;
      if (width > 0)
         {
            DEBUG9(XERRORPOS)
            XFillRectangle(display,
                           x_window,
                           win->xor_gc,
                           low_x, high_y,
                           width, win->line_height);
            DEBUG25(fprintf(stderr, "highlight_area(sl):  filled area x: %d, y: %d, width : %d, height: %d, line: %d\n",
                                    low_x, high_y, width, win->line_height, low_line_no);)
         }
   }
else
   {
      /***************************************************************
      *  First partial line:
      ***************************************************************/
      width = first_pixlen - (low_x - x);
      if (r_call)
         {
            width -= (x - win->sub_x);
            if (width <= 0)
               width = win->font->min_bounds.width;
         }
      if (width > 0)
         {
            if ((unsigned)width > win->sub_width) /* res - 1/15/93  elimitate very wide highlights */
               width = win->sub_width;
            DEBUG9(XERRORPOS)
            XFillRectangle(display,
                           x_window,
                           win->xor_gc,
                           MAX(low_x, win->sub_x), low_y,
                           width, win->line_height);
            DEBUG25(fprintf(stderr, "highlight_area(fl):  filled area x: %d, y: %d, width : %d, height: %d\n",
                                    low_x, low_y, width, win->line_height);)
         }
      low_line_no++;
      if (!r_call)
         x  = win->sub_x;

      /***************************************************************
      *  Get the intermediate lines.
      ***************************************************************/
      for (y = low_y + win->line_height;
           y < high_y;
           y += win->line_height)
      {
         width = win_lines[low_line_no].pix_len;
         if (r_call)
            {
               width -= (x - win->sub_x);
               if (width <= 0)
                  width = win->font->min_bounds.width;
            }
         if (width > 0)
            {
               if ((unsigned)width > win->sub_width) /* res - 1/15/93  elimitate very wide highlights */
                  width = win->sub_width;
               DEBUG9(XERRORPOS)
               XFillRectangle(display,
                              x_window,
                              win->xor_gc,
                              x, y,
                              width, win->line_height);
               DEBUG25(fprintf(stderr, "highlight_area(ml):  filled area x: %d, y: %d, width : %d, height: %d, line: %d\n",
                                       x, y, width, win->line_height, low_line_no);)
            }
         low_line_no++;
      }
      /***************************************************************
      *  Last Line
      ***************************************************************/
      if (high_x > win_lines[low_line_no].pix_len + x)
         width = win_lines[low_line_no].pix_len;
      else
         width = high_x - x;
      if (width > 0)
         {
            if ((unsigned)width > win->sub_width) /* res - 1/15/93  elimitate very wide highlights */
               width = win->sub_width;
            DEBUG9(XERRORPOS)
            XFillRectangle(display,
                           x_window,
                           win->xor_gc,
                           x, high_y,
                           width, win->line_height);
            DEBUG25(fprintf(stderr, "highlight_area(ll):  filled area x: %d, y: %d, width : %d, height: %d, line: %d\n",
                                    x, high_y, width, win->line_height, low_line_no);)
         }
   }  /* end of not same line */

} /* end of highlight_area */



/************************************************************************

NAME:      highlight_area_r

PURPOSE:    This routine highlights an area with a rectangle except in the
            special case of a zero width rectangle.  In this case the text
            is highlighted using normal highlighting except that the 
            highlighted area never goes to the left of the X position.

PARAMETERS:

   1.  display       - Display (Xlib type) (INPUT)
                       This is the display token needed for Xlib calls.

   2.  x_window - Window (Xlib type) (INPUT)
                       This is the X window id of the window being highlighted.

   3.  subwindow -  pointer to DRAWABLE_DESCR (INPUT)
                This is the pointer to the subwindow.  This is the
                valid part win which will accept character input.
                If NULL is specified, the whole window will
                accept input, so we are always in the subwindow.

   4.  base_x  - int (INPUT)
                This is the x coordinate of the start of a character position.
                It goes with base_y and determines the fixed mark point from which the
                rectagular highlighting is done.

   5.  base_y  - int (INPUT)
                This is the y coordinate of the top of a character position.
                It goes with base_x and defines the fixed mark point from which the
                rectagular highlighting is done.

   6.  old_x  - int (INPUT)
                This is the x coordinate of the start of a character position.
                It goes with old_y and along with base_x and y to define the existing
                rectangular highlight.  This box will be unhighlighted.

   7.  old_y  - int (INPUT)
                This is the y coordinate of the top of a character position.
                It goes with old_y and along with base_x and y to define the existing
                rectangular highlight.  This box will be unhighlighted.

   8.  new_x  - int (INPUT)
                This is the x coordinate of the start of a character position.
                It goes with new_y and along with base_x and y define the new rectangle to
                be highlighted.
         
   9.  new_y  - int (INPUT)
                This is the y coordinate of the top of a character position.
                It goes with new_y and along with base_x and y define the new rectangle to
                be highlighted.

   10. off_screen - int (INPUT)
                This flag indicates that the real mark is off the left side of the screen
                and the special highlight is not to be used.

   11. stopping - int (INPUT)
                This flag indicates that the routine is being called from stop text highlight
                instead of text highlight.  It is only useful when the new_x and new_y equal the
                old_x and old_y.  In the case of stopping, we want to erase the old highlight and
                leave it go at that.  If we are not stoping and are at the base point, we want to
                highlight the first line.

   12. window_lines - text data on the current window (input)
               The data on the window is contained in this array of structures.
               The data is the pixel length of each line on the screen.
               It is not used by this routine but is passed to highlight_area
               if that kind of call is needed.

GLOBAL DATA:
   1.  rect_highlight_exists  -  int
               This flag is used in rectangular highlighting to indicate that the highlight exists.
               This flag is needed because, unlike regular highlighting, if the base point and the
               cursor point are the same, the line the cursor is on is highlighted.


FUNCTIONS :

   1.   Erase the old box if it exists.

   2.   Draw the new box if is not of zero size.

   3.   If the new_x or old_x coorinate is the same as the base, then a special
        type of highlighting is done.  This is accomplished with a call to highlight_area.


*************************************************************************/

static void   highlight_area_r(Display          *display,      /* input  */
                               Window            x_window,     /* input  */
                               DRAWABLE_DESCR   *win,          /* input  */
                               int               base_x,       /* input  */
                               int               base_y,       /* input  */
                               int               old_x,        /* input  */
                               int               old_y,        /* input  */
                               int               new_x,        /* input  */
                               int               new_y,        /* input  */
                               int               off_screen,   /* input  */
                               int               stopping,     /* input  */
                               TXCURSOR_PRIVATE *private)      /* input  */

{

/***************************************************************
*  
*  First get rid of the old rectangle if there is one.
*  
***************************************************************/

if (private->rect_highlight_exists)
   {
      DEBUG9(XERRORPOS)
      if ((old_x != base_x) || off_screen)
         XFillRectangle(display,
                        x_window,
                        win->xor_gc,
                        MIN(base_x, old_x), MIN(base_y, old_y),
                        ABS2(old_x, base_x), (ABS2(old_y, base_y) + win->line_height));
      else
         highlight_area(display, x_window, win,
                        MIN(base_x, old_x), MIN(base_y, old_y),
                        MAX(base_x, old_x), MAX(base_y, old_y)+win->line_height,
                        1, private->highlight_winlines);
      private->rect_highlight_exists = 0;
      DEBUG8(
      fprintf(stderr, "highlight_area_r:  unhighlight area x: %d, y: %d, width : %d, height: %d\n",
                       MIN(base_x, old_x), MIN(base_y, old_y), ABS2(old_x, base_x), (ABS2(old_y, base_y) + win->line_height));
      )

   } /* end of something to erase */

/***************************************************************
*  
*  Draw the new rectangle
*  
***************************************************************/

if (new_x != base_x || new_y != base_y || !stopping)
   {
      DEBUG9(XERRORPOS)
      if ((new_x != base_x) || off_screen)
         XFillRectangle(display,
                        x_window,
                        win->xor_gc,
                        MIN(base_x, new_x), MIN(base_y, new_y),
                        ABS2(new_x, base_x), (ABS2(new_y, base_y) + win->line_height));
      else
         highlight_area(display, x_window, win,
                        MIN(base_x, new_x), MIN(base_y, new_y),
                        MAX(base_x, new_x), MAX(base_y, new_y)+win->line_height,
                        1, private->highlight_winlines);
      private->rect_highlight_exists = 1;
      DEBUG8(
      fprintf(stderr, "highlight_area_r:   highlight area x: %d, y: %d, width : %d, height: %d\n",
                       MIN(base_x, new_x), MIN(base_y, new_y), ABS2(new_x, base_x), (ABS2(new_y, base_y) + win->line_height));
      )

   } /* end of something to highlight */

} /* end of highlight_area_r */


/************************************************************************

NAME:      text_highlight

PURPOSE:    This is the external entry point used to highlight text.
            It sets up the parameters and calls the routine to do the real work.

PARAMETERS:
   1.  x_window   - Window (Xlib type) (INPUT)
                    This is the X window id of the window being highlighted.

   2.  dspl_descr - pointer to DISPLAY_DESCR(INPUT)
                    This is the current display description.  It is needed
                    to get to the private data for this routine along with
                    the display pointer for X calls.


FUNCTIONS :

   1.   Make sure that there is highlighting going on.  If not just return.
        This should never happen.

   2.   make sure the cursor is in the higlight window and that it is not on the
        same character it was at the last call.

   3.   For rectangular mode, call the special rectangular hightlight routine.

   4.   For normal highlighting, arrange the parms with the top position first
        and the bottom position second and call the hightlight routine to process.

*************************************************************************/

void  text_highlight(Window            x_window,     /* input  */
                     DRAWABLE_DESCR   *window,       /* input  */
                     DISPLAY_DESCR    *dspl_descr)   /* input  */
{
TXCURSOR_PRIVATE *private;

if ((private = check_private_data(dspl_descr)) == NULL)
   return;


DEBUG8(
   fprintf(stderr, "@text_highlight, base (%d,%d)  last (%d,%d)  %s window, echo mode = %d\n",
           private->base_highlight_x, private->base_highlight_y, private->last_x, private->last_y, ((private->highlight_window == window) ? "right" : "wrong"), private->highlight_echo_mode);
)

/***************************************************************
*  If there is no highlighting or the cursor is off screen, do nothing.
***************************************************************/
if (private->base_highlight_y == NO_VALUE || private->last_y == NO_VALUE)
   return;

/***************************************************************
*  If the cursor is in another window, do nothing.
***************************************************************/
if (private->highlight_window != window)
   return;

/***************************************************************
*  Handle rectangular highlight with one routine and regular
*  with the other.
***************************************************************/
if (private->highlight_echo_mode == RECTANGULAR_HIGHLIGHT)
   {
      if (private->last_highlight_y != private->last_y || private->last_highlight_x != private->last_x || !private->rect_highlight_exists)
         highlight_area_r(dspl_descr->display, x_window, window,
                          private->base_highlight_x, private->base_highlight_y,
                          private->last_highlight_x, private->last_highlight_y,
                          private->last_x,           private->last_y,
                          private->highlight_mark_off_screen, 0,
                          private);
   }
else
   {
      /***************************************************************
      *  Only do the highlight if the cursor has moved since last time.
      *  Then handle the same line as a special case.
      ***************************************************************/
      if (private->last_highlight_y != private->last_y || private->last_highlight_x != private->last_x)
         if (private->last_highlight_y == private->last_y )
            { 
               /* stayed on same line */
               if (private->last_highlight_x > private->last_x)
                   highlight_area(dspl_descr->display, x_window, window,
                                  private->last_x,           private->last_y,
                                  private->last_highlight_x, private->last_highlight_y,
                                  0, private->highlight_winlines);
               else
                  highlight_area(dspl_descr->display, x_window, window,
                                 private->last_highlight_x, private->last_highlight_y,
                                 private->last_x,           private->last_y,
                                 0, private->highlight_winlines);
            }
         else
            {
               /* Moved to another line */
               if (private->last_highlight_y > private->last_y)
                  highlight_area(dspl_descr->display, x_window, window,
                                 private->last_x,           private->last_y,
                                 private->last_highlight_x, private->last_highlight_y,
                                 0, private->highlight_winlines);
               else
                  highlight_area(dspl_descr->display, x_window, window,
                                 private->last_highlight_x, private->last_highlight_y,
                                 private->last_x,           private->last_y,
                                 0, private->highlight_winlines);
            }
   }

private->last_highlight_x  = private->last_x;
private->last_highlight_y  = private->last_y;
private->save_highlight_x  = private->last_highlight_x;
private->save_highlight_y  = private->last_highlight_y;

} /* end of text_highlight */


/************************************************************************

NAME:      text_rehighlight

PURPOSE:    This routine is used to refresh highlighting after a complete
            screen redraw.  It uses saved copies of the bounds of the 
            hightlight to get things redrawn.

PARAMETERS:

   1.  x_window   - Window (Xlib type) (INPUT)
                    This is the X window id of the window being highlighted.

   2.  subwindow -  pointer to DRAWABLE_DESCR (INPUT)
                    This is the pointer to the subwindow.  This is the
                    valid part win which will accept character input.
                    If NULL is specified, the whole window will
                    accept input, so we are always in the subwindow.
                    It is passed through to text_highlight.

   3.  dspl_descr - pointer to DISPLAY_DESCR(INPUT)
                    This is the current display description.  It is needed
                    to get to the private data for this routine along with
                    the display pointer for X calls.

FUNCTIONS :

   1.   Make sure that the passed window is the hightligh window
        and that highlighting is turned on.

   2.   Save put the current x and y coordinates on the side and
        replace them with the saved values.

   3.   Call text highlight to draw the highlight.

   4.   Put the current x and y values back.

*************************************************************************/

void  text_rehighlight(Window            x_window,     /* input  */
                       DRAWABLE_DESCR   *window,       /* input  */
                       DISPLAY_DESCR    *dspl_descr)   /* input  */
{
int               hold_x;
int               hold_y;
TXCURSOR_PRIVATE *private;

if ((private = check_private_data(dspl_descr)) == NULL)
   return;


DEBUG8(
   fprintf(stderr, "@text_rehighlight, base (%d,%d)  last (%d,%d)  %s window\n",
           private->base_highlight_x, private->base_highlight_y, private->save_highlight_x, private->save_highlight_y, ((private->highlight_window == window) ? "right" : "wrong"));
)


/***************************************************************
*  If the highlight is in another window, do nothing.
***************************************************************/
if ((private->highlight_window != window) || (private->base_highlight_y == NO_VALUE))
   return;

/***************************************************************
*  Save off the current last_x and last_y and sub int the
*  saved versions.  Call text highlight and then put everything 
*  back.
***************************************************************/
hold_x = private->last_x;
hold_y = private->last_y;

private->last_x = private->save_highlight_x;
private->last_y = private->save_highlight_y;

text_highlight(x_window, window, dspl_descr);

private->last_x = hold_x;
private->last_y = hold_y;


} /* end of text_rehighlight */


/************************************************************************

NAME:      start_text_highlight

PURPOSE:    This routine turns on text highlighting.  It establishes the
            base point from which the highlighting starts.  It is used
            for both regular and rectangular highlighting.

PARAMETERS:

   1.  win    - pointer to DRAWABLE_DESCR (INPUT)
                This is a pointer to the DRAWABLE_DESCR which
                which defines the window being operated on.
                It is the window the cursor is in.

   2.  subwindow -  pointer to DRAWABLE_DESCR (INPUT)
                This is the pointer to the subwindow.  This is the
                valid part win which will accept character input.
                If NULL is specified, the whole window will
                accept input, so we are always in the subwindow.

   3.  win_row -int (INPUT)
                This is the row on the window where the highlighting is
                to start.

   4.  win_col -int (INPUT)
                This is the col on the window where the highlighting is
                to start.  This value can be negative to indicate that
                the actual mark is off the left side of the screen.
                There are differences in the way text is highlighed based
                on this.

   5.  echo_mode -int (INPUT)
                This is the echo mode.  It is one of the values
                REGULAR_HIGHLIGHT or RECTANGULAR_HIGHLIGHT (txcursor.h).
                It tells what type of highlighting to turn on.

   6.  win_lines - pointer to array of WINDOW_LINE (INPUT)
                This is the current window lines structure.  It has
                pixel and text information about the data displayed on the
                screen.

   7.  start_line - pointer char (INPUT)
                This is the line which actually contains the mark.  It may be
                offscreen, but is needed for rectangular highlighting.
                This line is not tab expanded.

   8.  first_char - int (INPUT)
                This is the firstchar for the passed start line.  It is the
                first_char displayed on the screen.


GLOBAL DATA:
   1.  highlight_echo_mode  -  int
               This flag is set by start_text_highlight to indicate whether we are doing
               regular or rectangular highlighting.


FUNCTIONS :

   1.   Set the global variables held withing this module to show the base
        point for the text highlighting.


*************************************************************************/

void  start_text_highlight(Window            x_window,     /* input  */
                           DRAWABLE_DESCR   *window,       /* input  */
                           int               win_row,      /* input  */
                           int               win_col,      /* input  */
                           int               echo_mode,    /* input  */
                           WINDOW_LINE      *win_lines,    /* input  */
                           char             *start_line,   /* input  */
                           int               first_char,   /* input  */
                           DISPLAY_DESCR    *dspl_descr)   /* input  */
{
XFontStruct             *font;
int                      fixed_font;
int                      x;
int                      y;
int                      line_height;
TXCURSOR_PRIVATE *private;

if ((private = check_private_data(dspl_descr)) == NULL)
   return;


DEBUG8(fprintf(stderr, "@start_text_highlight [%d,%d], line \"%s\" echo mode = %d\n", win_row, win_col, start_line, echo_mode);)

font = window->font;
fixed_font = window->fixed_font;
x = window->sub_x;
y = window->sub_y;
line_height = window->line_height;

if (win_col < 0)
   {
      win_col = 0;
      private->highlight_mark_off_screen = 1;
   }
else
   private->highlight_mark_off_screen = 0;

private->base_highlight_y  = (win_row * line_height) + y;


/***************************************************************
*  Calculate the base x position.  For fixed format it is a 
*  straight calculation.  For non-fixed format fonts, if the
*  position is in the line, use the X function to calculate 
*  the x pixel.  If we are past the end of the line, assume
*  blanks past the end of the line.
***************************************************************/
if (fixed_font)
   private->base_highlight_x  = (win_col * fixed_font) + x;
else
   private->base_highlight_x = window_col_to_x_pixel(start_line, /* in tab.c */
                                                     win_col,
                                                     first_char,
                                                     x,
                                                     font,
                                                     dspl_descr->hex_mode);


/***************************************************************
*  Show no cursor motion yet.
***************************************************************/
private->last_highlight_x  = private->base_highlight_x;
private->last_highlight_y  = private->base_highlight_y;
private->highlight_echo_mode = echo_mode;

private->highlight_x_window   = x_window;
private->highlight_window     = window;
private->highlight_winlines   = win_lines;

DEBUG8(fprintf(stderr, "end start_text_highlight base (%d,%d)\n", private->base_highlight_x, private->base_highlight_y);)

/* RES 5/7/94 - Fix flash of old highlighed area - I hope */
/* RES 6/15/94 - Removed because of cursor mess up of highlighting after a tdm which causes a raise window */
/*private->save_highlight_x  = private->base_highlight_x;*/
/*private->save_highlight_y  = private->base_highlight_y;*/

} /* end of start_text_highlight */


/************************************************************************

NAME:      stop_text_highlight

PURPOSE:    This routine will turn off text highlighting by 
            telling the highllight routines that the cursor moved to
            the base position.

PARAMETERS:

   none

GLOBAL DATA:

   The variables last_* are static variables global to this module
   which remember where the cursor is and what state text
   highlighting is in.  

FUNCTIONS :

   1.   Show that the cursor moved to the base position.

   2.   For rectangular highlight we also have to say we are stopping
        to avoid having the zero width highlight rule take effect.

*************************************************************************/

void  stop_text_highlight(DISPLAY_DESCR    *dspl_descr)
{
TXCURSOR_PRIVATE *private;

if ((private = check_private_data(dspl_descr)) == NULL)
   return;

DEBUG8(fprintf(stderr, "@stop_text_highlight\n");)

if (private->base_highlight_x != NO_VALUE)
   if (private->highlight_echo_mode == RECTANGULAR_HIGHLIGHT)
      highlight_area_r(dspl_descr->display, private->highlight_x_window,
                       private->highlight_window,
                       private->base_highlight_x, private->base_highlight_y,
                       private->last_highlight_x, private->last_highlight_y,
                       private->base_highlight_x, private->base_highlight_y,
                       private->highlight_mark_off_screen, 1,
                       private);
   else
      if (private->last_highlight_y == private->base_highlight_y )
         { 
            /* stayed on same line */
               if (private->last_highlight_x > private->base_highlight_x)
                  highlight_area(dspl_descr->display, private->highlight_x_window,
                                 private->highlight_window,
                                 private->base_highlight_x, private->base_highlight_y,
                                 private->last_highlight_x, private->last_highlight_y,
                                 0, private->highlight_winlines);
               else
                  highlight_area(dspl_descr->display, private->highlight_x_window,
                                 private->highlight_window,
                                 private->last_highlight_x, private->last_highlight_y,
                                 private->base_highlight_x, private->base_highlight_y,
                                 0, private->highlight_winlines);
         }
      else
         {
            /* still below base line */
            if (private->last_highlight_y > private->base_highlight_y)
               highlight_area(dspl_descr->display, private->highlight_x_window,
                              private->highlight_window,
                              private->base_highlight_x, private->base_highlight_y,
                              private->last_highlight_x, private->last_highlight_y,
                              0, private->highlight_winlines);
            else
               highlight_area(dspl_descr->display, private->highlight_x_window,
                              private->highlight_window,
                              private->last_highlight_x, private->last_highlight_y,
                              private->base_highlight_x, private->base_highlight_y,
                              0, private->highlight_winlines);
         }

private->base_highlight_x  = NO_VALUE;
private->base_highlight_y  = NO_VALUE;
private->last_highlight_x  = NO_VALUE;
private->last_highlight_y  = NO_VALUE;
/* RES 6/15/94 - Fix flash of old highlighed area - cursor mess up of highlighting after a tdm which causes a raise window */
private->save_highlight_x  = private->base_highlight_x;
private->save_highlight_y  = private->base_highlight_y;

} /* end of stop_text_highlight */


/************************************************************************

NAME:      clear_text_highlight

PURPOSE:    This routine erases the record of text highlighting when
            the highlighing is blown away by a screen refresh.

PARAMETERS:

   none

GLOBAL DATA:

   The variables last_* are static variables global to this module
   which remember where the cursor is and what state text
   highlighting is in.  

FUNCTIONS :

   1.   Save the last position the highlight was in for text_rehighlight

   2.   Show that the cursor moved to the base position.


*************************************************************************/

void  clear_text_highlight(DISPLAY_DESCR *dspl_descr)
{
TXCURSOR_PRIVATE *private;

if ((private = check_private_data(dspl_descr)) == NULL)
   return;

DEBUG8(fprintf(stderr, "@clear_text_highlight\n");)
private->last_highlight_x  = private->base_highlight_x;
private->last_highlight_y  = private->base_highlight_y;
private->rect_highlight_exists = 0;

} /* end of stop_text_highlight */


/************************************************************************

NAME:      cursor_in_area

PURPOSE:    This routine determines if the cursor is in a rectangle
            whose position and dimension are passed in.

PARAMETERS:

   1.  window - (Xlib type) Window (INPUT)
                This is the window that the passed rectangle is in.

   2.  x     -  int (INPUT)
                This is X coordinate of the upper left corner of the
                rectangle to be considered.

   3.  y     -  int (INPUT)
                This is Y coordinate of the upper left corner of the
                rectangle to be considered.

   4.  width -  int (INPUT)
                This is width in pixels of the
                rectangle to be considered.

   5.  height - int (INPUT)
                This is height in pixels of the
                rectangle to be considered.

   6.  dspl_descr - pointer to DISPLAY_DESCR (INPUT)
                This is the current display description used to access the static
                text cursor data.



GLOBAL DATA:

   The variables last_* are static variables global to this module
   which remember where the cursor is and what state text
   highlighting is in.  



FUNCTIONS :

   1.   Determine if the current cursor position (global data) is within
        the rectanglein the passed window.

RETURNED VALUE:
   in_area    -  int (enumeration list)
                 One of the following three values is returned.
                 NOT_IN_AREA        =   0
                 FULLY_IN_AREA      =   1
                 PARTIALLY_IN_AREA  =   2

*************************************************************************/

int cursor_in_area(Window         window,
                   int            x,
                   int            y,
                   unsigned int   width,
                   unsigned int   height,
                   DISPLAY_DESCR *dspl_descr)
{
int               in_area;
TXCURSOR_PRIVATE *private;

if ((private = check_private_data(dspl_descr)) == NULL)
   return(NOT_IN_AREA);


DEBUG10(fprintf(stderr, "@cursor_in_area:  %dx%d+%d+%d  ", width, height, x, y); )


if (private->last_x == NO_VALUE)
   in_area = NOT_IN_AREA;
else
   if (window != private->last_window)
      in_area = NOT_IN_AREA;
   else
      if ((int)(private->last_x + private->last_width) < x || private->last_x > (int)(x + width))
         in_area = NOT_IN_AREA;
      else
         if ((int)(private->last_y + private->last_height) < y || private->last_y > (int)(y + height))
            in_area = NOT_IN_AREA;
         else
            if ((private->last_y < y) || ((private->last_y + private->last_height) > (y + height)))
               in_area = PARTIALLY_IN_AREA;
            else
               if ((private->last_x < x) || ((private->last_x + private->last_width) > (x + width)))
                  in_area = PARTIALLY_IN_AREA;
               else
                  in_area = FULLY_IN_AREA;

DEBUG10(
   fprintf(stderr, "Cursor (%d,%d) is %sin area\n", private->last_x, private->last_y, ((in_area == FULLY_IN_AREA) ? "Fully " : ((in_area == PARTIALLY_IN_AREA) ? "Partially " : "NOT ")));
)

return(in_area);
} /* end of cursor_in_area */


/************************************************************************

NAME:      check_private_data - Get the txcursor private data from the display description.

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
   private  -  pointer to TXCURSOR_PRIVATE
               This is the private static area used by the
               routines in this module.
               NULL is returned on malloc failure only.

*************************************************************************/

static TXCURSOR_PRIVATE *check_private_data(DISPLAY_DESCR  *dspl_descr)
{
TXCURSOR_PRIVATE    *private;

DEBUG8(if (dspl_descr->next != dspl_descr) fprintf(stderr, "txcursor.c:  display # %d\n", dspl_descr->display_no);)
if (dspl_descr->txcursor_area)
   return((TXCURSOR_PRIVATE *)dspl_descr->txcursor_area);

private = (TXCURSOR_PRIVATE *)CE_MALLOC(sizeof(TXCURSOR_PRIVATE));
if (!private)
   return(NULL);   
else
   memset((char *)private, 0, sizeof(TXCURSOR_PRIVATE));

dspl_descr->txcursor_area = (void *)private;

private->last_x = NO_VALUE;
private->last_y = NO_VALUE;
private->last_height = NO_VALUE;
private->last_width = NO_VALUE;
private->last_echo = 0;
                                                                                                          
private->last_win_row = -1;
                                                                                                          
private->base_highlight_x = NO_VALUE;
private->base_highlight_y = NO_VALUE;
private->last_highlight_x = NO_VALUE;
private->last_highlight_y = NO_VALUE;
private->highlight_echo_mode = NO_HIGHLIGHT;
private->highlight_mark_off_screen;
private->rect_highlight_exists = 0;
private->save_highlight_x = NO_VALUE;
private->save_highlight_y = NO_VALUE;

     /* used in which_line_on_screen only  */
private->last_top_pix = NO_VALUE;
private->last_line_height = NO_VALUE;
private->last_line_no = NO_VALUE;
private->last_win = (DRAWABLE_DESCR *)NO_VALUE;

     /* used in which_char_on_line only */
private->last_left_pix = NO_VALUE;
private->last_char_width = NO_VALUE;
private->last_char_pos = NO_VALUE;
private->last_line = NULL;
private->last_font = (XFontStruct *)NO_VALUE;

return(private);

} /* end of check_private_data */


/************************************************************************

NAME:      min_chars_on_window - Get mimimum number of chars to fill a window horizontally

PURPOSE:    This routine returns the minimum number of characters needed
            to fill a window.  For fixed width fonts, this is also the maximum.

PARAMETERS:

   window      -  pointer to DRAWABLE_DESCR (INPUT)
                  This is the window to do the calculation on.

FUNCTIONS :

   1.   Divide the usable area of the window by the maximum character width.

RETURNED VALUE
   chars    -  int
               This is the number of chars needed.

*************************************************************************/

int min_chars_on_window(DRAWABLE_DESCR   *window)
{

return(window->sub_width / window->font->max_bounds.width);

} /* end of min_chars_on_window */


