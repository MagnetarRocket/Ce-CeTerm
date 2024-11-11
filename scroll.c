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
*  module scroll.c
*
*  This routine is responsible for scrolling data on the screen.
*  It is separate from the other xutil drawing routines becase
*  it requires a lot more data.
*
*  Routines:
*         scroll_redraw         - Shift a pad up one or more lines.
*
*  Internal:
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h         */
#include <string.h>         /* /usr/include/string.h        */
#include <errno.h>          /* /usr/include/errno.h         */

#ifdef EXCLUDEP
#include "cd.h"
#endif
#include "debug.h"
#include "memdata.h"
#include "scroll.h"
#include "xerrorpos.h"
#include "xutil.h"
#include "lineno.h"


/***************************************************************
  
NAME:      scroll_redraw   - Shift a pad up one or more lines.


PURPOSE:    This routine scrolls the passed pad up one or more lines.

PARAMETERS:

   1.  scroll_pad -  pointer to DATA_TOKEN  (INPUT)
                     This is the pad description for the window being scrolled.
                     It happens that this is always the main window.

   2.  lineno_subarea   - pointer to DRAWABLE_DESCR (INPUT)
                     This is the window subarea description for the lineno
                     text area of the main window.  If line numbers are
                     off, this parm is NULL.

   3.  lines_to_scroll   - int (INPUT)
                     This is the number of lines to scroll.


FUNCTIONS :

   1.   Copy the pixmap up one line onto itself.

   2.   Shift the winlines data up in the array.

   3.   Draw the new lines which were shifted onto the screen.

   4.   If line numbers are on, draw the new line numbers.

   5.   Copy the pixmap to the main window.

   6.   Flush to make sure the event is on it's way.

NOTE:
   This is a drawing routine.  It does not modify the PAD_DESCRIPTION.
   It assumes the first_line field has already been moved by the
   correct number of lines.


*************************************************************************/

