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

/**************************************************************
*
*  Routines in xutil.c
*     resize_pixmap           - Resize a pixmap and copy the data to the newly created pixmap.
*     textfill_drawable       - Fill an area with text read from an input function.
*     draw_partial_line       - Draw a partial line.
*     colorname_to_pixel      - Convert a named color to a Pixel value.
*     translate_gc_function   - translate a GC function code to the name for debugging
*
*  Internal routines:
*     make_dots               - create a string of dots from a text string.
*     draw_fancy_line         -  Draw a line that uses multiple gc's
*     draw_xed_out_line       -  Draw the symbol which marks out 1 or more x'ed out lines
*     draw_xed_out_char       -  Draw the symbol which marks a partially x'ed out line
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */

#include "cd.h"
#include "cdgc.h"
#include "debug.h"
#include "tab.h"
#include "xerrorpos.h"
#include "xutil.h"  /* contains other includes */
#include "memdata.h"

/***************************************************************
*  
*  Declares for local static routines.
*  
***************************************************************/

static char  *make_dots(char *master);

static void draw_fancy_line(PAD_DESCR       *pad,
                            WINDOW_LINE     *win_lines,
                            int              x,
                            int              base_y,
                            char            *display_line,
                            int              line_len,
                            int              win_start_char,
                            FANCY_LINE      *fancy_line,
                            int              fancy_line_no,
                            int              file_line_no);

#ifdef EXCLUDEP
static void draw_xed_out_line(Display        *display,
                              Drawable        drawable,
                              DRAWABLE_DESCR *pix,
                              int             y,
                              int             lines_xed_out);

static void draw_xed_out_char(Display       *display,
                             Drawable       drawable,
                             GC             gc,
                             int            x,
                             int            y,
                             int            width,
                             int            height);
#endif


/************************************************************************

NAME:      resize_pixmap

PURPOSE:    This routine will fill a drawable with text read
            from the input routine passed as a function.

PARAMETERS:

   1.  event        - pointer to Xevent (Xlib type) (INPUT)
                      This is the configure event which is triggering
                      the resize.

   2.  pix          - pointer to DRAWABLE_DESCR (INPUT / OUTPUT)
                      This is the pixmap to be resized.

   3.  bit_gravity  - int (INPUT)
                      This is theXlib  enumeration type which tells how
                      to copy the data from the old pixmap to the
                      new pixmap.  Alignment in the values is between the
                      edges of the old and new pixmaps.  Note that when
                      resizing a pixmap, only two edges move due to a
                      corner being dragged.
                      Values:
                         ForgetGravity      - Throw the old pixmap away, leave the new one empty
                         NorthEastGravity   - Align the top and right edges.
                         NorthGravity       - Align the top edge and the edge of the pixmap which did not move.
                         NorthWestGravity   - Align the top and left edges.
                         WestGravity        - Align the left edge and the edge of the pixmap which did not move.
                         CenterGravity      - Keep the centers the same in the new and old pixmaps.
                         EastGravity        - Align the right edge and the edge of the pixmap which did not move
                         SouthWestGravity   - Align the bottom and left edges.
                         SouthGravity       - Align the bottom edge and the edge of the pixmap which did not move
                         SouthEastGravity   - Align the bottom and right edges.
                         StaticGravity      - Align with the edges which did not move.



FUNCTIONS :

   1.   Figure out if the pixmap was resize and if so, which corner was dragged.

   2.   Create the new pixmap.

   3.   Decide how to copy the data from the old pixmap to the new pixmap.

   3.   Copy the data from the old pixmap to the new if needed.

   4.   Destroy the old pixmap.


RETURED VALUE:
   resize   -  int
               This flag indicates whether the pixmap was resized or
               just moved.
               True  -  resized
               False -  just moved.

NOTES:
   1.   The Ce program only uses ForgetGravity, the pixmap data is never
        copied.
    
*************************************************************************/

