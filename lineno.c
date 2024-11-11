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
*  module lineno.c
*
*  Routines:
*         setup_lineno_window - Size up line number window subarea
*         write_lineno        - Write the line numbers
*         dm_lineno           - Toggle the display line number state
*
*  Internal Routines:
*         label_check         - See if we need a label on this line number
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h      */

#ifdef WIN32
#ifdef WIN32_XNT
#include "xnt.h"
#else
#include "X11/X.h"          /* /usr/include/X11/X.h      */
#include "X11/Xlib.h"       /* /usr/include/X11/Xlib.h   */
#include "X11/Xutil.h"      /* /usr/include/X11/Xutil.h  */
#endif
#else
#include <X11/X.h>          /* /usr/include/X11/X.h      */
#include <X11/Xlib.h>       /* /usr/include/X11/Xlib.h   */
#include <X11/Xutil.h>      /* /usr/include/X11/Xutil.h  */
#endif

#include "lineno.h"
#include "borders.h"
#include "debug.h"
#include "label.h"
#include "memdata.h"
#include "mark.h"           /* needed for toggle_switch */
#include "sendevnt.h"
#include "window.h"
#include "xerrorpos.h"


#define DIGITS_IN_WINDOW 8

/***************************************************************
*  
*  Local function declarations
*  
***************************************************************/

static int   label_check(DATA_TOKEN        *token,
                         int                file_line,
                         int               *color_line,
                         char              *target,
                         int                max_target_chars,
                         int                lines_left);


/************************************************************************

NAME:      setup_lineno_window  -  Size up line number window subarea

PURPOSE:    This routine will build the DRAWABLE_DESCR for the line number
            sub window.  Which is at the left side of the main window.

PARAMETERS:
   1.  main_window   - pointer to DRAWABLE_DESCR (INPUT)
               This is a pointer to the main window drawable description.
               The lineno_subarea will be a sub area in this window.

   2.  title_subarea  - pointer to DRAWABLE_DESCR (INPUT)
               This is a pointer to the titlebar subarea drawable description.
               It is needed to determine the top of the lineno sub area.

   3.  dminput_window  - pointer to DRAWABLE_DESCR (INPUT)
               This is a pointer to the dminput window drawable description.
               It is needed to determine the bottom of the lineno sub area.

   3.  unixcmd_window  - pointer to DRAWABLE_DESCR (INPUT)
               This is a pointer to the unix command window drawable description.
               It is needed to determine the bottom of the lineno sub area.
               It is NULL if we are not in pad mode.

   4.  lineno_subarea -  pointer to DRAWABLE_DESCR (OUTPUT)
               Upon return, this drawable description describes the
               line number subarea of the main window.


FUNCTIONS :

   1.   Determine the size of the window.  It is the width of the DIGITS_IN_WINDOW characters
        plus the margins.  It is the height of the main window minus the
        heights of the titlebar and DM windows.


*************************************************************************/

void  setup_lineno_window(DRAWABLE_DESCR   *main_drawable,        /* input   */
                          DRAWABLE_DESCR   *title_subarea,        /* input   */
                          DRAWABLE_DESCR   *dminput_window,       /* input   */
                          DRAWABLE_DESCR   *unixcmd_window,       /* input   */
                          DRAWABLE_DESCR   *lineno_subarea)       /* output  */