void  scroll_redraw(PAD_DESCR         *scroll_pad,
                    DRAWABLE_DESCR    *lineno_subarea,
                    int                lines_to_scroll)
{
char       *new_line;
int         i;
int         expose_x;
int         from_y;
int         to_y;
int         height;
int         first_redraw_winline;
int         last_redraw_winline;
int         scroll_up;

DEBUG5(fprintf(stderr, "scroll_redraw: Scrolling %d lines in %s\n", lines_to_scroll, which_window_names[scroll_pad->which_window]);)

if (!lines_to_scroll)
   return;  /* zero has no effect */

if (lines_to_scroll > 0)  /* scrolling down, data moves up. */
   scroll_up = False;
else
   scroll_up = True;

lines_to_scroll = ABSVALUE(lines_to_scroll);

if ((lines_to_scroll < scroll_pad->window->lines_on_screen) && !scroll_pad->formfeed_in_window)
   {
      if (!scroll_up)  /* scrolling down, data moves up. */
         {
            /***************************************************************
            *  Figure out the area to copy
            ***************************************************************/
            from_y = scroll_pad->window->sub_y + (scroll_pad->window->line_height * lines_to_scroll);
            height = scroll_pad->window->sub_height - (scroll_pad->window->line_height * lines_to_scroll);
            to_y   = scroll_pad->window->sub_y;

            /***************************************************************
            * Save the lines on the window to be redrawn
            ***************************************************************/
            first_redraw_winline = scroll_pad->window->lines_on_screen - lines_to_scroll;
            last_redraw_winline  = scroll_pad->window->lines_on_screen - 1;

            /***************************************************************
            *  Shift the winlines array by the correct amount. The empty 
            *  slots at the bottom will be filled in later.
            ***************************************************************/
            for (i = 0; i < first_redraw_winline; i++)
               scroll_pad->win_lines[i] = scroll_pad->win_lines[i+lines_to_scroll];
         }
      else
         {
            /***************************************************************
            *  scrolling up, data moves down, lines_to_scroll is negative.
            *  Figure out the area to copy
            ***************************************************************/
            from_y = scroll_pad->window->sub_y;
            height = scroll_pad->window->sub_height - (scroll_pad->window->line_height * lines_to_scroll);
            to_y   = scroll_pad->window->sub_y + (scroll_pad->window->line_height * lines_to_scroll);
            /* truncate height to even number of lines */
            height = (height / scroll_pad->window->line_height) * scroll_pad->window->line_height;

            /***************************************************************
            * Save the lines on the window to be redrawn
            ***************************************************************/
            first_redraw_winline = 0;
            last_redraw_winline  = lines_to_scroll - 1;

            /***************************************************************
            *  Shift the winlines array by the correct amount. The empty 
            *  slots at the top will be filled in later.
            ***************************************************************/
            for (i = scroll_pad->window->lines_on_screen - 1; i > last_redraw_winline; i--)
               scroll_pad->win_lines[i] = scroll_pad->win_lines[i-lines_to_scroll];
         }

      DEBUG5(fprintf(stderr, "scroll_redraw: from_y = %d, height = %d, to_y = %d\n", from_y, height, to_y);)

      if (height)
         {
            DEBUG9(XERRORPOS)
            XCopyArea(scroll_pad->display_data->display, scroll_pad->display_data->x_pixmap, scroll_pad->display_data->x_pixmap,
                      scroll_pad->window->gc,
                      0, from_y,
                      scroll_pad->window->width, height,
                      0, to_y);
         }

   }
else
   {
      DEBUG5(fprintf(stderr, "scroll_redraw: Full redraw\n");)
      first_redraw_winline = 0;
      last_redraw_winline  = scroll_pad->window->lines_on_screen - 1;
   }

/***************************************************************
*  Draw the new lines.
*  The first if is for scrolling down, (data moves up).  The
*  xcopy area filled the top part of the screen, so we fill in
*  the rest normally.  The else is for scrolling the other way,
*  The xcopyarea moved data to the bottom of the screen we will
*  draw the top lines.
***************************************************************/
if (last_redraw_winline  == scroll_pad->window->lines_on_screen - 1)
   {
      textfill_drawable(scroll_pad,
                        False,                    /* not overlay mode */
                        first_redraw_winline,     /* starting line on window */
                        False);                   /* do_dots */
   }
else
   {
      position_file_pointer(scroll_pad->token, first_redraw_winline+scroll_pad->first_line);

      for (i = first_redraw_winline; i <= last_redraw_winline; i++)
      {
         new_line = next_line(scroll_pad->token);
         draw_partial_line(scroll_pad,
                           i,
                           0,
                           scroll_pad->first_line+i,  /*win_line_to_file_line(scroll_pad->token, scroll_pad->first_line, i),*/
                           new_line,
                           False,      /* do_dots */
                           &expose_x); /* not used for anything in this case */
#ifdef EXCLUDEP
         if (scroll_pad->win_lines[i].w_file_lines_here != 1)
            {
               DEBUG5(fprintf(stderr, "scroll_redraw: Skipping %d lines to %d\n", scroll_pad->win_lines[i].w_file_lines_here, scroll_pad->first_line+scroll_pad->win_lines[i].w_file_line_offset+scroll_pad->win_lines[i].w_file_lines_here);)
               position_file_pointer(scroll_pad->token, scroll_pad->first_line+scroll_pad->win_lines[i].w_file_line_offset+scroll_pad->win_lines[i].w_file_lines_here);
            }
#endif
      }
   }


/***************************************************************
*  Redraw the line numbers.
***************************************************************/
if (lineno_subarea)
   write_lineno(scroll_pad->display_data->display,
                scroll_pad->display_data->x_pixmap,
                lineno_subarea,
                scroll_pad->first_line,
                0,    
                first_redraw_winline,  
                scroll_pad->token,
                scroll_pad->formfeed_in_window,
                scroll_pad->win_lines);

/***************************************************************
*  Copy the data to the displayable window.
***************************************************************/
DEBUG9(XERRORPOS)
XCopyArea(scroll_pad->display_data->display, scroll_pad->display_data->x_pixmap, scroll_pad->x_window,
          scroll_pad->window->gc,
          0, scroll_pad->window->sub_y,
          scroll_pad->window->width, scroll_pad->window->sub_height,
          0, scroll_pad->window->sub_y);

XFlush(scroll_pad->display_data->display);

} /* end of scroll_redraw  */