int  resize_pixmap(XEvent           *event,        /* input */
                   Drawable         *drawable,     /* input / output */
                   DRAWABLE_DESCR   *pix,          /* input / output */
                   int               bit_gravity)  /* input */
{
Pixmap              new_pixmap;
XWindowAttributes   window_attributes;
int                 old_x = 0;
int                 old_y = 0;
int                 new_x = 0;
int                 new_y = 0;
int                 move_width;
int                 move_height;
int                 corner;
int                 resized = 0;
int                 depth;

if ((*drawable != None) && (pix->width == (unsigned)event->xconfigure.width) && (pix->height == (unsigned)event->xconfigure.height))
   {
      /* window move, no size change */
      pix->x = event->xconfigure.x;
      pix->y = event->xconfigure.y;
   }
else
   {
      /***************************************************************
      *  
      *  change window size, build new pixmap
      *  
      ***************************************************************/

      DEBUG9(XERRORPOS)
      if (!XGetWindowAttributes(event->xconfigure.display,
                               event->xconfigure.window,
                               &window_attributes))
         {
            fprintf(stderr, "resize_pixmap:  Cannot get window attributes for window 0x%X\n", event->xconfigure.window);
            depth = DefaultDepth(event->xconfigure.display, DefaultScreen(event->xconfigure.display));
         }
      else
         depth = window_attributes.depth;

      resized = 1;
      DEBUG9(XERRORPOS)
      if (*drawable != None)
         new_pixmap = XCreatePixmap(event->xconfigure.display,
                                    event->xconfigure.window,
                                    event->xconfigure.width,
                                    event->xconfigure.height,
                                    depth);
      else
         new_pixmap = None;
      /* figure out which corner was grabbed    */
      /* this may be modified if gravity is set */
      /*  corners:     1 2                      */
      /*               3 4                      */

      if (pix->x == event->xconfigure.x)  /* edge 1-3 did not move */
         if (pix->y == event->xconfigure.y)  /* edge 1-2 did not move */
            corner = 4;
         else
            corner = 2;
      else
         if ( pix->y == event->xconfigure.y)  /* edge 1-2 did not move */
            corner = 3;
         else
            corner = 1;

      /**********************************************************
      *
      *  If gravity is on, the corner which moved may be overridden.
      *
      **********************************************************/
      switch(bit_gravity)
      {
      case ForgetGravity:
         corner = -1;  /* forget gravity says do not redraw the pixmap */
         break;

      case NorthEastGravity:
         corner = 3;
         break;

      case NorthGravity:
         if (corner == 2)
            corner = 4;
         else
            if (corner == 1)
               corner = 3;
         break;

      case NorthWestGravity:
         corner = 4;
         break;

      case WestGravity:
         if (corner == 1)
            corner = 2;
         else
            if (corner == 3)
               corner = 4;
         break;

      case CenterGravity:
         /* have not implemented center gravity, just leave it alone */
         break;

      case EastGravity:
         if (corner == 2)
            corner = 1;
         else
            if (corner == 4)
               corner = 3;
         break;

      case SouthWestGravity:
         corner = 2;
         break;

      case SouthGravity:
         if (corner == 4)
            corner = 2;
         else
            if (corner == 3)
               corner = 1;
         break;

      case SouthEastGravity:
         corner = 1;
         break;

      case StaticGravity:
         /* leave the corner alone */
         break;

      default:
         break;
      }  /* end of switch on bit gravity */


      /**********************************************************
      *
      *  determine where in the new pixmap to copy the old.
      *
      **********************************************************/
      switch(corner)
      {
      case 1:  /* move upper left corner */
         old_x = 0;
         new_x = 0;
         if (pix->y > event->xconfigure.y) /* getting taller on top */
            {
               old_y = 0;
               new_y = pix->y - event->xconfigure.y;
            }
         else
            {
               new_y = 0;
               old_y = event->xconfigure.y - pix->y;
            }
         break;

      case 2:  /* move upper right corner */
         if (pix->x < event->xconfigure.x) /* getting wider */
            {
               old_x = 0;
               new_x = pix->x - event->xconfigure.x;
            }
         else
            {
               new_x = 0;
               old_x = event->xconfigure.x - pix->x;
            }
         if (pix->y > event->xconfigure.y) /* getting taller on top */
            {
               old_y = 0;
               new_y = pix->y - event->xconfigure.y;
            }
         else
            {
               new_y = 0;
               old_y = event->xconfigure.y - pix->y;
            }
         break;

      case 3:  /* move lower left corner */
         if (pix->x < event->xconfigure.x) /* getting wider */
            {
               old_x = 0;
               new_x = pix->x - event->xconfigure.x;
            }
         else
            {
               new_x = 0;
               old_x = event->xconfigure.x - pix->x;
            }
         old_y = 0;
         new_y = 0;
         break;

      case 4:  /* move lower right corner */
         old_x = 0;
         old_y = 0;
         new_x = 0;
         new_y = 0;
         break;

      case -1:  /* forget gravity, (gravity says forget the copy) */
         break;

      }

      /***************************************************************
      *  
      *  If the corner value is other than forgetgravity, copy the
      *  pixmap.
      *  
      ***************************************************************/

      if (corner > 0)
         {
            if (pix->width > (unsigned)event->xconfigure.width) /* getting smaller */
               move_width = event->xconfigure.width;
            else
               move_width = pix->width;
            if (pix->height == (unsigned)event->xconfigure.height)
               move_height = event->xconfigure.height;
            else
               move_height = pix->height;

            DEBUG9(XERRORPOS)
            if (*drawable != None)
               XCopyArea(event->xconfigure.display,
                           *drawable, new_pixmap, pix->gc,
                           old_x, old_y,
                           move_width, move_height,
                           new_x, new_y);
         }

      /***************************************************************
      *  
      *  Put the new pixmap in its spot.
      *  
      ***************************************************************/

      if (*drawable != None)
         XFreePixmap(event->xconfigure.display, *drawable);
      *drawable     = new_pixmap;
      pix->width    = event->xconfigure.width;
      pix->height   = event->xconfigure.height;

   }  /* else screen size changed */

return(resized);

}  /* end of resizepixmap */


/************************************************************************

NAME:      textfill_drawable

PURPOSE:    This routine will fill a drawable with text read
            from the memdata structure associated with the passed in pad.

PARAMETERS:

   1.  pad   - pointer to PAD_DESCR (INPUT / OUTPUT)
               This is a pointer to the description of the
               Ce pad which is to be written to. (main, dm_input, dm_output, unixpad)

   2.  overlay -  int (INPUT)
               This flag, if non-zero (true) specifies that
               the area is not to be cleared before being
               drawn over.  This is useful if the area is
               known to already have been cleared or if
               the area is being overlayed.

   3.  skip_lines - int (INPUT)
               this is used to specify that drawing is to start
               skip_lines into the drawable area.  This many lines
               is skipped in the drawable and memdata structure.
               That is, the file is positioned the same no matter
               how many lines are to be skipped, including zero.
  
   8.  do_dots - int (INPUT)
               This flag, if true, says to output a string of dots instead of the string
               of characters.  It is used in password processing.
  

FUNCTIONS :

   1.   Verify that first_first char is >= 0 and fix it if it is not.

   2.   Get the area attributes from either the subwindow or the main 
        drawable.

   3.   Determine the background color and blank out the area with it.

   4.   Fill the area with text.

MODIFIED FIELDS IN THE PAD:

   1.  formfeed_in_window - int (OUTPUT)
               This the window line number (zero based) of the line
               which contains the form feed.  Note that a form feed
               on line zero and no form feed are the same thing.

   2.  win_lines - pointer to array of WINDOW_LINE (OUTPUT)
               This structure contains data for each line drawn.
               This includes pixel lengths, pointers to the string, and
               flags showing if there are tabs and or form feeds.

   3.  lines_displayed - int (OUTPUT)
               This is the number of lines written out.  Normally this
               would be the number of lines of on the screen (pix_window->lines_on_screen).
               If lines are excluded out, this number would be larger since it
               several lines may be compressed to 1 line.  At end of file
               and just before a page break this number could be less than
               lines on screen.


*************************************************************************/


