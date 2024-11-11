#ifndef _XUTIL_H_INCLUDED
#define _XUTIL_H_INCLUDED

/* static char *xutil_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*
*     resize_pixmap      -  Resize a pixmap and copy the data to the newly created pixmap.
*     textfill_drawable  - Fill an area with text read from an input function.
*     draw_partial_line  - Draw a partial line.
*     colorname_to_pixel - Convert a named color to a Pixel value.
*     translate_gc_function - translate a GC function code to the name for debugging
*
***************************************************************/

#ifdef WIN32
#ifdef WIN32_XNT
#include "xnt.h"
#else
#include "X11/Xlib.h"
#include "X11/Xutil.h"
#include "X11/keysym.h"
#endif
#else
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#endif

#include "buffer.h"    /* needed for def of DRAWABLE_DESCR and WINDOW_LINE */
#include "memdata.h"   /* needed for def of DATA_TOKEN */

/***************************************************************
*  
*  Function prototypes
*  
***************************************************************/


int  resize_pixmap(XEvent           *event,            /* input */
                   Drawable         *drawable,         /* input / output */
                   DRAWABLE_DESCR   *pix,              /* input / output */
                   int               bit_gravity);     /* input  */

#define lines_on_drawable(pix) ((pix)->sub_height / (pix)->line_height)

void  textfill_drawable(PAD_DESCR       *pad,               /* input/output */
                        int              overlay,           /* input  */
                        int              skip_lines,        /* input  */
                        int              do_dots);          /* input  */

void draw_partial_line(PAD_DESCR       *pad,               /* input/output */
                       int              win_line_no,       /* input  */
                       int              win_start_char,    /* input  */
                       int              file_line_no,      /* input  */
                       char            *text,              /* input  */
                       int              do_dots,           /* input  */
                       int             *left_side_x);      /* output */

#ifdef blah
void  textfill_drawable(Display          *display,          /* input  */
                        Drawable          drawable,         /* input  */
                        DRAWABLE_DESCR   *subwindow,        /* input  */
                        int               first_file_line,  /* input  */
                        int               first_char,       /* input  */
                        int               overlay,          /* input  */
                        DATA_TOKEN       *token,            /* input / opaque */
                        int               skip_lines,       /* input  */
                        int               do_dots,          /* input  */
                        int              *formfeedpos,      /* output */
                        WINDOW_LINE      *win_lines);       /* output */

void draw_partial_line(Display          *display,      /* input  */
                       Drawable          drawable,     /* input  */
                       DRAWABLE_DESCR   *subwindow,    /* input  */
                       int               line_no,      /* input  */
                       int               first_char,   /* input  */
                       int               char_in_col_0,/* input  */
                       char             *text,         /* input  */
                       int               do_dots,      /* input  */
                       int              *left_side_x,  /* output */
                       int              *formfeedflag, /* output */
                       WINDOW_LINE      *win_lines);   /* output */
#endif

unsigned long colorname_to_pixel(Display   *display,   /* input  */
                                 char      *color_name);/* input  */
/* value returned by colorname_to_pixel on error */
#define BAD_COLOR ((unsigned long)-1)


void translate_gc_function(int function, char *str);

/***************************************************************
*  
*  Test of exclude color extraction.
*  
***************************************************************/

#ifdef MCOLOR
char *get_color_by_num(DATA_TOKEN *token,       /* opaque */
                       int         line_no,     /* input  */
                       int        *color_line_no);  /* output */


#endif
#endif

