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
*  module titlebar.c
*
*  Routines:
*         get_titlebar_window - Initialize the title bar subwindow drawable description
*         write_titlebar      - Write the titlebar
*         resize_titlebar     - Handle a resize of the main window in the titlebar
*
*  Internal:
*         digit_count         - Return number of digits in a number;
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

#include "borders.h"
#include "debug.h"
#include "dmwin.h"
#include "gc.h"
#include "emalloc.h"
#include "titlebar.h"
#include "xerrorpos.h"

#define   TITLEBAR_HEIGHT_PADDING   60

static int digit_count(long int number, long int base);

/************************************************************************

NAME:      get_titlebar_window

PURPOSE:    This routine will build the DRAWABLE_DESCR for the titlebar
            sub window.  Which is at the top of the main window.

PARAMETERS:

   1.  main_window   - pointer to DRAWABLE_DESCR (INPUT)
               This is a pointer to the main window drawable description.
               The DM windows, will be children of this window.

   2.  titlebar_subwin -  pointer to DRAWABLE_DESCR (OUTPUT)
               Upon return, this drawable description describes the
               titlebar window.


FUNCTIONS :

   1.   Determine the size of the window.  It is the width of the main window
        minus 2 pixels on a side.  The height is 1 font highth + a #defined 
        constant percent of the font height.

   2.   The  titlebar is reverse video.  Swap the gc and reverse gc.


*************************************************************************/

void  get_titlebar_window(Display          *display,              /* input   */
                          Window            main_x_window,        /* input   */
                          DRAWABLE_DESCR   *main_drawable,        /* input   */
                          DRAWABLE_DESCR   *menubar_drawable,     /* input   */
                          char             *titlebarfont,         /* input   */
                          DRAWABLE_DESCR   *titlebar_subwin)      /* output  */

{
int                   window_height;
int                   line_spacer;
XGCValues             gcvalues;
unsigned long         valuemask;
XFontStruct          *temp_font;
char                  msg[256];

/***************************************************************
*
*  Do the fixed stuff first:
*
***************************************************************/

memcpy((char *)titlebar_subwin, (char *)main_drawable, sizeof(DRAWABLE_DESCR));


/***************************************************************
*  
*  Reverse the gc and reverse_gc
*  
***************************************************************/

titlebar_subwin->gc          = main_drawable->reverse_gc;
titlebar_subwin->reverse_gc  = main_drawable->gc;

/***************************************************************
*
*  If a font is specified, use it.  Otherwise use the main
*  window font.
*
***************************************************************/

if (titlebarfont && *titlebarfont)
   {
      DEBUG9(XERRORPOS)
      temp_font = XLoadQueryFont(display, titlebarfont);
      if (temp_font != NULL)
         {
            titlebar_subwin->font = temp_font;
            gcvalues.font = titlebar_subwin->font->fid;
            valuemask = GCFont;

            titlebar_subwin->gc         = dup_gc(display, main_x_window,
                                                 titlebar_subwin->gc, valuemask, &gcvalues);

            titlebar_subwin->reverse_gc = dup_gc(display, main_x_window,
                                                 titlebar_subwin->reverse_gc, valuemask, &gcvalues);

            titlebar_subwin->xor_gc     = dup_gc(display, main_x_window,
                                                 titlebar_subwin->xor_gc, valuemask, &gcvalues);

            line_spacer =  titlebar_subwin->line_height - (titlebar_subwin->font->ascent +  titlebar_subwin->font->descent);
            titlebar_subwin->line_height = titlebar_subwin->font->ascent + titlebar_subwin->font->descent + line_spacer;
            titlebar_subwin->fixed_font = titlebar_subwin->font->min_bounds.width;
         }
      else
         {
            snprintf(msg, sizeof(msg), "Cannot load font %s for title bar, using default\n", titlebarfont);
            dm_error(msg, DM_ERROR_BEEP);
         }
   }


/***************************************************************
*
*  Determine the height, width, X and Y of the window.
*
***************************************************************/

window_height = titlebar_subwin->font->ascent + titlebar_subwin->font->descent;
window_height += (window_height * TITLEBAR_HEIGHT_PADDING) / 100;

titlebar_subwin->height  = window_height;
titlebar_subwin->x       = 2;
titlebar_subwin->width   = main_drawable->width - 4;

titlebar_subwin->sub_height  = window_height;
titlebar_subwin->sub_x       = 2;
titlebar_subwin->sub_width   = main_drawable->width - 4;

if (menubar_drawable)
   titlebar_subwin->y = menubar_drawable->y + menubar_drawable->height + 1;
else
   titlebar_subwin->y       = 1;

titlebar_subwin->sub_y   = titlebar_subwin->y;

} /* end of get_titlebar_window  */