void  textfill_drawable(PAD_DESCR       *pad,               /* input/output */
                        int              overlay,           /* input  */
                        int              skip_lines,        /* input  */
                        int              do_dots)           /* input  */
{
int            x;
int            y;
int            i;
char          *cur_line;
int            line_len;
int            full_line_len;
int            line_spacer;
int            ending_y;
char          *display_line;
int            line_no_on_window = 0;
int            tabs_on_line;
char           work[MAX_LINE+1];
WINDOW_LINE   *win_lines = pad->win_lines;
Display       *display   = pad->display_data->display;
Drawable       drawable  = pad->display_data->x_pixmap;
int            cur_file_line;
FANCY_LINE    *fancy_line_ll = NULL;
int            fancy_line_no;
int            max_chars_displayable;


/***************************************************************
*  Special case for us being obscured or iconned, do no drawing.
***************************************************************/
if (drawable == None)
   return;

/***************************************************************
*  The first char can never be less than zero.
***************************************************************/
if (pad->first_char < 0)
   pad->first_char = 0;

/***************************************************************
*  Get the relevant data from the subarea into local variables.
*  y always points to the place to draw
*  ending_y is the bottom of the screen where we must stop drawing.
***************************************************************/

y        = pad->pix_window->sub_y;
ending_y = y + pad->pix_window->sub_height;

/***************************************************************
*  Figure out the maximum number of characters which could fit
*  in the window.  No sense drawing more than this.
*  RES 3/20/1998 - Add max chars displayable to limit value passed
*  to XDrawString.
***************************************************************/
if (pad->pix_window->font->min_bounds.width > 0)
   max_chars_displayable = (pad->pix_window->sub_width / pad->pix_window->font->min_bounds.width) + 15; /* 15 is a fudge padding */
else
   max_chars_displayable = (pad->pix_window->sub_width / 2) + 15; /* assume 2 pixel wide characters */

/***************************************************************
*  Get the inter line gap in pixels.  Variable 
*  padding_between_line is external and modifiable by the 
*  user.  It is the percent of the total character height to be
*  placed between lines.  Thus if padding_between_line is 10 (10%),
*  and the font is 80 pixels tall, the padding is 8 pixels.
***************************************************************/

line_spacer =  pad->pix_window->line_height - (pad->pix_window->font->ascent +  pad->pix_window->font->descent);

/***************************************************************
*  If we need to skip lines, and skip more than are on the screen,
*  we need do nothing.
***************************************************************/
if (pad->pix_window->lines_on_screen <= skip_lines)
   return;

/***************************************************************
*  Position the memdata structure for reading
***************************************************************/
pad->formfeed_in_window = 0;
cur_file_line = pad->first_line;

if (skip_lines == 0)
   {
      position_file_pointer(pad->token, pad->first_line);
   }
else
   {
      /***************************************************************
      *  If we need to skip lines, do so.
      ***************************************************************/
      if (!pad->display_data->hex_mode)
         for (i = 1; i < skip_lines && pad->formfeed_in_window == 0; i++)
             if (win_lines[i].tabs  & TAB_FORMFEED_FOUND)
                pad->formfeed_in_window = i;
      line_no_on_window += skip_lines;
      win_lines += skip_lines;
      cur_file_line += skip_lines;
#ifdef EXCLUDEP
      cur_file_line = pad->first_line+win_lines[-1].w_file_line_offset + win_lines[-1].w_file_lines_here;
#endif

      position_file_pointer(pad->token, cur_file_line);
      /*old if (!hex_mode && line_no_on_window && !(pad->formfeed_in_window) && cur_line && (strchr(cur_line, '\f') != NULL))
         pad->formfeed_in_window = line_no_on_window;*/
      y += skip_lines * pad->pix_window->line_height;
   }

/***************************************************************
*  Show how many lines are displayed above the starting point.
***************************************************************/
pad->lines_displayed = cur_file_line - pad->first_line;

/***************************************************************
*  Blank out the area to draw in using the background color.
*  Overlay is used when the area has already been blanked out.
*  some fonts have a negative lbearing, so we have to clear out more.
***************************************************************/

if (!overlay)
   {
      x = pad->pix_window->sub_x;
      if (pad->pix_window->font->min_bounds.lbearing < 0)
         x += pad->pix_window->font->min_bounds.lbearing;
      DEBUG9(XERRORPOS)
      XFillRectangle(display,
                     drawable,
                     pad->pix_window->reverse_gc,
                     x, y, pad->pix_window->sub_width, ending_y - y);
   }

/***************************************************************
*  RES 9/14/1998 - fix color not being drawn on cut.
*  flush the buffers at the beginning of the drawing cycle.
***************************************************************/
if (COLORED(pad->token))
   cdpad_curr_cdgc(pad, -1, &fancy_line_no);


/***************************************************************
*  y is set start to the top of the first line to output.
*  Set y to the baseline for the first line.
*  This is the starting y + the ascent for the first line.
***************************************************************/

while((line_no_on_window < pad->pix_window->lines_on_screen) && ((cur_line = next_line(pad->token)) != NULL))
{
   if ((tabs_on_line = untab(cur_line, 
                             work,
                             max_chars_displayable+pad->first_char,
                             pad->display_data->hex_mode)))
      display_line = work;
   else
      display_line = cur_line;

   /***************************************************************
   *  If there is a form feed on the line save it's window line number.
   *  If the form feed is not on line zero, skip this line and the rest
   *  of the window.
   ***************************************************************/

   if (!pad->display_data->hex_mode)
      {
         if (tabs_on_line & TAB_FORMFEED_FOUND)
            pad->formfeed_in_window = line_no_on_window;

         if (pad->formfeed_in_window)
            break;
      }

   full_line_len =
   line_len = strlen(display_line);


   if (line_len > pad->first_char)
      {
         display_line = &display_line[pad->first_char];
         line_len -= pad->first_char;
      }
   else
      {
         display_line = "";
         line_len = 0;
      }

   /***************************************************************
   *  Set the win_lines data.  This is the pointer to the line
   *  as displayed and the pixel len of that line.  If we are starting
   *  in column zero, add The character to highlight the newline.
   ***************************************************************/
   win_lines->line           = cur_line;
   win_lines->tabs           = tabs_on_line;
#ifdef EXCLUDEP
   win_lines->w_file_line_offset = cur_file_line - pad->first_line;
#endif

   if (full_line_len > pad->first_char)
      win_lines->w_first_char = pad->first_char;
   else
      win_lines->w_first_char = full_line_len;
   DEBUG9(XERRORPOS)
   if (pad->pix_window->fixed_font)
      win_lines->pix_len = pad->pix_window->fixed_font * line_len;
   else
      win_lines->pix_len = XTextWidth(pad->pix_window->font, display_line, MIN(line_len, max_chars_displayable));

   if (!pad->display_data->hex_mode)
   if (pad->first_char == 0 || line_len > 0)
      win_lines->pix_len +=  pad->pix_window->font->min_bounds.width;

   /***************************************************************
   *  Get any new special drawing instructions for this line.
   ***************************************************************/
   fancy_line_ll = cdpad_curr_cdgc(pad, cur_file_line, &fancy_line_no);
#ifdef EXCLUDEP
   win_lines->w_file_lines_here = 1;
#endif
   y +=  pad->pix_window->font->ascent;
   
   if (do_dots)
      display_line = make_dots(display_line);
   
   if (line_len)
      if (fancy_line_ll)
         draw_fancy_line(pad, win_lines,
                         pad->pix_window->sub_x,
                         y,
                         display_line, MIN(line_len, max_chars_displayable), 0, /* RES 3/20/1998, MIN test added */
                         fancy_line_ll, fancy_line_no, cur_file_line);
      else
         if (!win_lines->fancy_line)
            {
               DEBUG9(XERRORPOS)
               XDrawString(display, drawable, pad->pix_window->gc,
                           pad->pix_window->sub_x,
                           y,
                           display_line, MIN(line_len, max_chars_displayable)); /* RES 3/20/1998, MIN test added */
            }
         else
            draw_fancy_line(pad, win_lines,
                            pad->pix_window->sub_x,
                            y,
                            display_line, MIN(line_len, max_chars_displayable), 0,  /* RES 3/20/1998, MIN test added */
                            win_lines->fancy_line, cur_file_line, cur_file_line);

   /***************************************************************
   *  Move on to the next line
   ***************************************************************/
   y +=  pad->pix_window->font->descent + line_spacer;
   cur_file_line++;

   win_lines++;
   line_no_on_window++;

} /* end of do while */

pad->lines_displayed      = cur_file_line - pad->first_line;


/***************************************************************
*  
*  If there were lines left undrawn (end of file), clean up
*  win_lines or the values can get used in text highlighting.
*  
***************************************************************/

while(line_no_on_window < pad->pix_window->lines_on_screen)
{
   win_lines->tabs           = 0;
   win_lines->pix_len        = 0;
   win_lines->w_first_char   = 0;
   win_lines->line           = NULL;
   win_lines->fancy_line     = NULL;
#ifdef EXCLUDEP
   win_lines->w_file_line_offset = -1;
   win_lines->w_file_lines_here = 0;
#endif
   win_lines++;
   line_no_on_window++;
}

} /* end of textfill_drawable */