{
int                   window_height;


/***************************************************************
*
*  Do the fixed stuff first:
*
***************************************************************/

memcpy((char *)lineno_subarea, (char *)main_drawable, sizeof(DRAWABLE_DESCR));


/***************************************************************
*
*  Determine the height, width, X and Y of the window.
*
***************************************************************/

lineno_subarea->x       = MARGIN;
lineno_subarea->width   = (lineno_subarea->font->max_bounds.width * DIGITS_IN_WINDOW)  + (MARGIN * 2);


lineno_subarea->y        = MARGIN;
if (title_subarea != NULL)
   lineno_subarea->y += title_subarea->y + title_subarea->height;

window_height = main_drawable->height - lineno_subarea->y;

if (dminput_window != NULL)
   window_height -= (dminput_window->height + BORDER_WIDTH);
if (unixcmd_window != NULL)
   window_height -= (unixcmd_window->height + BORDER_WIDTH);

lineno_subarea->height = window_height;

lineno_subarea->sub_x      = lineno_subarea->x;
lineno_subarea->sub_y      = lineno_subarea->y;
lineno_subarea->sub_width  = lineno_subarea->width;
lineno_subarea->sub_height = lineno_subarea->height;


/***************************************************************
*  get the lines on screen and then adjust the height of the
*  subarea to be an integer number of lines.  Then copy the
*  lines on screen to the main window area.
***************************************************************/
lineno_subarea->lines_on_screen = lines_on_drawable(lineno_subarea);

DEBUG1(
   fprintf(stderr, "setup_lineno_window:  main:  %dx%d%+d%+d, lineno_subarea:  %dx%d%+d%+d\n",
                   main_drawable->width, main_drawable->height, main_drawable->x, main_drawable->y,
                   lineno_subarea->width, lineno_subarea->height, lineno_subarea->x, lineno_subarea->y);
)


} /* end of setup_lineno_window  */


/************************************************************************

NAME:      write_lineno  - Write the line numbers

PURPOSE:    This routine will Draw the line number subarea.
            This the line numbers displayed to the left hand side.

PARAMETERS:
   1.  display       - Display (Xlib type) (INPUT)
                       This is the display token needed for Xlib calls.

   2.  drawable      - Window (Xlib type) (INPUT)
                       This is the X window id of the main window.

   3.  lineno_subarea -  pointer to DRAWABLE_DESCR (INPUT)
               This is the subwindow description of the line number area.

   4.  first_line - int (INPUT)
               This is the line number of the first line in the screen.

   5.  overlay  - int (INPUT)
               This flag specifies whether the write is to overlay the existing
               area or the area should be blanked out first.  This normally true
               if the area has been previously cleared.

   6.  skip_lines - int (input)
               this is used to specify that drawing is to start
               skip_lines into the drawable area.  This many lines
               is skipped in the drawable and memdata structure.
               That is, the file is positioned the same no matter
               how many lines are to be skipped, including zero.
  
   7.  token - pointer to DATA_TOKEN (INPUT)
               This is the memdata token for the main window.
               It is needed for the total number of lines on
               the screen and to search for labels

   8.  formfeed_in_window - int (INPUT)
               This the window line number (zero based) of the line
               which contains the form feed.  Note that a form feed
               on line zero and no form feed are the same thing.
  

FUNCTIONS :

   1.   Determine the bounds of the area to draw in.

   2.   If needed clear the area to draw in.

   3.   Draw the line between the line numbers and the text.

   4.   Write out all the line numbers to the drawable.


*************************************************************************/