/************************************************************************

NAME:      write_titlebar

PURPOSE:    This routine will Draw the title bar subwindow.
            This includes the name, line no, and the boxes.

PARAMETERS:

   1.  dm_window -  pointer to DRAWABLE_DESCR (INPUT)
               This is the dm window to be drawn into.

   2.  text   - pointer to char (INPUT)
               This is the text title of the window.

   3.  line_no - int (INPUT)
               This is the line number to be right justified at the
               right of the title bar.  To use boxes 2 thru 4 and
               not put out a lineno, use value -1.

   4.  insert_mode - int (INPUT)
               If true, insert mode is on the I box is put in
               position 1.  This may be overridden by ro_mode.

   5.  writable - int (INPUT)
               writable / Read Only Mode.  If false, the R box is put in
               position 1.

   6.  hold_mode - int (INPUT)
               If true, and line no is not in use, the H box is put
               in box 2.

   7.  scroll_mode - int (INPUT)
               If true, and line no is not in use, the S box is put
               in box 3.

   8.  autohold_mode - int (INPUT)
               If true, and line no is not in use, the A box is put
               in box 4.

   9.  vt100_mode - int (INPUT)
               If true, and line no is not in use, the V box is put
               in box 3.

   10. window_wrap - int (INPUT)
               If true, and line no is not in use, the W box is put
               in box 0.

   11.  modified - int (INPUT)
               If true, and line no is in use, the M box is put
               in box 2.

FUNCTIONS :

   1.   calculate the y coordinate of the baseline for the text.
        (see note below)

   2.   If this is the dm input window, put in the prompt.

   3.   Write the text.


NOTES:

To center the font in the window, the font baseline must be offset from
the center of the window by the amount the baseline of the font is offcenter
from the center of the font.  The following diagram should help explain.

  +---------                        --+
  |                                   |
  |                                   |
  |                                   |
  |                                   |
  |                            ascent |
  |                                   |
  +--------- center of font           | ----+
  |                                   |     |
  |                                   |     | font_offcenter
  +------    baseline of font       --+ ----+
  |                                   |
  |                           descent |
  |                                   |
  +---------                        --+

A similar calculation is done to center the Letters in the window boxes.

*************************************************************************/

void  write_titlebar(Display          *display,            /* input   */
#ifdef WIN32
                     Drawable          x_pixmap,           /* input   */
#else
                     Pixmap            x_pixmap,           /* input   */
#endif
                     DRAWABLE_DESCR   *titlebar_subwin,    /* input  */
                     char             *text,               /* input  */
                     char             *node_name,          /* input  */
                     int               line_no,            /* input  */
                     int               col_no,             /* input  */
                     int               insert_mode,        /* input  */
                     int               writable,           /* input  */
                     int               hold_mode,          /* input  */
                     int               scroll_mode,        /* input  */
                     int               autohold_mode,      /* input  */
                     int               vt100_mode,         /* input  */
                     int               window_wrap,        /* input  */
#ifdef CMD_RECORD_BOX
                     int               recording_mode,     /* input  */
#endif
                     int               modified)           /* input  */