/************************************************************************

NAME:      draw_partial_line

PURPOSE:    This routine will draw part of a line into a drawing area.
            This is ususally a pixmap.

PARAMETERS:

   1.  pad   - pointer to PAD_DESCR (INPUT / OUTPUT)
               This is a pointer to the description of the
               Ce pad which is to be written to.

   2.  win_line_no -  int (INPUT)
               This is the window line to be updated.

   3.  win_start_char -  int (INPUT)
               This is the offset in chars to the first
               char to be redrawn on the line with respect to the
               window.  This is a window column number!

   4.  file_line_no - int (INPUT)
               This is the file line number.  It is needed to get any color
               data for this line.

   5.  text  - pointer to char (INPUT)
               this is the line to be redrawn.  It is often a pointer
               to a work buffer.
  
   6.  do_dots - int (INPUT)
               This flag, if true, says to output a string of dots instead of the string
               of characters.  It is used in password processing.

   7.  left_side_x - pointer to int (OUTPUT)
               The left most x coordinate redrawn is returned in this parm
  
  
FUNCTIONS :

   1.   Find the row in the window to start drawing on.

   2.   Find the column to start drawing on and the character to start
        drawing there.

   3.   blank out the area being redrawn.

   4.   Fill the area with text.

   5.   Update the winlines data for this line.


MODIFIED FIELDS IN THE PAD:

   1.  formfeed_in_window - int (OUTPUT)
               This the window line number (zero based) of the line
               which contains the form feed.  Note that a form feed
               on line zero and no form feed are the same thing.

   2.  win_lines - pointer to array of WINDOW_LINE (OUTPUT)
               This structure contains data for each line drawn.
               This includes pixel lengths, pointers to the string, and
               flags showing if there are tabs and or form feeds.

    
*************************************************************************/