void  write_lineno(Display           *display,            /* input  */
                   Drawable           drawable,           /* input  */
                   DRAWABLE_DESCR    *lineno_subarea,     /* input  */
                   int                first_line,         /* input  */
                   int                overlay,            /* input  */
                   int                skip_lines,         /* input  */
                   DATA_TOKEN        *token,              /* input  */
                   int                formfeed_in_window, /* input  */
                   WINDOW_LINE       *win_lines)          /* input  */
{

int            line_len;
int            line_spacer;
int            window_line;
int            x;
int            line_x;
int            y;
int            line_no;
int            ending_y;
char           line_number[16];
char          *line_number_p;
int            color_line = -1;
int            is_label;

DEBUG0(if (!lineno_subarea) {fprintf(stderr, "Bad lineno_subarea passed to write_lineno\n");return;})

y           = lineno_subarea->y;
ending_y    = lineno_subarea->y + lineno_subarea->height;
x           = lineno_subarea->x;
line_no     = first_line; /* line_no is zero based, displed as one based lin. */
window_line = 0;

/***************************************************************
*  Get the inter line gap in pixels.  Variable 
*  padding_between_line is external and modifiable by the 
*  user.  It is the percent of the total character height to be
*  placed between lines.  Thus if padding_between_line is 10 (10%),
*  and the font is 80 pixels tall, the padding is 8 pixels.
***************************************************************/

DEBUG0(if (!(lineno_subarea->font)) {fprintf(stderr, "Incomplete lineno_subarea passed to write_lineno\n");return;})
line_spacer =  lineno_subarea->line_height - (lineno_subarea->font->ascent + lineno_subarea->font->descent);

/***************************************************************
*  If we need to skip lines, do so.
***************************************************************/
if (lineno_subarea->lines_on_screen <= skip_lines)
   return;

if (skip_lines > 0)
   {
      y           += skip_lines * lineno_subarea->line_height;
      line_no     += skip_lines;
      window_line += skip_lines;
   }

/***************************************************************
*  Blank out the area to draw in using the background color.
***************************************************************/

DEBUG9(XERRORPOS)
if (!overlay)
   XFillRectangle(display,
                  drawable,
                  lineno_subarea->reverse_gc,
                  x, y,
                  lineno_subarea->width, ending_y-y);



/***************************************************************
*  Draw the vertical line to the right of the line numbers.
***************************************************************/

line_x = (lineno_subarea->width + lineno_subarea->x) - MARGIN;

DEBUG9(XERRORPOS)
XDrawLine(display,
          drawable,
          lineno_subarea->gc,
          line_x, y, line_x, ending_y);

/***************************************************************
*  y is set start to the top of the first line to output.
*  Set y to the baseline for the first line.
*  This is the starting y + the ascent for the first line.
*  Note window_line and line_no are zero based.
***************************************************************/

while ((window_line < lineno_subarea->lines_on_screen) &&
       (line_no < total_lines(token)) &&
       ((!formfeed_in_window) || (window_line < formfeed_in_window)))
{
   snprintf(line_number, sizeof(line_number), "%*d", DIGITS_IN_WINDOW, line_no+1); /* 10/16/2002, make width compilable, print as one based */
   line_len = strlen(line_number);

   /***************************************************************
   *  If line numbers go beyond 999,999, throw away
   *  the high digits, The titlebar will show them.
   ***************************************************************/
   line_number_p = line_number;
   if (line_len > DIGITS_IN_WINDOW)
      {
         line_number_p += line_len - DIGITS_IN_WINDOW;
         line_len = DIGITS_IN_WINDOW;
      }
   is_label = label_check(token,
                          line_no,
                          &color_line,
                          line_number_p,
                          DIGITS_IN_WINDOW+1, /* six chars available */
                          lineno_subarea->lines_on_screen - window_line);

   line_no++;
#ifdef EXCLUDEP
   line_no += win_lines[window_line].w_file_lines_here;
#endif
   y += lineno_subarea->font->ascent;

   if (is_label)
      {
         DEBUG9(XERRORPOS)
         XDrawImageString(display, drawable, lineno_subarea->reverse_gc, 
                    x, y,
                    line_number_p, line_len);
      }
   else
      {
         DEBUG9(XERRORPOS)
         XDrawString(display, drawable, lineno_subarea->gc,
                     x, y,
                     line_number_p, line_len);
      }

   y += lineno_subarea->font->descent + line_spacer;

   window_line++;
} /* end of do while */

/***************************************************************
*  
*  If we are leaving blank area because of a form feed, put
*  the word <more> in the lineno area. 
*  
***************************************************************/

if (formfeed_in_window && ((y + lineno_subarea->font->descent) < ending_y) &&  (line_no < total_lines(token)))
   {
      y += lineno_subarea->font->ascent;

      DEBUG9(XERRORPOS)
      XDrawString(display, drawable, lineno_subarea->gc,
                  x, y,
                  "<more>", 6);

   }

} /* end write_lineno  */


