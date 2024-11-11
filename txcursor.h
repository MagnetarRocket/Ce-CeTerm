#ifndef _TXCURSOR_INCLUDED
#define _TSCURSOR_INCLUDED

/* static char *txcursor_h_sccsid = "%Z% %M% %I% - %G% %U% ";  */



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
*  Routines in txcursor.c
*                                 
*         text_cursor_motion         - Make the text cursor track the mouse cursor.
*         which_line_on_screen       - Convert a Y coordinate to a line number
*         which_char_on_line         - Convert an X coordinate to a character offset
*         which_char_on_fixed_line   - Convert an X coordinate to a character offset for fixed width fonts
*         clear_text_cursor          - Erase the marker that shows where the text cursor is.
*         text_highlight             - Make the highlight area track the text cursor.
*         text_rehighlight           - Redo a cleared text highlight
*         start_text_highlight       - Initialize the highlight environment variables
*         stop_text_highlight        - Erase all text highlight
*         cursor_in_area             - Check if the text cursor is in an area
*         cover_up_cursor            - Copy pixmap data over the cursor
*         min_chars_on_window        - Get mimimum number of chars to fill a window horizontallyumber of chars needed to fill a text subarea of a DRAWABLE_DESCR.
*
***************************************************************/

#include "buffer.h"

/***************************************************************
*  
*  Literal values for echo_mode when doing text highlighting.
*  This is used for the echo mode parm to start_text_highlight.
*  NOTE: NO_HIGHLIGHT must have value zero because of it's use
*        in if statements which detect echo being on.
*  
***************************************************************/

#define  NO_HIGHLIGHT           0
#define  REGULAR_HIGHLIGHT      1
#define  RECTANGULAR_HIGHLIGHT  2




void  text_cursor_motion(DRAWABLE_DESCR   *win,          /* input  */
                         int               echo_mode,    /* input  */
                         XEvent           *event,        /* input  */
                         int              *win_row,      /* output */
                         int              *win_col,      /* output */
                         WINDOW_LINE      *window_lines, /* input  */
                         DISPLAY_DESCR    *dspl_descr);  /* input  */

int   which_line_on_screen(DRAWABLE_DESCR   *win,          /* input  */
                           int               y,            /* input  */
                           int              *line_top_pix);/* output  */

int   which_char_on_line(DRAWABLE_DESCR   *window,         /* input  */
                         XFontStruct      *font,           /* input  */
                         int               x,              /* input  */
                         char             *line,           /* input  */
                         int              *char_left_pix,  /* output  */
                         int              *char_width);    /* output  */

int   which_char_on_fixed_line(DRAWABLE_DESCR   *window,         /* input  */
                               int               x,              /* input  */
                               int               width,          /* input  */
                               int              *char_left_pix); /* output  */

void clear_text_cursor(Window         window,
                       DISPLAY_DESCR *dspl_descr);


void erase_text_cursor(DISPLAY_DESCR *dspl_descr);


void  text_highlight(Window            x_window,     /* input  */
                     DRAWABLE_DESCR   *window,       /* input  */
                     DISPLAY_DESCR    *dspl_descr);

void  text_rehighlight(Window            x_window,     /* input  */
                       DRAWABLE_DESCR   *window,       /* input  */
                       DISPLAY_DESCR    *dspl_descr);  /* input  */


void  start_text_highlight(Window            x_window,     /* input  */
                           DRAWABLE_DESCR   *win,          /* input  */
                           int               win_row,      /* input  */
                           int               win_col,      /* input  */
                           int               echo_mode,    /* input  */
                           WINDOW_LINE      *win_lines,    /* input  */
                           char             *start_line,   /* input  */
                           int               first_char,   /* input  */
                           DISPLAY_DESCR    *dspl_descr);  /* input  */

void  stop_text_highlight(DISPLAY_DESCR    *dspl_descr);

void  clear_text_highlight(DISPLAY_DESCR *dspl_descr);


/***************************************************************
*  
*  Literal values returned by cursor_in_area.  It specifies
*  whether the cursor is not in the area described, part way
*  in the area described, or fully in the area described.
*  
***************************************************************/

#define  NOT_IN_AREA            0
#define  FULLY_IN_AREA          1
#define  PARTIALLY_IN_AREA      2

int cursor_in_area(Window         window,
                   int            x,
                   int            y,
                   unsigned int   width,
                   unsigned int   height,
                   DISPLAY_DESCR *dspl_descr);


#ifdef blah
void cover_up_cursor(PAD_DESCR         *pad,
                     DISPLAY_DESCR     *dspl_descr);
#endif

int min_chars_on_window(DRAWABLE_DESCR   *main_text_area);


#endif