void draw_partial_line(PAD_DESCR       *pad,               /* input/output */
                       int              win_line_no,       /* input  */
                       int              win_start_char,    /* input  */
                       int              file_line_no,      /* input  */
                       char            *text,              /* input  */
                       int              do_dots,           /* input  */
                       int             *left_side_x)       /* output */
{
int            x;
int            line_len;
int            full_line_len;
char          *display_line;
int            corner_y;
int            x_offset;
int            y_offset;
int            tabs_on_line;
int            max_chars_displayable;
char           work[MAX_LINE+1];
WINDOW_LINE   *win_lines = pad->win_lines;
Display       *display   = pad->display_data->display;
Drawable       drawable  = pad->display_data->x_pixmap;
FANCY_LINE    *fancy_line_ll;
int            fancy_line_no;

DEBUG7(fprintf(stderr, "@dpl: char_in_col_0=%d, win_start_char=%d, line_no=%d \"%s\"\n", pad->first_char, win_start_char, win_line_no, text);)
/***************************************************************
*  Special case for us being obscured or iconned, do no drawing.
***************************************************************/
if (drawable == None)
   return;

/***************************************************************
*  The first char can never be less than zero.
***************************************************************/
if (win_start_char < 0)
   win_start_char = 0;

/***************************************************************
*  Figure out the maximum number of characters which could fit
*  in the window.  No sense drawing more than this.
*  RES 3/20/1998 - Add max chars displayable to limit value passed
*  to XDrawString.
***************************************************************/
if (pad->pix_window->font->min_bounds.width > 0)
   max_chars_displayable = (pad->pix_window->sub_width / pad->pix_window->font->min_bounds.width) + 15; /* 15 is a fudge padding */
else
   max_chars_displayable = (pad->pix_window->sub_width / 2) + 15; /* assume 2 pixel wide characters */

/***************************************************************
*  Get the relevant data from the subwindow into local variables.
***************************************************************/
x_offset    = pad->pix_window->sub_x;
y_offset    = pad->pix_window->sub_y;

/***************************************************************
*  Find the top left corner of the place to start
*  At the end of the sequence, left_side_x,a dn corner_y are set
*  and display_line points to the stuff to write.
***************************************************************/

corner_y = (pad->pix_window->line_height * win_line_no) + y_offset;

win_lines += win_line_no;

if (text == NULL)
   text = "";

if ((tabs_on_line = untab(text,
                          work,
                          max_chars_displayable+pad->first_char,
                          pad->display_data->hex_mode)))
   display_line = work;
else
   display_line = text;

if (tabs_on_line & TAB_FORMFEED_FOUND)
   {
      if (win_line_no && (!(pad->formfeed_in_window) || (win_line_no < pad->formfeed_in_window)))
         pad->formfeed_in_window = win_line_no;
      if (win_line_no != 0)
         return;
   }

/***************************************************************
*  Get the length of the line to be displayed.  Then adjust it for
*  the number of characters shifted out the left side of the window.
*  line_len is the number of chars in the window.  full_line_len
*  is the actual length of the line.
***************************************************************/
full_line_len =
line_len = strlen(display_line);

if (line_len > pad->first_char)
   {
      display_line = &display_line[pad->first_char];
      line_len -= pad->first_char;
   }
else
   {
      display_line = "";
      line_len = 0;
   }

/***************************************************************
*  Calculate left_side_x.  This is the left edge in the window
*  where drawing will start.  x_offset is the border for the window.
***************************************************************/
if (line_len > win_start_char)
   {
      DEBUG9(XERRORPOS)
      if (pad->pix_window->fixed_font)
         *left_side_x = (win_start_char * pad->pix_window->fixed_font) + x_offset;
      else
         *left_side_x = XTextWidth(pad->pix_window->font, display_line, win_start_char) + x_offset;
      display_line += win_start_char;
      line_len -= win_start_char;
   }
else
   {
      DEBUG12(fprintf(stderr, "draw_partial_line: Odd case, drawing past end of line, win=%d, file=%d\n", win_line_no, file_line_no);)
      DEBUG9(XERRORPOS)
      if (pad->pix_window->fixed_font)
         *left_side_x = (line_len * pad->pix_window->fixed_font)  + x_offset;
      else
         *left_side_x = XTextWidth(pad->pix_window->font, display_line, line_len)  + x_offset;
      display_line = "";
      line_len = 0;
   }


/***************************************************************
*  Blank out the area to draw in using the background color.
***************************************************************/
x = *left_side_x;
if ((win_start_char == 0) && (pad->pix_window->font->min_bounds.lbearing < 0))
   x += pad->pix_window->font->min_bounds.lbearing;

DEBUG9(XERRORPOS)
XFillRectangle(display,
               drawable,
               pad->pix_window->reverse_gc,
               x, corner_y,
               pad->pix_window->sub_width, pad->pix_window->line_height);


/***************************************************************
*  Get any special drawing instructions for this line.
***************************************************************/
fancy_line_ll = cdpad_curr_cdgc(pad, file_line_no, &fancy_line_no);

#ifdef EXCLUDEP
win_lines->w_file_line_offset    = file_line_no - pad->first_line;
win_lines->w_file_lines_here = 1;
#endif

/***************************************************************
*  Go down to the font baseline and draw the line.
*  Convert the line to dots if needed.
***************************************************************/
corner_y += pad->pix_window->font->ascent;

/***************************************************************
*  RES 9/14/1998, move next 2 lines from last 2 lines of routine.
*  Draw fancy line needs tabs on line.
***************************************************************/
win_lines->line       = text;
win_lines->tabs       = tabs_on_line;

if (do_dots)
   display_line = make_dots(display_line);

if (display_line[0] != '\0')
   if (fancy_line_ll)
      draw_fancy_line(pad, win_lines, 
                      *left_side_x, corner_y,
                      display_line, line_len,
                      win_start_char,
                      fancy_line_ll, fancy_line_no, file_line_no);
   else
      if (!win_lines->fancy_line)
         {
            DEBUG9(XERRORPOS)
            XDrawString(display, drawable, pad->pix_window->gc,
                        *left_side_x, corner_y,
                        display_line, line_len);
         }
      else
         draw_fancy_line(pad, win_lines, 
                         *left_side_x, corner_y,
                         display_line, line_len,
                         win_start_char,
                         win_lines->fancy_line, file_line_no, file_line_no);

line_len     += win_start_char; /* go back to looking at the whole line on the window */
display_line -= win_start_char; /* whole line being all that is visible on the window */

if (full_line_len > pad->first_char)
   win_lines->w_first_char = pad->first_char;
else
   win_lines->w_first_char = full_line_len;

DEBUG9(XERRORPOS)
if (pad->pix_window->fixed_font)
   win_lines->pix_len = pad->pix_window->fixed_font * line_len;
else
   win_lines->pix_len = XTextWidth(pad->pix_window->font, display_line, line_len) + pad->pix_window->font->min_bounds.width;

if (!pad->display_data->hex_mode)
   if (win_start_char == 0 || line_len > 0)
      win_lines->pix_len += pad->pix_window->font->min_bounds.width;


}  /* end of draw_partial_line */