/************************************************************************

NAME:      label_check  - See if we need a label on this line number

PURPOSE:    This routine checks the current line and replaces the line
            number with a label if needed.

PARAMETERS:

   1.  token       -  pointer to DATA_TOKEN (INPUT)
                      This is the main window memdata token used to
                      search for labels.

   2.  file_line   -  int (INPUT)
                      This is the current file line number being drawn.

   3.  color_line  -  pointer to int (INPUT/OUTPUT)
                      This is effectively static global data used by this
                      routine to save the next line number where there is a
                      lablel.  It is refreshed to -1 every time write_lineno
                      is called.

   4.  target      -  pointer to char (OUTPUT)
                      This is where the label gets written.

   5.  max_target_chars -  int (INPUT)
                      This is the maximum space to put the label into 
                      including space for the null byte.

   6.  lines_left -  int (INPUT)
                      This is the maximum space to put the label into 
                      including space for the null byte.


FUNCTIONS :

   1.   If the current color line number is less than the passed file line
        number call the label search routine to find the next label.  This
        is always true for the first invocation within a write_lineno call.

   2.   If the current color line number matches the current line, put
        the current label, truncated and right justfitied into the target area.

RETURNED VALUE:
   is_label      -   int
                     True   -   The target was filled in with a label
                     False  -   The target was unchanged.

*************************************************************************/

static int   label_check(DATA_TOKEN        *token,
                         int                file_line,
                         int               *color_line,
                         char              *target,
                         int                max_target_chars,
                         int                lines_left)
{
static char           label[20];
int                   offset;
int                   i;
int                   is_label = False;

if (*color_line < file_line)
   {
      *color_line = find_label(token,
                               file_line,
                               file_line + lines_left,
                               label,
                               sizeof(label));
      label[sizeof(label)-1] = '\0';
   }

if ((*color_line == file_line) && label[0])
   {
      if ((int)strlen(label) >= max_target_chars)
         {
            strncpy(target, label, max_target_chars);
            target[max_target_chars-1] = '\0';
         }
      else
         {
             offset = max_target_chars - (strlen(label)+1);
             for (i = 0; i < offset; i++)
                 target[i] = ' ';
             strcpy(target+offset, label);
         }
      is_label = True;
   }

return(is_label);

} /* end off label_check */


/************************************************************************

NAME:      dm_lineno  - Toggle the display line number state

PURPOSE:    This routine toggles the show lineno flag when a lineno
            command is executed.

PARAMETERS:

   1.  display       - Display (Xlib type) (INPUT)
                       This is the display token needed for Xlib calls.

   2.  drawable      - Window (Xlib type) (INPUT)
                       This is the X window id of the main window.

   3.  dmc    - pointer to DMC (INPUT)
               This is the command structure for the lineno command.
               If contains the on / off / toggle flag.

   4.  main_window   - pointer to DRAWABLE_DESCR (INPUT)
               This is a pointer to the main window drawable description.

   5.  show_lineno -  pointer to int (OUTPUT)
               This is the address of the flag to be set.


FUNCTIONS :

   1.   Toggle the switch according to the -on / -off / toggle rules.

   2.   If the value changed, resize the window to a new size and then
        resize it back.  This will cause a configure request to be
        generated and the configure code in the main event loop will
        handle putting in or taking out the lineno area.

RETURNED VALUE:
   changed   -  int flag
                True  - The lineno value changed, a configure event was generated.
                False - The lineno valud did not change

*************************************************************************/

int   dm_lineno(DMC              *dmc,                /* input  */
                PAD_DESCR        *main_pad,           /* input  */
                Display          *display,            /* input  */
                int              *show_lineno)        /* output */
{
int                   changed;
/*XEvent                configure_event;*/


*show_lineno = toggle_switch(dmc->lineno.mode, *show_lineno, &changed);

if (changed)
   reconfigure(main_pad, display);

return(changed);

} /* end of dm_lineno */