{
int                   baseline_y;
int                   text_len;
int                   font_offcenter;
int                   i;
int                   padding_pixels;
int                   box_y;
int                   box_side;
int                   box_4_x;  /* A - Autohold box */
int                   box_3_x;  /* S - Scroll box */
int                   box_2_x;  /* H - Hold box */
int                   box_1_x;  /* I / R (insert read only box) */
int                   box_0_x;  /* W - window wrap box */
int                   letter_x;
int                   max_name_pixel;
int                   title_width;
int                   title_start;
char                  printed_title[256];
char                  line_text[20];
char                 *box_char;
XCharStruct           overall;
int                   ascent;
int                   descent;
int                   direction;
int                   line_no_x;

static char          *long_title = NULL;
static int            long_title_storage_len = 0;


/***************************************************************
*  
*  First clear out the titlebar
*  
***************************************************************/

DEBUG9(XERRORPOS)
XFillRectangle(display,
               x_pixmap,
               titlebar_subwin->reverse_gc,
               titlebar_subwin->x,     titlebar_subwin->y,
               titlebar_subwin->sub_width, titlebar_subwin->sub_height);



/***************************************************************
*  
*  Figure out where the box will go.  Padding pixels is the
*  amount of space we have to play with in the title bar.  This is
*  the space not used up by the letters.  The box side is the 
*  size of the character plus some percentage of the padding.
*  the "* 40 ) / 100" is an integer way of doing 40%.
*  Box_y says where to put the top of the box.  This is half
*  of what is left plus the offset of the titlebar itself.
*  Thus if box_side uses 40%, There is 60% left over.  Half of
*  This is 30% which iw what is used in figuring out box_y.
*  
***************************************************************/

padding_pixels = titlebar_subwin->height - (titlebar_subwin->font->ascent + titlebar_subwin->font->descent);
box_side = titlebar_subwin->font->ascent + titlebar_subwin->font->descent + ((padding_pixels * 20) / 100);
box_y = ((padding_pixels * 40) / 100) + titlebar_subwin->y + 1; /* The + 1 is an astetic adjustment */
font_offcenter = titlebar_subwin->font->ascent - ((titlebar_subwin->font->ascent + titlebar_subwin->font->descent) / 2);
baseline_y = (box_side / 2) + font_offcenter + box_y - 1; /* The - 1 is an astetic adjustment */

box_4_x = (titlebar_subwin->x + titlebar_subwin->width) - (box_side + 10);
box_3_x = box_4_x - (2 * box_side);
box_2_x = box_3_x - (2 * box_side);
box_1_x = box_2_x - (2 * box_side);
box_0_x = box_1_x - (2 * box_side);

/***************************************************************
*  If we need the line numbers and column numbers, the column
*  numbers eat part of the titlebar space.  Otherwise this is
*  not an edit pad and the W box cannot appear.
***************************************************************/
#ifdef CMD_RECORD_BOX
if ((line_no >= 0) || recording_mode)
#else
if (line_no >= 0)
#endif
   max_name_pixel = box_0_x - (box_side + ((digit_count(col_no, 10) + 2) * titlebar_subwin->font->max_bounds.width));
else
   max_name_pixel = box_1_x - box_side;
title_start = titlebar_subwin->x + MARGIN;

/***************************************************************
*  
*  See if we need to append a node name to the file name.
*  If so, get storage for the string, if needed and load the
*  data into the string.  Then assume this is the title.
*  If the malloc fails, we continue without the node name.
*  
***************************************************************/

if (node_name != NULL && node_name[0] != '\0')
   {
      text_len = strlen(text) + strlen(node_name) + 8;  /* eight is enough padding for the angle brackets, space, and null char */
      if (text_len > long_title_storage_len)
         {
            if (long_title)
               free(long_title);
            long_title = CE_MALLOC(text_len);
         }
      if (long_title != NULL)  /* if the malloc fails, skip the node name in the titlebar */
         {
            snprintf(long_title, text_len, "%s <%s>", text, node_name);
            text = long_title;
         }
   }

/***************************************************************
*  
*  Figure out how much of the title we print.  First test for
*  The whole thing.
*  
***************************************************************/

text_len = strlen(text);
if (titlebar_subwin->fixed_font)
   title_width = (titlebar_subwin->fixed_font * text_len) + title_start;
else
   title_width = XTextWidth(titlebar_subwin->font, text, text_len) + title_start;

if (title_width <= max_name_pixel)
   strcpy(printed_title, text);
else
   {
      if (titlebar_subwin->fixed_font)
         i = (max_name_pixel - title_start) / titlebar_subwin->fixed_font;
      else
         {
            title_width = title_start;
            for (i = 0;
                 title_width < max_name_pixel;
                 i++)
               title_width += XTextWidth(titlebar_subwin->font, text, 1);
            i--;  /* back up 1 */
         }
      snprintf(printed_title, sizeof(printed_title), "...%s", &text[text_len - (i - 3)]);
   }

/***************************************************************
*  
*  Draw the title
*  
***************************************************************/

DEBUG9(XERRORPOS)
XDrawString(display,
            x_pixmap,
            titlebar_subwin->gc,
            title_start,baseline_y,
            printed_title, strlen(printed_title));

/***************************************************************
*
*  letter boxes
*  
*                                flags 0 1 2 3 4
*  title                               W I H S A  
*  title                               r R M   Lineno
*  title                                     V
*
*  Edit windows can have I, R, or blank in flag 1 and lineno
*  right justified in pos 4
*  
***************************************************************/


/***************************************************************
*  
*  Do we need box 0?
*  
***************************************************************/
#ifdef CMD_RECORD_BOX
if (recording_mode || ((line_no >= 0) && window_wrap))
#else
if ((line_no >= 0) && window_wrap)
#endif
   {
      DEBUG9(XERRORPOS)
      XFillRectangle(display,
                     x_pixmap,
                     titlebar_subwin->gc,
                     box_0_x, box_y,
                     box_side, box_side);
#ifdef CMD_RECORD_BOX
      if (recording_mode)
         box_char = "r";
      else
         box_char = "W";
#else
      box_char = "W";
#endif

      DEBUG9(XERRORPOS)
      XTextExtents(titlebar_subwin->font, box_char, 1,
                   &direction,
                   &ascent,
                   &descent,
                   &overall);
     i = overall.rbearing - overall.lbearing;
     letter_x = box_0_x + ((box_side / 2) - (overall.lbearing + (i/2))) - 1;

     DEBUG9(XERRORPOS)
     XDrawString(display,
                 x_pixmap,
                 titlebar_subwin->reverse_gc,
                 letter_x, baseline_y,
                 box_char, 1);
      
   }

/***************************************************************
*  
*  Do we need box 1?
*  
***************************************************************/

#define OVERSTRIKE_BOX
#ifndef OVERSTRIKE_BOX 
if (!writable || insert_mode)
#endif
   {
      DEBUG9(XERRORPOS)
      XFillRectangle(display,
                     x_pixmap,
                     titlebar_subwin->gc,
                     box_1_x, box_y,
                     box_side, box_side);
      if (!writable)
         box_char = "R";
      else
#ifdef OVERSTRIKE_BOX 
         if (insert_mode)
            box_char = "I";
         else
            box_char = "O";
#else
         box_char = "I";
#endif

      XTextExtents(titlebar_subwin->font, box_char, 1,
                   &direction,
                   &ascent,
                   &descent,
                   &overall);
     i = overall.rbearing - overall.lbearing;
     letter_x = box_1_x + ((box_side / 2) - (overall.lbearing + (i/2))) - 1;

     DEBUG9(XERRORPOS)
     XDrawString(display,
                 x_pixmap,
                 titlebar_subwin->reverse_gc,
                 letter_x, baseline_y,
                 box_char, 1);
      
   }

/***************************************************************
*  
*  Do we need a line number or to check on boxes 2 thu 4
*  
***************************************************************/

if (line_no >= 0)
   {
      sprintf(line_text, "%d", line_no+1); /* plus one because we display 1 based line numbers, internal numbers are zero based */
      text_len = strlen(line_text);
      if (titlebar_subwin->fixed_font)
         line_no_x = (box_4_x + box_side) - (titlebar_subwin->fixed_font * text_len);
      else
         line_no_x = (box_4_x + box_side) - XTextWidth(titlebar_subwin->font, line_text, text_len);

      DEBUG9(XERRORPOS)
      XDrawString(display,
                  x_pixmap,
                  titlebar_subwin->gc,
                  line_no_x, baseline_y,
                  line_text, text_len);

      if (col_no > 0)
         {
            sprintf(line_text, "%d", col_no+1); /* plus one because we display 1 based line numbers, internal numbers are zero based */
            text_len = strlen(line_text);
            if (titlebar_subwin->fixed_font)
               line_no_x = box_0_x - ((titlebar_subwin->fixed_font * text_len) + box_side);
            else
               line_no_x = box_0_x - (XTextWidth(titlebar_subwin->font, line_text, text_len) + box_side);

            DEBUG9(XERRORPOS)
            XDrawString(display,
                        x_pixmap,
                        titlebar_subwin->gc,
                        line_no_x, baseline_y,
                        line_text, text_len);

         }
      /***************************************************************
      *  
      *  Do we need the special M in box 2?
      *  
      ***************************************************************/
      if (modified && writable)
         {
             DEBUG9(XERRORPOS)
             XFillRectangle(display,
                            x_pixmap,
                            titlebar_subwin->gc,
                            box_2_x, box_y,
                            box_side, box_side);
             box_char = "M";

             DEBUG9(XERRORPOS)
             XTextExtents(titlebar_subwin->font, box_char, 1,
                          &direction,
                          &ascent,
                          &descent,
                          &overall);
            i = overall.rbearing - overall.lbearing;
            letter_x = box_2_x + ((box_side / 2) - (overall.lbearing + (i/2))) - 1;

            DEBUG9(XERRORPOS)
            XDrawString(display,
                        x_pixmap,
                        titlebar_subwin->reverse_gc,
                        letter_x, baseline_y,
                        box_char, 1);
      
         }

   }
else
   {  /* check out the boxes */
      /***************************************************************
      *  
      *  Do we need an H in box 2?
      *  
      ***************************************************************/

      if (hold_mode)
         {
             DEBUG9(XERRORPOS)
             XFillRectangle(display,
                            x_pixmap,
                            titlebar_subwin->gc,
                            box_2_x, box_y,
                            box_side, box_side);
             box_char = "H";

             DEBUG9(XERRORPOS)
             XTextExtents(titlebar_subwin->font, box_char, 1,
                          &direction,
                          &ascent,
                          &descent,
                          &overall);
            i = overall.rbearing - overall.lbearing;
            letter_x = box_2_x + ((box_side / 2) - (overall.lbearing + (i/2))) - 1;

            DEBUG9(XERRORPOS)
            XDrawString(display,
                        x_pixmap,
                        titlebar_subwin->reverse_gc,
                        letter_x, baseline_y,
                        box_char, 1);
      
         }

      /***************************************************************
      *  
      *  Do we need box 3?
      *  
      ***************************************************************/
#define JUMPBOX
#ifndef JUMPBOX
      if (scroll_mode | vt100_mode)
#endif
         {
            DEBUG9(XERRORPOS)
            XFillRectangle(display,
                           x_pixmap,
                           titlebar_subwin->gc,
                           box_3_x, box_y,
                           box_side, box_side);
            if (vt100_mode)
               box_char = "V";
            else
#ifdef JUMPBOX
               if (scroll_mode)
                  box_char = "S";
               else
                  box_char = "J";
#else
               box_char = "S";
#endif

            XTextExtents(titlebar_subwin->font, box_char, 1,
                         &direction,
                         &ascent,
                         &descent,
                         &overall);
           i = overall.rbearing - overall.lbearing;
           letter_x = box_3_x + ((box_side / 2) - (overall.lbearing + (i/2))) - 1;

           DEBUG9(XERRORPOS)
           XDrawString(display,
                       x_pixmap,
                       titlebar_subwin->reverse_gc,
                       letter_x, baseline_y,
                       box_char, 1);
      
         }


      /***************************************************************
      *  
      *  Do we need box 4?
      *  
      ***************************************************************/

      if (autohold_mode && !vt100_mode)
         {
            DEBUG9(XERRORPOS)
            XFillRectangle(display,
                           x_pixmap,
                           titlebar_subwin->gc,
                           box_4_x, box_y,
                           box_side, box_side);
            box_char = "A";

            XTextExtents(titlebar_subwin->font, box_char, 1,
                         &direction,
                         &ascent,
                         &descent,
                         &overall);
           i = overall.rbearing - overall.lbearing;
           letter_x = box_4_x + ((box_side / 2) - (overall.lbearing + (i/2))) - 1;

           DEBUG9(XERRORPOS)
           XDrawString(display,
                       x_pixmap,
                       titlebar_subwin->reverse_gc,
                       letter_x, baseline_y,
                       box_char, 1);
      
         }
   }  /* else checking out the boxes */




} /* end write_titlebar  */