/************************************************************************

NAME:      colorname_to_pixel  -  convenience function

PURPOSE:    Take a color name (char string) and return the pixel
            value for that color.

PARAMETERS:

   1.  display    -   Pointer to Display (INPUT)
                      This is the pointer to the current display.
                      The value returned by XOpenDisplay.

   2.  color_name -   Pointer to char (INPUT)
                      This is a pointer to the string containing the
                      color to be looked up.



FUNCTIONS :


   1.   Get the color data for the passed color name.
        If it cannot be found, return unsigned -1. (All 0xff)

   2.   Return the pixel value for the passed color.


RETURNED VALUE:

   The returned Pixel value is suitable for putting into a GC or 
   window attributes structure if it is valid.
   the value BAD_COLOR which is (unsigned long)-1 is returned if
   the color cannot be determined.

    
*************************************************************************/

unsigned long colorname_to_pixel(Display   *display,
                                 char      *color_name)
{
   unsigned long   pixel;
   int             rc;
   XColor          color;
   int             screen;


   if (!color_name)
      return(BAD_COLOR);

   screen = DefaultScreen(display);

   /***************************************************************
   *  Trim trailing blanks
   ***************************************************************/
   for (rc = strlen(color_name) -1 ;
        rc > 0 && color_name[rc] == ' ';
        rc--)
      color_name[rc] = '\0';

   /***************************************************************
   *  Check for black or white, any case.
   *  HP/UX will allocate a new color for it, so we do a
   *  special check.
   ***************************************************************/
   rc = strlen(color_name);
   if (rc == 5)
      {
         if (((color_name[0] | 0x20) == 'b') &&
             ((color_name[1] | 0x20) == 'l') &&
             ((color_name[2] | 0x20) == 'a') &&
             ((color_name[3] | 0x20) == 'c') &&
             ((color_name[4] | 0x20) == 'k'))
            return(BlackPixel(display, screen));

         if (((color_name[0] | 0x20) == 'w') &&
             ((color_name[1] | 0x20) == 'h') &&
             ((color_name[2] | 0x20) == 'i') &&
             ((color_name[3] | 0x20) == 't') &&
             ((color_name[4] | 0x20) == 'e'))
            return(WhitePixel(display, screen));
      }

   /***************************************************************
   *  RES 11/10/2005
   *  Passing colors as #rrggbb messes up the shell sometimes.  Allow
   *  @rrggbb as an option and translate the @ to # here.  Also deal
   *  with color names in quotes.
   ***************************************************************/
   if ((color_name[0] == '\'') || (color_name[0] == '"'))
      {
         color_name++; /* skip the quote */
         rc = strlen(color_name);
         if ((color_name[rc-1] == '\'') || (color_name[rc-1] == '"'))
            color_name[rc-1] = '\0'; /* get rid of trailing quote */
      }
   
   if ((color_name[0] == '@') && (strspn(color_name, "@0123456789abcdefABCDEF") == strlen(color_name)))
      {
         color_name[0] = '#';
      }

   DEBUG9(XERRORPOS)
   rc = XParseColor(display, DefaultColormap(display, screen),  color_name, &color);
   /*rc = XAllocNamedColor(display,  DefaultColormap(display, screen),  color_name, &color, &closest);*/

   if (rc == 0)
      {
         DEBUG1(fprintf(stderr, "Could not find color %s\n", color_name);)
         pixel = BAD_COLOR;
      }
   else
      {
         DEBUG9(XERRORPOS)
         rc = XAllocColor(display,  DefaultColormap(display, screen),  &color);
         if (rc == 0)
            {
               DEBUG1(fprintf(stderr, "Could not find color %s\n", color_name);)
               pixel = BAD_COLOR;
            }
         else
            pixel = color.pixel;
      }

   return(pixel);

} /* end of colorname_to_pixel */



