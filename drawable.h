#ifndef _DRAWABLE_H_INCLUDED
#define _DRAWABLE_H_INCLUDED

/* static char *drawable_h_sccsid = "%Z% %M% %I% - %G% %U% ";  */

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
*  Include file drawable.h
*
*  Description of a drawable.  
*  This structure holds the data relevant to a window or pixmap
*  all in one grouping.  Some of this data is always associated
*  with the drawable, such as position, and size.  Other things
*  are grouped for convenience.
*  
***************************************************************/

#ifdef WIN32
#ifdef WIN32_XNT
#include <xnt.h>
#else
#include "X11/X.h"      /* /usr/include/X11/X.h    */
#include "X11/Xlib.h"   /* /usr/include/X11/Xlib.h */
#endif
#else
#include <X11/X.h>      /* /usr/include/X11/X.h    */
#include <X11/Xlib.h>   /* /usr/include/X11/Xlib.h */
#endif

typedef struct {
   int             x;                      /* X origin relative to parent             */
   int             y;                      /* Y origin relative to parent             */
   unsigned int    width;                  /* Drawable Width                          */
   unsigned int    height;                 /* Drawable Height                         */
   GC              gc;                     /* Graphics Content                        */
   GC              reverse_gc;             /* Graphics Content fg x bg                */
   GC              xor_gc;                 /* Graphics Content text cursor            */
   XFontStruct    *font;                   /* font data for this window               */
   int             fixed_font;             /* if nonzero, fixed font width (pixels)   */
   int             line_height;            /* font_height+padding (pixels)            */
   int             lines_on_screen;        /* # lines on current window               */
   int             sub_x;                  /* X origin of usable area relative to x (1st element) */
   int             sub_y;                  /* Y origin of usable area relative to y (2nd element) */
   unsigned int    sub_width;              /* Width of usable area in pixels          */
   unsigned int    sub_height;             /* Height of usable area in pixels         */

} DRAWABLE_DESCR;


/***************************************************************
*  
*  Public part of the scroll bar drawing data.  This is different
*  than the other window descriptions because scroll bars are
*  graphic rather than text.
*  
***************************************************************/

typedef struct {
   Window             vert_window;         /* Xlib Vertical window id                 */
   Window             horiz_window;        /* Xlib horizontal window id               */
   int                vert_window_on;      /* True if scroll bar should be on         */
   int                horiz_window_on;     /* True if scroll bar should be on         */
   int                sb_mode;             /* One of SCROLL_BAR_OFF, ...              */
   int                sb_microseconds;     /* non-zero if sb button is down           */
   XEvent             button_push;         /* Event to pass when button is down       */
   void              *sb_private;          /* allocated by get_sb_windows             */

} SCROLLBAR_DESCR;

/***************************************************************
*  Valid values of SCROLLBAR_DESCR.sb_mode
***************************************************************/
#define SCROLL_BAR_OFF  0
#define SCROLL_BAR_ON   1
#define SCROLL_BAR_AUTO 2


/***************************************************************
*  
*  Linked list of column numbers and gc's for use in drawing
*  Fancy lines.  This list attached to the winlines structure
*  for a given line and contains special drawing information.
*  
***************************************************************/

struct FANCY_LINE  ;

struct FANCY_LINE {
        struct FANCY_LINE      *next;               /* next data in list                                    */
        char                   *bgfg;               /* background / foreground color names                  */
        int                     first_col;          /* First zero based column affected by this Fancy Line  */
        int                     end_col;            /* One past the last column affected by this Fancy Line */
        GC                      gc;                 /* GC with the fancy line information                   */
  };

typedef struct FANCY_LINE FANCY_LINE;


/***************************************************************
*  
*  Description of a window line description.
*  This structure holds pointers to lines currently displayed
*  in the window and their lengths in pixels.  
*  It is updated by textfil_drawable and draw_partial_line.
*  It is also fiddled with by the main program when the modifiable
*  buffer is being swapped in and out for different lines.
*  This structure is used by text_cursor_position and highlight_text.
*  For the main window there is an array of these structures with 
*  enough space for the maximum number of lines in a window.  The
*  same can be said of the dminput and dmoutput windows, but they
*  only have one line.
*  
*  Note that w_file_lines_here is used when lines are excluded.
*  It should only be considered valid during drawing and when
*  processing events.  Once we are in processing code, it the
*  way the screen looks may have changed logically.  Thus we
*  need to use routine lines_xed in cd.c to get this information.
*
***************************************************************/

typedef struct {
   char           *line;                   /* pointer to the line displayed (whole line)    */
   int             pix_len;                /* Length of the line in pixels                  */
   int             tabs;                   /* Flag if there are tabs in the line            */
   int             w_first_char;           /* First char displayed in window                */
#ifdef EXCLUDEP
   int             w_file_line_offset;     /* File line number being displayed - first line on screen, needed for x'ed out data */
   int             w_file_lines_here;      /* Number of lines excluded or 1 if not excluded, needed for x'ed out data */
#endif
   FANCY_LINE     *fancy_line;             /* NULL or pointer to list of fancy drawing data, vt100 only */
} WINDOW_LINE;


#endif