/************************************************************************

NAME:      resize_titlebar

PURPOSE:    This routine will resize the titlebar window to correctly fit back in the
            main window.
            NOTE:  This routine should be called after the main window
            data has been updated to reflect the resize but before the
            main window subarea calculations are done.  It also has to
            be done after the menu bar, which is above this is resized.

PARAMETERS:

   1.  main_window   - pointer to DRAWABLE_DESCR (INPUT)
               This is a pointer to the main window drawable description.
               The DM windows, will be children of this window.

   2.  titlebar_subwin -  pointer to DRAWABLE_DESCR (OUTPUT)
               Upon return, this drawable description describes the
               titlebar window.


FUNCTIONS :

   1.   Determine the new width of the windows. They are the
        the width of the main window - 4.

   2.   Resize each window.


*************************************************************************/

void  resize_titlebar(DRAWABLE_DESCR   *main_drawable,        /* input   */
                      DRAWABLE_DESCR   *menubar_drawable,     /* input   */
                      DRAWABLE_DESCR   *titlebar_subwin)      /* input / output  */
{
int                   window_height;
		
/***************************************************************
*
*  Determine the new width of the window.
*  The X, Y, and height stay the same.
*
***************************************************************/

window_height = titlebar_subwin->font->ascent + titlebar_subwin->font->descent;
window_height += (window_height * TITLEBAR_HEIGHT_PADDING) / 100;

titlebar_subwin->height    = window_height;
titlebar_subwin->x       = 2;
titlebar_subwin->width     = main_drawable->width - 4;

titlebar_subwin->sub_height  = window_height;
titlebar_subwin->sub_x       = 2;
titlebar_subwin->sub_width = main_drawable->width - 4;


if (menubar_drawable)
   titlebar_subwin->y = menubar_drawable->y + menubar_drawable->height + 1;
else
   titlebar_subwin->y       = 1;

titlebar_subwin->sub_y   = titlebar_subwin->y;


DEBUG1(
   fprintf(stderr, "resize_titlebar:  main:  %dx%d%+d%+d, titlebar:  %dx%d%+d%+d\n",
                   main_drawable->width, main_drawable->height, main_drawable->x, main_drawable->y,
                   titlebar_subwin->width, titlebar_subwin->height, titlebar_subwin->x, titlebar_subwin->y);
)


} /* end of resize_titlebar  */

/************************************************************************

NAME:      digit_count

PURPOSE:    This routine will return the number of  digits in
            a passed number.  Using the passed base to count the digits.

PARAMETERS:

   1.  number   - int (INPUT)
                  This is the number to be checked.

   2.  base     -  int (INPUT)
                   This is the base to use.  For normal numbers the
                   base is 10, just like the number of fingers owned
                   by most people.


FUNCTIONS :

   1.   Count the digits and return them.


RETURNED VALUE:

   The number of spaces needed to hold this number is returned.  Note that
   zero is returned for number value zero.

*************************************************************************/

static int digit_count(long int number, long int base)
{
int     i = 0;

while (number > 0)
{
   number = number / base;
   i++;
}

return(i);

} /* end of digit_count */