/************************************************************************

NAME:      translate_gc_function  -  debugging support function

PURPOSE:    Translate a GC function field to the string which is its
            literal coding value.

PARAMETERS:

   1.  function   -   int (INPUT)
                      This is function field from the GC values structure.

   2.  str        -   Pointer to char (OUPUT)
                      This is a pointer to the string which will be updated.


FUNCTIONS :

   1.   Check the passed value against the valid GC functions and 
        set the target string accordingly.

   2.   If the value is unknown, print it in the target string.


*************************************************************************/

void translate_gc_function(int function, char *str)
{
switch(function)
{
case GXclear:
   strcpy(str, "GXclear");
   break;
case GXand:
   strcpy(str, "GXand");
   break;
case GXandReverse:
   strcpy(str, "GXandReverse");
   break;
case GXcopy:
   strcpy(str, "GXcopy");
   break;
case GXandInverted:
   strcpy(str, "GXandInverted");
   break;
case GXnoop:
   strcpy(str, "GXnoop");
   break;
case GXxor:
   strcpy(str, "GXxor");
   break;
case GXor:
   strcpy(str, "GXor");
   break;
case GXnor:
   strcpy(str, "GXnor");
   break;
case GXequiv:
   strcpy(str, "GXequiv");
   break;
case GXinvert:
   strcpy(str, "GXinvert");
   break;
case GXorReverse:
   strcpy(str, "GXorReverse");
   break;
case GXcopyInverted:
   strcpy(str, "GXcopyInverted");
   break;
case GXorInverted:
   strcpy(str, "GXorInverted");
   break;
case GXnand:
   strcpy(str, "GXnand");
   break;
case GXset:
   strcpy(str, "GXset");
   break;
default:
   sprintf(str, "UNKNOWN VALUE 0x%08X", function);
   break;
} /* end of switch statement */
} /* end of translate_gc_function */


/************************************************************************

NAME:      make_dots  -  create a string of dots in a work string.

PURPOSE:    This routine takes and input string of up to MAX_LINE long
            and creates a string with the same number of characters, 
            except that the string is all dots.  The address of the
            static dot filled string is returned.

            This routine is used by the display routines when processing
            password lines.  What the user types is echoed back as dots
            instead of the real characters.  Other routines detect the
            non-echo mode and pass this information as a flag to
            textfill_drawable and draw_partial_line.

PARAMETERS:

   1.  master     -   Pointer to char (INPUT)
                      This is the string used as the model.  The returned
                      string has the same number of characters.


FUNCTIONS :

   1.   Walk the string creating a dot in the target string for
        each character in the master string.

   2.   Null terminate the dot string and return its address

RETURNED VALUE:
   dot_buffer  -  pointer to char 
                  The returned value is a pointer to the string
                  of dots.


*************************************************************************/

static char  *make_dots(char *master)
{
static char   dot_buffer[MAX_LINE];
char         *p;

if (master != NULL)
   for (p = dot_buffer; *master; p++, master++)
      *p = '.';
else
   p = dot_buffer;

*p = '\0';

return(dot_buffer);

} /* end of make_dots */


/************************************************************************

NAME:      draw_fancy_line  -  Draw a line that uses multiple gc's

PURPOSE:    This routine draws a string that has more than one gc.
            The linked list of fancy_line descriptions is passed in.

            Note that this routine can only take care of color differences.
            putting in a different font will not work because the font
            structure is passed in as a single parm.  To make this routine
            work with multiple fonts, an XFontStruct element would have to
            be added to the FANCY_LINE data type and this routine would have
            to return the resultant pix len for the window lines structure.
            We would also have to worry about the height of each line and how
            that affects the number of lines on the screen.
            Since this routine currently supports vt100 mode, we will not do it.

PARAMETERS:

   1.  display     -  Pointer to Display (Xlib type) (INPUT)
                      This is the open display needed for doing
                      X calls.

   2.  drawable    -  Pointer to Drawable (Xlib type) (INPUT)
                      This is the window or pixmap to draw on.

   3.  default_gc  -  Pointer to GC (Xlib type) (INPUT)
                      This is the gc to use in drawing the text
                      when none of the fancy line gc's apply.

   4.  x           -  int (INPUT)
                      This is the x coordinate to start drawing
                      the string.

   5.  base_y      -  int (INPUT)
                      This is the y coordinate to  draw
                      the string.

   6.  display_line - pointer to char (INPUT)
                      This is the string to draw.  It's first character
                      is (parm) start_char characters into the
                      real string.  Tabs have been expanded.

   7.  line_len     - int (INPUT)
                      This is the length of string display_line.

   8.  file_line    - pointer to char (INPUT)
                      This is the string as it is in the file without tabs
                      expanded.  Drawing columns in the FANCY_LINE list
                      refer to this column and have to be expanded.  If
                      we are in column zero and there are no tabs, this
                      value will be equal to display_line.

   3.  win_start_char -  int (INPUT)
                      This is the offset in chars to the first
                      char to be redrawn on the line with respect to the
                      window.  This is a window column number!

   10. fancy_line   - pointer to FANCY_LINE (INPUT)
                      This is a pointer to the head of a linked list of
                      structures which contain the GC's to use and the
                      columns of the string to apply the gc to.

   11. font         - pointer to XFontStruct (INPUT)
                      This is a the font description for the font used by
                      the default_gc.  It is used for all the fonts used by
                      the local gc's


FUNCTIONS :

   1.   Figure out where the window start character is in the file line.  This has
        to take into account any horizontal scrolling of the screen along with
        tab expansion along with where where on the window we are starting to.
        draw.

   2.   Get the graphics content and file end column for this segment of the line.

   3.   If this is an x'ed out region, draw the special graphic and bump the window
        position by 1 character and the file by the appropriate amount.

   4.   Otherwise convert the end column back to a window column value to get the drawn length.
        and draw the string.

   5.  Continue until we run out of window or line.



*************************************************************************/

static void draw_fancy_line(PAD_DESCR       *pad,
                            WINDOW_LINE     *win_lines,
                            int              x,
                            int              base_y,
                            char            *display_line,
                            int              line_len,
                            int              win_start_char,
                            FANCY_LINE      *fancy_line,
                            int              fancy_line_no,
                            int              file_line_no)
{
GC             cur_gc;
int            file_col;
int            win_col = win_start_char;
Display       *display   = pad->display_data->display;
Drawable       drawable  = pad->display_data->x_pixmap;
int            drawn_len;
int            tmp;
int            end_col;
int            end_win_col;

while((line_len > 0) && (x < (int)pad->pix_window->width))
{
   file_col = tab_cursor_adjust(win_lines->line, win_col, win_lines->w_first_char, TAB_LINE_OFFSET, pad->display_data->hex_mode);

   cur_gc = extract_gc(fancy_line, fancy_line_no, file_line_no, file_col, &end_col);
   if (cur_gc == DEFAULT_GC)
      cur_gc = pad->window->gc;

   if (end_col < MAX_LINE)
      {
         end_win_col = tab_cursor_adjust(win_lines->line, end_col, win_lines->w_first_char, TAB_EXPAND_POS, pad->display_data->hex_mode);
         drawn_len = end_win_col - win_col;

         if (drawn_len > line_len)
            drawn_len = line_len;
      }
   else
      drawn_len = line_len;
   
   DEBUG9(XERRORPOS)
   XDrawImageString(display, drawable, cur_gc, 
                    x, base_y,
                    display_line, drawn_len);



   tmp           = XTextWidth(pad->pix_window->font, display_line, drawn_len);
   x            += tmp;
   win_col      += drawn_len;
   display_line += drawn_len;
   line_len     -= drawn_len;

} /* while line len */

} /* end of draw_fancy_line */


#ifdef EXCLUDEP 
static void draw_xed_out_line(Display        *display,
                              Drawable        drawable,
                              DRAWABLE_DESCR *pix,
                              int             y,
                              int             lines_xed_out)
{
char           buff[40];
int            y2;
int            height = pix->font->ascent + pix->font->descent;

y2 = y + height/2;
XDrawLine(display, drawable, pix->gc, pix->sub_x+2, y2, pix->sub_x+2+pix->sub_width, y2); 
sprintf(buff, " %d LINES EXCLUDED ", lines_xed_out);
XDrawImageString(display, drawable, pix->reverse_gc, pix->sub_x+6, y+pix->font->ascent, buff, strlen(buff)); 
XDrawRectangle(display, drawable, pix->gc, pix->sub_x+8, y, XTextWidth(pix->font, buff, strlen(buff))-2, height); 


} /* end of draw_xed_out_line */


static void draw_xed_out_char(Display       *display,
                             Drawable       drawable,
                             GC             gc,
                             int            x,
                             int            y,
                             int            width,
                             int            height)
{
XPoint  points[4];

/* describe a diamond inside the rectangle */
points[0].x = x + width/2;   /* start at top middle of rectangle */
points[0].y = y;
points[1].x = width/2;       /* next point at right side middle  */
points[1].y = height/2;
points[2].x = -(width/2);    /* next point at middle of bottom   */
points[2].y = height/2;
points[3].x = -(width/2);    /* next point at left side middle   */
points[3].y = -(height/2);
                             /* last point assumed to be first   */


XDrawRectangle(display, drawable, gc, x, y, width, height); 
XFillPolygon(display, drawable, gc, points, 4, Convex, CoordModePrevious);

} /* end of draw_xed_out_char */
#endif




#ifdef MCOLOR
char *get_color_by_num(DATA_TOKEN *token,       /* opaque */
                       int         line_no,     /* input  */
                       int        *color_line_no)  /* output */
{
if (line_no < 100000000)
   {
      *color_line_no = 0;
      return(NULL);
   }
else
   if (line_no < 10)
      {
         *color_line_no = 5;
         return("0,4097,XCLUDE;");
      }
   else
      if (line_no < 20)
      {
         *color_line_no = 10;
         return("0,4097,DEFAULT;");
      }
   else
      {
         *color_line_no = 20;
         return("0,0,XCLUDE;10,20,blue/yellow");
      }
    
} /* end of get_color_by_num */
#endif
